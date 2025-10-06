#include <ap_axi_sdata.h>
#include <ap_fixed.h>
#include <assert.h>
#include <stdint.h>

#include "hls_math.h"

#ifndef WIDTH
#define WIDTH 256  // Default size if not defined
#endif

#ifndef HEIGHT
#define HEIGHT 256  // Default size if not defined
#endif

#define K 0.04  // Harris constant
#define Rseuil 500

#define IMG_SIZE WIDTH *HEIGHT

typedef uint8_t T;  // 8-bit integer
typedef ap_fixed<32, 16> harris_fixed_t;
typedef ap_fixed<3, 1> K_fixed_t;
typedef uint32_t T32;
typedef ap_axiu<32, 4, 5, 5> AXI_VAL;  // 8 bits for the AXI interface

// Function prototypes
void standalone_HCD_filter(T I_x[HEIGHT][WIDTH], T I_y[HEIGHT][WIDTH], T output[HEIGHT][WIDTH]);
void HLS_accel(AXI_VAL in_stream[IMG_SIZE], AXI_VAL out_stream[IMG_SIZE]);

// Function to apply smoothing directly, without a kernel matrix
template <typename T>
T apply_kernel_single_block(T input[3][3]) {
    T sum = 0;
Sum_window_rows:
    for (int i = 0; i < 3; i++) {
    Sum_window_cols:
        for (int j = 0; j < 3; j++) {
#pragma HLS UNROLL
            sum += input[i][j];
        }
    }
    return sum / 9;
}

template <typename T, int IMG_HEIGHT, int IMG_WIDTH>
void HCD_filter_hw(T I_x[IMG_HEIGHT][IMG_WIDTH], T I_y[IMG_HEIGHT][IMG_WIDTH],
                   T output_img[IMG_HEIGHT][IMG_WIDTH]) {
    // Line buffers to store last 3 rows of Ix and Iy
    T linebuf_x[3][IMG_WIDTH];

    T linebuf_y[3][IMG_WIDTH];

    // 3x3 sliding windows
    T window_x[3][3];
    T window_y[3][3];

    // Accumulation buffers
    T S_x2, S_y2, S_xy;

// Initialize line buffers
Init_line_buffers_rows:
    for (int r = 0; r < 3; r++) {
    Init_line_buffers_cols:
        for (int c = 0; c < IMG_WIDTH; c++) {
            linebuf_x[r][c] = 0;
            linebuf_y[r][c] = 0;
        }
    }

// Main loop over image
Process_image_rows:
    for (int y = 0; y < IMG_HEIGHT; y++) {
    Process_image_cols:
        for (int x = 0; x < IMG_WIDTH; x++) {
            // Shift line buffers vertically
            linebuf_x[0][x] = linebuf_x[1][x];
            linebuf_x[1][x] = linebuf_x[2][x];
            linebuf_x[2][x] = I_x[y][x];

            linebuf_y[0][x] = linebuf_y[1][x];
            linebuf_y[1][x] = linebuf_y[2][x];
            linebuf_y[2][x] = I_y[y][x];

        // Shift window left
        Shift_window_left:
            for (int wy = 0; wy < 3; wy++) {
                window_x[wy][0] = window_x[wy][1];
                window_x[wy][1] = window_x[wy][2];

                window_y[wy][0] = window_y[wy][1];
                window_y[wy][1] = window_y[wy][2];
            }

            // Insert new rightmost column of window from line buffers
            window_x[0][2] = linebuf_x[0][x];
            window_x[1][2] = linebuf_x[1][x];
            window_x[2][2] = linebuf_x[2][x];

            window_y[0][2] = linebuf_y[0][x];
            window_y[1][2] = linebuf_y[1][x];
            window_y[2][2] = linebuf_y[2][x];

            // Only compute if we have a full 3x3 window
            if (y >= 2 && x >= 2) {
                T Ix2_window[3][3];
                T Iy2_window[3][3];
                T Ixy_window[3][3];

            Compute_gradient_products_rows:
                for (int wy = 0; wy < 3; wy++) {
                Compute_gradient_products_cols:
                    for (int wx = 0; wx < 3; wx++) {
                    mul_xx:
                        Ix2_window[wy][wx] = window_x[wy][wx] * window_x[wy][wx];
                    mul_yy:
                        Iy2_window[wy][wx] = window_y[wy][wx] * window_y[wy][wx];
                    mul_xy:
                        Ixy_window[wy][wx] = window_x[wy][wx] * window_y[wy][wx];
                    }
                }

                S_x2 = apply_kernel_single_block<T>(Ix2_window);
                S_y2 = apply_kernel_single_block<T>(Iy2_window);
                S_xy = apply_kernel_single_block<T>(Ixy_window);

                // Harris response
                harris_fixed_t prod1 = S_x2 * S_y2;
                harris_fixed_t prod2 = S_xy * S_xy;
                harris_fixed_t det_M = prod1 - prod2;

                harris_fixed_t trace_M = S_x2 + S_y2;
                harris_fixed_t trace_sq_intermediate = trace_M * trace_M;
                // #pragma HLS resource variable=trace_sq_intermediate core=FMul_maxdsp
                harris_fixed_t trace_sq = (K_fixed_t)K * trace_sq_intermediate;
                // #pragma HLS resource variable=trace_sq core=FMul_maxdsp

                harris_fixed_t R = det_M - trace_sq;

                // Threshold
                if (R > Rseuil) {
                    output_img[y - 1][x - 1] = 1;  // shift because of window delay
                } else {
                    output_img[y - 1][x - 1] = 0;
                }
            }
        }
    }
}

