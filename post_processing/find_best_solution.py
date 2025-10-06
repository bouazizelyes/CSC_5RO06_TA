import pandas as pd
import os

# --- Configuration ---
CSV_FILE = os.path.join("..", "dse_synthesis_results_vivado.csv")

def find_best_solutions():
    """
    Loads DSE results, calculates time-based latency, and identifies the
    solution(s) with the best performance (lowest latency).
    """
    # --- 1. Load and Validate Data ---
    if not os.path.exists(CSV_FILE):
        print(f"ERROR: CSV file not found at '{CSV_FILE}'")
        return

    print(f"INFO: Loading data from '{CSV_FILE}'...")
    df = pd.read_csv(CSV_FILE)

    # --- 2. Clean and Prepare Data ---
    cols_to_convert = ['WorstLatency_cycles', 'EstimatedClock_ns', 'LUT_Used', 'FF_Used', 'DSP48E_Used', 'BRAM_18K_Used']
    
    # Handle variations in DSP column name
    if 'DSP4E_Used' in df.columns and 'DSP48E_Used' not in cols_to_convert:
        cols_to_convert.append('DSP4E_Used')
        
    for col in cols_to_convert:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')

    df.dropna(subset=cols_to_convert, inplace=True)

    if df.empty:
        print("ERROR: No valid data found after cleaning. Cannot determine the best solution.")
        return

    # --- 3. Calculate Latency in Milliseconds ---
    df['WorstLatency_ms'] = df['WorstLatency_cycles'] * df['EstimatedClock_ns'] * 1e-6
    
    # Also calculate total resource usage for tie-breaking
    df['Total_LUTs_FFs'] = df['LUT_Used'] + df['FF_Used']

    # --- 4. Find the Best Performance ---
    min_latency = df['WorstLatency_ms'].min()

    print(f"\n--- Analysis Complete ---")
    print(f"The best execution time achieved was: {min_latency:.4f} ms")

    # --- 5. Filter for All Solutions Achieving This Performance ---
    best_solutions_df = df[df['WorstLatency_ms'] == min_latency]
    
    # Sort the best solutions by resource usage (least first) to find the most efficient
    best_solutions_df = best_solutions_df.sort_values(by='Total_LUTs_FFs', ascending=True)

    print("\nThe following solution(s) achieved this best time:")
    print("=" * 80)

    # --- 6. Display the Results in a Readable Format ---
    # Define the columns to display for a clean output
    display_columns = [
        'SolutionName',
        'WorstLatency_ms',
        'EstimatedClock_ns',
        'LUT_Used',
        'FF_Used',
        'DSP48E_Used' if 'DSP48E_Used' in df.columns else 'DSP4E_Used',
        'BRAM_18K_Used'
    ]
    
    # Ensure all display columns exist in the dataframe
    display_columns = [col for col in display_columns if col in best_solutions_df.columns]

    print(best_solutions_df[display_columns].to_string(index=False))
    print("=" * 80)
    
    if len(best_solutions_df) > 1:
        print("\nRecommendation: All solutions above are equally fast. The one at the top of the list")
        print("is the most resource-efficient (uses the fewest LUTs and FFs combined).")
    else:
        print("\nThis is the single best solution in terms of performance.")


if __name__ == '__main__':
    find_best_solutions()