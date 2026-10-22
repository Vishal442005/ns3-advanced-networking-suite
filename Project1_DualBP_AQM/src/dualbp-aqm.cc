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

// Packet Tag to record send time
class DualSendTimeTag : public Tag
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("ns3::DualSendTimeTag")
                                .SetParent<Tag>()
                                .AddConstructor<DualSendTimeTag>();
        return tid;
    }
    TypeId GetInstanceTypeId() const override { return GetTypeId(); }
    uint32_t GetSerializedSize() const override { return sizeof(uint64_t); }
    void Serialize(TagBuffer i) const override { i.WriteU64(m_sendTimeNs); }
    void Deserialize(TagBuffer i) override { m_sendTimeNs = i.ReadU64(); }
    void Print(std::ostream& os) const override { os << "SendTime=" << m_sendTimeNs; }

    void SetTime(Time t) { m_sendTimeNs = t.GetNanoSeconds(); }
    Time GetTime() const { return NanoSeconds(m_sendTimeNs); }

  private:
    uint64_t m_sendTimeNs{0};
};

NS_OBJECT_ENSURE_REGISTERED(DualSendTimeTag);

// ============================================================================
// Single-Queue Burst-Persistence AQM (SingleBP)
// ============================================================================
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

    Time m_interval{MilliSeconds(10)};
    double m_capacityBps{20e6};
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
    SetMaxSize(QueueSize("1500p"));
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetStream(77);
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

bool
BurstAqmQueueDisc::CheckConfig()
{
    if (GetNQueueDiscClasses() != 0 || GetNPacketFilters() != 0)
    {
        return false;
    }
    if (GetNInternalQueues() == 0)
    {
        Ptr<DropTailQueue<QueueDiscItem>> queue = CreateObject<DropTailQueue<QueueDiscItem>>();
        queue->SetMaxSize(GetMaxSize());
        AddInternalQueue(queue);
    }
    return GetNInternalQueues() == 1;
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
    m_updateEvent = Simulator::Schedule(m_interval, &BurstAqmQueueDisc::Update, this);
}

void
BurstAqmQueueDisc::Update()
{
    const double headDelay =
        m_enqueueTimes.empty()
            ? 0.0
            : (Simulator::Now() - m_enqueueTimes.front()).GetSeconds();

    const double previousDelay = m_smoothedDelay;
    m_smoothedDelay = 0.8 * previousDelay + 0.2 * headDelay;

    const double intervalCapacityBytes = m_capacityBps * m_interval.GetSeconds() / 8.0;

    if (static_cast<double>(m_arrivalBytes) > intervalCapacityBytes)
    {
        m_overloadIntervals = std::min(m_overloadIntervals + 1, 100000u);
    }
    else
    {
        m_overloadIntervals = 0;
    }

    const double error = (m_smoothedDelay - m_targetSeconds) / m_targetSeconds;
    const double trend = (m_smoothedDelay - previousDelay) / m_targetSeconds;

    m_baseProbability = std::clamp(m_baseProbability + 0.015 * error + 0.035 * trend, 0.0, 1.0);

    const double safetyDelay = 3.0 * m_targetSeconds;
    if (m_safetyGuard && headDelay > safetyDelay)
    {
        const double excess = (headDelay - safetyDelay) / safetyDelay;
        m_baseProbability = std::clamp(m_baseProbability + 0.06 * excess, 0.0, 1.0);
    }

    m_effectiveProbability = m_baseProbability;
    const bool briefOverload = (m_overloadIntervals > 0 && m_overloadIntervals <= 3);

    if (m_burstTolerance && briefOverload && headDelay < safetyDelay)
    {
        m_effectiveProbability *= 0.20;
    }

    m_arrivalBytes = 0;
    m_updateEvent = Simulator::Schedule(m_interval, &BurstAqmQueueDisc::Update, this);
}

bool
BurstAqmQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    m_arrivalBytes += item->GetSize();
    if (GetNPackets() >= GetMaxSize().GetValue())
    {
        m_limitDrops++;
        DropBeforeEnqueue(item, "SingleBP buffer limit");
        return false;
    }

    if (m_rng->GetValue() < m_effectiveProbability)
    {
        if (m_ecnEnabled && Mark(item, "SingleBP probabilistic mark"))
        {
            m_ecnMarks++;
        }
        else
        {
            m_probDrops++;
            DropBeforeEnqueue(item, "SingleBP probabilistic drop");
            return false;
        }
    }

    bool accepted = GetInternalQueue(0)->Enqueue(item);
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
    QueueDisc::DoDispose();
}

