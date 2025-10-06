# ============================================================================
# Tcl Script to Export Synthesis Reports from a Vivado HLS DSE Project
#
# VERSION-COMPATIBLE with Vivado HLS 2019.2 and older.
# This script opens a project, finds all solutions by searching for their
# directories, and copies the XML reports to a common folder.
# ============================================================================

# --- Configuration ---
set HLS_PROJECT_NAME "hcd_full_dse_project"
set OUTPUT_DIR_NAME "xml_reports"

# --- Main Logic ---

# Get the directory where this script is located
set SCRIPT_DIR [pwd] ;# Use pwd as a robust way to get current working directory

# Define the full path to the HLS project and the output directory
set PROJECT_PATH [file join $SCRIPT_DIR $HLS_PROJECT_NAME]
set OUTPUT_PATH [file join $SCRIPT_DIR $OUTPUT_DIR_NAME]

# Check if the project exists
if {![file isdirectory $PROJECT_PATH]} {
    puts "ERROR: Project directory not found at '$PROJECT_PATH'"
    puts "ERROR: Please run this script from the same directory as your project."
    return -code error
}

# --- FIX 1: Open project with just its name (relative path) ---
puts "INFO: Opening project '$HLS_PROJECT_NAME'..."
open_project $HLS_PROJECT_NAME

# Create the output directory if it doesn't exist
if {![file isdirectory $OUTPUT_PATH]} {
    puts "INFO: Creating output directory '$OUTPUT_PATH'..."
    file mkdir $OUTPUT_PATH
}

puts "INFO: Exporting reports for all solutions..."

# --- FIX 2: Manually find solutions using 'glob' instead of 'list_solutions' ---
set solutions {}
set solution_paths [glob -nocomplain -directory $PROJECT_PATH "sol_*"]

foreach path $solution_paths {
    if {[file isdirectory $path]} {
        # Extract just the directory name (e.g., "sol_pipe_on_unroll_4...")
        lappend solutions [file tail $path]
    }
}

if {[llength $solutions] == 0} {
    puts "WARNING: No solutions found in project '$HLS_PROJECT_NAME'."
}

# --- Continue with the rest of the script, which is compatible ---
foreach sol_name $solutions {
    set report_xml_path [file join $PROJECT_PATH $sol_name "syn/report/csynth.xml"]

    if {[file exists $report_xml_path]} {
        set dest_file_path [file join $OUTPUT_PATH "${sol_name}_csynth.xml"]
        puts "  -> Exporting report for solution: $sol_name"
        file copy -force $report_xml_path $dest_file_path
    } else {
        puts "WARNING: Could not find report for solution: $sol_name. Skipping."
    }
}

puts "INFO: Report export complete."
close_project
exit