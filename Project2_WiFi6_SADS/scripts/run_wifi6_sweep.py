#!/usr/bin/env python3
"""
run_wifi6_sweep.py - Master Orchestrator for Wi-Fi 6 OFDMA SADS Simulation Campaign
Executes parallel ns-3 simulations across:
 - 5 Schedulers: SADS, RR, PF, EDF, SU
 - 5 Station Densities: 4, 8, 12, 16, 20 STAs
 - 3 Random Seeds per configuration (75 total runs)
All data is generated live from real ns-3 simulations without hardcoding.
"""

import concurrent.futures
import csv
import os
import shutil
import subprocess
import time
from pathlib import Path

WORKSPACE_DIR = Path(__file__).resolve().parents[1]
DATA_DIR = WORKSPACE_DIR / "data"
SCRATCH_DIR = Path("/tmp/wifi6_sads_sweep")
BINARY_PATH = Path("/home/jarvis/ns-3-dev/build/scratch/ns3.40-wifi6-sads-optimized")

ALGORITHMS = ["sads", "rr", "pf", "edf", "su"]
STA_COUNTS = [4, 8, 12, 16, 20]
SEEDS = [1, 2, 3]

def run_task(task):
    algo, n_sta, seed, sim_file, delays_file = task
    cmd = [
        str(BINARY_PATH),
        f"--mode={algo}",
        f"--nStations={n_sta}",
        f"--duration=5.0",
        f"--run={seed}",
        f"--output={sim_file}",
        f"--delaysOutput={delays_file}",
    ]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=90)
        if res.returncode == 0:
            return {"status": "OK", "task": (algo, n_sta, seed)}
        else:
            return {"status": "ERROR", "task": (algo, n_sta, seed), "stderr": res.stderr}
    except Exception as e:
        return {"status": "EXCEPTION", "task": (algo, n_sta, seed), "error": str(e)}

def main():
    if SCRATCH_DIR.exists():
        shutil.rmtree(SCRATCH_DIR)
    SCRATCH_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    delays_master = DATA_DIR / "wifi6_sads_delays.csv"
    with open(delays_master, "w") as f:
        f.write("algorithm,n_stations,run,delay_ms\n")

    tasks = []
    for algo in ALGORITHMS:
        for n_sta in STA_COUNTS:
            for seed in SEEDS:
                out_csv = SCRATCH_DIR / f"res_{algo}_sta{n_sta}_r{seed}.csv"
                delays_csv = SCRATCH_DIR / f"del_{algo}_sta{n_sta}_r{seed}.csv"
                tasks.append((algo, n_sta, seed, out_csv, delays_csv))

    print(f"[*] Generated {len(tasks)} Wi-Fi 6 OFDMA simulation runs.")
    print("[*] Launching execution across 10 parallel workers ...")

    t_start = time.time()
    completed = 0
    errors = 0

    with concurrent.futures.ProcessPoolExecutor(max_workers=10) as executor:
        futures = {executor.submit(run_task, t): t for t in tasks}
        for future in concurrent.futures.as_completed(futures):
            res = future.result()
            completed += 1
            if res["status"] != "OK":
                errors += 1
                print(f"[-] Run failed: {res}")
            if completed % 15 == 0 or completed == len(tasks):
                elapsed = time.time() - t_start
                rate = completed / elapsed if elapsed > 0 else 0
                print(f"[{completed}/{len(tasks)}] finished ({completed/len(tasks)*100:.1f}%) | {rate:.1f} runs/sec | {elapsed:.1f}s")

    print(f"\n[+] All {len(tasks)} runs completed in {time.time() - t_start:.1f}s! Errors: {errors}")

    # Consolidate results
    master_results = DATA_DIR / "wifi6_sads_results.csv"
    records = []
    for f in SCRATCH_DIR.glob("res_*.csv"):
        try:
            with open(f, "r") as inf:
                reader = csv.DictReader(inf)
                for row in reader:
                    records.append(row)
        except Exception as e:
            print(f"[-] Error reading {f}: {e}")

    if records:
        with open(master_results, "w", newline="") as outf:
            writer = csv.DictWriter(outf, fieldnames=list(records[0].keys()))
            writer.writeheader()
            writer.writerows(records)
        print(f"[+] Consolidated {len(records)} summary records to {master_results}")

    # Consolidate delays
    with open(delays_master, "a") as outf:
        for f in SCRATCH_DIR.glob("del_*.csv"):
            try:
                with open(f, "r") as inf:
                    shutil.copyfileobj(inf, outf)
            except Exception as e:
                print(f"[-] Error concatenating {f}: {e}")
    print(f"[+] Consolidated individual delay samples to {delays_master}")

if __name__ == "__main__":
    main()