// ============================================================================
// DualBP Queue Disc: Coupled Dual-Queue for L4S and Classic Traffic (RFC 9332)
// Queue 0: L4S Queue (ECT1/CE) - Step Marking at 1.0 ms Target Delay
// Queue 1: Classic Queue (Not-ECT / ECT0) - Burst-Persistence AQM
// Coupling: p_L = min(1.0, k_coupled * p_C^2)
// Deficit Scheduler: Guarantees zero delay leakage and throughput fairness
// ============================================================================
class DualBpQueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    DualBpQueueDisc();
    ~DualBpQueueDisc() override = default;

    void Configure(double capacityBps,
                   double classicTargetMs,
                   double l4sTargetMs,
                   bool burstTolerance,
                   bool safetyGuard,
                   double kCoupled);

    void SetTraceFile(const std::string& filename);

    uint64_t GetL4sMarks() const { return m_l4sMarks; }
    uint64_t GetClassicDrops() const { return m_classicDrops; }
    uint64_t GetClassicMarks() const { return m_classicMarks; }
    uint64_t GetLimitDrops() const { return m_limitDrops; }
    uint64_t GetL4sPackets() const { return m_l4sPackets; }
    uint64_t GetClassicPackets() const { return m_classicPackets; }

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    void DoDispose() override;

    void Update();

    Ptr<UniformRandomVariable> m_rng;
    std::deque<Time> m_l4sEnqueueTimes;
    std::deque<Time> m_classicEnqueueTimes;
    EventId m_updateEvent;
    std::ofstream m_trace;

    Time m_interval{MilliSeconds(10)};
    double m_capacityBps{20e6};
    double m_classicTargetSec{0.010};   // 10 ms
    double m_l4sTargetSec{0.001};       // 1 ms (RFC 9332 target)
    double m_classicSmoothedDelay{0.0};
    double m_classicBaseProb{0.0};
    double m_classicEffProb{0.0};
    double m_coupledL4sProb{0.0};
    double m_kCoupled{2.0};

    uint64_t m_classicArrivalBytes{0};
    uint32_t m_classicOverloadIntervals{0};

    bool m_burstTolerance{true};
    bool m_safetyGuard{true};

    // Deficit scheduler state (credits in bytes)
    int32_t m_l4sDeficit{1500};
    int32_t m_classicDeficit{1500};
    const int32_t m_quantum{1500};

    uint64_t m_l4sMarks{0};
    uint64_t m_classicDrops{0};
    uint64_t m_classicMarks{0};
    uint64_t m_limitDrops{0};
    uint64_t m_l4sPackets{0};
    uint64_t m_classicPackets{0};
};

NS_OBJECT_ENSURE_REGISTERED(DualBpQueueDisc);

TypeId
DualBpQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DualBpQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<DualBpQueueDisc>();
    return tid;
}

DualBpQueueDisc::DualBpQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES)
{
    SetMaxSize(QueueSize("1500p"));
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetStream(42);
}

void
DualBpQueueDisc::Configure(double capacityBps,
                          double classicTargetMs,
                          double l4sTargetMs,
                          bool burstTolerance,
                          bool safetyGuard,
                          double kCoupled)
{
    m_capacityBps = capacityBps;
    m_classicTargetSec = classicTargetMs / 1000.0;
    m_l4sTargetSec = l4sTargetMs / 1000.0;
    m_burstTolerance = burstTolerance;
    m_safetyGuard = safetyGuard;
    m_kCoupled = kCoupled;
}

void
DualBpQueueDisc::SetTraceFile(const std::string& filename)
{
    m_trace.open(filename);
    if (m_trace.is_open())
    {
        m_trace << "time_s,classic_delay_ms,l4s_delay_ms,classic_eff_prob,coupled_l4s_prob,"
                << "l4s_qlen,classic_qlen\n";
    }
}

bool
DualBpQueueDisc::CheckConfig()
{
    if (GetNQueueDiscClasses() != 0 || GetNPacketFilters() != 0)
    {
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // Queue 0: L4S Queue
        Ptr<DropTailQueue<QueueDiscItem>> qL4s = CreateObject<DropTailQueue<QueueDiscItem>>();
        qL4s->SetMaxSize(QueueSize("1000p"));
        AddInternalQueue(qL4s);

        // Queue 1: Classic Queue
        Ptr<DropTailQueue<QueueDiscItem>> qClassic = CreateObject<DropTailQueue<QueueDiscItem>>();
        qClassic->SetMaxSize(QueueSize("1000p"));
        AddInternalQueue(qClassic);
    }

    return GetNInternalQueues() == 2;
}

void
DualBpQueueDisc::InitializeParams()
{
    m_classicArrivalBytes = 0;
    m_classicOverloadIntervals = 0;
    m_classicSmoothedDelay = 0.0;
    m_classicBaseProb = 0.0;
    m_classicEffProb = 0.0;
    m_coupledL4sProb = 0.0;
    m_l4sMarks = 0;
    m_classicDrops = 0;
    m_classicMarks = 0;
    m_limitDrops = 0;
    m_l4sPackets = 0;
    m_classicPackets = 0;
    m_l4sDeficit = m_quantum;
    m_classicDeficit = m_quantum;

    m_updateEvent = Simulator::Schedule(m_interval, &DualBpQueueDisc::Update, this);
}

