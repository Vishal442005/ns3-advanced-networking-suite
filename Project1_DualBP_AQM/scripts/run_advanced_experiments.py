#!/usr/bin/env python3
"""
run_advanced_experiments.py - Master High-Performance ns-3 Experiment Orchestrator
Executes full parametric sweeps across 8 algorithms (including COBALT, BP-AQM, CoDel, PIE, FQ-CoDel, DropTail),
dynamic capacity collapse (5G/Wi-Fi fading), BBR/CUBIC/NewReno stacks, multi-size flow transfers,
and exports granular delay distributions for EVT and ECDF modeling.
All data is generated live from real ns-3 simulations.
"""

import argparse
import concurrent.futures
import csv
import itertools
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ALGORITHMS = [
    "burst",
    "codel",
    "pie",
    "cobalt",
    "fq",
    "droptail",
    "no-burst",
    "no-guard",
]

DEFAULT_NS3_DIR = Path("/home/jarvis/ns-3-dev")
WORKSPACE_DIR = Path(__file__).resolve().parents[1]
FINAL_RESULTS_DIR = WORKSPACE_DIR / "data" / "raw_results"
SCRATCH_RESULTS_DIR = Path("/tmp/burst_aqm_advanced_results")
BINARY_PATH = DEFAULT_NS3_DIR / "build" / "scratch" / "ns3.40-burst-aqm-optimized"


def run_single_sim(task):
    """Executes a single ns-3 run."""
    (
        algo,
        run_id,
        duration,
        capacity,
        delay,
        burst_dur,
        dynamic_drop,
        tcp_proto,
        export_delays,
        result_file,
        trace_file
    ) = task

    cmd = [
        str(BINARY_PATH),
        f"--algorithm={algo}",
        f"--run={run_id}",
        f"--duration={duration}",
        f"--capacityMbps={capacity}",
        f"--linkDelayMs={delay}",
        f"--burstDuration={burst_dur}",
        f"--dynamicDrop={dynamic_drop}",
        f"--tcp={tcp_proto}",
        f"--output={result_file}",
        f"--exportDelays={1 if export_delays else 0}",
    ]

    if trace_file:
        cmd.append(f"--trace={trace_file}")

    try:
        res = subprocess.run(
            cmd,
            cwd=DEFAULT_NS3_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=120,
        )
        if res.returncode != 0:
            return {"status": "ERROR", "task": task, "stderr": res.stderr}
        return {"status": "OK", "result_file": result_file}
    except Exception as e:
        return {"status": "EXCEPTION", "task": task, "error": str(e)}


