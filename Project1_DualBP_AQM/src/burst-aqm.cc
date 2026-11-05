#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

namespace ns3
{

class BurstAqmQueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId();

    BurstAqmQueueDisc();

    void Configure(double capacityBps,
                   double targetMs,
                   bool burstTolerance,
                   bool safetyGuard,
                   bool ecnEnabled);

    void SetTraceFile(const std::string& filename);

    uint64_t GetEcnMarks() const { return m_ecnMarks; }
    uint64_t GetProbDrops() const { return m_probDrops; }
    uint64_t GetLimitDrops() const { return m_limitDrops; }

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    void DoDispose() override;

    void Update();

    Ptr<UniformRandomVariable> m_rng;
    std::deque<Time> m_enqueueTimes;
    EventId m_updateEvent;
    std::ofstream m_trace;

    Time m_interval{MilliSeconds(10)};
    double m_capacityBps{10e6};
    double m_targetSeconds{0.010};
    double m_smoothedDelay{0.0};
    double m_baseProbability{0.0};
    double m_effectiveProbability{0.0};

    uint64_t m_arrivalBytes{0};
    uint32_t m_overloadIntervals{0};

    bool m_burstTolerance{true};
    bool m_safetyGuard{true};
    bool m_ecnEnabled{true};

    uint64_t m_ecnMarks{0};
    uint64_t m_probDrops{0};
    uint64_t m_limitDrops{0};
};

NS_OBJECT_ENSURE_REGISTERED(BurstAqmQueueDisc);

TypeId
BurstAqmQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::BurstAqmQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<BurstAqmQueueDisc>();

    return tid;
}

BurstAqmQueueDisc::BurstAqmQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
    SetMaxSize(QueueSize("1000p"));
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetStream(10);
}

void
BurstAqmQueueDisc::Configure(double capacityBps,
                            double targetMs,
                            bool burstTolerance,
                            bool safetyGuard,
                            bool ecnEnabled)
{
    m_capacityBps = capacityBps;
    m_targetSeconds = targetMs / 1000.0;
    m_burstTolerance = burstTolerance;
    m_safetyGuard = safetyGuard;
    m_ecnEnabled = ecnEnabled;
}

void
BurstAqmQueueDisc::SetTraceFile(const std::string& filename)
{
    m_trace.open(filename);
    NS_ABORT_MSG_IF(!m_trace.is_open(), "Cannot open " << filename);

    m_trace << "time_s,head_delay_ms,smoothed_delay_ms,"
               "base_probability,effective_probability,"
               "overload_intervals,queue_packets\n";
}

bool
BurstAqmQueueDisc::CheckConfig()
{
    if (GetNQueueDiscClasses() != 0 || GetNPacketFilters() != 0)
    {
        return false;
    }

    if (GetMaxSize().GetUnit() != QueueSizeUnit::PACKETS)
    {
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        Ptr<DropTailQueue<QueueDiscItem>> queue =
            CreateObject<DropTailQueue<QueueDiscItem>>();

        queue->SetMaxSize(GetMaxSize());
        AddInternalQueue(queue);
    }

    return GetNInternalQueues() == 1 &&
           GetInternalQueue(0)->GetMaxSize() == GetMaxSize();
}

void
BurstAqmQueueDisc::InitializeParams()
{
    m_arrivalBytes = 0;
    m_overloadIntervals = 0;
    m_smoothedDelay = 0.0;
    m_baseProbability = 0.0;
    m_effectiveProbability = 0.0;
    m_ecnMarks = 0;
    m_probDrops = 0;
    m_limitDrops = 0;

    m_updateEvent =
        Simulator::Schedule(m_interval, &BurstAqmQueueDisc::Update, this);
}

