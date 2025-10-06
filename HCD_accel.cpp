#include <stdio.h>
#include <stdlib.h>

#include "HCD.h"

// Ajustement de IMG_SIZE pour des transferts en blocs de 32 bits (4 pixels de 8 bits par bloc)
//#define IMG_SIZE (WIDTH * HEIGHT)

void standalone_HCD_filter(T I_x[HEIGHT][WIDTH], T I_y[HEIGHT][WIDTH], T output_img[HEIGHT][WIDTH]) {
    HCD_filter_hw<T, HEIGHT, WIDTH>(I_x, I_y, output_img);
}

void HLS_accel(AXI_VAL INPUT_STREAM[2*IMG_SIZE], AXI_VAL OUTPUT_STREAM[IMG_SIZE]) {
    #pragma HLS INTERFACE s_axilite port=return bundle=CONTROL_BUS
    #pragma HLS INTERFACE axis port=OUTPUT_STREAM
    #pragma HLS INTERFACE axis port=INPUT_STREAM

	wrapped_HCD_filter_hw<T, WIDTH, HEIGHT, IMG_SIZE, 4, 5, 5>(INPUT_STREAM, OUTPUT_STREAM);
}