void
DualBpQueueDisc::Update()
{
    const double headDelayClassic =
        m_classicEnqueueTimes.empty()
            ? 0.0
            : (Simulator::Now() - m_classicEnqueueTimes.front()).GetSeconds();

    const double previousDelay = m_classicSmoothedDelay;
    m_classicSmoothedDelay = 0.8 * previousDelay + 0.2 * headDelayClassic;

    const double intervalCapacityBytes = m_capacityBps * m_interval.GetSeconds() / 8.0;

    if (static_cast<double>(m_classicArrivalBytes) > intervalCapacityBytes)
    {
        m_classicOverloadIntervals = std::min(m_classicOverloadIntervals + 1, 100000u);
    }
    else
    {
        m_classicOverloadIntervals = 0;
    }

    const double error = (m_classicSmoothedDelay - m_classicTargetSec) / m_classicTargetSec;
    const double trend = (m_classicSmoothedDelay - previousDelay) / m_classicTargetSec;

    // Non-linear Lyapunov adaptive damping:
    const double absTrend = std::abs(trend);
    const double nlDamping = 0.035 * (1.0 + std::tanh(2.5 * absTrend));
    const double bdpFactor = std::clamp(20e6 / m_capacityBps, 0.5, 2.0);

    // Classic PI control update with non-linear damping and BDP adaptation
    m_classicBaseProb = std::clamp(m_classicBaseProb + 0.015 * error * bdpFactor + nlDamping * trend, 0.0, 1.0);

    const double safetyDelay = 3.0 * m_classicTargetSec;
    if (m_safetyGuard && headDelayClassic > safetyDelay)
    {
        const double excess = (headDelayClassic - safetyDelay) / safetyDelay;
        m_classicBaseProb = std::clamp(m_classicBaseProb + 0.06 * excess, 0.0, 1.0);
    }

    m_classicEffProb = m_classicBaseProb;
    const bool briefOverload = (m_classicOverloadIntervals > 0 && m_classicOverloadIntervals <= 3);

    // Burst persistence headroom attenuation
    if (m_burstTolerance && briefOverload && headDelayClassic < safetyDelay)
    {
        m_classicEffProb *= 0.20;
    }

    // Coupled L4S probability (RFC 9332 square law)
    m_coupledL4sProb = std::clamp(m_kCoupled * (m_classicEffProb * m_classicEffProb), 0.0, 1.0);

    const double headDelayL4s =
        m_l4sEnqueueTimes.empty()
            ? 0.0
            : (Simulator::Now() - m_l4sEnqueueTimes.front()).GetSeconds();

    if (m_trace.is_open())
    {
        m_trace << Simulator::Now().GetSeconds() << ","
                << headDelayClassic * 1000.0 << ","
                << headDelayL4s * 1000.0 << ","
                << m_classicEffProb << ","
                << m_coupledL4sProb << ","
                << GetInternalQueue(0)->GetNPackets() << ","
                << GetInternalQueue(1)->GetNPackets() << "\n";
    }

    m_classicArrivalBytes = 0;
    m_updateEvent = Simulator::Schedule(m_interval, &DualBpQueueDisc::Update, this);
}

bool
DualBpQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
// Total aggregate queue limit
    if (GetNPackets() >= GetMaxSize().GetValue())
    {
        m_limitDrops++;
        DropBeforeEnqueue(item, "DualBP aggregate buffer limit exceeded");
        return false;
    }

    // Inspect IP ECN codepoint
    uint8_t tosByte = 0;
    bool isL4s = false;
    if (item->GetUint8Value(QueueItem::IP_DSFIELD, tosByte))
    {
        uint8_t ecn = tosByte & 0x03;
        if (ecn == 1 || ecn == 3) // ECT(1) or CE -> L4S
        {
            isL4s = true;
        }
    }

    if (isL4s)
    {
        // Enqueue to L4S Queue (Queue 0)
        m_l4sPackets++;
        bool accepted = GetInternalQueue(0)->Enqueue(item);
        if (accepted)
        {
            m_l4sEnqueueTimes.push_back(Simulator::Now());
        }
        return accepted;
    }
    else
    {
        // Classic Queue (Queue 1)
        m_classicPackets++;
        m_classicArrivalBytes += item->GetSize();

        // BP-AQM proactive drop/mark decision
        if (m_rng->GetValue() < m_classicEffProb)
        {
            // If ECT(0) is set, mark CE instead of dropping
            if ((tosByte & 0x03) == 2 && Mark(item, "DualBP Classic ECT0 probabilistic mark"))
            {
                m_classicMarks++;
            }
            else
            {
                m_classicDrops++;
                DropBeforeEnqueue(item, "DualBP Classic probabilistic drop");
                return false;
            }
        }

        bool accepted = GetInternalQueue(1)->Enqueue(item);
        if (accepted)
        {
            m_classicEnqueueTimes.push_back(Simulator::Now());
        }
        return accepted;
    }
}

