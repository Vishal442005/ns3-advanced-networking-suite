import matplotlib.pyplot as plt
import matplotlib.patches as patches
from pathlib import Path
from PIL import Image

def generate_square_cgps_rpl_flowchart():
    # Exactly Square Canvas: 7.5 x 7.5 inches @ 300 DPI -> 2250 x 2250 pixels
    fig, ax = plt.subplots(figsize=(7.5, 7.5), dpi=300)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis('off')

    line_color = '#000000'
    fill_color = '#FFFFFF'
    text_color = '#000000'
    font_family = 'sans-serif'

    def draw_oval(cx, cy, w, h, text):
        box = patches.FancyBboxPatch(
            (cx - w/2, cy - h/2), w, h,
            boxstyle="round,pad=0.2,rounding_size=3.5",
            facecolor=fill_color, edgecolor=line_color, linewidth=2.0
        )
        ax.add_patch(box)
        ax.text(cx, cy, text, ha='center', va='center', fontsize=9.5,
                color=text_color, family=font_family, weight='bold', linespacing=1.25)

    def draw_rect(cx, cy, w, h, text, bold_title=None):
        box = patches.Rectangle(
            (cx - w/2, cy - h/2), w, h,
            facecolor=fill_color, edgecolor=line_color, linewidth=1.6
        )
        ax.add_patch(box)
        if bold_title:
            ax.text(cx, cy + h*0.22, bold_title, ha='center', va='center', fontsize=9.2,
                    color=text_color, family=font_family, weight='bold')
            ax.text(cx, cy - h*0.20, text, ha='center', va='center', fontsize=8.6,
                    color=text_color, family=font_family, weight='normal', linespacing=1.2)
        else:
            ax.text(cx, cy, text, ha='center', va='center', fontsize=8.9,
                    color=text_color, family=font_family, weight='normal', linespacing=1.25)

    def draw_diamond(cx, cy, w, h, text, font_sz=8.3):
        pts = [[cx, cy + h/2], [cx + w/2, cy], [cx, cy - h/2], [cx - w/2, cy]]
        poly = patches.Polygon(pts, facecolor=fill_color, edgecolor=line_color, linewidth=1.6)
        ax.add_patch(poly)
        ax.text(cx, cy, text, ha='center', va='center', fontsize=font_sz,
                color=text_color, family=font_family, weight='bold', linespacing=1.2)

    def draw_arrow(x1, y1, x2, y2, label=None, label_side='right', label_offset=(0, 0)):
        ax.annotate(
            '', xy=(x2, y2), xytext=(x1, y1),
            arrowprops=dict(arrowstyle='->,head_width=0.35,head_length=0.45',
                            color=line_color, lw=1.5, shrinkA=0, shrinkB=0)
        )
        if label:
            lx = (x1 + x2) / 2 + label_offset[0]
            ly = (y1 + y2) / 2 + label_offset[1]
            ha = 'left' if label_side == 'right' else ('right' if label_side == 'left' else 'center')
            ax.text(lx, ly, label, fontsize=8.5, weight='bold', color=text_color, ha=ha, va='center',
                    bbox=dict(boxstyle='square,pad=0.15', facecolor='#FFFFFF', edgecolor='none', alpha=0.9))

    def draw_manhattan_arrow(pts, label=None, label_pos=None):
        for i in range(len(pts) - 2):
            ax.plot([pts[i][0], pts[i+1][0]], [pts[i][1], pts[i+1][1]], color=line_color, lw=1.5)
        ax.annotate(
            '', xy=pts[-1], xytext=pts[-2],
            arrowprops=dict(arrowstyle='->,head_width=0.35,head_length=0.45',
                            color=line_color, lw=1.5, shrinkA=0, shrinkB=0)
        )
        if label and label_pos:
            ax.text(label_pos[0], label_pos[1], label, fontsize=8.5, weight='bold', color=text_color,
                    ha='center', va='center', bbox=dict(boxstyle='square,pad=0.15', facecolor='#FFFFFF', edgecolor='none', alpha=0.9))

    def draw_segment(x1, y1, x2, y2):
        ax.plot([x1, x2], [y1, y2], color=line_color, lw=1.5)

    # 1. Top Oval: Sensor Data Packet Arrival
    draw_oval(50, 94.0, 58, 7.0, "Sensor Data Packet Arrival\n(Node u, Queue Q_u, ETX Metric)")
    draw_arrow(50, 90.5, 50, 85.5)

    # 2. Rect: Extract In-Band Piggybacked Telemetry & Compute Confidence
    draw_rect(50, 80.5, 72, 8.5, "Extract In-Band Piggybacked IPB Telemetry\nCompute Local Congestion Confidence C_u(t)", bold_title="Telemetry Extraction & Metric Computation")
    draw_arrow(50, 76.25, 50, 70.5)

    # 3. Decision Diamond 1: Microburst / Alarm Incast Check (w=34, h=12)
    draw_diamond(50, 64.5, 34, 12.0, "Is C_u(t) > θ_burst?\n(Alarm Incast)", font_sz=8.5)

    # Branch NO from D1 -> Box Left (Nominal Preferred Parent)
    draw_manhattan_arrow([[33, 64.5], [18, 64.5], [18, 30.25]], label="NO", label_pos=(25.5, 66.5))

    # Branch YES from D1 -> Decision Diamond 2
    draw_arrow(50, 58.5, 50, 51.5, label="YES", label_side='right', label_offset=(1.8, 0))

    # 4. Decision Diamond 2: Loop-Free Alternative Parent Check (w=38, h=13)
    draw_diamond(50, 45.0, 38, 13.0, "Is Loop-Free Parent\nv Available?\n(Rank(v) < Rank(u))", font_sz=8.2)

    # Branch YES from D2 -> Box Center (Acyclic Deflection)
    draw_arrow(50, 38.5, 50, 30.25, label="YES", label_side='right', label_offset=(1.8, 0))

    # Branch NO from D2 -> Box Right (Backpressure Damping)
    draw_manhattan_arrow([[69, 45.0], [82, 45.0], [82, 30.25]], label="NO", label_pos=(75.5, 47.0))

    # 5. Three Action Boxes (Aligned horizontally at Y = 24.5, Height = 11.5)
    # Action 1 (Left): Nominal Preferred Parent Forwarding
    draw_rect(18, 24.5, 27, 11.5, "Preferred Parent P_pref\nForwarding via Standard Metric\nSteady Trickle State", bold_title="Nominal Forwarding")

    # Action 2 (Center): Acyclic Emergency Deflection
    draw_rect(50, 24.5, 29, 11.5, "Route to Candidate Parent v\nSuppress Trickle Resets\nEliminate DIO Broadcast Storm", bold_title="Acyclic Deflection")

    # Action 3 (Right): Local Backpressure Damping & Drop
    draw_rect(82, 24.5, 27, 11.5, "Local Backpressure Damping\nControlled RED Drop\nSignal Upstream Flow", bold_title="Backpressure Damping")

    # 6. Elegant Convergence to Single Central Trunk:
    # Left box drops to y=11.5, then horizontal to (50, 11.5)
    draw_segment(18, 18.75, 18, 11.5)
    draw_segment(18, 11.5, 50, 11.5)

    # Right box drops to y=11.5, then horizontal to (50, 11.5)
    draw_segment(82, 18.75, 82, 11.5)
    draw_segment(82, 11.5, 50, 11.5)

    # Center box drops to y=11.5
    draw_segment(50, 18.75, 50, 11.5)

    # Single unified arrow from junction (50, 11.5) into Bottom Oval (50, 7.5)
    draw_arrow(50, 11.5, 50, 7.5)

    # 7. Bottom Oval: Transmission Complete
    draw_oval(50, 4.5, 50, 6.0, "Transmission Complete\nReliability Preserved")

    plt.tight_layout()

    out_paths = [
        Path('3. IOT/fig5_iot_cgps_rpl_flowchart_square.png'),
        Path('3. IOT/fig5_iot_cgps_rpl_flowchart_square.pdf'),
        Path('3. IOT/plots/fig5_iot_cgps_rpl_flowchart_square.png'),
        Path('3. IOT/fig5_iot_cgps_rpl_flowchart_bw.png'),
        Path('3. IOT/fig5_iot_cgps_rpl_flowchart.png'),
        Path('plots/fig5_iot_cgps_rpl_flowchart_square.png'),
        Path('E:/ns3_research/3. IOT/fig5_iot_cgps_rpl_flowchart_square.png'),
        Path('E:/ns3_research/3. IOT/fig5_iot_cgps_rpl_flowchart_square.pdf'),
        Path('E:/ns3_research/3. IOT/fig5_iot_cgps_rpl_flowchart_bw.png'),
        Path('E:/ns3_research/3. IOT/fig5_iot_cgps_rpl_flowchart.png'),
        Path(r'C:\Users\Visha\.gemini\antigravity\brain\4c6a520a-6273-48ca-8321-490bb514a64a\fig5_iot_cgps_rpl_flowchart_square.png')
    ]
    for p in out_paths:
        p.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(p, dpi=300, facecolor='#FFFFFF')
        print(f"Saved: {p}")

    plt.close()

    im = Image.open('3. IOT/fig5_iot_cgps_rpl_flowchart_square.png')
    print("Clean Square Flowchart Generated:", im.size, "Aspect Ratio:", round(im.size[0] / im.size[1], 2))

if __name__ == '__main__':
    generate_square_cgps_rpl_flowchart()
