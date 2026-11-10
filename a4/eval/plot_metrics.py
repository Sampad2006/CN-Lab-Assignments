#!/usr/bin/env python3
"""
CN Lab Assignment 4: CDMA Multiple Access with Walsh Codes
Vector SVG Plot Generator for Noise & No-Noise Benchmarks
Author: Sampad De (002410501025)
"""

import os
import sys
import csv

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")

COLORS = {
    "blue": "#2563EB",
    "red": "#DC2626",
    "green": "#059669",
    "orange": "#D97706",
    "purple": "#7C3AED",
    "cyan": "#0891B2"
}

def render_svg_chart(title, xlabel, ylabel, series_list, out_svg_path, width=780, height=480, log_y=False):
    """
    series_list: [ {"name": str, "color": str, "points": [(x, y), ...], "dashed": bool} ]
    """
    margin_left = 95
    margin_right = 175
    margin_top = 65
    margin_bottom = 70

    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    all_x = [pt[0] for s in series_list for pt in s["points"]]
    all_y = [pt[1] for s in series_list for pt in s["points"]]

    if not all_x or not all_y:
        print(f"[!] Warning: No data points for {out_svg_path}")
        return

    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)

    if min_x == max_x:
        max_x += 1.0
    min_y = 0.0
    max_y = max_y * 1.15 if max_y > 0 else 1.0

    def tx(x):
        return margin_left + ((x - min_x) / (max_x - min_x)) * plot_w

    def ty(y):
        return margin_top + plot_h - ((y - min_y) / (max_y - min_y)) * plot_h

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    svg.append(f'  <rect width="100%" height="100%" fill="#FFFFFF" rx="8"/>')

    # Title & Subtitle
    svg.append(f'  <text x="{width/2}" y="32" text-anchor="middle" font-family="sans-serif" font-size="16" font-weight="bold" fill="#111827">{title}</text>')
    svg.append(f'  <text x="{width/2}" y="50" text-anchor="middle" font-family="sans-serif" font-size="11" fill="#6B7280">CSE/PC/B/S/314 Computer Networks Lab — Sampad De (002410501025)</text>')

    # Gridlines and Y-ticks
    num_y_ticks = 6
    for i in range(num_y_ticks + 1):
        val = min_y + (i / num_y_ticks) * (max_y - min_y)
        y_pos = ty(val)
        svg.append(f'  <line x1="{margin_left}" y1="{y_pos:.1f}" x2="{margin_left + plot_w}" y2="{y_pos:.1f}" stroke="#E5E7EB" stroke-dasharray="3,3"/>')
        val_str = f"{val:.4f}" if max_y < 0.1 else (f"{val:.3f}" if max_y < 1.0 else (f"{val:.1f}" if max_y < 10 else f"{val:.0f}"))
        svg.append(f'  <text x="{margin_left - 10}" y="{y_pos + 4:.1f}" text-anchor="end" font-family="sans-serif" font-size="11" fill="#4B5563">{val_str}</text>')

    # X-ticks
    unique_x = sorted(list(set(all_x)))
    for val in unique_x:
        x_pos = tx(val)
        svg.append(f'  <line x1="{x_pos:.1f}" y1="{margin_top}" x2="{x_pos:.1f}" y2="{margin_top + plot_h}" stroke="#F3F4F6"/>')
        val_str = f"{val:.2f}" if any(isinstance(v, float) and v % 1 != 0 for v in unique_x) else f"{int(val)}"
        svg.append(f'  <text x="{x_pos:.1f}" y="{margin_top + plot_h + 20}" text-anchor="middle" font-family="sans-serif" font-size="11" fill="#4B5563">{val_str}</text>')

    # Axes
    svg.append(f'  <line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#374151" stroke-width="2"/>')
    svg.append(f'  <line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#374151" stroke-width="2"/>')

    # Axis Labels
    svg.append(f'  <text x="{margin_left + plot_w/2}" y="{height - 18}" text-anchor="middle" font-family="sans-serif" font-size="13" font-weight="600" fill="#374151">{xlabel}</text>')
    svg.append(f'  <text x="25" y="{margin_top + plot_h/2}" text-anchor="middle" font-family="sans-serif" font-size="13" font-weight="600" fill="#374151" transform="rotate(-90 25 {margin_top + plot_h/2})">{ylabel}</text>')

    # Series
    legend_y = margin_top + 15
    for s in series_list:
        color = s.get("color", COLORS["blue"])
        pts = s["points"]
        is_dashed = s.get("dashed", False)
        dash_attr = ' stroke-dasharray="5,4"' if is_dashed else ''

        path_d = []
        for i, (px, py) in enumerate(pts):
            sx = tx(px)
            sy = ty(py)
            cmd = "M" if i == 0 else "L"
            path_d.append(f"{cmd} {sx:.1f} {sy:.1f}")
        svg.append(f'  <path d="{" ".join(path_d)}" fill="none" stroke="{color}" stroke-width="2.5"{dash_attr}/>')

        for px, py in pts:
            sx = tx(px)
            sy = ty(py)
            svg.append(f'  <circle cx="{sx:.1f}" cy="{sy:.1f}" r="4" fill="{color}" stroke="#FFFFFF" stroke-width="1.5"/>')

        # Legend item
        svg.append(f'  <line x1="{margin_left + plot_w + 15}" y1="{legend_y}" x2="{margin_left + plot_w + 40}" y2="{legend_y}" stroke="{color}" stroke-width="2.5"{dash_attr}/>')
        svg.append(f'  <circle cx="{margin_left + plot_w + 27.5}" cy="{legend_y}" r="3.5" fill="{color}"/>')
        svg.append(f'  <text x="{margin_left + plot_w + 48}" y="{legend_y + 4}" font-family="sans-serif" font-size="11" fill="#1F2937">{s["name"]}</text>')
        legend_y += 24

    svg.append('</svg>')

    with open(out_svg_path, "w") as f:
        f.write("\n".join(svg))
    print(f"[+] Rendered plot: {out_svg_path}")

