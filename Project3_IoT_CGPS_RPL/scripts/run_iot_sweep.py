#!/usr/bin/env python3
"""
run_iot_sweep.py - Master Orchestrator for 6LoWPAN / RPL Sensor Mesh Simulation Campaign
Executes parallel ns-3 simulations across:
 - 4 Routing Disciplines: CGPS-RPL, MRHOF, OF0, STATIC
 - 4 Node Densities: 20, 35, 50, 65 Nodes
 - 5 Random Seeds per configuration (80 total runs)
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
SCRATCH_DIR = Path("/tmp/iot_rpl_sweep")
BINARY_PATH = Path("/home/jarvis/ns-3-dev/build/scratch/ns3.40-iot-cgps-rpl-optimized")

ALGORITHMS = ["cgps", "mrhof", "of0", "static"]
NODE_COUNTS = [20, 35, 50, 65]
SEEDS = [1, 2, 3, 4, 5]

def run_task(task):
    algo, n_nodes, seed, sim_file, delays_file = task
    cmd = [
        str(BINARY_PATH),
        f"--mode={algo}",
        f"--nNodes={n_nodes}",
        f"--duration=15.0",
        f"--run={seed}",
        f"--output={sim_file}",
        f"--delaysOutput={delays_file}",
    ]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=60)
        if res.returncode == 0:
            return {"status": "OK", "task": (algo, n_nodes, seed)}
        else:
            return {"status": "ERROR", "task": (algo, n_nodes, seed), "stderr": res.stderr}
    except Exception as e:
        return {"status": "EXCEPTION", "task": (algo, n_nodes, seed), "error": str(e)}

def main():
    if SCRATCH_DIR.exists():
        shutil.rmtree(SCRATCH_DIR)
    SCRATCH_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    delays_master = DATA_DIR / "iot_cgps_rpl_delays.csv"
    with open(delays_master, "w") as f:
        f.write("algorithm,n_nodes,run,delay_ms\n")

    tasks = []
    for algo in ALGORITHMS:
        for n in NODE_COUNTS:
            for seed in SEEDS:
                out_csv = SCRATCH_DIR / f"res_{algo}_n{n}_r{seed}.csv"
                delays_csv = SCRATCH_DIR / f"del_{algo}_n{n}_r{seed}.csv"
                tasks.append((algo, n, seed, out_csv, delays_csv))

    print(f"[*] Generated {len(tasks)} 6LoWPAN RPL simulation runs.")
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
            if completed % 20 == 0 or completed == len(tasks):
                elapsed = time.time() - t_start
                rate = completed / elapsed if elapsed > 0 else 0
                print(f"[{completed}/{len(tasks)}] finished ({completed/len(tasks)*100:.1f}%) | {rate:.1f} runs/sec | {elapsed:.1f}s")

    print(f"\n[+] All {len(tasks)} runs completed in {time.time() - t_start:.1f}s! Errors: {errors}")

    # Consolidate results
    master_results = DATA_DIR / "iot_cgps_rpl_results.csv"
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
