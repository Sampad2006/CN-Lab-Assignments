#!/usr/bin/env python3
import os
import sys
import csv
import math

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")

def render_svg_multiline(title, xlabel, ylabel, series_dict, out_svg_path, width=800, height=500):
    """
    Pure Python vector SVG generator for multi-series line charts with grids, markers, and legends.
    series_dict: { 'SeriesName': [(x1, y1), (x2, y2), ...] }
    """
    margin_left = 90
    margin_right = 180
    margin_top = 60
    margin_bottom = 70

    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    all_x = [pt[0] for pts in series_dict.values() for pt in pts]
    all_y = [pt[1] for pts in series_dict.values() for pt in pts]

    if not all_x or not all_y:
        return

    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)

    if min_x == max_x:
        max_x += 1.0
    if min_y == max_y:
        max_y += 1.0

    min_y = 0.0 if min_y > 0 else min_y
    max_y = max_y * 1.1 if max_y > 0 else 1.0

    def transform_x(x):
        return margin_left + ((x - min_x) / (max_x - min_x)) * plot_w

    def transform_y(y):
        return margin_top + plot_h - ((y - min_y) / (max_y - min_y)) * plot_h

    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b"]
    markers = ["circle", "square", "triangle", "diamond"]

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}" style="background-color:#ffffff; font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Helvetica,Arial,sans-serif;">',
        f'<rect width="{width}" height="{height}" fill="#ffffff"/>',
        # Title
        f'<text x="{width/2}" y="35" text-anchor="middle" font-size="18" font-weight="bold" fill="#111827">{title}</text>',
        # Background grid & axes box
        f'<rect x="{margin_left}" y="{margin_top}" width="{plot_w}" height="{plot_h}" fill="#f9fafb" stroke="#e5e7eb" stroke-width="1"/>'
    ]

    # Gridlines & Y-ticks (5 steps)
    y_steps = 5
    for i in range(y_steps + 1):
        y_val = min_y + (i / y_steps) * (max_y - min_y)
        y_pos = transform_y(y_val)
        svg.append(f'<line x1="{margin_left}" y1="{y_pos}" x2="{margin_left + plot_w}" y2="{y_pos}" stroke="#e5e7eb" stroke-dasharray="4" stroke-width="1"/>')
        svg.append(f'<text x="{margin_left - 12}" y="{y_pos + 4}" text-anchor="end" font-size="11" fill="#4b5563">{y_val:.1f}</text>')

    # X-ticks
    x_steps = 6
    for i in range(x_steps + 1):
        x_val = min_x + (i / x_steps) * (max_x - min_x)
        x_pos = transform_x(x_val)
        svg.append(f'<line x1="{x_pos}" y1="{margin_top}" x2="{x_pos}" y2="{margin_top + plot_h}" stroke="#e5e7eb" stroke-dasharray="4" stroke-width="1"/>')
        svg.append(f'<text x="{x_pos}" y="{margin_top + plot_h + 20}" text-anchor="middle" font-size="11" fill="#4b5563">{x_val:.2f}</text>')

    # Axis labels
    svg.append(f'<text x="{margin_left + plot_w / 2}" y="{height - 20}" text-anchor="middle" font-size="13" font-weight="600" fill="#374151">{xlabel}</text>')
    svg.append(f'<text x="25" y="{margin_top + plot_h / 2}" text-anchor="middle" font-size="13" font-weight="600" fill="#374151" transform="rotate(-90 25 {margin_top + plot_h / 2})">{ylabel}</text>')

    # Draw series lines and markers
    series_idx = 0
    legend_y = margin_top + 10

    for name, points in series_dict.items():
        color = colors[series_idx % len(colors)]
        pts_svg = []
        for x, y in sorted(points, key=lambda p: p[0]):
            px = transform_x(x)
            py = transform_y(y)
            pts_svg.append((px, py))

        # Polyline
        pts_str = " ".join([f"{px:.1f},{py:.1f}" for px, py in pts_svg])
        svg.append(f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{pts_str}" stroke-linejoin="round"/>')

        # Markers
        for px, py in pts_svg:
            svg.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="4" fill="{color}" stroke="#ffffff" stroke-width="1.5"/>')

        # Legend entry
        leg_x = margin_left + plot_w + 25
        svg.append(f'<line x1="{leg_x}" y1="{legend_y}" x2="{leg_x + 20}" y2="{legend_y}" stroke="{color}" stroke-width="2.5"/>')
        svg.append(f'<circle cx="{leg_x + 10}" cy="{legend_y}" r="3.5" fill="{color}"/>')
        svg.append(f'<text x="{leg_x + 28}" y="{legend_y + 4}" font-size="11" font-weight="500" fill="#1f2937">{name}</text>')
        legend_y += 24
        series_idx += 1

    svg.append('</svg>')

    with open(out_svg_path, "w") as f:
        f.write("\n".join(svg))
    print(f"[Plot Generated] {out_svg_path}")

def generate_all_plots():
    p_csv = os.path.join(RESULTS_DIR, "p_persistent_sweep.csv")
    cmp_csv = os.path.join(RESULTS_DIR, "csma_comparison_sweep.csv")

    # -------------------------------------------------------------
    # 1. Experiment (i): p-Persistent Analysis
    # -------------------------------------------------------------
    if os.path.exists(p_csv):
        p_vals = []
        col_pts = []
        delay_pts = []
        tp_pts = []
        eff_pts = []
        util_pts = []

        with open(p_csv, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                p = float(row["P"])
                p_vals.append(p)
                col_pts.append((p, float(row["Collisions"])))
                delay_pts.append((p, float(row["AvgDelayMs"])))
                tp_pts.append((p, float(row["ThroughputKbps"])))
                eff_pts.append((p, float(row["TxEfficiencyPct"])))
                util_pts.append((p, float(row["ChannelUtilizationPct"])))

        render_svg_multiline(
            "p-Persistent CSMA: Collision Count vs Probability p (N=5)",
            "Probability p",
            "Number of Collisions",
            {"p-Persistent CSMA (N=5)": col_pts},
            os.path.join(RESULTS_DIR, "exp1_p_persistent_collisions.svg")
        )

        render_svg_multiline(
            "p-Persistent CSMA: Average Delay vs Probability p (N=5)",
            "Probability p",
            "Average Transmission Delay (ms)",
            {"p-Persistent CSMA (N=5)": delay_pts},
            os.path.join(RESULTS_DIR, "exp1_p_persistent_delay.svg")
        )

        render_svg_multiline(
            "p-Persistent CSMA: Throughput vs Probability p (N=5)",
            "Probability p",
            "Effective Channel Throughput (kbps)",
            {"p-Persistent CSMA (N=5)": tp_pts},
            os.path.join(RESULTS_DIR, "exp1_p_persistent_throughput.svg")
        )

        render_svg_multiline(
            "p-Persistent CSMA: Protocol Tx Efficiency vs Probability p (N=5)",
            "Probability p",
            "Protocol Transmission Efficiency (%)",
            {"p-Persistent CSMA (N=5)": eff_pts},
            os.path.join(RESULTS_DIR, "exp1_p_persistent_efficiency.svg")
        )

    # -------------------------------------------------------------
    # 2. Experiment (ii): Comparative Analysis of All CSMA Schemes vs N
    # -------------------------------------------------------------
    if os.path.exists(cmp_csv):
        col_series = {}
        delay_series = {}
        tp_series = {}
        eff_series = {}
        util_series = {}

        with open(cmp_csv, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                name = row["ProtocolName"]
                n = float(row["Stations"])
                col = float(row["Collisions"])
                delay = float(row["AvgDelayMs"])
                tp = float(row["ThroughputKbps"])
                eff = float(row["TxEfficiencyPct"])
                util = float(row["ChannelUtilizationPct"])

                if name not in col_series:
                    col_series[name] = []
                    delay_series[name] = []
                    tp_series[name] = []
                    eff_series[name] = []
                    util_series[name] = []

                col_series[name].append((n, col))
                delay_series[name].append((n, delay))
                tp_series[name].append((n, tp))
                eff_series[name].append((n, eff))
                util_series[name].append((n, util))

        render_svg_multiline(
            "CSMA Schemes: Collision Count vs Contending Stations (N)",
            "Number of Contending Stations (N)",
            "Total Collision Events",
            col_series,
            os.path.join(RESULTS_DIR, "exp2_csma_comparison_collisions.svg")
        )

        render_svg_multiline(
            "CSMA Schemes: Average Transmission Delay vs Contending Stations (N)",
            "Number of Contending Stations (N)",
            "Average Transmission Delay (ms)",
            delay_series,
            os.path.join(RESULTS_DIR, "exp2_csma_comparison_delay.svg")
        )

        render_svg_multiline(
            "CSMA Schemes: Channel Throughput vs Contending Stations (N)",
            "Number of Contending Stations (N)",
            "Channel Throughput (kbps)",
            tp_series,
            os.path.join(RESULTS_DIR, "exp2_csma_comparison_throughput.svg")
        )

        render_svg_multiline(
            "CSMA Schemes: Protocol Tx Efficiency vs Contending Stations (N)",
            "Number of Contending Stations (N)",
            "Protocol Transmission Efficiency (%)",
            eff_series,
            os.path.join(RESULTS_DIR, "exp2_csma_comparison_efficiency.svg")
        )

        render_svg_multiline(
            "CSMA Schemes: Channel Air Utilization vs Contending Stations (N)",
            "Number of Contending Stations (N)",
            "Channel Air Utilization (%)",
            util_series,
            os.path.join(RESULTS_DIR, "exp2_csma_comparison_utilization.svg")
        )

if __name__ == "__main__":
    generate_all_plots()
