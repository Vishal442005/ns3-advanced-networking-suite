#!/usr/bin/env python3
"""
generate_arch_diagram.py - Generates IEEE Publication-Grade Architectural Diagram (Figure 0)
Illustrates BP-AQM and DualBP Pipeline Architecture at 300 DPI.
"""

from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib.patches as patches

PLOTS_DIR = Path("/home/jarvis/ns3_research/plots")
PLOTS_DIR.mkdir(parents=True, exist_ok=True)

def generate_figure_0():
    fig, ax = plt.subplots(figsize=(12, 5.5), dpi=300)
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 6)
    ax.axis("off")

    # Font styles
    title_font = {"fontsize": 11, "fontweight": "bold", "ha": "center", "va": "center"}
    sub_font = {"fontsize": 8.5, "ha": "center", "va": "center"}
    box_style = dict(boxstyle="round,pad=0.5", ec="#263238", lw=1.5)

    # 1. Ingress Flows Box
    ax.add_patch(patches.FancyBboxPatch((0.4, 1.8), 2.2, 2.8, boxstyle="round,pad=0.2",
                                        facecolor="#E3F2FD", edgecolor="#1565C0", lw=1.8))
    ax.text(1.5, 4.2, "Ingress Traffic", **title_font, color="#0D47A1")
    ax.text(1.5, 3.4, "- Classic TCP (CUBIC/BBR)\n- L4S Scalable (Prague)\n- Interactive Microbursts\n- UDP Volumetric Incast",
            **sub_font, color="#1A237E")

    # Arrow 1 -> Classifier
    ax.annotate("", xy=(3.0, 3.2), xytext=(2.6, 3.2),
                arrowprops=dict(arrowstyle="->", lw=2, color="#37474F"))

    # 2. Classifier / ECT Demux Box
    ax.add_patch(patches.FancyBboxPatch((3.0, 1.8), 2.0, 2.8, boxstyle="round,pad=0.2",
                                        facecolor="#FFF3E0", edgecolor="#E65100", lw=1.8))
    ax.text(4.0, 4.2, "IP-ECN Demux", **title_font, color="#E65100")
    ax.text(4.0, 3.2, "Inspect IP Header\nECN Bits:\nECT(1) -> L-Queue\nECT(0)/Not-ECT -> C-Queue",
            **sub_font, color="#BF360C")

    # Arrows to Queues
    ax.annotate("ECT(1)", xy=(5.4, 3.9), xytext=(5.0, 3.6),
                arrowprops=dict(arrowstyle="->", lw=1.8, color="#2E7D32"))
    ax.annotate("Classic", xy=(5.4, 2.5), xytext=(5.0, 2.8),
                arrowprops=dict(arrowstyle="->", lw=1.8, color="#1565C0"))

    # 3. DualBP Coupled Queues Box
    ax.add_patch(patches.Rectangle((5.4, 0.8), 3.4, 4.6, facecolor="#F5F5F5",
                                  edgecolor="#455A64", lw=2, linestyle="--"))
    ax.text(7.1, 5.1, "DualBP Coupled Core Engine", **title_font, color="#263238")

    # L-Queue (Low Latency)
    ax.add_patch(patches.FancyBboxPatch((5.6, 3.4), 3.0, 1.3, boxstyle="round,pad=0.15",
                                        facecolor="#E8F5E9", edgecolor="#2E7D32", lw=1.6))
    ax.text(7.1, 4.3, "L4S Queue (Low Latency)", fontsize=9.5, fontweight="bold", ha="center", color="#1B5E20")
    ax.text(7.1, 3.8, r"Native BP Step-Threshold $\tau_{L}$ (1 ms)" + "\nPersistence-Decoupled CE Marking",
            fontsize=8, ha="center", color="#2E7D32")

    # C-Queue (Classic)
    ax.add_patch(patches.FancyBboxPatch((5.6, 1.3), 3.0, 1.6, boxstyle="round,pad=0.15",
                                        facecolor="#E1F5FE", edgecolor="#0288D1", lw=1.6))
    ax.text(7.1, 2.5, "Classic Queue (Burst-Aware)", fontsize=9.5, fontweight="bold", ha="center", color="#01579B")
    ax.text(7.1, 1.8, r"BP-AQM Controller: $\Delta = 10\text{ ms}$" + "\n" +
            r"Burst Persistence Estimator $P_b$" + "\n" +
            r"Dynamic Slope $\alpha(t), \beta(t)$ Modulation", fontsize=8, ha="center", color="#0277BD")

    # Coupling Mechanism Indicator
    ax.annotate("", xy=(7.1, 3.4), xytext=(7.1, 2.9),
                arrowprops=dict(arrowstyle="<->", lw=1.8, color="#D32F2F", linestyle=":"))
    ax.text(7.1, 3.15, "Coupled Probability $p_{CL}' = (p_C)^2$ (Gated by Burst Filter)",
            fontsize=7.5, fontweight="bold", ha="center", color="#B71C1C",
            bbox=dict(boxstyle="round,pad=0.2", fc="#FFEBEE", ec="#D32F2F", lw=0.8))

    # Arrow to Scheduler
    ax.annotate("", xy=(9.2, 3.2), xytext=(8.8, 3.2),
                arrowprops=dict(arrowstyle="->", lw=2, color="#37474F"))

    # 4. WRR Scheduler & Line Rate Egress Box
    ax.add_patch(patches.FancyBboxPatch((9.2, 1.8), 2.4, 2.8, boxstyle="round,pad=0.2",
                                        facecolor="#EDE7F6", edgecolor="#512DA8", lw=1.8))
    ax.text(10.4, 4.2, "Egress Scheduler", **title_font, color="#4A148C")
    ax.text(10.4, 3.2, "Deficit / WRR (85:15)\nCapacity Exhaustion Guard\n" +
            r"Sub-microsecond Wire Rate" + "\nHardware Pipeline / eBPF",
            **sub_font, color="#311B92")

    out_png = PLOTS_DIR / "fig0_bp_aqm_architecture.png"
    out_pdf = PLOTS_DIR / "fig0_bp_aqm_architecture.pdf"
    plt.savefig(out_png, dpi=300, bbox_inches="tight")
    plt.savefig(out_pdf, bbox_inches="tight")
    plt.close()
    print(f"[+] Successfully generated Figure 0: {out_png}")

if __name__ == "__main__":
    generate_figure_0()