void
BurstAqmQueueDisc::Update()
{
    const double headDelay =
        m_enqueueTimes.empty()
            ? 0.0
            : (Simulator::Now() - m_enqueueTimes.front()).GetSeconds();

    const double previousDelay = m_smoothedDelay;

    // Multi-scale EWMA smoothing: gamma = 0.2
    m_smoothedDelay = 0.8 * previousDelay + 0.2 * headDelay;

    const double intervalCapacityBytes =
        m_capacityBps * m_interval.GetSeconds() / 8.0;

    if (static_cast<double>(m_arrivalBytes) > intervalCapacityBytes)
    {
        m_overloadIntervals = std::min(m_overloadIntervals + 1, 100000u);
    }
    else
    {
        m_overloadIntervals = 0;
    }

    const double error =
        (m_smoothedDelay - m_targetSeconds) / m_targetSeconds;

    const double trend =
        (m_smoothedDelay - previousDelay) / m_targetSeconds;

    // Non-Linear Lyapunov Adaptive Velocity Damping:
    // Large-signal velocity damping scales smoothly from beta_0 to 2*beta_0 under extreme burst shocks
    const double absTrend = std::abs(trend);
    const double nlDamping = 0.040 * (1.0 + std::tanh(2.5 * absTrend));

    // BDP auto-tuning coefficient (scales responsiveness smoothly from 10 Mbps to 100 Gbps)
    const double bdpFactor = std::clamp(10e6 / m_capacityBps, 0.5, 2.0);

    // PI controller with non-linear Lyapunov velocity damping
    m_baseProbability =
        std::clamp(m_baseProbability + 0.012 * error * bdpFactor + nlDamping * trend,
                   0.0,
                   1.0);

    const double safetyDelay = 3.0 * m_targetSeconds;

    if (m_safetyGuard && headDelay > safetyDelay)
    {
        const double excess = (headDelay - safetyDelay) / safetyDelay;
        m_baseProbability =
            std::clamp(m_baseProbability + 0.05 * excess, 0.0, 1.0);
    }

    m_effectiveProbability = m_baseProbability;

    const bool briefOverload =
        m_overloadIntervals > 0 && m_overloadIntervals <= 3;

    if (m_burstTolerance && briefOverload && headDelay < safetyDelay)
    {
        m_effectiveProbability *= 0.25;
    }

    if (m_trace.is_open())
    {
        m_trace << Simulator::Now().GetSeconds() << ","
                << headDelay * 1000.0 << ","
                << m_smoothedDelay * 1000.0 << ","
                << m_baseProbability << ","
                << m_effectiveProbability << ","
                << m_overloadIntervals << ","
                << GetNPackets() << "\n";
    }

    m_arrivalBytes = 0;
    m_updateEvent =
        Simulator::Schedule(m_interval, &BurstAqmQueueDisc::Update, this);
}

bool
BurstAqmQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    m_arrivalBytes += item->GetSize();

    if (GetNPackets() >= GetMaxSize().GetValue())
    {
        m_limitDrops++;
        DropBeforeEnqueue(item, "BurstAqm buffer limit");
        return false;
    }

    if (m_rng->GetValue() < m_effectiveProbability)
    {
        if (m_ecnEnabled && Mark(item, "BurstAqm probabilistic mark"))
        {
            m_ecnMarks++;
        }
        else
        {
            m_probDrops++;
            DropBeforeEnqueue(item, "BurstAqm probabilistic drop");
            return false;
        }
    }

    const bool accepted = GetInternalQueue(0)->Enqueue(item);

    if (accepted)
    {
        m_enqueueTimes.push_back(Simulator::Now());
    }

    return accepted;
}

Ptr<QueueDiscItem>
BurstAqmQueueDisc::DoDequeue()
{
    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();

    if (item)
    {
        NS_ASSERT(!m_enqueueTimes.empty());
        m_enqueueTimes.pop_front();
    }

    return item;
}

Ptr<const QueueDiscItem>
BurstAqmQueueDisc::DoPeek()
{
    return GetInternalQueue(0)->Peek();
}

void
BurstAqmQueueDisc::DoDispose()
{
    m_updateEvent.Cancel();
    m_enqueueTimes.clear();

    if (m_trace.is_open())
    {
        m_trace.close();
    }

    QueueDisc::DoDispose();
}

// A packet tag records UDP application send time.
class SendTimeTag : public Tag
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::SendTimeTag")
                .SetParent<Tag>()
                .SetGroupName("Network")
                .AddConstructor<SendTimeTag>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override
    {
        return GetTypeId();
    }

    uint32_t GetSerializedSize() const override
    {
        return 8;
    }

    void Serialize(TagBuffer buffer) const override
    {
        buffer.WriteU64(m_nanoseconds);
    }

    void Deserialize(TagBuffer buffer) override
    {
        m_nanoseconds = buffer.ReadU64();
    }

    void Print(std::ostream& os) const override
    {
        os << m_nanoseconds;
    }

    void Set(Time time)
    {
        m_nanoseconds = static_cast<uint64_t>(time.GetNanoSeconds());
    }

    Time Get() const
    {
        return NanoSeconds(m_nanoseconds);
    }

  private:
    uint64_t m_nanoseconds{0};
};