Ptr<QueueDiscItem>
DualBpQueueDisc::DoDequeue()
{
    // RFC 9332 Section 2.5: Deficit Round Robin Anti-Starvation Guard
    // Guarantees Classic traffic cannot be starved by aggressive L4S bursts while maintaining sub-ms latency
    bool l4sHasPackets = (GetInternalQueue(0)->GetNPackets() > 0);
    bool classicHasPackets = (GetInternalQueue(1)->GetNPackets() > 0);

    if (!l4sHasPackets && !classicHasPackets)
    {
        return nullptr;
    }

    // If only one queue has packets, serve it directly (work-conserving)
    if (l4sHasPackets && !classicHasPackets)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_l4sEnqueueTimes.empty());
            Time sojourn = Simulator::Now() - m_l4sEnqueueTimes.front();
            m_l4sEnqueueTimes.pop_front();
            if (sojourn.GetSeconds() > m_l4sTargetSec || m_rng->GetValue() < m_coupledL4sProb)
            {
                if (Mark(item, "DualBP L4S CE mark")) m_l4sMarks++;
            }
            return item;
        }
    }
    else if (!l4sHasPackets && classicHasPackets)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(1)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_classicEnqueueTimes.empty());
            m_classicEnqueueTimes.pop_front();
            return item;
        }
    }

    // Both queues active: apply RFC 9332 Deficit Round Robin scheduling
    if (m_l4sDeficit > 0 && l4sHasPackets)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item)
        {
            m_l4sDeficit -= item->GetSize();
            NS_ASSERT(!m_l4sEnqueueTimes.empty());
            Time sojourn = Simulator::Now() - m_l4sEnqueueTimes.front();
            m_l4sEnqueueTimes.pop_front();
            if (sojourn.GetSeconds() > m_l4sTargetSec || m_rng->GetValue() < m_coupledL4sProb)
            {
                if (Mark(item, "DualBP L4S CE mark")) m_l4sMarks++;
            }
            return item;
        }
    }

    // Serve Classic queue and replenish deficits to ensure guaranteed minimum line-rate share
    if (classicHasPackets)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(1)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_classicEnqueueTimes.empty());
            m_classicEnqueueTimes.pop_front();
            m_l4sDeficit += m_quantum;
            return item;
        }
    }

    return nullptr;
}

Ptr<const QueueDiscItem>
DualBpQueueDisc::DoPeek()
{
    if (GetInternalQueue(0)->GetNPackets() > 0)
    {
        return GetInternalQueue(0)->Peek();
    }
    if (GetInternalQueue(1)->GetNPackets() > 0)
    {
        return GetInternalQueue(1)->Peek();
    }
    return nullptr;
}

void
DualBpQueueDisc::DoDispose()
{
    m_updateEvent.Cancel();
    m_l4sEnqueueTimes.clear();
    m_classicEnqueueTimes.clear();
    if (m_trace.is_open())
    {
        m_trace.close();
    }
    QueueDisc::DoDispose();
}

// ============================================================================
// DualPI2 Queue Disc (RFC 9332 Standard Baseline)
// Traditional PI2 on Classic queue, step-marking on L4S, standard coupling
// ============================================================================
class DualPi2QueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    DualPi2QueueDisc();
    ~DualPi2QueueDisc() override = default;

    void Configure(double capacityBps, double classicTargetMs, double l4sTargetMs);

    uint64_t GetL4sMarks() const { return m_l4sMarks; }
    uint64_t GetClassicDrops() const { return m_classicDrops; }
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
    std::deque<Time> m_l4sEnqueueTimes;
    std::deque<Time> m_classicEnqueueTimes;
    EventId m_updateEvent;

    Time m_interval{MilliSeconds(10)};
    double m_capacityBps{20e6};
    double m_classicTargetSec{0.010};
    double m_l4sTargetSec{0.001};
    double m_classicCurrentDelay{0.0};
    double m_classicDropProb{0.0};
    double m_coupledL4sProb{0.0};

    int32_t m_l4sDeficit{1500};
    int32_t m_classicDeficit{1500};
    const int32_t m_quantum{1500};

    uint64_t m_l4sMarks{0};
    uint64_t m_classicDrops{0};
    uint64_t m_limitDrops{0};
};

NS_OBJECT_ENSURE_REGISTERED(DualPi2QueueDisc);

TypeId
DualPi2QueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DualPi2QueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<DualPi2QueueDisc>();
    return tid;
}

DualPi2QueueDisc::DualPi2QueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES)
{
    SetMaxSize(QueueSize("1500p"));
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetStream(99);
}

void
DualPi2QueueDisc::Configure(double capacityBps, double classicTargetMs, double l4sTargetMs)
{
    m_capacityBps = capacityBps;
    m_classicTargetSec = classicTargetMs / 1000.0;
    m_l4sTargetSec = l4sTargetMs / 1000.0;
}

bool
DualPi2QueueDisc::CheckConfig()
{
    if (GetNInternalQueues() == 0)
    {
        Ptr<DropTailQueue<QueueDiscItem>> qL4s = CreateObject<DropTailQueue<QueueDiscItem>>();
        qL4s->SetMaxSize(QueueSize("1000p"));
        AddInternalQueue(qL4s);

        Ptr<DropTailQueue<QueueDiscItem>> qClassic = CreateObject<DropTailQueue<QueueDiscItem>>();
        qClassic->SetMaxSize(QueueSize("1000p"));
        AddInternalQueue(qClassic);
    }
    return GetNInternalQueues() == 2;
}

void
DualPi2QueueDisc::InitializeParams()
{
    m_classicCurrentDelay = 0.0;
    m_classicDropProb = 0.0;
    m_coupledL4sProb = 0.0;
    m_l4sMarks = 0;
    m_classicDrops = 0;
    m_limitDrops = 0;
    m_l4sDeficit = m_quantum;
    m_classicDeficit = m_quantum;

    m_updateEvent = Simulator::Schedule(m_interval, &DualPi2QueueDisc::Update, this);
}

