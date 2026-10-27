| Algorithm | Control Paradigm | State Space Monitored | Large-Signal Damping | Burst Tolerance Mechanism | L4S Delay Isolation | Hardware Programmability (P4/eBPF) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **RED (RFC 2309)** | Heuristic Linear Drop | Average Queue Length (EWMA) | None (Static Linear Slope) | None (Fixed Thresholds) | No (Single FIFO) | Hardware ASIC (Fixed) |
| **PIE (RFC 8033)** | Classical Proportional-Integral | Queue Latency + Delay Trend | Linear Fixed Gain ($\alpha, \beta$) | Burst Allowance Timer (100 ms) | No (Single FIFO) | Software Kernel / Cable DOCSIS |
| **CoDel (RFC 8289)** | Heuristic Sojourn Tracking | Minimum Sojourn in Window | None (Square-Root Stepping) | Target Delay Window (100 ms) | No (Single FIFO) | Linux Kernel (`sch_codel`) |
| **FQ-CoDel (RFC 8290)** | Per-Flow Deficit Round Robin | Per-Flow Sojourn + Hash Table | Per-Flow Queue Isolation | Sparse Flow Optimization | Partial (L4S CE Threshold Hack) | CPU-Intensive (Hash Collisions) |
| **DualPI2 (RFC 9332)** | Dual-Queue Coupled $\text{PI}^2$ | Coupled Classic Delay $p'$ | Linear Fixed Derivative Gain | Step Threshold (1 ms) | Yes (Dual Queue Coupled) | Linux Kernel (`sch_dualpi2`) |
| **PAQMAN (2022)** | Semi-Markov Decision Process (SMDP) | Queue Backlog + Gamma Arrival Rate | Discrete Bellman Optimality | State Transition Optimization | No (Single Queue Focus) | Complex Offline Training |
| **DualBP (Proposed)** | Non-Linear Lyapunov Damped AQM | Multi-Scale Delay + Burst Persistence | Non-Linear Saturation Damping $\beta_{\text{nl}}(v)$ | Multi-Interval Headroom Scaling ($2\times-10\times$) | Yes (RFC 9332 DRR Anti-Starvation) | Line-Rate P4-16 & Linux XDP/eBPF |
