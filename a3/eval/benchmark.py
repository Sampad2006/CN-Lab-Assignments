#!/usr/bin/env python3
import os
import sys
import subprocess
import csv
import statistics

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
A3_DIR = os.path.dirname(SCRIPT_DIR)
SIMULATOR_BIN = os.path.join(A3_DIR, "simulator")
RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")

os.makedirs(RESULTS_DIR, exist_ok=True)

def run_simulation(protocol, stations, p_val=0.1, frames=20, slot=1.0, txtime=5.0, jamtime=1.0):
    cmd = [
        SIMULATOR_BIN,
        "-p", protocol,
        "-N", str(stations),
        "--prob", str(p_val),
        "-n", str(frames),
        "--slot", str(slot),
        "--txtime", str(txtime),
        "--jamtime", str(jamtime),
        "--csv"
    ]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"[Error] Simulation failed: {res.stderr}")
        return None
    
    line = res.stdout.strip()
    if not line:
        return None
    
    parts = line.split(",")
    # Protocol,Stations,P,FramesPerStation,TotalAttempted,Successful,Dropped,Collisions,Jams,AvgDelayMs,ThroughputKbps,ChannelUtilizationPct,TxEfficiencyPct,SimDurationMs
    return {
        "protocol": parts[0],
        "stations": int(parts[1]),
        "p": float(parts[2]),
        "frames_per_station": int(parts[3]),
        "attempted": int(parts[4]),
        "successful": int(parts[5]),
        "dropped": int(parts[6]),
        "collisions": int(parts[7]),
        "jams": int(parts[8]),
        "avg_delay_ms": float(parts[9]),
        "throughput_kbps": float(parts[10]),
        "utilization_pct": float(parts[11]),
        "tx_efficiency_pct": float(parts[12]),
        "sim_time_ms": float(parts[13])
    }

def run_p_persistent_sweep():
    print("==========================================================")
    print(" Running Experiment 1: p-Persistent CSMA Sweep (Varying p)")
    print("==========================================================")
    
    csv_file = os.path.join(RESULTS_DIR, "p_persistent_sweep.csv")
    p_values = [0.01, 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]
    fixed_n = 5
    trials = 3

    with open(csv_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "P", "Stations", "Collisions", "AvgDelayMs", "ThroughputKbps", 
            "ChannelUtilizationPct", "TxEfficiencyPct", "SuccessRatePct"
        ])

        for p in p_values:
            collisions_list = []
            delay_list = []
            tp_list = []
            util_list = []
            eff_list = []
            succ_list = []

            for _ in range(trials):
                data = run_simulation("pp", fixed_n, p_val=p, frames=10)
                if data:
                    collisions_list.append(data["collisions"])
                    delay_list.append(data["avg_delay_ms"])
                    tp_list.append(data["throughput_kbps"])
                    util_list.append(data["utilization_pct"])
                    eff_list.append(data["tx_efficiency_pct"])
                    succ_pct = (data["successful"] * 100.0 / data["attempted"]) if data["attempted"] > 0 else 0.0
                    succ_list.append(succ_pct)

            avg_col = statistics.mean(collisions_list) if collisions_list else 0
            avg_delay = statistics.mean(delay_list) if delay_list else 0
            avg_tp = statistics.mean(tp_list) if tp_list else 0
            avg_util = statistics.mean(util_list) if util_list else 0
            avg_eff = statistics.mean(eff_list) if eff_list else 0
            avg_succ = statistics.mean(succ_list) if succ_list else 0

            writer.writerow([
                f"{p:.2f}", fixed_n, f"{avg_col:.1f}", f"{avg_delay:.2f}", 
                f"{avg_tp:.2f}", f"{avg_util:.2f}", f"{avg_eff:.2f}", f"{avg_succ:.2f}"
            ])
            print(f"  p = {p:<4.2f} | Collisions: {avg_col:>4.1f} | Avg Delay: {avg_delay:>6.2f} ms | Throughput: {avg_tp:>6.2f} kbps | Eff: {avg_eff:>5.1f}% | Util: {avg_util:>5.1f}%")

    print(f"\n[Saved] Results written to {csv_file}\n")

def run_csma_comparison_sweep():
    print("==========================================================")
    print(" Running Experiment 2: Comparative Analysis of All Schemes")
    print(" (Non-Persistent, 1-Persistent, p-Persistent, CSMA/CD vs N)")
    print("==========================================================")

    csv_file = os.path.join(RESULTS_DIR, "csma_comparison_sweep.csv")
    schemes = [
        ("np", "Non-Persistent CSMA", lambda n: 0.1),
        ("1p", "1-Persistent CSMA", lambda n: 1.0),
        ("pp", "p-Persistent CSMA (p=0.1)", lambda n: 0.1),
        ("pp_best", "p-Persistent CSMA (Best p=1/N)", lambda n: round(max(0.01, min(1.0, 1.0 / n)), 3)),
        ("cd", "CSMA/CD", lambda n: 1.0)
    ]
    station_counts = [1, 2, 4, 6, 8, 10, 15, 20, 25, 30]
    trials = 3

    with open(csv_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "ProtocolKey", "ProtocolName", "Stations", "Collisions", "AvgDelayMs", 
            "ThroughputKbps", "ChannelUtilizationPct", "TxEfficiencyPct", "SuccessRatePct"
        ])

        for key, name, p_func in schemes:
            proto_key = "pp" if key == "pp_best" else key
            print(f"\nEvaluating scheme: {name}")
            for N in station_counts:
                p_val = p_func(N)
                collisions_list = []
                delay_list = []
                tp_list = []
                util_list = []
                eff_list = []
                succ_list = []

                for _ in range(trials):
                    data = run_simulation(proto_key, N, p_val=p_val, frames=10)
                    if data:
                        collisions_list.append(data["collisions"])
                        delay_list.append(data["avg_delay_ms"])
                        tp_list.append(data["throughput_kbps"])
                        util_list.append(data["utilization_pct"])
                        eff_list.append(data["tx_efficiency_pct"])
                        succ_pct = (data["successful"] * 100.0 / data["attempted"]) if data["attempted"] > 0 else 0.0
                        succ_list.append(succ_pct)

                avg_col = statistics.mean(collisions_list) if collisions_list else 0
                avg_delay = statistics.mean(delay_list) if delay_list else 0
                avg_tp = statistics.mean(tp_list) if tp_list else 0
                avg_util = statistics.mean(util_list) if util_list else 0
                avg_eff = statistics.mean(eff_list) if eff_list else 0
                avg_succ = statistics.mean(succ_list) if succ_list else 0

                writer.writerow([
                    key, name, N, f"{avg_col:.1f}", f"{avg_delay:.2f}", 
                    f"{avg_tp:.2f}", f"{avg_util:.2f}", f"{avg_eff:.2f}", f"{avg_succ:.2f}"
                ])
                print(f"  N = {N:<2} | Collisions: {avg_col:>5.1f} | Delay: {avg_delay:>7.2f} ms | Throughput: {avg_tp:>6.2f} kbps | Eff: {avg_eff:>5.1f}% | Util: {avg_util:>5.1f}%")

    print(f"\n[Saved] Results written to {csv_file}\n")

if __name__ == "__main__":
    if not os.path.exists(SIMULATOR_BIN):
        print(f"Building simulator binary first...")
        subprocess.run(["make", "-C", A3_DIR, "simulator"], check=True)

    run_p_persistent_sweep()
    run_csma_comparison_sweep()
    print("All benchmark experiments completed successfully!")
