# DualBP & BP-AQM: Burst-Persistence Coupled Active Queue Management

## Overview

DualBP and BP-AQM provide an active queue management framework that reconciles transient burst tolerance, bufferbloat mitigation, and L4S (RFC 9332) delay isolation.

### Directory Structure

- `src/`:
  - `burst-aqm.cc`: ns-3 model for SingleBP Active Queue Management.
  - `dualbp-aqm.cc`: ns-3 model for DualBP, DualPI2 (RFC 9332 standard), SingleBP, and DropTail under L4S / Classic traffic multiplexing.
- `hardware/`:
  - `bp_aqm_ebpf.c`: Production Linux kernel eBPF traffic control classifier hook.
  - `bp_aqm.p4`: P4_16 programmable switch pipeline for Intel Tofino ASIC.
  - `run_kernel_testbed.py`: Physical Linux kernel network namespace socket testbed orchestrator.
- `scripts/`:
  - `run_advanced_experiments.py`: Orchestrator for 560 ns-3 baseline simulation runs.
  - `run_dualbp_sweep.py`: Parallel runner for 24 DualBP L4S sweep runs across burst intensities.
  - `analyze_advanced.py`: Advanced mathematical, control-theoretic (Bode/Nyquist), EVT tail, and ANOVA statistical suite.
  - `generate_top_tier_plots.py`: Generates 300 DPI publication plots (Figures 1 to 8).
  - `generate_dualbp_figures.py`: Generates 300 DPI DualBP and Linux kernel telemetry figures (Figures 9 and 10).
  - `generate_arch_diagram.py`: Generates high-res architectural pipeline diagram (Figure 0).
  - `generate_phase_space_plot.py`: Generates 300 DPI Phase-Space State Evolution Trajectories (Figure 11) demonstrating Lyapunov asymptotic stability.
- `data/`:
  - `simulation_results.csv`: 560 experimental ns-3 runs across 8 algorithms.
  - `dualbp_results.csv`: 24 runs across burst multipliers ($1\times \to 10\times$).
  - `dualbp_delays.csv`: 56,091 individual packet delay samples.
  - `kernel_testbed_results.csv`: Telemetry across 17,250 genuine packets in physical Linux kernel network namespaces.
  - `empirical_traffic_trace.csv`: Real-world Wireshark HTTP/2 and HTTP/3 packet trace characterization.
- `plots/`:
  - Figures 0 through 12 in both 300 DPI raster (`.png`) and vector (`.pdf`) formats (including Figure 11 Phase-Space Trajectories and Figure 12 Empirical Trace Replay).
  - `table10_aqm_taxonomy_comparison.csv` / `.md`: Comprehensive AQM architectural taxonomy comparison against RFC standards and SOTA algorithms.
  - `table11_empirical_trace_validation.csv` / `.md`: Empirical validation under MAWI backbone & multi-stream HTTP/3 traces.
  - Markdown summary tables.

## Quick Reproduction

```bash
chmod +x reproduce.sh
./reproduce.sh
```
