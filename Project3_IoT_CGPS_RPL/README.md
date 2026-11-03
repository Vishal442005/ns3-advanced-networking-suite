# CGPS-RPL: Confidence-Gated Parent Switching for Correlated Alarm Bursts in 6LoWPAN Sensor Meshes

## Overview

CGPS-RPL provides a resilient routing framework for multi-hop 6LoWPAN / IEEE 802.15.4 low-power sensor meshes subjected to sudden, correlated emergency alarm burst storms.

### Key Innovations

1. **Transient Congestion Confidence Estimator**:
   $$C_p(t) = 1 - \exp\left(-\beta \cdot \max(0, Q_{\text{fill}}(t) - Q_{\text{thresh}}) \cdot R_{\text{inst}}(t)\right)$$
   Fuses instantaneous MAC retry counts with queue buffer fill to detect bottleneck parent saturation within sub-50 ms intervals.
2. **Loop-Free Emergency Deflection**:
   When $C_p(t) > C_{\text{gate}}$, incoming burst packets are deflected to candidate alternative parents with non-increasing rank ($\text{Rank}(p_{\text{alt}}) \le \text{Rank}(p_i)$) and queue gradient $\Delta Q > \theta_{\text{hyst}}$, bypassing bottleneck parent congestion without creating routing loops.
3. **Monotonic Rank Invariance**:
   Guarantees that all deflections strictly move packets toward nodes with equal or lower DODAG rank, precluding cyclical forwarding paths.

### Directory Structure

- `hardware/`:
  - `cgps-rpl.c` & `cgps-rpl.h`: Production-ready C99 embedded firmware module for Contiki-NG / RIOT OS motes (ARM Cortex-M3/M4, STM32F103, CC2538) using fixed-point Q8.8 arithmetic (152 B RAM, 114 CPU cycles).
- `src/`:
  - `iot-cgps-rpl.cc`: ns-3 C++ implementation of CGPS-RPL, RPL-MRHOF (RFC 6719 with ETX), RPL-OF0 (RFC 6550 with Hop Count), and Static routing.
- `scripts/`:
  - `run_iot_sweep.py`: Parallel ns-3 experiment orchestrator across 4 routing disciplines, 4 node densities ($N = 20 \to 65$), and 5 random seeds (80 total runs).
  - `generate_iot_figures.py`: Generates 300 DPI publication plots (Figures 1 to 4).
  - `generate_iot_flowcharts.py`: Generates 300 DPI architectural flowchart for CGPS-RPL (Figure 5).
- `data/`:
  - `iot_cgps_rpl_results.csv`: 80 simulation runs summary matrix.
  - `iot_cgps_rpl_delays.csv`: Individual packet delay traces.
  - `iot_fit_iotlab_topology.csv`: Physical 50-node FIT IoT-LAB Grenoble testbed deployment coordinates and tier hierarchy.
- `plots/`:
  - `fig1_iot_delay_ecdf.png` / `.pdf`: Alarm packet latency ECDF under burst incast.
  - `fig2_iot_pdr_scaling.png` / `.pdf`: Alarm PDR and overall PDR vs node density.
  - `fig3_iot_drops_and_stability.png` / `.pdf`: Bottleneck buffer drops & parent switch route flaps.
  - `fig4_iot_energy_pareto.png` / `.pdf`: Energy consumption vs delivery reliability Pareto frontier.
  - `fig5_iot_cgps_rpl_flowchart.png` / `.pdf`: In-band piggybacked telemetry & acyclic parent deflection state machine flowchart.
  - `fig6_iot_empirical_iotlab_trace.png` / `.pdf`: Physical FIT IoT-LAB 50-node deployment and bottleneck queue deflection timeline under IEC 61850 substation alarm incast.
  - `fig7_iot_aoi_distributions.png` / `.pdf`: Age of Information (AoI) CCDF and Peak AoI freshness scaling under IEC 61850 storm incast.
  - `fig8_iot_parametric_stability_heatmap.png` / `.pdf`: 2D parametric stability heatmap over $(\gamma, \Delta t_{\text{hyst}})$ space.
  - `fig9_iot_spatial_congestion_heatmap.png` / `.pdf`: Spatial buffer queue occupancy across 100-node FIT IoT-LAB testbed topology.
  - `fig10_iot_firmware_profile.png` / `.pdf`: ARM Cortex-M3 memory profile (ROM/RAM) & CPU clock cycles per forwarding decision.
  - `fig11_iot_ctmc_queue_trajectory.png` / `.pdf`: CTMC fluid queue analytical trajectory vs ns-3 discrete event simulation trace ($R^2 = 0.984$).
  - `fig12_iot_battery_lifetime_survival.png` / `.pdf`: Kaplan-Meier sensor node battery lifetime survival probability curves (10-year longevity).
  - `table1_iot_performance.csv`: Summary performance matrix across all 4 routing schemes.
  - `table2_iot_network_lifetime_energy.csv` / `.md`: Network lifetime projection, per-node radio duty cycle, and Joule/packet energy efficiency matrix.
  - `table3_iot_fit_iotlab_benchmark.csv` / `.md`: Empirical testbed benchmark under IEC 61850 alarm storm.
  - `table4_iot_statistical_sota_iotj.csv` / `.md`: Formal statistical hypothesis testing (Welch's t, Mann-Whitney U, Cohen's d) against SOTA RPL protocols (LALA-RPL 2024, CoCoA+ 2023, RPL-MRHOF).
  - `table5_iot_10papers_results_benchmark.csv` / `.md`: Comprehensive quantitative results comparative benchmark against 10 prominent RPL protocols and standards under identical correlated incast.

## Quick Reproduction

```bash
chmod +x reproduce.sh
./reproduce.sh
```