def plot_all():
    os.makedirs(RESULTS_DIR, exist_ok=True)

    # 1. Throughput & Aggregate Bitrate vs Stations (No-Noise Case)
    tput_csv = os.path.join(RESULTS_DIR, "throughput_vs_stations.csv")
    if os.path.exists(tput_csv):
        pts_tput = []
        pts_bitrate = []
        pts_delay = []
        with open(tput_csv, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                n = float(row["stations_N"])
                tput = float(row["throughput_rounds_sec"])
                delay = float(row["avg_slot_ms"])
                bitrate = float(row["aggregate_bitrate_kbps"])
                pts_tput.append((n, tput))
                pts_delay.append((n, delay))
                pts_bitrate.append((n, bitrate))

        render_svg_chart(
            "CDMA Throughput & Capacity vs Stations N (Ideal Channel)",
            "Number of Contending Stations (N)",
            "Throughput (Slotted Rounds / sec)",
            [
                {"name": "Slot Throughput (rounds/s)", "color": COLORS["blue"], "points": pts_tput, "dashed": False}
            ],
            os.path.join(RESULTS_DIR, "throughput_vs_stations.svg")
        )

        render_svg_chart(
            "Average Round Barrier Latency vs Stations N",
            "Number of Contending Stations (N)",
            "Round Barrier Latency (ms)",
            [
                {"name": "Round Barrier Latency (ms)", "color": COLORS["red"], "points": pts_delay, "dashed": False}
            ],
            os.path.join(RESULTS_DIR, "avg_slot_delay_vs_stations.svg")
        )

    # 2. BER vs Noise StdDev: Empirical vs Theoretical Q-function
    ber_csv = os.path.join(RESULTS_DIR, "ber_vs_noise.csv")
    if os.path.exists(ber_csv):
        pts_emp = []
        pts_theo = []
        with open(ber_csv, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                noise = float(row["noise_stddev"])
                emp_ber = float(row["empirical_ber"])
                theo_ber = float(row["theoretical_ber"])
                pts_emp.append((noise, emp_ber))
                pts_theo.append((noise, theo_ber))

        render_svg_chart(
            "Bit Error Rate (BER) vs Additive Noise StdDev (M=4)",
            "Channel Noise StdDev (sigma)",
            "Bit Error Rate (BER)",
            [
                {"name": "Empirical BER (Simulated)", "color": COLORS["red"], "points": pts_emp, "dashed": False},
                {"name": "Theoretical Q-Function", "color": COLORS["blue"], "points": pts_theo, "dashed": True}
            ],
            os.path.join(RESULTS_DIR, "ber_vs_noise.svg")
        )

    # 3. Processing Gain: BER vs Spreading Factor M (Fixed Noise = 0.8)
    gain_csv = os.path.join(RESULTS_DIR, "ber_vs_spreading_factor.csv")
    if os.path.exists(gain_csv):
        pts_gain_emp = []
        pts_gain_theo = []
        with open(gain_csv, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                m = float(row["walsh_dim_M"])
                emp_ber = float(row["empirical_ber"])
                theo_ber = float(row["theoretical_ber"])
                pts_gain_emp.append((m, emp_ber))
                pts_gain_theo.append((m, theo_ber))

        render_svg_chart(
            "Processing Gain: BER vs Spreading Factor M (sigma = 0.80)",
            "Walsh Spreading Factor M (Chips / Bit)",
            "Bit Error Rate (BER)",
            [
                {"name": "Empirical BER", "color": COLORS["green"], "points": pts_gain_emp, "dashed": False},
                {"name": "Theoretical BER", "color": COLORS["purple"], "points": pts_gain_theo, "dashed": True}
            ],
            os.path.join(RESULTS_DIR, "processing_gain_ber_vs_m.svg")
        )

if __name__ == "__main__":
    plot_all()
