#!/usr/bin/env python3
"""
Benchmark analysis script for EfficientLimitOrderBook.
Computes statistics and performance summaries from benchmark CSV.
"""

import pandas as pd
import argparse
from pathlib import Path
from tabulate import tabulate


def load_benchmark_csv(csv_path):
    """Load benchmark CSV file."""
    df = pd.read_csv(csv_path)
    return df


def analyze_by_scenario(df):
    """Analyze benchmark results grouped by scenario."""
    print("\n" + "="*80)
    print("PERFORMANCE ANALYSIS BY SCENARIO")
    print("="*80)
    
    scenarios = sorted(df['scenario'].unique())
    
    for scenario in scenarios:
        scenario_data = df[df['scenario'] == scenario]
        
        print(f"\n{scenario.upper()}:")
        print("-" * 40)
        
        # Create summary table
        summary_rows = []
        for _, row in scenario_data.sort_values('N').iterrows():
            summary_rows.append([
                int(row['N']),
                f"{row['seconds']:.6f}",
                f"{row['throughput_ops_s']:.0f}",
                int(row['trades']),
                f"{row['trades'] / row['orders'] * 100:.1f}%"
            ])
        
        headers = ['Orders', 'Time (s)', 'Throughput (ops/s)', 'Trades', 'Trade Ratio']
        print(tabulate(summary_rows, headers=headers, tablefmt='grid'))
        
        # Statistics
        throughputs = scenario_data['throughput_ops_s'].values
        print(f"\nThroughput stats: min={throughputs.min():.0f}, max={throughputs.max():.0f}, "
              f"mean={throughputs.mean():.0f}, std={throughputs.std():.0f}")


def analyze_by_ordercount(df):
    """Analyze benchmark results grouped by order count."""
    print("\n" + "="*80)
    print("PERFORMANCE ANALYSIS BY ORDER COUNT")
    print("="*80)
    
    ns = sorted(df['N'].unique())
    
    for n in ns:
        n_data = df[df['N'] == n]
        
        print(f"\nN={n}:")
        print("-" * 40)
        
        # Create summary table
        summary_rows = []
        for _, row in n_data.sort_values('scenario').iterrows():
            summary_rows.append([
                row['scenario'],
                f"{row['seconds']:.6f}",
                f"{row['throughput_ops_s']:.0f}",
                int(row['trades']),
                f"{row['trades'] / row['orders'] * 100:.1f}%"
            ])
        
        headers = ['Scenario', 'Time (s)', 'Throughput (ops/s)', 'Trades', 'Trade Ratio']
        print(tabulate(summary_rows, headers=headers, tablefmt='grid'))
        
        # Find best performing scenario for this N
        best_idx = n_data['throughput_ops_s'].idxmax()
        best_scenario = n_data.loc[best_idx, 'scenario']
        best_throughput = n_data.loc[best_idx, 'throughput_ops_s']
        print(f"\n✓ Best scenario for N={n}: {best_scenario} ({best_throughput:.0f} ops/s)")


def analyze_scalability(df):
    """Analyze how throughput scales with order count."""
    print("\n" + "="*80)
    print("SCALABILITY ANALYSIS")
    print("="*80)
    
    scenarios = sorted(df['scenario'].unique())
    
    for scenario in scenarios:
        scenario_data = df[df['scenario'] == scenario].sort_values('N')
        
        ns = scenario_data['N'].values
        throughputs = scenario_data['throughput_ops_s'].values
        
        # Calculate scaling efficiency (throughput / N)
        efficiencies = throughputs / ns
        
        print(f"\n{scenario.upper()}:")
        print("-" * 40)
        
        summary_rows = []
        for n, tp, eff in zip(ns, throughputs, efficiencies):
            summary_rows.append([
                int(n),
                f"{tp:.0f}",
                f"{eff:.3f}"
            ])
        
        headers = ['Orders', 'Throughput (ops/s)', 'Efficiency (ops/order)']
        print(tabulate(summary_rows, headers=headers, tablefmt='grid'))
        
        # Scaling factor
        scaling = throughputs[-1] / throughputs[0]
        print(f"\nScaling factor (50K/1K): {scaling:.2f}x")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze EfficientLimitOrderBook benchmark results'
    )
    parser.add_argument(
        'csv_file',
        nargs='?',
        default='bench_results.csv',
        help='Path to benchmark CSV file (default: bench_results.csv)'
    )
    
    args = parser.parse_args()
    
    # Load CSV
    csv_path = Path(args.csv_file)
    if not csv_path.exists():
        print(f"Error: {csv_path} not found")
        exit(1)
    
    df = load_benchmark_csv(csv_path)
    print(f"Loaded {len(df)} benchmark rows from {csv_path}\n")
    
    # Perform analyses
    analyze_by_scenario(df)
    analyze_by_ordercount(df)
    analyze_scalability(df)
    
    print("\n" + "="*80)


if __name__ == '__main__':
    main()
