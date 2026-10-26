#!/usr/bin/env python3
"""
OrderForge - Performance Benchmark Analysis Visualizer
Reads output/benchmark_results.csv and visualizes:
1. Engine Throughput (events/second) across workloads.
2. Latency percentiles (p50, p95, p99) comparison.
3. Scaling performance across input sizes.
"""

import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("output", "benchmark_results.csv")
    output_png = sys.argv[2] if len(sys.argv) > 2 else os.path.join("output", "benchmark_analysis.png")

    if not os.path.isfile(csv_path):
        print(f"[Error] File not found: {csv_path}")
        sys.exit(1)

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"[Error] Failed to read CSV: {e}")
        sys.exit(1)

    required_cols = {"workload", "event_count", "throughput_ops_sec", "p50_lat_us", "p95_lat_us", "p99_lat_us"}
    if not required_cols.issubset(df.columns):
        print(f"[Error] CSV missing required columns. Found: {list(df.columns)}")
        sys.exit(1)

    # Separate standard 100k workloads from scaling workloads
    std_df = df[~df["workload"].str.startswith("Mixed-Scale")].copy()
    scale_df = df[df["workload"].isin(["Mixed-Scale-10K", "Mixed-Scale-50K", "Mixed-Realistic-100K"])].copy()

    fig = plt.figure(figsize=(14, 9), dpi=150)
    gs = fig.add_gridspec(2, 2, hspace=0.35, wspace=0.25)

    ax1 = fig.add_subplot(gs[0, :])
    ax2 = fig.add_subplot(gs[1, 0])
    ax3 = fig.add_subplot(gs[1, 1])

    # 1. Throughput by Workload (Top Plot)
    throughput_m = std_df["throughput_ops_sec"] / 1e6
    colors = ["#2b5c8f", "#3690c0", "#67a9cf", "#02818a", "#67a9cf", "#bdc9e1", "#016450"]
    bars = ax1.bar(std_df["workload"], throughput_m, color=colors[:len(std_df)], edgecolor="#014636", alpha=0.85)
    ax1.set_ylabel("Throughput (Million ops/sec)", fontsize=11, fontweight="bold")
    ax1.set_title("OrderForge Engine Throughput Across Microstructure Workloads", fontsize=13, fontweight="bold")
    ax1.grid(True, linestyle="--", alpha=0.5, axis="y")
    ax1.set_ylim(0, max(throughput_m) * 1.2)

    for bar in bars:
        height = bar.get_height()
        ax1.annotate(f"{height:.2f}M",
                     xy=(bar.get_x() + bar.get_width() / 2, height),
                     xytext=(0, 4), textcoords="offset points",
                     ha="center", va="bottom", fontsize=10, fontweight="bold")

    # 2. Latency Percentiles (Bottom Left)
    x = np.arange(len(std_df))
    width = 0.25

    ax2.bar(x - width, std_df["p50_lat_us"], width, label="p50 (Median)", color="#74a9cf", edgecolor="#02818a")
    ax2.bar(x, std_df["p95_lat_us"], width, label="p95", color="#3690c0", edgecolor="#016c59")
    ax2.bar(x + width, std_df["p99_lat_us"], width, label="p99 (Tail)", color="#02818a", edgecolor="#014636")

    ax2.set_xticks(x)
    ax2.set_xticklabels(std_df["workload"], rotation=30, ha="right", fontsize=9)
    ax2.set_ylabel("Latency (microseconds / event)", fontsize=10, fontweight="bold")
    ax2.set_title("Latency Percentile Distribution (p50, p95, p99)", fontsize=11, fontweight="bold")
    ax2.grid(True, linestyle="--", alpha=0.5, axis="y")
    ax2.legend(loc="upper left")

    # 3. Scaling across Workload Sizes (Bottom Right)
    if not scale_df.empty:
        scale_df = scale_df.sort_values(by="event_count")
        ax3.plot(scale_df["event_count"] / 1000, scale_df["throughput_ops_sec"] / 1e6, marker="o", linewidth=2, color="#d95f02", label="Throughput (M ops/s)")
        ax3.set_xlabel("Event Count (Thousands)", fontsize=10, fontweight="bold")
        ax3.set_ylabel("Throughput (Million ops/sec)", color="#d95f02", fontsize=10, fontweight="bold")
        ax3.tick_params(axis="y", labelcolor="#d95f02")
        ax3.set_title("Throughput Scaling with Event Count", fontsize=11, fontweight="bold")
        ax3.grid(True, linestyle="--", alpha=0.5)

        ax3_twin = ax3.twinx()
        ax3_twin.plot(scale_df["event_count"] / 1000, scale_df["p99_lat_us"], marker="s", linestyle="--", linewidth=2, color="#7570b3", label="p99 Latency (us)")
        ax3_twin.set_ylabel("p99 Latency (us)", color="#7570b3", fontsize=10, fontweight="bold")
        ax3_twin.tick_params(axis="y", labelcolor="#7570b3")

    os.makedirs(os.path.dirname(os.path.abspath(output_png)), exist_ok=True)
    plt.savefig(output_png)
    plt.close()
    print(f"[Success] Saved benchmark analysis chart to: {output_png}")

if __name__ == "__main__":
    main()
