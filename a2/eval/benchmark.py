#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import re
import csv

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
A2_DIR = os.path.dirname(SCRIPT_DIR)
SENDER_BIN = os.path.join(A2_DIR, "sender")
RECEIVER_BIN = os.path.join(A2_DIR, "receiver")
TEST_INPUT = os.path.join(A2_DIR, "tests", "test_input.txt")
RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")

os.makedirs(RESULTS_DIR, exist_ok=True)

def parse_metrics(output_text):
    metrics = {
        "original_frames": 0,
        "total_transmissions": 0,
        "retransmissions": 0,
        "acks": 0,
        "naks": 0,
        "timeouts": 0,
        "corrupted": 0,
        "wire_bytes": 0,
        "payload_bytes": 0,
        "elapsed_ms": 0.0,
        "avg_rtt_ms": 0.0,
        "rto_ms": 0.0,
        "efficiency_pct": 0.0,
        "throughput_kbps": 0.0
    }

    patterns = {
        "original_frames": r"Original Data Frames Sent:\s*(\d+)",
        "total_transmissions": r"Total Frames Transmitted:\s*(\d+)",
        "retransmissions": r"Retransmissions:\s*(\d+)",
        "acks": r"ACKs Processed.*?:\s*(\d+)",
        "naks": r"NAKs Processed:\s*(\d+)",
        "timeouts": r"Timeouts Occurred:\s*(\d+)",
        "corrupted": r"Corrupted Frames Detected:\s*(\d+)",
        "wire_bytes": r"Total Wire Bytes Sent:\s*(\d+)",
        "payload_bytes": r"Payload Sent.*?:\s*(\d+)",
        "elapsed_ms": r"Total Elapsed Time:\s*([\d\.]+)\s*ms",
        "avg_rtt_ms": r"Average Round-Trip Time:\s*([\d\.]+)\s*ms",
        "rto_ms": r"Final Timeout \(RTO\):\s*([\d\.]+)\s*ms",
        "efficiency_pct": r"Protocol Efficiency:\s*([\d\.]+)\s*%",
        "throughput_kbps": r"Effective Throughput:\s*([\d\.]+)\s*kbps"
    }

    for key, pat in patterns.items():
        m = re.search(pat, output_text)
        if m:
            val_str = m.group(1)
            metrics[key] = float(val_str) if "." in val_str else int(val_str)

    # Compute link utilization efficiency: (original_frames * T_tx) / elapsed_ms
    # where T_tx = 0.8 ms per frame when delay > 0
    if metrics["elapsed_ms"] > 0:
        ideal_tx_ms = metrics["original_frames"] * 0.8
        metrics["link_util_pct"] = min(100.0, (ideal_tx_ms / metrics["elapsed_ms"]) * 100.0)
    else:
        metrics["link_util_pct"] = 100.0

    return metrics

