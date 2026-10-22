#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "Project 1: DualBP & BP-AQM Full Replication Suite"
echo "=========================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 1. Verify / Render figures from genuine data
echo "[*] Step 1: Rendering Architectural Pipeline Diagram (Figure 0)..."
python3 scripts/generate_arch_diagram.py

echo "[*] Step 2: Generating Baseline Evaluation Suite Figures 1-8..."
python3 scripts/generate_top_tier_plots.py

echo "[*] Step 3: Generating DualBP and Kernel eBPF Telemetry Figures 9-10..."
python3 scripts/generate_dualbp_figures.py

echo "[*] Step 4: Running Statistical Suite & Generating Tables..."
python3 scripts/analyze_advanced.py

echo ""
echo "[+] Project 1 replication completed successfully! All 11 figures updated in plots/."
