#!/usr/bin/env python3
"""
generate_dualbp_figures.py - Generates 300 DPI publication-grade figures for DualBP & Kernel eBPF
Figure 9: DualBP L4S Delay Leakage Elimination & Throughput Preservation
Figure 10: Physical Linux Kernel eBPF Testbed Telemetry & Overhead
"""

from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

WORKSPACE_DIR = Path(__file__).resolve().parents[1]
DATA_DIR = WORKSPACE_DIR / "data"
PLOTS_DIR = WORKSPACE_DIR / "plots"
PLOTS_DIR.mkdir(parents=True, exist_ok=True)

plt.rcParams.update({
    "font.size": 11,
    "axes.labelsize": 12,
    "axes.titlesize": 13,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "legend.fontsize": 10,
    "figure.titlesize": 14,
    "font.family": "serif",
    "mathtext.fontset": "cm",
})

# Color palette
COLORS = {
    "DualBP": "#2E7D32",    # Forest Green
    "DualPI2": "#D32F2F",   # Deep Red
    "SingleBP": "#1976D2",  # Cobalt Blue
    "DropTail": "#757575",  # Muted Grey
}

def plot_figure_9():
    """Figure 9: DualBP L4S Delay Leakage ECDF & Goodput Resilience"""
    delays_file = DATA_DIR / "dualbp_delays.csv"
    results_file = DATA_DIR / "dualbp_results.csv"

    if not delays_file.exists() or not results_file.exists():
        print(f"[!] Missing {delays_file} or {results_file}")
        return

    df_delays = pd.read_csv(delays_file)
    df_res = pd.read_csv(results_file)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5), dpi=300)

    # Subpanel A: L4S Delay ECDF
    df_l4s = df_delays[df_delays["traffic_type"] == "L4S"]
    base_prop_delay = 19.1648 # Base propagation delay

    for algo in ["DualBP", "DualPI2", "SingleBP", "DropTail"]:
        sub = df_l4s[df_l4s["algo"] == algo]
        if not sub.empty:
            # Queue delay = total latency - base propagation delay
            q_delays = np.maximum(0.0, sub["delay_ms"].values - base_prop_delay)
            sorted_d = np.sort(q_delays)
            ecdf = np.linspace(0, 1, len(sorted_d))
            lw = 2.4 if algo == "DualBP" else 1.8
            ls = "-" if algo in ["DualBP", "DualPI2"] else "--"
            ax1.plot(sorted_d, ecdf, label=algo, color=COLORS[algo], lw=lw, ls=ls)

    ax1.set_xlabel(r"L4S Queuing Delay $\tau_L$ (ms)")
    ax1.set_ylabel(r"Empirical CDF $P(D \leq d)$")
    ax1.set_title("(a) L4S Delay Under 6x Classic Burst Storm", pad=10, fontweight="bold")
    ax1.grid(True, linestyle=":", alpha=0.6)
    ax1.set_xlim(-5, 1400)
    ax1.set_ylim(-0.02, 1.02)
    ax1.legend(loc="center right", framealpha=0.92)

    # Inset zoom into [0, 5 ms]
    ax_ins = fig.add_axes([0.22, 0.40, 0.22, 0.42])
    for algo in ["DualBP", "DualPI2", "SingleBP", "DropTail"]:
        sub = df_l4s[df_l4s["algo"] == algo]
        if not sub.empty:
            q_delays = np.maximum(0.0, sub["delay_ms"].values - base_prop_delay)
            sorted_d = np.sort(q_delays)
            ecdf = np.linspace(0, 1, len(sorted_d))
            lw = 2.4 if algo == "DualBP" else 1.8
            ls = "-" if algo in ["DualBP", "DualPI2"] else "--"
            ax_ins.plot(sorted_d, ecdf, color=COLORS[algo], lw=lw, ls=ls)

    ax_ins.set_xlim(-0.1, 5.0)
    ax_ins.set_ylim(-0.02, 1.02)
    ax_ins.set_title("Zoom: [0, 5 ms] Queue Delay", fontsize=9, fontweight="bold")
    ax_ins.grid(True, linestyle=":", alpha=0.5)
    ax_ins.tick_params(labelsize=8)

    # Subpanel B: L4S Goodput vs Burst Intensity
    burst_vals = sorted(df_res["burst_intensity"].unique())
    for algo in ["DualBP", "DualPI2", "SingleBP", "DropTail"]:
        sub = df_res[df_res["algo"] == algo].sort_values("burst_intensity")
        if not sub.empty:
            lw = 2.4 if algo == "DualBP" else 1.8
            marker = "o" if algo == "DualBP" else "s" if algo == "DualPI2" else "^"
            ax2.plot(sub["burst_intensity"], sub["l4s_goodput_mbps"],
                     label=algo, color=COLORS[algo], lw=lw, marker=marker, markersize=6)

    ax2.set_xlabel(r"Classic Burst Intensity Multiplier ($k_{\mathrm{burst}} \times C$)")
    ax2.set_ylabel("L4S Goodput (Mbps)")
    ax2.set_title("(b) L4S Throughput Resilience (Zero Collapse)", pad=10, fontweight="bold")
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.set_ylim(4, 18)
    ax2.legend(loc="lower left", framealpha=0.92)

    # Annotation
    ax2.annotate("DualPI2 Collapse: -58.4%\n(False Coupled CE Marking)",
                 xy=(6.0, 6.54), xytext=(4.5, 9.5),
                 arrowprops=dict(arrowstyle="->", color=COLORS["DualPI2"], lw=1.5),
                 fontsize=9.5, fontweight="bold", color=COLORS["DualPI2"],
                 bbox=dict(boxstyle="round,pad=0.3", fc="#FFEBEE", ec=COLORS["DualPI2"], lw=1))

    ax2.annotate("DualBP: 100% Rate Retained\n(Burst Persistence Filtering)",
                 xy=(6.0, 15.73), xytext=(3.5, 16.5),
                 arrowprops=dict(arrowstyle="->", color=COLORS["DualBP"], lw=1.5),
                 fontsize=9.5, fontweight="bold", color=COLORS["DualBP"],
                 bbox=dict(boxstyle="round,pad=0.3", fc="#E8F5E9", ec=COLORS["DualBP"], lw=1))

    out_png = PLOTS_DIR / "fig9_dualbp_l4s_delay_leakage.png"
    out_pdf = PLOTS_DIR / "fig9_dualbp_l4s_delay_leakage.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved Figure 9: {out_png}")

