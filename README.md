# Advanced Computer Networking Research Artifacts Repository

This repository contains the complete simulation models, programmable hardware data-plane implementations (eBPF & P4_16), experimental orchestrators, empirical datasets, and publication-grade figure suites for three cutting-edge networking research projects.

---

## Repository Structure

```
repository/
├── Project1_DualBP_AQM/          # DualBP & BP-AQM for Ultra-Low Latency & L4S (RFC 9332)
│   ├── src/                      # ns-3 C++ models (burst-aqm.cc, dualbp-aqm.cc)
│   ├── hardware/                 # Linux kernel eBPF (C) & Intel Tofino P4_16 data-planes
│   ├── scripts/                  # Parametric sweep runners & statistical analysis suites
│   ├── data/                     # 584 ns-3 simulation runs + 17,250 kernel packet traces
│   ├── plots/                    # 11 publication figures at 300 DPI (.png & vector .pdf)
│   ├── reproduce.sh              # One-line full replication script
│   └── README.md                 # Project documentation & mathematical formulations
│
├── Project2_WiFi6_SADS/          # Wi-Fi 6 (802.11ax) OFDMA Slack-and-Airtime Deficit Scheduling
│   ├── src/                      # ns-3 C++ model (wifi6-sads.cc)
│   ├── scripts/                  # Multi-station sweep runner, flowchart & plot generators
│   ├── data/                     # 75 ns-3 simulation runs + individual delay traces
│   ├── plots/                    # 5 publication figures at 300 DPI (.png & vector .pdf)
│   ├── reproduce.sh              # One-line full replication script
│   └── README.md                 # 802.11ax OFDMA RU allocation & Lyapunov stability proofs
│
└── Project3_IoT_CGPS_RPL/        # Confidence-Gated RPL for Correlated Alarm Bursts in 6LoWPAN
    ├── src/                      # ns-3 C++ model (iot-cgps-rpl.cc)
    ├── scripts/                  # Mesh sweep runner, architecture flowchart & plot generators
    ├── data/                     # 80 ns-3 simulation runs + individual delay traces
    ├── plots/                    # 5 publication figures at 300 DPI (.png & vector .pdf)
    ├── reproduce.sh              # One-line full replication script
    └── README.md                 # Confidence estimator, loop-freedom theorems & evaluation
```

---

## Research Projects Overview

### 1. DualBP & BP-AQM: Coupled Active Queue Management for L4S & Hardware Deployment
- **Focus**: Eliminates **delay leakage** in IETF RFC 9332 Coupled Dual-Queue AQMs under high-intensity Classic microburst storms.
- **Key Breakthrough**: DualBP restricts L4S median queuing delay to **$<1.0\text{ ms}$** and p95 tail delay to **$<1.35\text{ ms}$**, retaining **100% goodput** (15.73 Mbps) where RFC 9332 standard DualPI2 suffers severe delay leakage ($p50 = 1219.8\text{ ms}$) and an abrupt **58.4% throughput collapse** (6.54 Mbps).
- **Hardware Validation**: Production Linux kernel eBPF socket hook evaluated across 17,250 packets in virtual network namespaces ($242–404\text{ ns}$ execution overhead, 0.0% loss) and Intel Tofino P4_16 pipeline ($<15\text{ ns}$ lookup).

### 2. SADS: Wi-Fi 6 (IEEE 802.11ax) Multi-User OFDMA Deadline & Airtime Scheduling
- **Focus**: Multi-User Downlink OFDMA Resource Unit (RU) allocation for delay-critical (VR/AR/teleoperation) traffic multiplexed with high-throughput bulk transfers.
- **Key Breakthrough**: Introduces an exponential urgency slack gradient gated by a smooth sigmoid function. SADS slashes deadline violations from **76.97%** (legacy Single-User) and **20.56%** (Earliest Deadline First) to **0.76%** under 16-station contention, while sustaining high aggregate cell goodput (**64.56 Mbps**) and long-term airtime equity.
- **Theory**: Formally proved queue stability via Lyapunov drift-plus-penalty optimization.

### 3. CGPS-RPL: Confidence-Gated RPL Routing for Bursty Sensor Meshes in 6LoWPAN
- **Focus**: Fast-timescale transient congestion mitigation under synchronized emergency alarm burst storms in IEEE 802.15.4 / 6LoWPAN industrial meshes.
- **Key Breakthrough**: Fuses instantaneous MAC retransmission counts with queue buffer occupancy in a non-linear confidence gate $C_p(t)$. When confidence triggers, the node executes loop-free emergency deflections to non-increasing rank alternative parents.
- **Empirical Impact**: Elevates alarm delivery ratio from **38.0%** (static) and **72.0%** (RPL-MRHOF) to **92.0%**, slashing bottleneck buffer drops by **72.7%** (from 44 to 12 packets).

---

## Environment & Prerequisites

- **Network Simulator**: [ns-3.40](https://www.nsnam.org/) (configured in `optimized` build mode)
- **Compiler**: GCC / Clang supporting C++17
- **Operating System**: Linux (Ubuntu 22.04 LTS or compatible kernel 5.15+)
- **Python Stack**: Python 3.10+, NumPy, Pandas, Matplotlib, SciPy, Seaborn, lxml
- **Hardware Toolchain (Optional)**: Clang/LLVM, `libbpf`, `bpftool`, `iproute2` (for eBPF kernel testbed)

---

## Quick Replication

To reproduce any project, navigate to its respective directory and execute the reproduction script:

```bash
# Project 1: DualBP & BP-AQM
cd Project1_DualBP_AQM && ./reproduce.sh

# Project 2: Wi-Fi 6 SADS Scheduler
cd Project2_WiFi6_SADS && ./reproduce.sh

# Project 3: 6LoWPAN CGPS-RPL
cd Project3_IoT_CGPS_RPL && ./reproduce.sh
```

---

## License & Citation

All simulation source code, hardware definitions, and datasets are released under the **GNU General Public License v2.0 (GPLv2)** in compliance with ns-3 licensing terms.