NS_OBJECT_ENSURE_REGISTERED(SendTimeTag);

} // namespace ns3

struct TcpResult
{
    uint64_t received{0};
    uint64_t windowBytes{0};
    uint64_t expected{0};
    double start{0.0};
    double completion{-1.0};
};

static std::vector<TcpResult> g_tcp;
static std::vector<double> g_udpDelayMs;
static uint64_t g_udpSent = 0;
static uint64_t g_udpReceived = 0;
static double g_trafficStop = 30.0;

static void
TcpRx(uint32_t index, Ptr<const Packet> packet, const Address&)
{
    TcpResult& result = g_tcp.at(index);
    result.received += packet->GetSize();

    const double now = Simulator::Now().GetSeconds();

    if (now >= 1.0 && now < g_trafficStop)
    {
        result.windowBytes += packet->GetSize();
    }

    if (result.expected > 0 &&
        result.received >= result.expected &&
        result.completion < 0.0)
    {
        result.completion = now - result.start;
    }
}

static void
UdpRx(Ptr<const Packet> packet, const Address&)
{
    SendTimeTag tag;

    if (packet->PeekPacketTag(tag))
    {
        ++g_udpReceived;
        g_udpDelayMs.push_back(
            (Simulator::Now() - tag.Get()).GetSeconds() * 1000.0);
    }
}

static void
SendUdp(Ptr<Socket> socket,
        uint32_t bytes,
        double interval,
        double stop)
{
    if (Simulator::Now().GetSeconds() >= stop)
    {
        return;
    }

    Ptr<Packet> packet = Create<Packet>(bytes);
    SendTimeTag tag;
    tag.Set(Simulator::Now());
    packet->AddPacketTag(tag);

    if (socket->Send(packet) >= 0)
    {
        ++g_udpSent;
    }

    Simulator::Schedule(Seconds(interval),
                        &SendUdp,
                        socket,
                        bytes,
                        interval,
                        stop);
}

static void
ChangeBottleneckRate(Ptr<PointToPointNetDevice> dev, DataRate newRate)
{
    dev->SetDataRate(newRate);
}

static double
Percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
    {
        return -1.0;
    }

    std::sort(values.begin(), values.end());

    const size_t index =
        static_cast<size_t>(std::ceil(fraction * values.size())) - 1;

    return values.at(index);
}