def run_single_test(protocol, window, loss, error, delay, ack_loss=0.0, port=9900, timeout_init=120.0):
    output_file = os.path.join(A2_DIR, "tests", f"eval_{protocol}_{port}.txt")
    if os.path.exists(output_file):
        os.remove(output_file)

    recv_cmd = [
        RECEIVER_BIN,
        "--protocol", protocol,
        "--port", str(port),
        "--window", str(window),
        "--loss", str(ack_loss),
        "--output", output_file,
        "--quiet"
    ]

    recv_proc = subprocess.Popen(recv_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.12)

    send_cmd = [
        SENDER_BIN,
        "--protocol", protocol,
        "--file", TEST_INPUT,
        "--port", str(port),
        "--window", str(window),
        "--payload", "64",
        "--loss", str(loss),
        "--error", str(error),
        "--delay", str(delay),
        "--timeout", str(timeout_init),
        "--quiet"
    ]

    send_proc = subprocess.run(send_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        recv_proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        recv_proc.kill()

    diff_proc = subprocess.run(["diff", "-q", TEST_INPUT, output_file], stdout=subprocess.PIPE)
    verified = (diff_proc.returncode == 0)

    if os.path.exists(output_file):
        os.remove(output_file)

    metrics = parse_metrics(send_proc.stdout)
    metrics["verified"] = verified
    metrics["protocol"] = protocol
    metrics["window"] = window
    metrics["loss"] = loss
    metrics["error"] = error
    metrics["ack_loss"] = ack_loss
    metrics["delay"] = delay
    return metrics

def run_case_1_rtt_delay():
    print("\n=====================================================================")
    print("  CASE 1: Propagation Delay vs. Packet-to-ACK Reception Time (RTT)   ")
    print("=====================================================================")
    delays = [0, 5, 10, 20, 30]
    protocols = ["sw", "gbn", "sr"]
    results = []

    port = 11000
    for d in delays:
        for p in protocols:
            port += 1
            win = 1 if p == "sw" else 4
            print(f"Running {p.upper():3s} (Win={win}) | Delay={d:2d}ms ...", end=" ", flush=True)
            res = run_single_test(protocol=p, window=win, loss=0.0, error=0.0, delay=d, port=port)
            results.append(res)
            print(f"RTT={res['avg_rtt_ms']:6.2f} ms | TotalTime={res['elapsed_ms']:7.2f} ms | Tput={res['throughput_kbps']:7.1f} kbps | Verified={res['verified']}")

    csv_path = os.path.join(RESULTS_DIR, "case1_rtt_delay.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    return results

def run_case_2_lossless_efficiency():
    print("\n=====================================================================")
    print("  CASE 2: Efficiency Without Error or Lost Frame (Window Scaling)    ")
    print("=====================================================================")
    windows = [1, 2, 4, 8, 16]
    results = []
    port = 11100

    # Baseline SW (Window = 1) at Delay = 10ms
    port += 1
    print(f"Running SW  (Win=1 ) | Delay=10ms ...", end=" ", flush=True)
    res_sw = run_single_test(protocol="sw", window=1, loss=0.0, error=0.0, delay=10.0, port=port)
    results.append(res_sw)
    print(f"TotalTime={res_sw['elapsed_ms']:6.2f} ms | LinkUtil={res_sw['link_util_pct']:5.2f}% | Tput={res_sw['throughput_kbps']:6.1f} kbps")

    for w in windows:
        for p in ["gbn", "sr"]:
            port += 1
            print(f"Running {p.upper():3s} (Win={w:2d}) | Delay=10ms ...", end=" ", flush=True)
            res = run_single_test(protocol=p, window=w, loss=0.0, error=0.0, delay=10.0, port=port)
            results.append(res)
            print(f"TotalTime={res['elapsed_ms']:6.2f} ms | LinkUtil={res['link_util_pct']:5.2f}% | Tput={res['throughput_kbps']:6.1f} kbps")

    csv_path = os.path.join(RESULTS_DIR, "case2_lossless_window.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    return results

def run_case_3a_packet_loss():
    print("\n=====================================================================")
    print("  CASE 3A: Efficiency Across Packet Loss Probabilities (0.1 - 0.5)   ")
    print("=====================================================================")
    probs = [0.1, 0.2, 0.3, 0.4, 0.5]
    protocols = ["sw", "gbn", "sr"]
    results = []
    port = 11200

    for prob in probs:
        for p in protocols:
            port += 1
            win = 1 if p == "sw" else 4
            print(f"Running {p.upper():3s} (Win={win}) | LossProb={prob:.1f} ...", end=" ", flush=True)
            res = run_single_test(protocol=p, window=win, loss=prob, error=0.0, delay=5.0, port=port, timeout_init=60.0)
            results.append(res)
            print(f"BwEff={res['efficiency_pct']:5.2f}% | Retrans={res['retransmissions']:2d} | Tput={res['throughput_kbps']:6.1f} kbps | Verified={res['verified']}")

    csv_path = os.path.join(RESULTS_DIR, "case3a_packet_loss.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    return results

def run_case_3b_bit_error():
    print("\n=====================================================================")
    print("  CASE 3B: Efficiency Across Bit Error Probabilities (0.1 - 0.5)     ")
    print("=====================================================================")
    probs = [0.1, 0.2, 0.3, 0.4, 0.5]
    protocols = ["sw", "gbn", "sr"]
    results = []
    port = 11300

    for prob in probs:
        for p in protocols:
            port += 1
            win = 1 if p == "sw" else 4
            print(f"Running {p.upper():3s} (Win={win}) | ErrorProb={prob:.1f} ...", end=" ", flush=True)
            res = run_single_test(protocol=p, window=win, loss=0.0, error=prob, delay=5.0, port=port, timeout_init=60.0)
            results.append(res)
            print(f"BwEff={res['efficiency_pct']:5.2f}% | Retrans={res['retransmissions']:2d} | NAKs={res['naks']:2d} | Tput={res['throughput_kbps']:6.1f} kbps | Verified={res['verified']}")

    csv_path = os.path.join(RESULTS_DIR, "case3b_bit_error.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    return results

def run_case_3c_ack_loss():
    print("\n=====================================================================")
    print("  CASE 3C: Efficiency Across ACK Loss Probabilities (0.1 - 0.5)      ")
    print("=====================================================================")
    probs = [0.1, 0.2, 0.3, 0.4, 0.5]
    protocols = ["sw", "gbn", "sr"]
    results = []
    port = 11400

    for prob in probs:
        for p in protocols:
            port += 1
            win = 1 if p == "sw" else 4
            print(f"Running {p.upper():3s} (Win={win}) | AckLossProb={prob:.1f} ...", end=" ", flush=True)
            res = run_single_test(protocol=p, window=win, loss=0.0, error=0.0, delay=5.0, ack_loss=prob, port=port, timeout_init=60.0)
            results.append(res)
            print(f"BwEff={res['efficiency_pct']:5.2f}% | Retrans={res['retransmissions']:2d} | Tput={res['throughput_kbps']:6.1f} kbps | Verified={res['verified']}")

    csv_path = os.path.join(RESULTS_DIR, "case3c_ack_loss.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    return results

def main():
    c1 = run_case_1_rtt_delay()
    c2 = run_case_2_lossless_efficiency()
    c3a = run_case_3a_packet_loss()
    c3b = run_case_3b_bit_error()
    c3c = run_case_3c_ack_loss()
    print("\n>>> ALL BENCHMARK SUITES COMPLETED SUCCESSFULLY! <<<")

if __name__ == "__main__":
    main()