void
DualPi2QueueDisc::Update()
{
    double headDelayClassic =
        m_classicEnqueueTimes.empty()
            ? 0.0
            : (Simulator::Now() - m_classicEnqueueTimes.front()).GetSeconds();

    double prevDelay = m_classicCurrentDelay;
    m_classicCurrentDelay = headDelayClassic;

    // Standard RFC 8033 PI controller (no burst awareness)
    double error = (m_classicCurrentDelay - m_classicTargetSec) / m_classicTargetSec;
    double trend = (m_classicCurrentDelay - prevDelay) / m_classicTargetSec;

    m_classicDropProb = std::clamp(m_classicDropProb + 0.015 * error + 0.035 * trend, 0.0, 1.0);
    // RFC 9332 standard coupling: p_L = 2.0 * p_C^2
    m_coupledL4sProb = std::clamp(2.0 * (m_classicDropProb * m_classicDropProb), 0.0, 1.0);

    m_updateEvent = Simulator::Schedule(m_interval, &DualPi2QueueDisc::Update, this);
}

bool
DualPi2QueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    if (GetNPackets() >= GetMaxSize().GetValue())
    {
        m_limitDrops++;
        DropBeforeEnqueue(item, "DualPI2 buffer limit");
        return false;
    }

    uint8_t tosByte = 0;
    bool isL4s = false;
    if (item->GetUint8Value(QueueItem::IP_DSFIELD, tosByte))
    {
        uint8_t ecn = tosByte & 0x03;
        if (ecn == 1 || ecn == 3)
        {
            isL4s = true;
        }
    }

    if (isL4s)
    {
        bool accepted = GetInternalQueue(0)->Enqueue(item);
        if (accepted)
        {
            m_l4sEnqueueTimes.push_back(Simulator::Now());
        }
        return accepted;
    }
    else
    {
        if (m_rng->GetValue() < m_classicDropProb)
        {
            m_classicDrops++;
            DropBeforeEnqueue(item, "DualPI2 Classic drop");
            return false;
        }
        bool accepted = GetInternalQueue(1)->Enqueue(item);
        if (accepted)
        {
            m_classicEnqueueTimes.push_back(Simulator::Now());
        }
        return accepted;
    }
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DoDequeue()
{
    if (GetInternalQueue(0)->GetNPackets() > 0 && (m_l4sDeficit > 0 || GetInternalQueue(1)->GetNPackets() == 0))
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_l4sEnqueueTimes.empty());
            Time sojourn = Simulator::Now() - m_l4sEnqueueTimes.front();
            m_l4sEnqueueTimes.pop_front();

            if (sojourn.GetSeconds() > m_l4sTargetSec || m_rng->GetValue() < m_coupledL4sProb)
            {
                if (Mark(item, "DualPI2 L4S CE mark"))
                {
                    m_l4sMarks++;
                }
            }
            m_l4sDeficit -= item->GetSize();
            if (m_l4sDeficit <= 0 && GetInternalQueue(1)->GetNPackets() > 0)
            {
                m_classicDeficit += m_quantum;
            }
            return item;
        }
    }

    if (GetInternalQueue(1)->GetNPackets() > 0)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(1)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_classicEnqueueTimes.empty());
            m_classicEnqueueTimes.pop_front();
            m_classicDeficit -= item->GetSize();
            if (m_classicDeficit <= 0 || GetInternalQueue(1)->GetNPackets() == 0)
            {
                m_l4sDeficit += m_quantum;
                m_classicDeficit = m_quantum;
            }
            return item;
        }
    }

    if (GetInternalQueue(0)->GetNPackets() > 0)
    {
        m_l4sDeficit = m_quantum;
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        if (item)
        {
            NS_ASSERT(!m_l4sEnqueueTimes.empty());
            m_l4sEnqueueTimes.pop_front();
            return item;
        }
    }

    return nullptr;
}

Ptr<const QueueDiscItem>
DualPi2QueueDisc::DoPeek()
{
    if (GetInternalQueue(0)->GetNPackets() > 0)
    {
        return GetInternalQueue(0)->Peek();
    }
    if (GetInternalQueue(1)->GetNPackets() > 0)
    {
        return GetInternalQueue(1)->Peek();
    }
    return nullptr;
}

void
DualPi2QueueDisc::DoDispose()
{
    m_updateEvent.Cancel();
    m_l4sEnqueueTimes.clear();
    m_classicEnqueueTimes.clear();
    QueueDisc::DoDispose();
}

} // namespace ns3

// Global collectors for delay logs
static std::vector<double> g_l4sDelaysMs;
static std::vector<double> g_classicDelaysMs;
static uint64_t g_l4sRxBytes = 0;
static uint64_t g_classicRxBytes = 0;

