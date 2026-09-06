#include "tap_hint.h"

#include <math.h>

#include "pico/stdlib.h"
#include "ws2812_helper.h"

#define TAP_HINT_SHOW_ITME_US (150*1000)
#define TAP_HINT_DIFF_THRESHOLD 0.5f
#define TAP_HINT_SCREEN_INTENSITY_MULT 1
#define TAP_HINT_SCREEN_INTENSITY_DIV 4
#define o 0xFFE100
uint32_t tap_hint_pixel_map_front[NUM_ROWS][NUM_COLS] = {
    {0, 0, o, 0, 0, o, 0, 0},
    {0, 0, 0, o, o, 0, 0, 0},
    {o, 0, 0, 0, 0, 0, 0, o},
    {0, o, 0, 0, 0, 0, o, 0},
    {0, o, 0, 0, 0, 0, o, 0},
    {o, 0, 0, 0, 0, 0, 0, o},
    {0, 0, 0, o, o, 0, 0, 0},
    {0, 0, o, 0, 0, o, 0, 0}
};
#define O 0xE100FF
uint32_t tap_hint_pixel_map_back[NUM_ROWS][NUM_COLS] = {
    {0, 0, O, 0, 0, O, 0, 0},
    {0, 0, 0, O, O, 0, 0, 0},
    {O, 0, 0, 0, 0, 0, 0, O},
    {0, O, 0, 0, 0, 0, O, 0},
    {0, O, 0, 0, 0, 0, O, 0},
    {O, 0, 0, 0, 0, 0, 0, O},
    {0, 0, 0, O, O, 0, 0, 0},
    {0, 0, O, 0, 0, O, 0, 0}
};

bool show_tap_hint = false;
uint64_t previous_tap_hint_time_us = 0;

float previous_acc[3] = {0};

void check_if_should_show_tap_hint(float acc[3]) {
    for (int i = 0; i < 3; i++) {
        if (!show_tap_hint && fabsf(previous_acc[i] - acc[i]) >= TAP_HINT_DIFF_THRESHOLD) {
            show_tap_hint = true;
            previous_tap_hint_time_us = time_us_64();
            break;
        }
    }

    for (int i = 0; i < 3; i++) {
        previous_acc[i] = acc[i];
    }
}

void maybe_show_tap_hint() {
    if (show_tap_hint) {// && state != FFT) {
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                if (tap_hint_pixel_map_front[r][c] != 0) ws2812_write_screen_pixel(FRONT, r, c, hex2rgb_t_f_modified_intensity(tap_hint_pixel_map_front[r][c], TAP_HINT_SCREEN_INTENSITY_MULT, TAP_HINT_SCREEN_INTENSITY_DIV));
                if (tap_hint_pixel_map_back[r][c] != 0) ws2812_write_screen_pixel(BACK, r, c, hex2rgb_t_f_modified_intensity(tap_hint_pixel_map_back[r][c], TAP_HINT_SCREEN_INTENSITY_MULT, TAP_HINT_SCREEN_INTENSITY_DIV));
            }
        }
    }
    if (show_tap_hint && time_us_64() - previous_tap_hint_time_us > TAP_HINT_SHOW_ITME_US) {
        show_tap_hint = false;
    }
}
