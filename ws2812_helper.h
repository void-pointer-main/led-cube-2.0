/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Basically just a repackaging of the ws2812 pi pico example */

#ifndef WS2812_HELPER
#define WS2812_HELPER

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define NUM_PIXELS 64
#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_SCREENS 6

#define WS2812_PIN_TOP 0 
#define WS2812_PIN_BOTTOM 5
#define WS2812_PIN_LEFT 2
#define WS2812_PIN_RIGHT 4
#define WS2812_PIN_FRONT 3
#define WS2812_PIN_BACK 1

enum screen_enum {
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
    FRONT,
    BACK
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

typedef struct {
    PIO pio;
    uint sm;
    uint offset;
    uint pin;
    rgb_t pixels[NUM_ROWS][NUM_COLS];
    bool row_inverted;
    bool col_inverted;
    float pixel_vectors[NUM_ROWS][NUM_COLS][3];
} screen_t;

extern screen_t screens[NUM_SCREENS];

void ws2812_init();
// Don't need a release, it's all about the LEDs

void ws2812_display_screens();
void ws2812_write_screen_pixel(uint screen, uint row, uint col, rgb_t rgb);
void ws2812_write_screen_pixel_modified_intensity(uint screen, uint row, uint col, rgb_t rgb, uint mult, uint div);
void ws2812_blank_screen(uint screen);

static inline uint32_t rgb_t2grb_u32(rgb_t pixel);
rgb_t rgb2rgb_t_f(uint8_t r, uint8_t g, uint8_t b);
rgb_t hex2rgb_t_f(uint32_t rgb);
rgb_t hex2rgb_t_f_modified_intensity(uint32_t rgb, uint mult, uint div);

#endif