void
DualRxTrace(bool isL4s, Ptr<const Packet> packet, const Address& from, const Address& local)
{
    DualSendTimeTag tag;
    if (packet->PeekPacketTag(tag))
    {
        Time delay = Simulator::Now() - tag.GetTime();
        double delayMs = delay.GetSeconds() * 1000.0;
        if (isL4s)
        {
            g_l4sDelaysMs.push_back(delayMs);
            g_l4sRxBytes += packet->GetSize();
        }
        else
        {
            g_classicDelaysMs.push_back(delayMs);
            g_classicRxBytes += packet->GetSize();
        }
    }
}

int
main(int argc, char* argv[])
{
    std::string algo = "DualBP";
    double bottleneckMbps = 20.0;      // 20 Mbps
    double bottleneckDelayMs = 15.0;   // 15 ms one-way (30 ms RTT)
    uint32_t numL4sFlows = 4;
    uint32_t numClassicFlows = 8;
    double simTime = 10.0;
    double burstIntensity = 2.5;       // 2.5x link capacity microbursts
    std::string outCsv = "";
    std::string delayCsv = "";

    CommandLine cmd(__FILE__);
    cmd.AddValue("algo", "Algorithm: DualBP, DualPI2, SingleBP, DropTail", algo);
    cmd.AddValue("capacityMbps", "Bottleneck Capacity in Mbps", bottleneckMbps);
    cmd.AddValue("l4sFlows", "Number of L4S flows", numL4sFlows);
    cmd.AddValue("classicFlows", "Number of Classic flows", numClassicFlows);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("burstIntensity", "Burst multiplier", burstIntensity);
    cmd.AddValue("outCsv", "Output summary CSV", outCsv);
    cmd.AddValue("delayCsv", "Output delays CSV", delayCsv);
    cmd.Parse(argc, argv);

    double bottleneckBps = bottleneckMbps * 1e6; // Convert Mbps to bps

    // 1. Configure TCP stacks
    // L4S flows: DCTCP with ECT(1)
    // Classic flows: CUBIC
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1440));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1048576));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1048576));

    // Nodes: L4S sources, Classic sources, Router 0, Router 1, Sink
    NodeContainer l4sSenders;
    l4sSenders.Create(numL4sFlows);

    NodeContainer classicSenders;
    classicSenders.Create(numClassicFlows);

    NodeContainer routers;
    routers.Create(2); // r0, r1

    NodeContainer sinkNode;
    sinkNode.Create(1);

    // Install Internet stack FIRST on all nodes (prevents automatic default queue discs)
    InternetStackHelper internet;
    internet.Install(l4sSenders);
    internet.Install(classicSenders);
    internet.Install(routers);
    internet.Install(sinkNode);

    // Point-to-point links
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));
    accessLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize("2000p")));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBps)));
    bottleneckLink.SetChannelAttribute("Delay", TimeValue(MilliSeconds(bottleneckDelayMs)));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize("1p")));

    // Connect L4S nodes to r0
    std::vector<NetDeviceContainer> l4sDevices;
    for (uint32_t i = 0; i < numL4sFlows; ++i)
    {
        l4sDevices.push_back(accessLink.Install(l4sSenders.Get(i), routers.Get(0)));
    }

    // Connect Classic nodes to r0
    std::vector<NetDeviceContainer> classicDevices;
    for (uint32_t i = 0; i < numClassicFlows; ++i)
    {
        classicDevices.push_back(accessLink.Install(classicSenders.Get(i), routers.Get(0)));
    }

    // Bottleneck link between r0 and r1
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(routers.Get(0), routers.Get(1));

    // Egress link from r1 to sink
    NetDeviceContainer egressDevices = accessLink.Install(routers.Get(1), sinkNode.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> l4sInterfaces;
    for (uint32_t i = 0; i < numL4sFlows; ++i)
    {
        l4sInterfaces.push_back(ipv4.Assign(l4sDevices[i]));
        ipv4.NewNetwork();
    }

    std::vector<Ipv4InterfaceContainer> classicInterfaces;
    for (uint32_t i = 0; i < numClassicFlows; ++i)
    {
        classicInterfaces.push_back(ipv4.Assign(classicDevices[i]));
        ipv4.NewNetwork();
    }

    Ipv4InterfaceContainer bottleneckIf = ipv4.Assign(bottleneckDevices);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer egressIf = ipv4.Assign(egressDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install Queue Disc on Bottleneck Link r0 -> r1 (Forward path r0 -> r1)
    NetDeviceContainer forwardBottleneck;
    forwardBottleneck.Add(bottleneckDevices.Get(0));

    TrafficControlHelper tch;
    tch.Uninstall(forwardBottleneck);
    Ptr<QueueDisc> installedQdisc = nullptr;

    if (algo == "DualBP")
    {
        tch.SetRootQueueDisc("ns3::DualBpQueueDisc");
        QueueDiscContainer qdiscs = tch.Install(forwardBottleneck);
        installedQdisc = qdiscs.Get(0);
        Ptr<DualBpQueueDisc> dbp = DynamicCast<DualBpQueueDisc>(installedQdisc);
        if (dbp)
        {
            dbp->Configure(bottleneckBps, 10.0, 1.0, true, true, 2.0);
            dbp->SetTraceFile("/tmp/dbp_trace.csv");
        }
    }
    else if (algo == "DualPI2")
    {
        tch.SetRootQueueDisc("ns3::DualPi2QueueDisc");
        QueueDiscContainer qdiscs = tch.Install(forwardBottleneck);
        installedQdisc = qdiscs.Get(0);
        Ptr<DualPi2QueueDisc> dpi = DynamicCast<DualPi2QueueDisc>(installedQdisc);
        if (dpi)
        {
            dpi->Configure(bottleneckBps, 10.0, 1.0);
        }
    }
    else if (algo == "SingleBP")
    {
        tch.SetRootQueueDisc("ns3::BurstAqmQueueDisc");
        QueueDiscContainer qdiscs = tch.Install(forwardBottleneck);
        installedQdisc = qdiscs.Get(0);
        Ptr<BurstAqmQueueDisc> bp = DynamicCast<BurstAqmQueueDisc>(installedQdisc);
        if (bp)
        {
            bp->Configure(bottleneckBps, 10.0, true, true, true);
        }
    }
    else
    {
        // DropTail default
        tch.SetRootQueueDisc("ns3::FifoQueueDisc");
        tch.Install(forwardBottleneck);
    }

    // Install Sink on sinkNode
    uint16_t l4sPort = 5000;
    uint16_t classicPort = 6000;

    Address sinkLocalAddressL4s(InetSocketAddress(Ipv4Address::GetAny(), l4sPort));
    PacketSinkHelper sinkHelperL4s("ns3::UdpSocketFactory", sinkLocalAddressL4s);
    ApplicationContainer sinkAppL4s = sinkHelperL4s.Install(sinkNode.Get(0));
    sinkAppL4s.Start(Seconds(0.0));
    sinkAppL4s.Stop(Seconds(simTime + 1.0));

    Address sinkLocalAddressClassic(InetSocketAddress(Ipv4Address::GetAny(), classicPort));
    PacketSinkHelper sinkHelperClassic("ns3::UdpSocketFactory", sinkLocalAddressClassic);
    ApplicationContainer sinkAppClassic = sinkHelperClassic.Install(sinkNode.Get(0));
    sinkAppClassic.Start(Seconds(0.0));
    sinkAppClassic.Stop(Seconds(simTime + 1.0));

    // Connect sink rx traces to measure per-packet latency
    Ptr<PacketSink> psL4s = DynamicCast<PacketSink>(sinkAppL4s.Get(0));
    Ptr<PacketSink> psClassic = DynamicCast<PacketSink>(sinkAppClassic.Get(0));
    if (psL4s)
    {
        psL4s->TraceConnectWithoutContext("RxWithAddresses",
            MakeBoundCallback(&DualRxTrace, true));
    }
    if (psClassic)
    {
        psClassic->TraceConnectWithoutContext("RxWithAddresses",
            MakeBoundCallback(&DualRxTrace, false));
    }

    // Traffic Generators:
    // 1. L4S Traffic: Periodic high-frequency packets with ECT(1) codepoint set (TOS = 0x01)
    for (uint32_t i = 0; i < numL4sFlows; ++i)
    {
        Ptr<Socket> l4sSocket = Socket::CreateSocket(l4sSenders.Get(i), UdpSocketFactory::GetTypeId());
        InetSocketAddress l4sRemote(egressIf.GetAddress(1), l4sPort);
        l4sRemote.SetTos(0x01); // Explicitly configure ECT(1) for L4S
        l4sSocket->Connect(l4sRemote);
        l4sSocket->SetIpTos(0x01);

        // Schedule periodic packet transmissions: 500 packets/sec per flow (4 Mbps aggregate)
        double intervalSec = 0.002; // 2 ms
        for (double t = 0.1 + i * 0.0005; t < simTime; t += intervalSec)
        {
            Simulator::Schedule(Seconds(t), [l4sSocket]() {
                Ptr<Packet> p = Create<Packet>(1000);
                DualSendTimeTag tag;
                tag.SetTime(Simulator::Now());
                p->AddPacketTag(tag);
                // Attach SocketIpTosTag as well to guarantee IPv4 header TOS is ECT(1)
                SocketIpTosTag tosTag;
                tosTag.SetTos(0x01);
                p->AddPacketTag(tosTag);
                l4sSocket->Send(p);
            });
        }
    }

    // 2. Classic Traffic: Continuous background flows (TOS = 0x00)
    for (uint32_t i = 0; i < numClassicFlows; ++i)
    {
        Ptr<Socket> classicSocket = Socket::CreateSocket(classicSenders.Get(i), UdpSocketFactory::GetTypeId());
        InetSocketAddress classicRemote(egressIf.GetAddress(1), classicPort);
        classicRemote.SetTos(0x00); // Not-ECT
        classicSocket->Connect(classicRemote);
        classicSocket->SetIpTos(0x00);

        // Continuous classic saturation rate (4.0 Mbps per flow -> 32 Mbps total on 20 Mbps link)
        double intervalSec = 0.002; // 2 ms (saturates classic queue to target)
        for (double t = 0.2 + i * 0.001; t < simTime; t += intervalSec)
        {
            Simulator::Schedule(Seconds(t), [classicSocket]() {
                Ptr<Packet> p = Create<Packet>(1000);
                DualSendTimeTag tag;
                tag.SetTime(Simulator::Now());
                p->AddPacketTag(tag);
                classicSocket->Send(p);
            });
        }

        // 3. Realistic Web Microbursts injected on Classic Flows (Pareto-distributed burst storms)
        // Bursts occur at t = 2.0s, 4.0s, 6.0s, 8.0s
        for (double burstEpoch : {2.0, 4.0, 6.0, 8.0})
        {
            uint32_t burstPackets = static_cast<uint32_t>(35 * burstIntensity);
            for (uint32_t b = 0; b < burstPackets; ++b)
            {
                double burstTime = burstEpoch + (b * 0.0001); // 100 microsecond inter-arrival
                if (burstTime < simTime)
                {
                    Simulator::Schedule(Seconds(burstTime), [classicSocket]() {
                        Ptr<Packet> p = Create<Packet>(1440);
                        DualSendTimeTag tag;
                        tag.SetTime(Simulator::Now());
                        p->AddPacketTag(tag);
                        classicSocket->Send(p);
                    });
                }
            }
        }
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime + 0.1));
    Simulator::Run();

    // Compute metrics
    auto calcPercentile = [](std::vector<double>& v, double p) -> double {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        size_t idx = static_cast<size_t>(p * (v.size() - 1));
        return v[idx];
    };

    double l4sP50 = calcPercentile(g_l4sDelaysMs, 0.50);
    double l4sP95 = calcPercentile(g_l4sDelaysMs, 0.95);
    double l4sP99 = calcPercentile(g_l4sDelaysMs, 0.99);
    double l4sP999 = calcPercentile(g_l4sDelaysMs, 0.999);

    double classicP50 = calcPercentile(g_classicDelaysMs, 0.50);
    double classicP95 = calcPercentile(g_classicDelaysMs, 0.95);
    double classicP99 = calcPercentile(g_classicDelaysMs, 0.99);

    double l4sGoodputMbps = (g_l4sRxBytes * 8.0) / (simTime * 1e6);
    double classicGoodputMbps = (g_classicRxBytes * 8.0) / (simTime * 1e6);

    uint64_t l4sMarks = 0;
    uint64_t classicDrops = 0;
    uint64_t classicMarks = 0;
    uint64_t limitDrops = 0;

    if (algo == "DualBP" && installedQdisc)
    {
        Ptr<DualBpQueueDisc> dbp = DynamicCast<DualBpQueueDisc>(installedQdisc);
        if (dbp)
        {
            l4sMarks = dbp->GetL4sMarks();
            classicDrops = dbp->GetClassicDrops();
            classicMarks = dbp->GetClassicMarks();
            limitDrops = dbp->GetLimitDrops();
        }
    }
    else if (algo == "DualPI2" && installedQdisc)
    {
        Ptr<DualPi2QueueDisc> dpi = DynamicCast<DualPi2QueueDisc>(installedQdisc);
        if (dpi)
        {
            l4sMarks = dpi->GetL4sMarks();
            classicDrops = dpi->GetClassicDrops();
            limitDrops = dpi->GetLimitDrops();
        }
    }

    std::cout << "\n=======================================================\n"
              << " Algorithm:             " << algo << "\n"
              << " L4S Packets Received:  " << g_l4sDelaysMs.size() << "\n"
              << " L4S Goodput:           " << l4sGoodputMbps << " Mbps\n"
              << " L4S p50 Latency:       " << l4sP50 << " ms\n"
              << " L4S p95 Latency:       " << l4sP95 << " ms\n"
              << " L4S p99 Latency:       " << l4sP99 << " ms\n"
              << " L4S p99.9 Latency:     " << l4sP999 << " ms\n"
              << " L4S CE Marks:          " << l4sMarks << "\n"
              << " Classic Goodput:       " << classicGoodputMbps << " Mbps\n"
              << " Classic p50 Latency:   " << classicP50 << " ms\n"
              << " Classic p95 Latency:   " << classicP95 << " ms\n"
              << " Classic Drops:         " << classicDrops << "\n"
              << " Buffer Limit Drops:    " << limitDrops << "\n"
              << "=======================================================\n";

    if (!outCsv.empty())
    {
        std::ofstream out(outCsv, std::ios::app);
        out << algo << ","
            << bottleneckBps / 1e6 << ","
            << burstIntensity << ","
            << l4sGoodputMbps << ","
            << l4sP50 << ","
            << l4sP95 << ","
            << l4sP99 << ","
            << l4sP999 << ","
            << l4sMarks << ","
            << classicGoodputMbps << ","
            << classicP50 << ","
            << classicP95 << ","
            << classicDrops << ","
            << limitDrops << "\n";
        out.close();
    }

    if (!delayCsv.empty())
    {
        std::ofstream out(delayCsv);
        out << "traffic_type,delay_ms\n";
        for (double d : g_l4sDelaysMs)
        {
            out << "L4S," << d << "\n";
        }
        for (double d : g_classicDelaysMs)
        {
            out << "Classic," << d << "\n";
        }
        out.close();
    }

    Simulator::Destroy();
    return 0;
}
