# ============================================================================
# Vitis HLS Tcl Script for Full Design Space Exploration (DSE)
#
# Explores Pipelining, Unrolling, Flattening, and Partitioning combinations.
# ============================================================================

# Step 1: Project Setup
# ----------------------------------------------------------------------------
puts "INFO: Setting up HLS project..."
open_project -reset hcd_full_dse_project
set_top HLS_accel
add_files HCD_accel.cpp
add_files -tb HCD_test.cpp

# Define the target part. CHANGE THIS to match your FPGA device.


# Step 2: Define the Design Space Knobs (See explanation above)
# ----------------------------------------------------------------------------
set PIPELINE_OPTIONS {on off}
set UNROLL_FACTORS   {1 2 4 8 9 16 24 32} ;# 1 means no unroll directive is applied
set FLATTEN_OPTIONS  {on off}
set PARTITION_TYPE   {complete cyclic}

# Step 3: Iterate Through Every Combination
# ----------------------------------------------------------------------------
puts "INFO: Starting Full Design Space Exploration..."

foreach pipe_opt $PIPELINE_OPTIONS {
    foreach unroll_f $UNROLL_FACTORS {
        foreach flat_opt $FLATTEN_OPTIONS {
            foreach part_type $PARTITION_TYPE {

                # --- Create a unique name for this specific combination ---
                set sol_name "sol_pipe_${pipe_opt}_unroll_${unroll_f}_flat_${flat_opt}_part_${part_type}"
                puts "################################################################"
                puts "INFO: Configuring solution: $sol_name"
                puts "################################################################"

                # Create a new solution for this combination
                open_solution $sol_name
                set_part {xc7z020clg484-1}
                create_clock -period 4 -name default
                config_schedule -effort medium  -relax_ii_for_timing=0 

                # --- Apply BASELINE Directives (always enabled) ---
                set_directive_array_reshape -type cyclic -factor 3 -dim 1 "HCD_filter_hw" linebuf_x
                set_directive_array_partition -type cyclic -factor 3 -dim 1 "HCD_filter_hw" linebuf_y
                set_directive_inline "pop_stream"
                set_directive_inline "push_stream"
                set_directive_pipeline -II 1 "wrapped_HCD_filter_hw/Read_Ix_stream_cols"
                set_directive_pipeline -II 1 "wrapped_HCD_filter_hw/Read_Iy_stream_cols"
                set_directive_pipeline -II 1 "wrapped_HCD_filter_hw/Write_output_stream_cols"
                set_directive_pipeline -II 1 "HCD_filter_hw/Init_line_buffers_rows"

                # --- Apply DYNAMIC Directives based on the current combination ---

                # Knob 1: Pipelining the main processing loop
                if { $pipe_opt == "on" } {
                    set_directive_pipeline -II 1 "HCD_filter_hw/Process_image_cols"
                }

                # Knob 2: Loop Unrolling for the main processing loop
                if { $unroll_f > 1 } {
                    # This directive is only applied if the factor is greater than 1
                    set_directive_unroll -factor $unroll_f "HCD_filter_hw/Process_image_cols"
                }

                # Knob 3: Loop Flattening
                if { $flat_opt == "on" } {
                    set_directive_loop_flatten "HCD_filter_hw/Process_image_rows"
                }

                # Knob 4: Array Partitioning for key arrays
                if { $part_type == "complete" } {
                    set_directive_array_partition -type complete -dim 0 "HCD_filter_hw" window_x
                    set_directive_array_partition -type complete -dim 0 "HCD_filter_hw" window_y
                    set_directive_array_partition -type complete -dim 0 "HCD_filter_hw" Ix2_window
                    set_directive_array_partition -type complete -dim 0 "HCD_filter_hw" Iy2_window
                    set_directive_array_partition -type complete -dim 0 "HCD_filter_hw" Ixy_window
                } elseif { $part_type == "cyclic" } {
                    # When cyclic, we must provide a factor. 3 is logical for a 3x3 window.
                    set_directive_array_partition -type cyclic -factor 3 -dim 0 "HCD_filter_hw" window_x
                    set_directive_array_partition -type cyclic -factor 3 -dim 0 "HCD_filter_hw" window_y
                    set_directive_array_partition -type cyclic -factor 3 -dim 0 "HCD_filter_hw" Ix2_window
                    set_directive_array_partition -type cyclic -factor 3 -dim 0 "HCD_filter_hw" Iy2_window
                    set_directive_array_partition -type cyclic -factor 3 -dim 0 "HCD_filter_hw" Ixy_window
                }


                # --- Run Synthesis for this combination ---
                puts "INFO: Running C synthesis for $sol_name..."
                if {[catch {csynth_design} res]} {
                    puts "ERROR: Synthesis FAILED for solution $sol_name. Message: $res"
                } else {
                    puts "INFO: Synthesis SUCCEEDED for solution $sol_name."
                }
            }
        }
    }
}

puts "================================================================"
puts "INFO: Full Design Space Exploration Complete."
puts "================================================================"
exit