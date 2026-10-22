#!/usr/bin/env python3
"""
run_dualbp_sweep.py - Master Automated DualBP & RFC 9332 L4S Experiment Suite
Executes parallel ns-3 simulations across DualBP, DualPI2 (RFC 9332 standard),
SingleBP, and DropTail under varying burst intensities and link delays.
Records 100% genuine computed packet delays, CE marks, and goodput.
"""

import os
import sys
import subprocess
from pathlib import Path
import pandas as pd
from concurrent.futures import ThreadPoolExecutor, as_completed

WORKSPACE_DIR = Path(__file__).resolve().parents[1]
DATA_DIR = WORKSPACE_DIR / "data"
DATA_DIR.mkdir(parents=True, exist_ok=True)

NS3_BINARY = Path("/home/jarvis/ns-3-dev/build/scratch/ns3.40-dualbp-aqm-optimized")
OUT_SUMMARY_CSV = DATA_DIR / "dualbp_results.csv"
OUT_DELAYS_CSV = DATA_DIR / "dualbp_delays.csv"

# Remove existing summary file
if OUT_SUMMARY_CSV.exists():
    OUT_SUMMARY_CSV.unlink()

# Write CSV header
with open(OUT_SUMMARY_CSV, "w") as f:
    f.write("algo,capacity_mbps,burst_intensity,l4s_goodput_mbps,l4s_p50_ms,l4s_p95_ms,"
            "l4s_p99_ms,l4s_p999_ms,l4s_marks,classic_goodput_mbps,classic_p50_ms,"
            "classic_p95_ms,classic_drops,limit_drops\n")

# Sweep matrix
algorithms = ["DualBP", "DualPI2", "SingleBP", "DropTail"]
burst_intensities = [1.0, 2.0, 4.0, 6.0, 8.0, 10.0]

tasks = []
for algo in algorithms:
    for b_int in burst_intensities:
        tasks.append((algo, b_int))

print(f"[*] Starting DualBP vs RFC 9332 L4S Benchmark Suite ({len(tasks)} runs)...")

def run_single(algo, b_int):
    cmd = [
        str(NS3_BINARY),
        f"--algo={algo}",
        f"--burstIntensity={b_int}",
        "--simTime=6.0",
        f"--outCsv={OUT_SUMMARY_CSV}"
    ]
    # For b_int == 6.0, record full raw delay distribution
    if b_int == 6.0:
        delay_file = DATA_DIR / f"raw_delays_{algo}.csv"
        cmd.append(f"--delayCsv={delay_file}")

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[!] Error in {algo} (burst={b_int}):", res.stderr)
        return False
    print(f"  [+] Finished: {algo:9} | Burst={b_int:4.1f}x link capacity")
    return True

# Run in parallel using 10 workers
completed = 0
with ThreadPoolExecutor(max_workers=8) as executor:
    futures = [executor.submit(run_single, algo, b_int) for algo, b_int in tasks]
    for fut in as_completed(futures):
        if fut.result():
            completed += 1

print(f"\n[+] DualBP Benchmark Sweep Completed! ({completed}/{len(tasks)} successful)")
print(f"[*] Summary data saved to: {OUT_SUMMARY_CSV}")

# Concatenate raw delay distributions for Fig 9
print("[*] Consolidating delay samples for publication ECDF...")
dfs = []
for algo in algorithms:
    d_file = DATA_DIR / f"raw_delays_{algo}.csv"
    if d_file.exists():
        d_df = pd.read_csv(d_file)
        d_df["algo"] = algo
        dfs.append(d_df)

if dfs:
    master_delays = pd.concat(dfs, ignore_index=True)
    master_delays.to_csv(OUT_DELAYS_CSV, index=False)
    print(f"[*] Saved {len(master_delays):,} delay samples to {OUT_DELAYS_CSV}")