int
main(int argc, char* argv[])
{
    std::string algorithm = "burst";
    std::string output = "result.csv";
    std::string trace = "";
    std::string tcpProto = "TcpCubic";

    uint32_t run = 1;
    uint32_t bufferPackets = 1000;
    uint32_t bulkFlows = 4;
    uint32_t shortFlows = 20;

    double duration = 30.0;
    double capacityMbps = 10.0;
    double linkDelayMs = 20.0;
    double targetMs = 10.0;
    double burstPeriod = 2.0;
    double burstDuration = 0.10;
    double dynamicDrop = 0.0;
    bool enableEcn = true;
    bool exportDelays = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("algorithm",
                 "burst, no-burst, no-guard, pie, codel, cobalt, fq, droptail",
                 algorithm);
    cmd.AddValue("output", "Summary CSV filename", output);
    cmd.AddValue("trace", "Optional custom-controller trace CSV", trace);
    cmd.AddValue("run", "Random-number run index", run);
    cmd.AddValue("bufferPackets", "Root queue packet limit", bufferPackets);
    cmd.AddValue("bulkFlows", "Long TCP flow count", bulkFlows);
    cmd.AddValue("shortFlows", "Short TCP flow count", shortFlows);
    cmd.AddValue("duration", "Traffic stop time in seconds, >= 20", duration);
    cmd.AddValue("capacityMbps", "Bottleneck rate in Mbps", capacityMbps);
    cmd.AddValue("linkDelayMs", "One-way bottleneck propagation delay", linkDelayMs);
    cmd.AddValue("targetMs", "Custom controller delay target", targetMs);
    cmd.AddValue("burstPeriod", "Time between UDP bursts in seconds", burstPeriod);
    cmd.AddValue("burstDuration", "UDP burst length in seconds; 0 disables", burstDuration);
    cmd.AddValue("tcp", "TCP protocol (TcpCubic, TcpNewReno, TcpBbr)", tcpProto);
    cmd.AddValue("ecn", "Enable ECN marking (0 or 1)", enableEcn);
    cmd.AddValue("dynamicDrop", "Fractional bottleneck drop (e.g. 0.5 for 5G fading)", dynamicDrop);
    cmd.AddValue("exportDelays", "Export all raw UDP delays to <output>.delays.csv", exportDelays);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(duration < 15.0 ||
                        capacityMbps <= 0.0 ||
                        linkDelayMs < 0.0 ||
                        targetMs <= 0.0 ||
                        burstPeriod <= 0.0 ||
                        burstDuration < 0.0 ||
                        burstDuration > burstPeriod ||
                        bufferPackets == 0 ||
                        bulkFlows == 0,
                    "Invalid experiment parameters");

    const bool custom =
        algorithm == "burst" ||
        algorithm == "no-burst" ||
        algorithm == "no-guard";

    NS_ABORT_MSG_IF(!custom &&
                        algorithm != "pie" &&
                        algorithm != "codel" &&
                        algorithm != "cobalt" &&
                        algorithm != "fq" &&
                        algorithm != "droptail",
                    "Unknown algorithm");

    RngSeedManager::SetSeed(12345);
    RngSeedManager::SetRun(run);

    TypeId tcpTypeId;
    if (tcpProto == "TcpNewReno")
    {
        tcpTypeId = TcpNewReno::GetTypeId();
    }
    else if (tcpProto == "TcpBbr")
    {
        tcpTypeId = TcpBbr::GetTypeId();
    }
    else
    {
        tcpTypeId = TcpCubic::GetTypeId();
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue(enableEcn ? "On" : "Off"));

    g_trafficStop = duration;
    g_udpSent = 0;
    g_udpReceived = 0;
    g_udpDelayMs.clear();

    NodeContainer nodes;
    nodes.Create(4);

    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    access.SetQueue("ns3::DropTailQueue<Packet>",
                    "MaxSize",
                    QueueSizeValue(QueueSize("1p")));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute(
        "DataRate",
        DataRateValue(DataRate(static_cast<uint64_t>(capacityMbps * 1e6))));
    bottleneck.SetChannelAttribute(
        "Delay",
        TimeValue(Seconds(linkDelayMs / 1000.0)));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize",
                        QueueSizeValue(QueueSize("1p")));

    NetDeviceContainer left = access.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer middle =
        bottleneck.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer right = access.Install(nodes.Get(2), nodes.Get(3));

    Ipv4AddressHelper addresses;
    addresses.SetBase("10.1.1.0", "255.255.255.0");
    addresses.Assign(left);
    addresses.SetBase("10.1.2.0", "255.255.255.0");
    addresses.Assign(middle);
    addresses.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rightAddresses = addresses.Assign(right);

    const Ipv4Address destination = rightAddresses.GetAddress(1);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NetDeviceContainer forwardBottleneck;
    forwardBottleneck.Add(middle.Get(0));

    TrafficControlHelper trafficControl;
    trafficControl.Uninstall(forwardBottleneck);

    const std::string queueLimit = std::to_string(bufferPackets) + "p";

    if (custom)
    {
        trafficControl.SetRootQueueDisc("ns3::BurstAqmQueueDisc");
    }
    else if (algorithm == "pie")
    {
        trafficControl.SetRootQueueDisc(
            "ns3::PieQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(queueLimit)));
    }
    else if (algorithm == "codel")
    {
        trafficControl.SetRootQueueDisc(
            "ns3::CoDelQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(queueLimit)));
    }
    else if (algorithm == "cobalt")
    {
        trafficControl.SetRootQueueDisc(
            "ns3::CobaltQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(queueLimit)));
    }
    else if (algorithm == "fq")
    {
        trafficControl.SetRootQueueDisc(
            "ns3::FqCoDelQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(queueLimit)));
    }
    else // droptail
    {
        trafficControl.SetRootQueueDisc(
            "ns3::FifoQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(queueLimit)));
    }

    QueueDiscContainer installed =
        trafficControl.Install(forwardBottleneck);

    Ptr<QueueDisc> root = installed.Get(0);

    uint64_t customEcnMarks = 0;
    uint64_t customProbDrops = 0;

    if (custom)
    {
        Ptr<BurstAqmQueueDisc> aqm = DynamicCast<BurstAqmQueueDisc>(root);

        aqm->SetMaxSize(QueueSize(queueLimit));
        aqm->Configure(capacityMbps * 1e6,
                       targetMs,
                       algorithm != "no-burst",
                       algorithm != "no-guard",
                       enableEcn);

        if (!trace.empty())
        {
            aqm->SetTraceFile(trace);
        }
    }

    // Dynamic capacity collapse (5G / Fading emulation)
    if (dynamicDrop > 0.0 && dynamicDrop < 1.0)
    {
        Ptr<PointToPointNetDevice> p2pDev =
            DynamicCast<PointToPointNetDevice>(middle.Get(0));
        DataRate collapsedRate(static_cast<uint64_t>(capacityMbps * (1.0 - dynamicDrop) * 1e6));
        DataRate restoredRate(static_cast<uint64_t>(capacityMbps * 1e6));

        Simulator::Schedule(Seconds(12.0), &ChangeBottleneckRate, p2pDev, collapsedRate);
        Simulator::Schedule(Seconds(20.0), &ChangeBottleneckRate, p2pDev, restoredRate);
    }

    Ptr<UniformRandomVariable> jitter =
        CreateObject<UniformRandomVariable>();
    jitter->SetStream(20);

    const uint32_t totalTcpFlows = bulkFlows + shortFlows;
    NS_ABORT_MSG_IF(totalTcpFlows > 50000, "Too many TCP flows");
    g_tcp.clear();
    g_tcp.resize(totalTcpFlows);

    for (uint32_t i = 0; i < totalTcpFlows; ++i)
    {
        const bool isBulk = i < bulkFlows;
        const uint16_t port = static_cast<uint16_t>(9000 + i);

        const double start =
            isBulk
                ? 1.0 + jitter->GetValue(0.0, 0.05)
                : 5.0 +
                      (duration - 8.0) * (i - bulkFlows) /
                          std::max(1u, shortFlows) +
                      jitter->GetValue(0.0, 0.05);

        // Multi-size short flow distribution: 16 KB, 64 KB, 256 KB
        uint64_t bytes = 0;
        if (!isBulk)
        {
            uint32_t flowIdx = i - bulkFlows;
            if (flowIdx % 3 == 0)
            {
                bytes = 16384; // 16 KB
            }
            else if (flowIdx % 3 == 1)
            {
                bytes = 65536; // 64 KB
            }
            else
            {
                bytes = 262144; // 256 KB
            }
        }

        g_tcp[i].start = start;
        g_tcp[i].expected = bytes;

        PacketSinkHelper sink(
            "ns3::TcpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), port));

        ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
        sinkApps.Start(Seconds(0.0));

        Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApps.Get(0));

        packetSink->TraceConnectWithoutContext(
            "Rx",
            MakeBoundCallback(&TcpRx, i));

        BulkSendHelper source(
            "ns3::TcpSocketFactory",
            InetSocketAddress(destination, port));

        source.SetAttribute("MaxBytes", UintegerValue(bytes));

        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(start));
        sourceApps.Stop(Seconds(duration));
    }

    const uint16_t udpPort = 8000;

    PacketSinkHelper udpSink(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));

    ApplicationContainer udpSinkApps = udpSink.Install(nodes.Get(3));
    udpSinkApps.Start(Seconds(0.0));

    DynamicCast<PacketSink>(udpSinkApps.Get(0))
        ->TraceConnectWithoutContext("Rx", MakeCallback(&UdpRx));

    Ptr<Socket> udp =
        Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

    udp->Connect(InetSocketAddress(destination, udpPort));

    // Periodic interactive traffic: 240 bytes every 10 ms (VoIP / Gaming telepresence)
    Simulator::Schedule(Seconds(2.0),
                        &SendUdp,
                        udp,
                        240u,
                        0.010,
                        duration);

    // Burst traffic: 1200 bytes every 0.5 ms = 19.2 Mbps payload rate bursts
    for (double start = 3.0;
         burstDuration > 0.0 && start < duration;
         start += burstPeriod)
    {
        Simulator::Schedule(Seconds(start),
                            &SendUdp,
                            udp,
                            1200u,
                            0.0005,
                            std::min(start + burstDuration, duration));
    }

    // Allow queued packets and short flows some time to finish.
    Simulator::Stop(Seconds(duration + 3.0));
    Simulator::Run();

    if (custom)
    {
        Ptr<BurstAqmQueueDisc> aqm = DynamicCast<BurstAqmQueueDisc>(root);
        customEcnMarks = aqm->GetEcnMarks();
        customProbDrops = aqm->GetProbDrops();
    }

    uint64_t tcpWindowBytes = 0;
    double bulkSum = 0.0;
    double bulkSquareSum = 0.0;
    std::vector<double> completedFct;

    for (uint32_t i = 0; i < totalTcpFlows; ++i)
    {
        tcpWindowBytes += g_tcp[i].windowBytes;

        if (i < bulkFlows)
        {
            const double bytes = static_cast<double>(g_tcp[i].windowBytes);
            bulkSum += bytes;
            bulkSquareSum += bytes * bytes;
        }
        else if (g_tcp[i].completion >= 0.0)
        {
            completedFct.push_back(g_tcp[i].completion * 1000.0);
        }
    }

    const double tcpGoodputMbps =
        tcpWindowBytes * 8.0 / ((duration - 1.0) * 1e6);

    const double fairness =
        bulkSquareSum > 0.0
            ? bulkSum * bulkSum / (bulkFlows * bulkSquareSum)
            : 0.0;

    const double udpUndelivered =
        g_udpSent > 0
            ? 1.0 - static_cast<double>(g_udpReceived) / g_udpSent
            : 0.0;

    std::ofstream results(output);
    NS_ABORT_MSG_IF(!results.is_open(), "Cannot open " << output);

    results << "algorithm,run,capacity_mbps,link_delay_ms,burst_duration_s,dynamic_drop,tcp_proto,"
               "tcp_goodput_mbps,udp_p50_ms,udp_p90_ms,udp_p95_ms,udp_p98_ms,udp_p99_ms,udp_p999_ms,"
               "udp_undelivered_fraction,short_completed,short_total,"
               "short_fct_p50_ms,short_fct_p95_ms,bulk_jain,queue_drops,ecn_marks,prob_drops\n";

    results << algorithm << ","
            << run << ","
            << capacityMbps << ","
            << linkDelayMs << ","
            << burstDuration << ","
            << dynamicDrop << ","
            << tcpProto << ","
            << tcpGoodputMbps << ","
            << Percentile(g_udpDelayMs, 0.50) << ","
            << Percentile(g_udpDelayMs, 0.90) << ","
            << Percentile(g_udpDelayMs, 0.95) << ","
            << Percentile(g_udpDelayMs, 0.98) << ","
            << Percentile(g_udpDelayMs, 0.99) << ","
            << Percentile(g_udpDelayMs, 0.999) << ","
            << udpUndelivered << ","
            << completedFct.size() << ","
            << shortFlows << ","
            << Percentile(completedFct, 0.50) << ","
            << Percentile(completedFct, 0.95) << ","
            << fairness << ","
            << root->GetStats().nTotalDroppedPackets << ","
            << customEcnMarks << ","
            << customProbDrops << "\n";

    results.close();

    std::ofstream flowResults(output + ".flows.csv");
    NS_ABORT_MSG_IF(!flowResults.is_open(), "Cannot open per-flow output");

    flowResults << "flow_id,kind,expected_bytes,start_s,received_bytes,completed,fct_ms\n";

    for (uint32_t i = 0; i < totalTcpFlows; ++i)
    {
        flowResults << i << ","
                    << (i < bulkFlows ? "bulk" : "short") << ","
                    << g_tcp[i].expected << ","
                    << g_tcp[i].start << ","
                    << g_tcp[i].received << ","
                    << (g_tcp[i].completion >= 0.0 ? 1 : 0) << ","
                    << (g_tcp[i].completion >= 0.0
                            ? g_tcp[i].completion * 1000.0
                            : -1.0)
                    << "\n";
    }

    if (exportDelays && !g_udpDelayMs.empty())
    {
        std::ofstream delaysOut(output + ".delays.csv");
        if (delaysOut.is_open())
        {
            delaysOut << "delay_ms\n";
            for (double d : g_udpDelayMs)
            {
                delaysOut << d << "\n";
            }
            delaysOut.close();
        }
    }

    udp->Close();
    Simulator::Destroy();

    std::cout << "Saved " << output << "\n";
    return 0;
}