// Functions to read and write 32-bit pixels via AXI
template <int U, int TI, int TD>
void pop_stream(const AXI_VAL &e, T &pixel_8) {
#pragma HLS INLINE
    pixel_8 = static_cast<T>(e.data & 0xFF);  // Extract lower 8 bits
}

template <int U, int TI, int TD>
AXI_VAL push_stream(const T &pixel_8, bool last = false) {
#pragma HLS INLINE
    AXI_VAL e;
    e.data = static_cast<T32>(pixel_8);  // Insert 8-bit pixel into 32 bits
    e.strb = -1;                         // 32 bits, so 4 bytes
    e.keep = 15;                         // 32 bits, so 4 bytes
    e.user = 0;
    e.last = last ? 1 : 0;
    e.id = 0;
    e.dest = 0;
    return e;
}

// AXI4-Stream interface function with block processing
template <typename T, int IMG_WIDTH, int IMG_HEIGHT, int SIZE, int U, int TI, int TD>
void wrapped_HCD_filter_hw(AXI_VAL in_stream[SIZE * 2], AXI_VAL out_stream[SIZE]) {
#pragma HLS INLINE
    T I_x[IMG_HEIGHT][IMG_WIDTH];
    T I_y[IMG_HEIGHT][IMG_WIDTH];
    T output_img[IMG_HEIGHT][IMG_WIDTH];

// Read I_x
Read_Ix_stream_rows:
    for (int y = 0; y < IMG_HEIGHT; y++) {
    Read_Ix_stream_cols:
        for (int x = 0; x < IMG_WIDTH; x++) {
#pragma HLS PIPELINE II = 1
            int idx = y * IMG_WIDTH + x;
            T pixel_8;
            pop_stream<U, TI, TD>(in_stream[idx], pixel_8);
            I_x[y][x] = pixel_8;
        }
    }

// Read I_y (offset by SIZE for the second image)
Read_Iy_stream_rows:
    for (int y = 0; y < IMG_HEIGHT; y++) {
    Read_Iy_stream_cols:
        for (int x = 0; x < IMG_WIDTH; x++) {
#pragma HLS PIPELINE II = 1
            int idx = y * IMG_WIDTH + x + SIZE;
            T pixel_8;
            pop_stream<U, TI, TD>(in_stream[idx], pixel_8);
            I_y[y][x] = pixel_8;
        }
    }

    // Call the block processing function
    HCD_filter_hw<T, IMG_HEIGHT, IMG_WIDTH>(I_x, I_y, output_img);

// Convert output_img to the output stream
Write_output_stream_rows:
    for (int y = 0; y < IMG_HEIGHT; y++) {
    Write_output_stream_cols:
        for (int x = 0; x < IMG_WIDTH; x++) {
#pragma HLS PIPELINE II = 1
            int idx = y * IMG_WIDTH + x;
            T pixel_8 = output_img[y][x];
            out_stream[idx] = push_stream<U, TI, TD>(pixel_8, idx == (SIZE - 1));
        }
    }
}
