#!/usr/bin/env python3
"""
generate_wifi6_flowcharts.py
Inspired by A2P (Agbeve et al., IEEE WCNC 2025 / arXiv:2502.00430) and Dovelos et al. (2021).
Renders:
  Figure 5: SADS Dual-State Algorithmic Flowcharts for Station (STA) and Access Point (AP)
  demonstrating sub-SIFS compliant Trigger Frame scheduling and Virtual Deadline Estimation.
"""

from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib.patches as patches

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent if SCRIPT_DIR.name == "scripts" else SCRIPT_DIR.parents[1] / "repository" / "Project2_WiFi6_SADS"
PLOTS_DIR = PROJECT_DIR / "plots"
PLOTS_DIR.mkdir(parents=True, exist_ok=True)

plt.rcParams["font.family"] = "DejaVu Serif"

def render_flowchart():
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15.0, 7.5), dpi=300)
    
    # Configure axes
    for ax in (ax1, ax2):
        ax.set_xlim(0, 10)
        ax.set_ylim(0, 10)
        ax.axis("off")
        
    box_props = dict(boxstyle="round,pad=0.5", facecolor="#EBF3FB", edgecolor="#004080", linewidth=1.5)
    decision_props = dict(boxstyle="square,pad=0.5", facecolor="#FFF2CC", edgecolor="#B27B00", linewidth=1.5)
    action_props = dict(boxstyle="round,pad=0.5", facecolor="#E2F0D9", edgecolor="#385723", linewidth=1.5)
    arrow_props = dict(facecolor="#003366", edgecolor="#003366", width=1.2, headwidth=6, headlength=6)
    
    # -------------------------------------------------------------
    # (a) Station (STA) Side Flowchart
    # -------------------------------------------------------------
    ax1.set_title("(a) SADS Station (STA) Dual-State Channel Access Procedure", fontsize=12, fontweight="bold", pad=12)
    
    # Nodes
    ax1.text(5, 9.3, "Packet Generation at STA\n(Audio / Sensor Stream)", ha="center", va="center", bbox=box_props, fontsize=9.5)
    ax1.annotate("", xy=(5, 8.2), xytext=(5, 8.8), arrowprops=arrow_props)
    
    ax1.text(5, 7.7, "Compute Virtual Slack:\n$\\hat{S}_i(t) = D_{\\mathrm{budget}}(\\mathrm{TID}_i) - (t - t_{\\mathrm{arr}})$", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax1.annotate("", xy=(5, 6.6), xytext=(5, 7.2), arrowprops=arrow_props)
    
    ax1.text(5, 6.1, "Proactive Deadline Pruning (PDP):\nIs Remaining Slack $\\hat{S}_i(t) \\leq 0$?", ha="center", va="center", bbox=decision_props, fontsize=9)
    
    # Yes -> Drop expired
    ax1.annotate("Yes (Expired)", xy=(8.5, 6.1), xytext=(6.8, 6.1), arrowprops=arrow_props, fontsize=8.5, color="red")
    ax1.text(8.8, 6.1, "Prune at HOL\n(Reclaim Airtime)", ha="left", va="center", bbox=dict(boxstyle="round", facecolor="#F8CBAD", edgecolor="#C65911"), fontsize=8.5)
    
    # No -> Check EDCA state
    ax1.annotate("No (Viable)", xy=(5, 4.9), xytext=(5, 5.5), arrowprops=arrow_props, fontsize=8.5, color="green")
    ax1.text(5, 4.4, "Is Station in AP Polling List\n(MU-EDCA Timer Active)?", ha="center", va="center", bbox=decision_props, fontsize=9)
    
    # Yes -> OFDMA
    ax1.annotate("Yes", xy=(2.5, 3.1), xytext=(3.8, 4.0), arrowprops=arrow_props, fontsize=9)
    ax1.text(2.2, 2.6, "Disable EDCA Contention;\nWait for Trigger Frame (TF)\n& Transmit on Assigned RU", ha="center", va="center", bbox=action_props, fontsize=9)
    
    # No -> Contend via EDCA
    ax1.annotate("No", xy=(7.5, 3.1), xytext=(6.2, 4.0), arrowprops=arrow_props, fontsize=9)
    ax1.text(7.8, 2.6, "Contend via Legacy EDCA;\nTransmit Initial BSR Frame;\nActivate MU-EDCA Timer ($Y$ TUs)", ha="center", va="center", bbox=box_props, fontsize=9)
    
    ax1.text(5, 0.8, "Reset MU-EDCA Timer upon BlockAck;\nRe-enable EDCA only on Timer Expiry ($Y=0$)", ha="center", va="center", bbox=dict(boxstyle="round", facecolor="#EDEDED", edgecolor="gray"), fontsize=9)
    ax1.annotate("", xy=(5, 1.4), xytext=(2.2, 2.0), arrowprops=arrow_props)
    ax1.annotate("", xy=(5, 1.4), xytext=(7.8, 2.0), arrowprops=arrow_props)

    # -------------------------------------------------------------
    # (b) Access Point (AP) Side Flowchart
    # -------------------------------------------------------------
    ax2.set_title("(b) SADS Access Point (AP) Sub-SIFS Multi-User Scheduling Procedure", fontsize=12, fontweight="bold", pad=12)
    
    ax2.text(5, 9.3, "Scheduling Epoch $t$ (TXOP Acquired)", ha="center", va="center", bbox=box_props, fontsize=9.5)
    ax2.annotate("", xy=(5, 8.2), xytext=(5, 8.8), arrowprops=arrow_props)
    
    ax2.text(5, 7.7, "Buffer Status Report (BSR) & TID Ingestion\nUpdate Historical Rates $\\bar{R}_i(t)$ & Deficits $D_i(t)$", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax2.annotate("", xy=(5, 6.6), xytext=(5, 7.2), arrowprops=arrow_props)
    
    ax2.text(5, 6.1, "Compute SADS Priority Metric:\n$\\Pi_i(t) = w_i \\frac{R_i}{\\bar{R}_i}(1-\\Phi_i) + \\gamma e^{-\\frac{\\hat{S}_i-\\tau}{\\kappa}}\\Phi_i + \\lambda D_i$", ha="center", va="center", bbox=box_props, fontsize=9.5)
    ax2.annotate("", xy=(5, 5.0), xytext=(5, 5.6), arrowprops=arrow_props)
    
    ax2.text(5, 4.4, "Multi-RU Grouping & Padding Minimization:\nGroup STAs by Packet Size to Equalize PPDU Length", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax2.annotate("", xy=(5, 3.3), xytext=(5, 3.9), arrowprops=arrow_props)
    
    ax2.text(5, 2.7, "Greedy Sub-SIFS Allocation $\\mathcal{O}(N_{\\mathrm{act}} + K \\log K)$:\nAssign Optimal Resource Units (26/52/106 Tones)", ha="center", va="center", bbox=action_props, fontsize=9.5)
    ax2.annotate("", xy=(5, 1.6), xytext=(5, 2.2), arrowprops=arrow_props)
    
    ax2.text(5, 0.9, "Broadcast Basic Trigger Frame (TF) to STAs\n(Execution Verified in $1.85\\ \\mu\\mathrm{s} \\ll 16\\ \\mu\\mathrm{s}$ SIFS)", ha="center", va="center", bbox=dict(boxstyle="round", facecolor="#D9E1F2", edgecolor="#2F5597", linewidth=1.5), fontsize=9.5, fontweight="bold")
    
    fig.suptitle("Figure 5: SADS Dual-State Coordination and Sub-SIFS Multi-User OFDMA Scheduling Architecture\n(Standard-compliant IEEE 802.11ax/be channel access combining proactive deadline pruning and airtime deficit balancing)", fontsize=13, fontweight="bold", y=0.98)
    
    plt.subplots_adjust(left=0.03, right=0.97, top=0.90, bottom=0.05, wspace=0.15)
    
    out_png = PLOTS_DIR / "fig5_wifi6_sads_state_machine_flowchart.png"
    out_pdf = PLOTS_DIR / "fig5_wifi6_sads_state_machine_flowchart.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Successfully generated Figure 5: {out_png}")

if __name__ == "__main__":
    render_flowchart()
