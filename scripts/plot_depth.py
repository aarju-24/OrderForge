#!/usr/bin/env python3
"""
OrderForge - Market Depth & Spread Visualizer
Reads output/depth_snapshots.csv and visualizes:
1. Best Bid, Best Ask, and Bid-Ask Spread over event sequence.
2. Total Resting Bid Volume vs Total Resting Ask Volume (Market Liquidity).
"""

import os
import sys
import pandas as pd
import matplotlib.pyplot as plt

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("output", "depth_snapshots.csv")
    output_png = sys.argv[2] if len(sys.argv) > 2 else os.path.join("output", "depth_and_spread.png")

    if not os.path.isfile(csv_path):
        print(f"[Error] File not found: {csv_path}")
        sys.exit(1)

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"[Error] Failed to read CSV: {e}")
        sys.exit(1)

    required_cols = {"sequence_num", "best_bid_price", "best_ask_price", "spread", "total_bid_volume", "total_ask_volume"}
    if not required_cols.issubset(df.columns):
        print(f"[Error] CSV missing required columns. Found: {list(df.columns)}")
        sys.exit(1)

    if df.empty:
        print("[Warning] Dataset is empty. Nothing to plot.")
        sys.exit(0)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True, dpi=150)
    plt.subplots_adjust(hspace=0.25)

    # Subplot 1: Best Bid, Best Ask, and Spread
    ax1.plot(df["sequence_num"], df["best_bid_price"], label="Best Bid Price", color="#1b9e77", marker="o", markersize=4, linestyle="-", linewidth=1.5)
    ax1.plot(df["sequence_num"], df["best_ask_price"], label="Best Ask Price", color="#d95f02", marker="s", markersize=4, linestyle="-", linewidth=1.5)
    ax1.set_ylabel("Price (ticks)", fontsize=11, fontweight="bold")
    ax1.set_title("OrderForge: Price Discovery & Top-of-Book Evolution", fontsize=13, fontweight="bold")
    ax1.grid(True, linestyle="--", alpha=0.6)
    ax1.legend(loc="upper left")

    # Secondary y-axis for spread
    ax1_twin = ax1.twinx()
    ax1_twin.plot(df["sequence_num"], df["spread"], label="Spread (Ask - Bid)", color="#7570b3", linestyle=":", linewidth=1.8)
    ax1_twin.set_ylabel("Spread (ticks)", color="#7570b3", fontsize=11, fontweight="bold")
    ax1_twin.tick_params(axis="y", labelcolor="#7570b3")
    ax1_twin.legend(loc="upper right")

    # Subplot 2: Total Resting Liquidity Depth
    ax2.fill_between(df["sequence_num"], 0, df["total_bid_volume"], label="Total Bid Depth", color="#1b9e77", alpha=0.35)
    ax2.fill_between(df["sequence_num"], 0, df["total_ask_volume"], label="Total Ask Depth", color="#d95f02", alpha=0.35)
    ax2.plot(df["sequence_num"], df["total_bid_volume"], color="#1b9e77", linewidth=1.5)
    ax2.plot(df["sequence_num"], df["total_ask_volume"], color="#d95f02", linewidth=1.5)
    ax2.set_xlabel("Event Sequence Number", fontsize=11, fontweight="bold")
    ax2.set_ylabel("Total Resting Shares", fontsize=11, fontweight="bold")
    ax2.set_title("Aggregated Level-2 Book Depth (Total Resting Liquidity)", fontsize=12, fontweight="bold")
    ax2.grid(True, linestyle="--", alpha=0.6)
    ax2.legend(loc="upper left")

    os.makedirs(os.path.dirname(os.path.abspath(output_png)), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_png)
    plt.close()
    print(f"[Success] Saved depth and spread chart to: {output_png}")

if __name__ == "__main__":
    main()
