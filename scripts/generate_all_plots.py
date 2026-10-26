#!/usr/bin/env python3
"""
OrderForge - Run all analysis and visualization scripts
"""

import subprocess
import sys
import os

def run_script(script_name):
    script_path = os.path.join(os.path.dirname(__file__), script_name)
    print(f"\n--- Running {script_name} ---")
    ret = subprocess.run([sys.executable, script_path])
    if ret.returncode != 0:
        print(f"[Warning] {script_name} returned code {ret.returncode}")
    else:
        print(f"[Done] {script_name} completed successfully.")

def main():
    print("==================================================")
    print("      OrderForge Python Visualization Suite       ")
    print("==================================================")
    run_script("plot_depth.py")
    run_script("plot_trades.py")
    run_script("plot_benchmarks.py")
    print("\nAll visualizations generated. Check the output/ directory for PNG artifacts.")

if __name__ == "__main__":
    main()