def main():
    parser = argparse.ArgumentParser(description="Run advanced ns-3 experiments.")
    parser.add_argument("--ns3-dir", type=Path, default=DEFAULT_NS3_DIR)
    parser.add_argument("--workers", type=int, default=10)
    parser.add_argument("--runs", type=int, default=10, help="Random seeds per config")
    parser.add_argument("--duration", type=float, default=25.0)
    args = parser.parse_args()

    # Clean & recreate scratch dir
    if SCRATCH_RESULTS_DIR.exists():
        shutil.rmtree(SCRATCH_RESULTS_DIR)
    SCRATCH_RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    FINAL_RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    print(f"[*] Recompiling ns-3 binary in {args.ns3_dir} ...")
    subprocess.run(["./ns3", "build", "burst-aqm"], cwd=args.ns3_dir, check=True)

    tasks = []

    # 1. Main Benchmark Matrix: 8 algorithms x 4 burst durations x 10 seeds (Capacity=10M, Delay=20ms)
    bursts = [0.0, 0.02, 0.10, 0.20]
    for b in bursts:
        for run_id in range(1, args.runs + 1):
            for algo in ALGORITHMS:
                stem = f"base_{algo}_b{int(b*1000)}ms_r{run_id}"
                res_csv = SCRATCH_RESULTS_DIR / f"{stem}.csv"
                trace_csv = (SCRATCH_RESULTS_DIR / f"{stem}.trace.csv") if (run_id == 1 and algo in ["burst", "no-burst", "no-guard"]) else None
                export_delays = (run_id == 1 and b == 0.02) # Export full delay distribution for run 1 of baseline
                tasks.append((algo, run_id, args.duration, 10.0, 20.0, b, 0.0, "TcpCubic", export_delays, res_csv, trace_csv))

    # 2. Dynamic 5G Capacity Collapse Stress Test: 50% drop at t=12s (8 algorithms x 5 seeds)
    for run_id in range(1, 6):
        for algo in ALGORITHMS:
            stem = f"dynamic5G_{algo}_r{run_id}"
            res_csv = SCRATCH_RESULTS_DIR / f"{stem}.csv"
            tasks.append((algo, run_id, args.duration, 10.0, 20.0, 0.02, 0.5, "TcpCubic", False, res_csv, None))

    # 3. Modern Transport Stack Diversity (TcpBbr vs. TcpCubic vs. TcpNewReno)
    for tcp_proto in ["TcpBbr", "TcpNewReno"]:
        for run_id in range(1, 6):
            for algo in ["burst", "codel", "cobalt", "pie", "droptail"]:
                stem = f"proto_{tcp_proto}_{algo}_r{run_id}"
                res_csv = SCRATCH_RESULTS_DIR / f"{stem}.csv"
                tasks.append((algo, run_id, args.duration, 10.0, 20.0, 0.02, 0.0, tcp_proto, False, res_csv, None))

    # 4. Capacity & Propagation Delay Scaling Study
    for cap in [5.0, 25.0, 50.0]:
        for run_id in range(1, 6):
            for algo in ["burst", "codel", "cobalt", "pie", "fq", "droptail"]:
                stem = f"cap{int(cap)}M_{algo}_r{run_id}"
                res_csv = SCRATCH_RESULTS_DIR / f"{stem}.csv"
                tasks.append((algo, run_id, args.duration, cap, 20.0, 0.02, 0.0, "TcpCubic", False, res_csv, None))

    for delay in [10.0, 50.0]:
        for run_id in range(1, 6):
            for algo in ["burst", "codel", "cobalt", "pie", "fq", "droptail"]:
                stem = f"del{int(delay)}ms_{algo}_r{run_id}"
                res_csv = SCRATCH_RESULTS_DIR / f"{stem}.csv"
                tasks.append((algo, run_id, args.duration, 10.0, delay, 0.02, 0.0, "TcpCubic", False, res_csv, None))

    print(f"\n[+] Total experimental runs generated: {len(tasks)}")
    print(f"[*] Launching parallel execution across {args.workers} worker processes ...\n")

    t_start = time.time()
    completed = 0
    errors = 0

    with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as executor:
        futures = {executor.submit(run_single_sim, task): task for task in tasks}
        for future in concurrent.futures.as_completed(futures):
            res = future.result()
            completed += 1
            if res["status"] != "OK":
                errors += 1
                print(f"[-] Sim failed: {res}")
            if completed % 25 == 0 or completed == len(tasks):
                elapsed = time.time() - t_start
                rate = completed / elapsed if elapsed > 0 else 0
                print(f"[{completed}/{len(tasks)}] runs finished ({completed/len(tasks)*100:.1f}%) | Rate: {rate:.1f} runs/sec | Elapsed: {elapsed:.1f}s")

    t_total = time.time() - t_start
    print(f"\n[+] All {len(tasks)} simulation runs completed in {t_total:.1f} seconds! (Errors: {errors})")

    # 5. Synchronize files to FINAL_RESULTS_DIR and consolidate
    print(f"[*] Synchronizing results from {SCRATCH_RESULTS_DIR} to {FINAL_RESULTS_DIR} ...")
    master_csv = WORKSPACE_DIR / "data" / "simulation_results.csv"

    records = []
    for csv_file in SCRATCH_RESULTS_DIR.glob("*.csv"):
        dest_file = FINAL_RESULTS_DIR / csv_file.name
        shutil.copyfile(csv_file, dest_file)

        if not csv_file.name.endswith(".flows.csv") and not csv_file.name.endswith(".trace.csv") and not csv_file.name.endswith(".delays.csv"):
            try:
                with open(csv_file, "r") as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        records.append(row)
            except Exception as e:
                print(f"[-] Error reading {csv_file}: {e}")

    if records:
        keys = list(records[0].keys())
        with open(master_csv, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=keys)
            writer.writeheader()
            writer.writerows(records)
        print(f"[+] Successfully wrote {len(records)} experimental records to {master_csv}")
    else:
        print("[-] No records found to aggregate!")


if __name__ == "__main__":
    main()
