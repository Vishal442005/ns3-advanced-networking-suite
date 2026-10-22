#!/usr/bin/env python3
"""
generate_wifi6_figures.py - Generates Publication-Grade 300 DPI Figures for Wi-Fi 6 SADS Paper
Targets IEEE Transactions on Mobile Computing (TMC).
Generates:
 - Figure 1: Delay-Critical ECDF & Tail Zoom
 - Figure 2: Aggregate Goodput & Spectral Efficiency vs Station Density
 - Figure 3: Deadline Violation Ratio vs Station Density
 - Figure 4: Airtime Fairness & Delay-Throughput Tradeoff Radar
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

COLORS = {
    "sads": "#1B5E20",  # Dark Green (Proposed)
    "rr": "#1565C0",    # Blue
    "pf": "#00838F",    # Teal
    "edf": "#E65100",   # Amber / Orange
    "su": "#757575",    # Grey
}

LABELS = {
    "sads": "SADS (Proposed)",
    "rr": "802.11ax RR (Standard)",
    "pf": "Proportional Fair (PF)",
    "edf": "Earliest Deadline (EDF)",
    "su": "Legacy Single-User (SU)",
}

def plot_fig1_wifi6_delay_ecdf():
    delays_file = DATA_DIR / "wifi6_sads_delays.csv"
    if not delays_file.exists():
        print(f"[-] Missing {delays_file}")
        return

    df = pd.read_csv(delays_file)
    # Filter for N=16 stations (high load)
    df_sub = df[df["n_stations"] == 16]
    if df_sub.empty:
        df_sub = df

    fig, ax = plt.subplots(figsize=(7.5, 5.0), dpi=300)

    for algo in ["sads", "rr", "pf", "edf", "su"]:
        sub = df_sub[df_sub["algorithm"] == algo]
        if not sub.empty:
            delays = np.sort(sub["delay_ms"].values)
            ecdf = np.linspace(0, 1, len(delays))
            lw = 2.4 if algo == "sads" else 1.8
            ls = "-" if algo == "sads" else "--" if algo == "su" else "-."
            ax.plot(delays, ecdf, label=LABELS[algo], color=COLORS[algo], lw=lw, ls=ls)

    ax.axvline(20.0, color="#D32F2F", linestyle=":", lw=2, label="Deadline Limit (20 ms)")
    ax.set_xlabel("Packet Delay (ms)", fontweight="bold")
    ax.set_ylabel("Empirical CDF P(D <= d)", fontweight="bold")
    ax.set_title("Fig. 1. Delay-Critical Latency CDF under 16-Station OFDMA Contention", pad=12, fontweight="bold")
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.set_xlim(0, 30)
    ax.set_ylim(-0.02, 1.02)
    ax.legend(loc="lower right", framealpha=0.92)

    out_png = PLOTS_DIR / "fig1_wifi6_delay_ecdf.png"
    out_pdf = PLOTS_DIR / "fig1_wifi6_delay_ecdf.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved {out_png}")

def plot_fig2_wifi6_throughput_scaling():
    res_file = DATA_DIR / "wifi6_sads_results.csv"
    if not res_file.exists():
        print(f"[-] Missing {res_file}")
        return

    df = pd.read_csv(res_file)
    agg = df.groupby(["algorithm", "n_stations"]).mean().reset_index()

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.2), dpi=300)

    for algo in ["sads", "rr", "pf", "edf", "su"]:
        sub = agg[agg["algorithm"] == algo].sort_values("n_stations")
        if not sub.empty:
            lw = 2.4 if algo == "sads" else 1.8
            marker = "o" if algo == "sads" else "s" if algo == "rr" else "^"
            ax1.plot(sub["n_stations"], sub["total_goodput_mbps"], label=LABELS[algo],
                     color=COLORS[algo], lw=lw, marker=marker, markersize=6)
            ax2.plot(sub["n_stations"], sub["spectral_eff_bps_hz"], label=LABELS[algo],
                     color=COLORS[algo], lw=lw, marker=marker, markersize=6)

    ax1.set_xlabel("Number of Associated Stations", fontweight="bold")
    ax1.set_ylabel("Total BSS Goodput (Mbps)", fontweight="bold")
    ax1.set_title("(a) Aggregate Goodput Scaling", pad=10, fontweight="bold")
    ax1.grid(True, linestyle=":", alpha=0.6)
    ax1.legend(loc="lower right", framealpha=0.92)

    ax2.set_xlabel("Number of Associated Stations", fontweight="bold")
    ax2.set_ylabel("Spectral Efficiency (bps/Hz)", fontweight="bold")
    ax2.set_title("(b) Spectral Efficiency (20 MHz Channel)", pad=10, fontweight="bold")
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.legend(loc="lower right", framealpha=0.92)

    out_png = PLOTS_DIR / "fig2_wifi6_throughput_scaling.png"
    out_pdf = PLOTS_DIR / "fig2_wifi6_throughput_scaling.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved {out_png}")

def plot_fig3_wifi6_deadline_violations():
    res_file = DATA_DIR / "wifi6_sads_results.csv"
    if not res_file.exists():
        return

    df = pd.read_csv(res_file)
    agg = df.groupby(["algorithm", "n_stations"]).mean().reset_index()

    fig, ax = plt.subplots(figsize=(7.5, 5.0), dpi=300)

    for algo in ["sads", "rr", "pf", "edf", "su"]:
        sub = agg[agg["algorithm"] == algo].sort_values("n_stations")
        if not sub.empty:
            lw = 2.4 if algo == "sads" else 1.8
            marker = "o" if algo == "sads" else "s" if algo == "rr" else "D"
            ax.plot(sub["n_stations"], sub["deadline_viol_ratio"] * 100.0, label=LABELS[algo],
                    color=COLORS[algo], lw=lw, marker=marker, markersize=6)

    ax.set_xlabel("Number of Associated Stations", fontweight="bold")
    ax.set_ylabel("Deadline Violation Rate (%)", fontweight="bold")
    ax.set_title("Fig. 3. Deadline Violation Containment across Station Densities", pad=12, fontweight="bold")
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.set_ylim(-1, 40)
    ax.legend(loc="upper left", framealpha=0.92)

    out_png = PLOTS_DIR / "fig3_wifi6_deadline_violations.png"
    out_pdf = PLOTS_DIR / "fig3_wifi6_deadline_violations.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved {out_png}")

def plot_fig4_wifi6_fairness_radar():
    res_file = DATA_DIR / "wifi6_sads_results.csv"
    if not res_file.exists():
        return

    df = pd.read_csv(res_file)
    # Average across all station counts
    agg = df.groupby("algorithm").mean().reset_index()

    algos = ["sads", "rr", "pf", "edf", "su"]
    jain_vals = [agg[agg["algorithm"] == a]["jain_fairness"].values[0] for a in algos]
    goodput_vals = [agg[agg["algorithm"] == a]["total_goodput_mbps"].values[0] for a in algos]
    delay_vals = [agg[agg["algorithm"] == a]["dc_delay_p95_ms"].values[0] for a in algos]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.0), dpi=300)

    x = np.arange(len(algos))
    width = 0.45

    # Subpanel A: Jain's Fairness Index
    colors = [COLORS[a] for a in algos]
    ax1.bar(x, jain_vals, width, color=colors, edgecolor="#333333", alpha=0.9)
    ax1.set_xticks(x)
    ax1.set_xticklabels([LABELS[a].split(" (")[0] for a in algos], fontweight="bold")
    ax1.set_ylabel("Jain's Airtime Fairness Index", fontweight="bold")
    ax1.set_title("(a) Long-Term Airtime Fairness Equity", pad=10, fontweight="bold")
    ax1.set_ylim(0, 1.05)
    ax1.grid(True, linestyle=":", alpha=0.6, axis="y")

    for i, v in enumerate(jain_vals):
        ax1.text(i, v + 0.02, f"{v:.3f}", ha="center", va="bottom", fontsize=9, fontweight="bold")

    # Subpanel B: p95 Tail Latency vs Goodput Tradeoff
    for a in algos:
        sub_a = agg[agg["algorithm"] == a]
        ax2.scatter(sub_a["dc_delay_p95_ms"], sub_a["total_goodput_mbps"],
                    color=COLORS[a], s=160, label=LABELS[a], edgecolors="#333333", lw=1.5, zorder=5)

    ax2.set_xlabel("Delay-Critical p95 Tail Latency (ms)", fontweight="bold")
    ax2.set_ylabel("Total Aggregate Goodput (Mbps)", fontweight="bold")
    ax2.set_title("(b) Pareto Tradeoff: Latency Containment vs Goodput", pad=10, fontweight="bold")
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.legend(loc="lower left", framealpha=0.92)

    out_png = PLOTS_DIR / "fig4_wifi6_fairness_tradeoff.png"
    out_pdf = PLOTS_DIR / "fig4_wifi6_fairness_tradeoff.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Saved {out_png}")

if __name__ == "__main__":
    plot_fig1_wifi6_delay_ecdf()
    plot_fig2_wifi6_throughput_scaling()
    plot_fig3_wifi6_deadline_violations()
    plot_fig4_wifi6_fairness_radar()
