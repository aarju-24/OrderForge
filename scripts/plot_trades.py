#!/usr/bin/env python3
"""
OrderForge - Trade Execution & Price Distribution Visualizer
Reads output/trades.csv and visualizes:
1. Trade execution prices over time/trade sequence.
2. Trade size distribution and volume profile by price.
"""

import os
import sys
import pandas as pd
import matplotlib.pyplot as plt

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("output", "trades.csv")
    output_png = sys.argv[2] if len(sys.argv) > 2 else os.path.join("output", "trade_executions.png")

    if not os.path.isfile(csv_path):
        print(f"[Error] File not found: {csv_path}")
        sys.exit(1)

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"[Error] Failed to read CSV: {e}")
        sys.exit(1)

    required_cols = {"trade_id", "execution_price", "executed_quantity"}
    # Note: price column might be named 'price' or 'execution_price'
    if "price" in df.columns:
        df["execution_price"] = df["price"]
    if "quantity" in df.columns:
        df["executed_quantity"] = df["quantity"]

    if not {"trade_id", "execution_price", "executed_quantity"}.issubset(df.columns):
        print(f"[Error] CSV missing required columns. Found: {list(df.columns)}")
        sys.exit(1)

    if df.empty:
        print("[Warning] No trades in dataset. Nothing to plot.")
        sys.exit(0)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5), dpi=150)

    # Subplot 1: Trade Executions over Sequence
    ax1.plot(df["trade_id"], df["execution_price"], color="#2c7fb8", marker="o", linestyle="-", linewidth=1.5, alpha=0.8)
    ax1.set_xlabel("Trade ID", fontsize=11, fontweight="bold")
    ax1.set_ylabel("Execution Price (ticks)", fontsize=11, fontweight="bold")
    ax1.set_title("Executed Trades Price Path", fontsize=12, fontweight="bold")
    ax1.grid(True, linestyle="--", alpha=0.6)

    # Subplot 2: Volume Profile (Total executed quantity by price level)
    vol_by_price = df.groupby("execution_price")["executed_quantity"].sum().reset_index()
    ax2.barh(vol_by_price["execution_price"].astype(str), vol_by_price["executed_quantity"], color="#41b6c4", edgecolor="#225ea8")
    ax2.set_xlabel("Total Executed Volume (shares)", fontsize=11, fontweight="bold")
    ax2.set_ylabel("Price Level (ticks)", fontsize=11, fontweight="bold")
    ax2.set_title("Volume Profile by Price", fontsize=12, fontweight="bold")
    ax2.grid(True, linestyle="--", alpha=0.6, axis="x")

    os.makedirs(os.path.dirname(os.path.abspath(output_png)), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_png)
    plt.close()
    print(f"[Success] Saved trade execution chart to: {output_png}")

if __name__ == "__main__":
    main()
