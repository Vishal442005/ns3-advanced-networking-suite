#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "Project 2: Wi-Fi 6 OFDMA SADS Scheduler Replication Suite"
echo "=========================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "[*] Step 1: Rendering Publication Figures 1-4 at 300 DPI..."
python3 scripts/generate_wifi6_figures.py

echo ""
echo "[+] Project 2 replication completed successfully! All 4 figures updated in plots/."
