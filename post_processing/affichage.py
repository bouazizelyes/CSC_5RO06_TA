import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import plotly.express as px
import os

# --- Configuration ---
CSV_FILE = os.path.join("..", "dse_synthesis_results_vivado.csv")
OUTPUT_DIR = os.path.join("..", "dse_plots_advanced")

# --- NEW: Choose which type of plots to generate ---
GENERATE_STATIC_PLOTS = True   # Generate PNG images using Seaborn
GENERATE_INTERACTIVE_PLOTS = True # Generate interactive HTML files using Plotly

# --- NEW: Performance metric is now calculated in milliseconds ---
# We will create this column from latency in cycles and the estimated clock period.

RESOURCE_COLUMNS = [
    'BRAM_18K_Used',
    'DSP4E_Used', # Note: Name may vary slightly based on tool version (e.g., DSP48E)
    'FF_Used',
    'LUT_Used'
]

def analyze_and_plot_advanced():
    """
    Loads DSE results, calculates time-based latency, and generates
    both static (faceted) and interactive plots for clearer analysis.
    """
    # --- 1. Load and Validate Data ---
    if not os.path.exists(CSV_FILE):
        print(f"ERROR: CSV file not found: '{CSV_FILE}'")
        return

    print(f"INFO: Loading data from '{CSV_FILE}'...")
    df = pd.read_csv(CSV_FILE)

    # --- 2. Clean and Prepare Data ---
    
    # Check for DSP column name variations
    if 'DSP48E_Used' in df.columns:
        RESOURCE_COLUMNS[1] = 'DSP48E_Used'
        
    cols_to_convert = [
        'WorstLatency_cycles', 'EstimatedClock_ns', 'UnrollFactor'
    ] + RESOURCE_COLUMNS
    
    for col in cols_to_convert:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')
        else:
            print(f"WARNING: Column '{col}' not found. It will be skipped.")
            if col in RESOURCE_COLUMNS:
                RESOURCE_COLUMNS.remove(col)

    original_rows = len(df)
    df.dropna(subset=cols_to_convert, inplace=True)
    cleaned_rows = len(df)
    print(f"INFO: Data cleaned. Kept {cleaned_rows} of {original_rows} valid solutions.")

    if df.empty:
        print("ERROR: No valid data to plot.")
        return

    # --- 3. Feature Engineering: Calculate Latency in Milliseconds ---
    # Latency (ms) = WorstLatency_cycles * EstimatedClock_ns * 1e-6
    df['WorstLatency_ms'] = df['WorstLatency_cycles'] * df['EstimatedClock_ns'] * 1e-6
    print("INFO: Calculated 'WorstLatency_ms' for true time-based comparison.")

    # --- 4. Create Plots ---
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"INFO: Generating plots in '{OUTPUT_DIR}/'...")

    # A. Generate Static Faceted Plots (for reports, papers)
    if GENERATE_STATIC_PLOTS:
        generate_faceted_static_plots(df)

    # B. Generate Interactive Plots (for exploration)
    if GENERATE_INTERACTIVE_PLOTS:
        generate_interactive_plots(df)

    print("\nINFO: Plot generation complete!")

def generate_faceted_static_plots(df):
    """Generates clear, faceted PNG plots using Seaborn."""
    print("INFO: Generating static (PNG) faceted plots...")
    
    for resource in RESOURCE_COLUMNS:
        print(f"  -> Static plot for {resource}")
        
        # Use relplot to create a figure-level faceted plot
        g = sns.relplot(
            data=df,
            x=resource,
            y='WorstLatency_ms',
            style='PartitionType', # Marker style
            size='UnrollFactor',   # Marker size
            hue='Flatten',         # Use color for Flatten to add another dimension
            sizes=(50, 400),
            alpha=0.7,
            palette='muted',
            col='Pipeline',        # <<< THE KEY FIX: Create columns for Pipeline on/off
            height=6, aspect=1.2   # Adjust figure size and aspect ratio
        )

        # --- Customize the plot for clarity ---
        g.fig.suptitle(f'Performance vs. {resource} Usage', fontsize=16, y=1.03)
        g.set_axis_labels(f'{resource} Count', 'Worst Latency (ms)')
        g.set_titles("Pipeline: {col_name}") # Set titles for each subplot
        
        # Use a log scale to better visualize the Pareto front
        g.set(xscale="log", yscale="log")
        g.tight_layout(rect=[0, 0, 1, 0.96]) # Adjust layout to make space for title

        # --- Save the plot ---
        output_filename = os.path.join(OUTPUT_DIR, f'static_faceted_vs_{resource}.png')
        g.savefig(output_filename)
        plt.close()

def generate_interactive_plots(df):
    """Generates interactive HTML plots using Plotly."""
    print("INFO: Generating interactive (HTML) plots...")

    for resource in RESOURCE_COLUMNS:
        print(f"  -> Interactive plot for {resource}")

        fig = px.scatter(
            df,
            x=resource,
            y='WorstLatency_ms',
            color='Pipeline',      # Color by Pipeline setting
            symbol='PartitionType',# Use different symbols
            size='UnrollFactor',   # Vary point size
            log_x=True,            # Use log scale on axes
            log_y=True,
            title=f'Interactive: Performance vs. {resource} Usage',
            labels={
                resource: f'{resource} Count (log scale)',
                'WorstLatency_ms': 'Worst Latency in ms (log scale)'
            },
            # This makes the pop-up info box incredibly useful
            hover_data=['SolutionName', 'EstimatedClock_ns']
        )
        
        # --- Save the plot as an HTML file ---
        output_filename = os.path.join(OUTPUT_DIR, f'interactive_vs_{resource}.html')
        fig.write_html(output_filename)

if __name__ == '__main__':
    analyze_and_plot_advanced()