#include "matrix_rain.h"

#include "ws2812_helper.h"

#include <stdlib.h>

#define MATRIX_LOOP_FREQUENCY_DIVIDER 25
#define DROP_START_HEIGHT 6
#define DROP_INITIAL_INTENSITY 64
#define REDROP_PROBABILITY 8 // percentage

uint8_t matrix_rain_intensity[NUM_SCREENS][NUM_ROWS][NUM_COLS];
int drops[NUM_SCREENS][NUM_COLS];
uint drop_screens[4] = {FRONT, RIGHT, BACK, LEFT};

void matrix_rain_init() {
    srand(time_us_32());
    ws2812_blank_screen(FRONT);
    for (int k = 0; k < NUM_SCREENS; k++) {
        ws2812_blank_screen(k);
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                matrix_rain_intensity[k][r][c] = 0;
            }
        }
        for (int c = 0; c < NUM_COLS; c++) {
            drops[k][c] = -rand() % DROP_START_HEIGHT;
        }
    }
}

void matrix_rain_release() {}

void matrix_rain_update() {
    static int counter = 0;

    if (counter++ < MATRIX_LOOP_FREQUENCY_DIVIDER) {
        return;
    }
    counter = 0;

    for (int i = 0; i < 4; i++) {
        int k = drop_screens[i];
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                matrix_rain_intensity[k][r][c] = matrix_rain_intensity[k][r][c]*5/8;
            }
        }
        for (int c = 0; c < NUM_COLS; c++) {
            if (drops[k][c] >= 0 && drops[k][c] < NUM_ROWS) {
                matrix_rain_intensity[k][drops[k][c]][c] = DROP_INITIAL_INTENSITY;
            }
            if (drops[k][c] < NUM_ROWS) {
                drops[k][c] += 1;
            } else if (rand() % 100 > (100-REDROP_PROBABILITY)) {
                drops[k][c] = -rand() % DROP_START_HEIGHT;
            }
        }
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                ws2812_write_screen_pixel(k, r, c, rgb2rgb_t_f(0, matrix_rain_intensity[k][r][c], 0));
            }
        }
    }
}
