#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "Project 3: 6LoWPAN CGPS-RPL Routing Replication Suite"
echo "=========================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "[*] Step 1: Rendering Publication Figures 1-4 at 300 DPI..."
python3 scripts/generate_iot_figures.py

echo ""
echo "[+] Project 3 replication completed successfully! All 4 figures updated in plots/."