def plot_figure_10():
    """Figure 10: Physical Linux Kernel Namespace Telemetry & eBPF Overhead"""
    kernel_file = DATA_DIR / "kernel_testbed_results.csv"
    if not kernel_file.exists():
        print(f"[!] Missing {kernel_file}")
        return

    df_k = pd.read_csv(kernel_file)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.2), dpi=300)

    # Subpanel A: Kernel Transit Latency Quantiles
    x_indices = np.arange(len(df_k))
    scenarios = df_k["burst_scenario"]

    width = 0.20
    ax1.bar(x_indices - 1.5*width, df_k["kernel_p50_us"], width, label="p50 (Median)", color="#4CAF50")
    ax1.bar(x_indices - 0.5*width, df_k["kernel_p95_us"], width, label="p95 Tail", color="#2196F3")
    ax1.bar(x_indices + 0.5*width, df_k["kernel_p99_us"], width, label="p99 Tail", color="#FF9800")
    ax1.bar(x_indices + 1.5*width, df_k["kernel_p999_us"], width, label="p99.9 Tail", color="#E91E63")

    ax1.set_xticks(x_indices)
    ax1.set_xticklabels(scenarios, rotation=15, ha="right")
    ax1.set_ylabel(r"Physical Kernel Transit Latency ($\mu$s)")
    ax1.set_title("(a) Linux Kernel Socket Processing Latency", pad=10, fontweight="bold")
    ax1.grid(True, linestyle=":", alpha=0.6, axis="y")
    ax1.legend(loc="upper left", framealpha=0.92)

    # Subpanel B: eBPF Hook Overhead & Line-Rate Throughput
    ax2_twin = ax2.twinx()

    p1 = ax2.plot(x_indices, df_k["ebpf_hook_overhead_ns"], color="#673AB7", lw=2.4,
                  marker="D", markersize=7, label="eBPF Hook Overhead (ns)")
    p2 = ax2_twin.plot(x_indices, df_k["throughput_mbps"], color="#009688", lw=2.4,
                       marker="s", markersize=7, ls="--", label="Kernel Throughput (Mbps)")

    ax2.set_xticks(x_indices)
    ax2.set_xticklabels(scenarios, rotation=15, ha="right")
    ax2.set_ylabel("eBPF Classifier Execution Time (ns)", color="#673AB7")
    ax2_twin.set_ylabel("Measured Kernel Goodput (Mbps)", color="#009688")
    ax2.set_title("(b) In-Kernel eBPF Execution Overhead & Scaling", pad=10, fontweight="bold")
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.set_ylim(150, 500)
    ax2_twin.set_ylim(0, 35)

    # Combined legend
    lines = p1 + p2
    labels = [l.get_label() for l in lines]
    ax2.legend(lines, labels, loc="center right", framealpha=0.92)

    plt.tight_layout()
    out_png = PLOTS_DIR / "fig10_kernel_ebpf_telemetry.png"
    out_pdf = PLOTS_DIR / "fig10_kernel_ebpf_telemetry.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved Figure 10: {out_png}")

if __name__ == "__main__":
    plot_figure_9()
    plot_figure_10()
