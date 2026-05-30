import sys
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) > 1:
        file_path = Path(sys.argv[1])
    else:
        print("Timings file is not specified in arguments")
        return
    
    try:
        df = pd.read_csv(file_path, sep=',')
    except FileNotFoundError:
        print(format(f"File '{file_path}' not found"))
        return

    plt.figure(figsize=(10, 6))

    plt.plot(df['time_point'], df['request_time_ms'],
             label='Frame request time, ms', 
             color='#1f77b4',
             linewidth=0, 
             marker='o', # Dots
             markersize=4)

    plt.plot(df['time_point'], df['send_time_ms'], 
             label='Frame send time, ms', 
             color='#ff7f0e',
             linewidth=0, 
             marker='s', # Squares
             markersize=4)

    plt.title(f'{file_path.stem} session timings', fontsize=14, fontweight='bold', pad=15)
    plt.xlabel('Time point (s)', fontsize=12)
    plt.ylabel('Execution time (ms)', fontsize=12)
    
    plt.grid(True, linestyle='--', alpha=0.6)

    plt.legend(fontsize=11, loc='upper right')

    plt.tight_layout()

    plt.show()

if __name__ == "__main__":
    main()