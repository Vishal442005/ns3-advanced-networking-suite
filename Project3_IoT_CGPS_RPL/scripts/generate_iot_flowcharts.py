#!/usr/bin/env python3
"""
generate_iot_flowcharts.py
Inspired by LALARPL (Homaei, 2024) and Farag & Stefanovic (2021).
Renders:
  Figure 5: CGPS-RPL In-Band Piggybacked Telemetry & Acyclic Parent Deflection Flowchart
  illustrating Trickle Timer decoupling and the Traffic-Index load balancing architecture.
"""

from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib.patches as patches

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent if SCRIPT_DIR.name == "scripts" else SCRIPT_DIR.parents[1] / "repository" / "Project3_IoT_CGPS_RPL"
PLOTS_DIR = PROJECT_DIR / "plots"
PLOTS_DIR.mkdir(parents=True, exist_ok=True)

plt.rcParams["font.family"] = "DejaVu Serif"

def render_flowchart():
    fig, ax = plt.subplots(figsize=(11.0, 8.5), dpi=300)
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 10)
    ax.axis("off")
    
    box_props = dict(boxstyle="round,pad=0.5", facecolor="#EBF3FB", edgecolor="#004080", linewidth=1.5)
    decision_props = dict(boxstyle="square,pad=0.5", facecolor="#FFF2CC", edgecolor="#B27B00", linewidth=1.5)
    action_props = dict(boxstyle="round,pad=0.5", facecolor="#E2F0D9", edgecolor="#385723", linewidth=1.5)
    highlight_props = dict(boxstyle="round,pad=0.5", facecolor="#FCE4D6", edgecolor="#C65911", linewidth=1.5)
    arrow_props = dict(facecolor="#003366", edgecolor="#003366", width=1.3, headwidth=7, headlength=7)
    
    # Flowchart Blocks
    ax.text(5, 9.5, "Sensor Data Packet Arrival at Sensor Mote $i$\n(Periodic Telemetry or Alarm Surge)", ha="center", va="center", bbox=box_props, fontsize=10)
    ax.annotate("", xy=(5, 8.6), xytext=(5, 9.0), arrowprops=arrow_props)
    
    ax.text(5, 8.2, "RFC 6553 In-Band Piggybacking (IPB):\nAppend 16-bit Packed Queue Gradient & Traffic Index $\\mathrm{TI}_i$\nto IPv6 Hop-by-Hop Header (Zero Control Packet Overhead)", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax.annotate("", xy=(5, 7.3), xytext=(5, 7.8), arrowprops=arrow_props)
    
    ax.text(5, 6.8, "Forward Packet to Preferred Parent $p_{\\mathrm{pref}}$;\nObserve Instantaneous MAC Retries $R_{\\mathrm{inst}}$ & ACK Queue Fill $Q_{\\mathrm{fill}}$", ha="center", va="center", bbox=box_props, fontsize=9.5)
    ax.annotate("", xy=(5, 5.9), xytext=(5, 6.4), arrowprops=arrow_props)
    
    ax.text(5, 5.4, "Compute Saturation Confidence Gate:\n$C_p(t) = 1 - \\exp\\left(-\\beta \\cdot \\max(0, Q_{\\mathrm{fill}} - 0.60) \\cdot R_{\\mathrm{inst}}\\right)$\nIs $C_p(t) > 0.45$ (Persistent Bottleneck Congestion)?", ha="center", va="center", bbox=decision_props, fontsize=9.5)
    
    # No branch
    ax.annotate("No (Transient Normal)", xy=(8.5, 5.4), xytext=(7.2, 5.4), arrowprops=arrow_props, fontsize=9, color="green")
    ax.text(8.7, 5.4, "Retain Current Parent;\nMaintain Exponential\nTrickle Doubling ($I(t) \\to I_{\\max}$)", ha="left", va="center", bbox=dict(boxstyle="round", facecolor="#E2F0D9", edgecolor="#385723"), fontsize=8.5)
    
    # Yes branch
    ax.annotate("Yes (Emergency Congestion)", xy=(5, 4.3), xytext=(5, 4.8), arrowprops=arrow_props, fontsize=9, color="red")
    
    ax.text(5, 3.8, "Theorem 1 Safe Candidate Parent Filtering:\n$\\mathcal{P}_{\\mathrm{safe}}(i) = \\left\\{p \\in \\mathcal{N}(i) \\mid \\mathrm{Rank}(p) < \\mathrm{Rank}(i) - \\mathrm{MinHopRankIncrease}\\right\\}$\n(Strictly Enforces Acyclic DAG Invariant $\\to$ Zero Routing Loops)", ha="center", va="center", bbox=highlight_props, fontsize=9.5)
    ax.annotate("", xy=(5, 2.9), xytext=(5, 3.3), arrowprops=arrow_props)
    
    ax.text(5, 2.4, "Select Alternate Parent $p_{\\mathrm{alt}} \\in \\mathcal{P}_{\\mathrm{safe}}(i)$ Minimizing Joint Cost:\n$M_{ip}(t) = \\alpha Q_{\\mathrm{fill}}(p) + (1-\\alpha) \\mathrm{ETX}_{ip} + \\mathrm{TI}_p$\nSubject to Hysteresis Guard $Q_{\\mathrm{fill}}(p_{\\mathrm{alt}}) < Q_{\\mathrm{fill}}(p_{\\mathrm{cur}}) - 0.20$", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax.annotate("", xy=(5, 1.5), xytext=(5, 1.9), arrowprops=arrow_props)
    
    ax.text(5, 0.9, "Switch Default Route to $p_{\\mathrm{alt}}$;\nPreserve Undisturbed RPL Trickle Interval $I(t)$;\nUpdate 4-Byte Packed Neighbor Descriptor (80 Bytes RAM Overhead)", ha="center", va="center", bbox=dict(boxstyle="round", facecolor="#D9E1F2", edgecolor="#2F5597", linewidth=1.5), fontsize=9.5, fontweight="bold")
    
    fig.suptitle("Figure 5: CGPS-RPL In-Band Piggybacked Telemetry & Acyclic Deflection Architecture\n(Decouples routing updates from RPL Trickle timers, eliminating DIO broadcast storms and bounding mote energy drain)", fontsize=12, fontweight="bold", y=0.98)
    
    plt.subplots_adjust(left=0.03, right=0.97, top=0.92, bottom=0.05)
    
    out_png = PLOTS_DIR / "fig5_iot_cgps_rpl_flowchart.png"
    out_pdf = PLOTS_DIR / "fig5_iot_cgps_rpl_flowchart.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Successfully generated Figure 5: {out_png}")

if __name__ == "__main__":
    render_flowchart()
