import os
import csv
import xml.etree.ElementTree as ET

# --- Configuration ---
XML_REPORTS_DIR = os.path.join("..", "xml_reports")
OUTPUT_CSV_FILE = os.path.join("..", "dse_synthesis_results_vivado.csv")

def parse_solution_name(filename):
    """Extracts DSE parameters from the solution filename."""
    params = {}
    # Handles both .xml and _csynth.xml suffixes
    clean_name = filename.replace('_csynth.xml', '').replace('.xml', '')
    parts = clean_name.replace('sol_', '').split('_')
    try:
        params['SolutionName'] = clean_name
        params['Pipeline'] = parts[1]
        params['UnrollFactor'] = parts[3]
        params['Flatten'] = parts[5]
        params['PartitionType'] = parts[7]
    except IndexError:
        print(f"Warning: Could not fully parse filename '{filename}'")
        params['SolutionName'] = clean_name
    return params

def parse_xml_report(xml_file):
    """
    Parses a single csynth.xml file from Vivado HLS 2019.2 and extracts key metrics.
    This version is tailored to the simpler XML structure without namespaces.
    """
    try:
        tree = ET.parse(xml_file)
        root = tree.getroot()
    except ET.ParseError:
        print(f"ERROR: Could not parse XML file: {xml_file}. It might be empty or corrupt.")
        return None

    def find_text(path, default='N/A'):
        # Helper to find text safely in a namespace-free XML
        element = root.find(path)
        return element.text if element is not None else default

    data = {}
    
    # --- Performance Metrics ---
    # Note the direct paths without any 'hls:' prefix
    data['TargetClock_ns'] = find_text('./UserAssignments/TargetClockPeriod')
    data['EstimatedClock_ns'] = find_text('./PerformanceEstimates/SummaryOfTimingAnalysis/EstimatedClockPeriod')
    
    # Vivado HLS 2019.2 does not provide Worst Negative Slack directly in the XML summary
    # It can only be inferred from Target vs Estimated clock.
    
    data['BestLatency_cycles'] = find_text('./PerformanceEstimates/SummaryOfOverallLatency/Best-caseLatency')
    data['AvgLatency_cycles'] = find_text('./PerformanceEstimates/SummaryOfOverallLatency/Average-caseLatency')
    data['WorstLatency_cycles'] = find_text('./PerformanceEstimates/SummaryOfOverallLatency/Worst-caseLatency')
    data['Interval_min_cycles'] = find_text('./PerformanceEstimates/SummaryOfOverallLatency/Interval-min')
    data['Interval_max_cycles'] = find_text('./PerformanceEstimates/SummaryOfOverallLatency/Interval-max')
    
    # --- Area / Resource Usage ---
    # This logic is completely rewritten to match the Vivado HLS 2019.2 format
    resources_used_root = root.find('./AreaEstimates/Resources')
    if resources_used_root is not None:
        for resource in resources_used_root:
            data[f'{resource.tag}_Used'] = resource.text

    resources_avail_root = root.find('./AreaEstimates/AvailableResources')
    if resources_avail_root is not None:
        for resource in resources_avail_root:
            data[f'{resource.tag}_Available'] = resource.text

    return data

def main():
    """Main function to find reports, parse them, and write to CSV."""
    if not os.path.isdir(XML_REPORTS_DIR):
        print(f"ERROR: Directory '{XML_REPORTS_DIR}' not found.")
        print("Please run the 'extract_reports_vivado.tcl' script first.")
        return

    all_results = []
    print(f"INFO: Parsing XML files from '{XML_REPORTS_DIR}'...")

    for filename in sorted(os.listdir(XML_REPORTS_DIR)):
        if filename.endswith(".xml"):
            print(f"  -> Processing {filename}")
            report_data = parse_xml_report(os.path.join(XML_REPORTS_DIR, filename))
            
            if report_data: # Only process if the XML was parsed successfully
                solution_params = parse_solution_name(filename)
                full_result = {**solution_params, **report_data}
                all_results.append(full_result)

    if not all_results:
        print("WARNING: No valid XML reports were found to parse.")
        return

    # --- Write to CSV ---
    print(f"INFO: Writing {len(all_results)} results to '{OUTPUT_CSV_FILE}'...")
    
    headers = set()
    for result in all_results:
        headers.update(result.keys())
    
    preferred_order = [
        'SolutionName', 'Pipeline', 'UnrollFactor', 'Flatten', 'PartitionType',
        'EstimatedClock_ns', 'TargetClock_ns',
        'Interval_min_cycles', 'Interval_max_cycles', 'BestLatency_cycles', 'WorstLatency_cycles', 'AvgLatency_cycles',
        'BRAM_18K_Used', 'BRAM_18K_Available', 'DSP48E_Used', 'DSP48E_Available',
        'FF_Used', 'FF_Available', 'LUT_Used', 'LUT_Available', 'URAM_Used', 'URAM_Available'
    ]
    
    final_headers = [h for h in preferred_order if h in headers]
    final_headers += sorted([h for h in headers if h not in preferred_order])

    with open(OUTPUT_CSV_FILE, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=final_headers)
        writer.writeheader()
        writer.writerows(all_results)

    print(f"INFO: Successfully created CSV report: '{OUTPUT_CSV_FILE}'")

if __name__ == '__main__':
    main()