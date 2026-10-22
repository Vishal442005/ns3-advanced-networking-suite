# Wi-Fi 6 (IEEE 802.11ax) Multi-User OFDMA Slack-and-Airtime Deficit Scheduling (SADS)

## Overview

SADS provides a Downlink Multi-User OFDMA scheduler for IEEE 802.11ax (Wi-Fi 6) Access Points multiplexing ultra-low latency interactive traffic (VR/AR, teleoperation) with high-throughput bulk transfers.

### Key Innovations

1. **Urgency Slack Gradient**:
   $$\Pi_i(t) = w_i \frac{R_i(t)}{\bar{R}_i(t)} (1 - \Phi_i(t)) + \gamma \exp\left(-\frac{S_i(t) - \tau_{\min}}{\kappa}\right) \Phi_i(t) + \lambda D_i(t)$$
2. **Sigmoid Gating Mechanism**:
   $$\Phi_i(t) = \frac{1}{1 + \exp\left(\frac{S_i(t) - S_{\text{thresh}}}{\sigma}\right)}$$
   Maintains spectral-efficient deficit-weighted airtime round-robin during slack-rich regimes and switches to hard urgency scheduling only when packet expiration nears ($D = 20\text{ ms}$).
3. **Lyapunov Stability Guarantee**:
   Guarantees queue stability while bounding deadline violation probability $P(\text{delay} > D) \le C_0 e^{-V D / \kappa}$.

### Directory Structure

- `src/`:
  - `wifi6-sads.cc`: ns-3 C++ implementation of SADS, Round-Robin (RR), Proportional Fair (PF), Earliest Deadline First (EDF), and Legacy Single-User (SU) schedulers.
- `scripts/`:
  - `run_wifi6_sweep.py`: Parallel ns-3 experiment orchestrator across 5 schedulers, 5 station densities ($N = 4 \to 20$), and 3 seeds (75 total runs).
  - `generate_wifi6_figures.py`: Generates 300 DPI publication plots (Figures 1 to 4).
- `data/`:
  - `wifi6_sads_results.csv`: 75 simulation runs summary matrix.
  - `wifi6_sads_delays.csv`: Individual packet delay traces.
- `plots/`:
  - `fig1_wifi6_delay_ecdf.png` / `.pdf`: Delay-critical packet latency ECDF.
  - `fig2_wifi6_throughput_scaling.png` / `.pdf`: BSS goodput & spectral efficiency vs station density.
  - `fig3_wifi6_deadline_violations.png` / `.pdf`: Deadline violation containment vs station density.
  - `fig4_wifi6_fairness_tradeoff.png` / `.pdf`: Airtime fairness & delay-throughput Pareto frontier.
  - `table1_wifi6_performance.csv`: Summary performance matrix across all 5 schedulers.

## Quick Reproduction

```bash
chmod +x reproduce.sh
./reproduce.sh
```
