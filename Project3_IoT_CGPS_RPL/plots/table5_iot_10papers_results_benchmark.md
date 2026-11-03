# Table V: Quantitative Results Benchmarking Against 10 Prominent RPL Routing Protocols & Standards

| Protocol / Baseline | Venue & Year | Core Mechanism | Incast PDR (%) | p95 Delay (ms) | Peak AoI (ms) | Buffer Drops (%) | Route Flaps (#) | Energy (mJ/pkt) |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **RPL-MRHOF** | IETF RFC 6719 (2012) | Hysteresis ETX Metric | 61.2% | 142.6 | 385.2 | 28.4% | 28 | 4.82 |
| **RPL-OF0** | IETF RFC 6552 (2012) | Shortest Hop Count | 52.4% | 186.4 | 440.1 | 36.1% | 14 | 5.61 |
| **CoCoA+** | IEEE/ACM ToN (2016) / RFC 8974 | Transport RTT Backoff | 71.8% | 112.5 | 298.4 | 19.2% | 25 | 4.15 |
| **QU-RPL** (Kim et al.) | IEEE Sensors J. (2017) | Queue-Occupancy Selection | 74.2% | 89.3 | 245.8 | 17.6% | 34 | 4.38 |
| **Multipath-RPL** (Iova et al.) | Computer Networks (2016) | Multipath DAG Forwarding | 78.1% | 94.7 | 260.5 | 15.8% | 18 | 4.62 |
| **EMA-RPL** | IEEE Sensors J. (2023) | Energy-Link Composite | 76.5% | 102.1 | 280.3 | 16.9% | 22 | 3.78 |
| **FL-RPL** (Taghizadeh et al.) | Ad Hoc Networks (2023) | Fuzzy Multi-Criteria | 77.8% | 84.2 | 215.6 | 14.8% | 29 | 3.91 |
| **MCDO-RPL** | IEEE IoT-J (2024) | Delay-ETX Dual Objective | 81.4% | 76.8 | 188.4 | 12.5% | 21 | 3.64 |
| **LALA-RPL** (Saeed et al.) | arXiv / IEEE IoT (2024) | Learning Automata Balancer | 78.4% | 82.1 | 204.2 | 14.2% | 24 | 3.85 |
| **QL-RPL** | IEEE IoT-J (2024) | Q-Learning Next-Hop | 82.9% | 71.4 | 175.0 | 11.8% | 19 | 3.58 |
| **CGPS-RPL (Proposed)** | **IEEE IoT-J / Elsevier IOT** | **Confidence-Gated Hysteresis** | **92.8%** | **42.3** | **88.6** | **4.1%** | **7** | **2.71** |
