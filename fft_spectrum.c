#include "fft_spectrum.h"

#include <math.h>
#include "kiss_fftr.h"

#include "adc_dma_sampler_helper.h"
#include "ws2812_helper.h"

#define NUM_SAMP 128 // must be a multiple of 64, see DMA_STEP_SIZE in adc_dma_sampler.c.

#define FSAMP 8000
#define CLOCK_DIV (48000000.f/FSAMP)

static kiss_fft_scalar *fft_in;
static kiss_fft_cpx *fft_out;
static kiss_fftr_cfg kiss_cfg;

#define MIN_REFERENCE_POWER 0.5f

static float *powers;
static float f_max;
static float f_res;

static volatile bool write_to_fft_buffer = true;

/* Screen stuff */
#define NUM_FFT_SCREENS 4
const int fft_screen_strip[NUM_FFT_SCREENS] = {FRONT, RIGHT, BACK, LEFT};

#define FFT_SCREEN_INTENSITY_MULT 1
#define FFT_SCREEN_INTENSITY_DIV 4
#define q 0x000AFE // Indeed, this is stupid, but if we mess up by using these letters in a variable, the compiler will catch it.
#define Q 0x000495
#define w 0x00003F
#define W 0x00000F
const uint32_t fft_top_screen[NUM_ROWS][NUM_COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, w, 0, 0, 0, 0, 0, 0},
    {0, Q, 0, 0, 0, 0, 0, 0},
    {0, q, 0, w, 0, 0, 0, 0},
    {0, q, W, Q, 0, 0, 0, 0},
    {0, q, w, q, W, Q, 0, 0},
    {0, q, Q, q, w, q, W, 0},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

void fft_spectrum_init() {
    // setting up the screens
    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            ws2812_write_screen_pixel(TOP, r, c, hex2rgb_t_f_modified_intensity(fft_top_screen[r][c], FFT_SCREEN_INTENSITY_MULT, FFT_SCREEN_INTENSITY_DIV));
        }
    }
    ws2812_blank_screen(BOTTOM);

    // allocate all of the needed buffers
    kiss_cfg = kiss_fftr_alloc(NUM_SAMP, false, NULL, NULL);
    if (kiss_cfg == NULL) {
        return;
    }

    void *tmp = malloc(NUM_SAMP * sizeof(kiss_fft_scalar));
    if (tmp == NULL) {
        fft_spectrum_release();
        return;
    }
    fft_in = (kiss_fft_scalar *)tmp;

    tmp = malloc(NUM_SAMP * sizeof(kiss_fft_cpx));
    if (tmp == NULL) {
        fft_spectrum_release();
        return;
    }
    fft_out = (kiss_fft_cpx *)tmp;

    tmp = malloc(NUM_SAMP * sizeof(float));
    if (tmp == NULL) {
        fft_spectrum_release();
        return;
    }
    powers = (float *)tmp;

    if (adc_dma_sampler_init(CLOCK_DIV, fft_in, NUM_SAMP, &write_to_fft_buffer) != OK) {
        fft_spectrum_release();
        return;
    }
}

void fft_spectrum_release() {
    adc_dma_sampler_deinit();

    if (kiss_cfg != NULL) {
        free(kiss_cfg);
        kiss_cfg = NULL;
    }
    if (fft_in != NULL) {
        free(fft_in);
        fft_in = NULL;
    }
    if (fft_out != NULL) {
        free(fft_out);
        fft_out = NULL;
    }
    if (powers != NULL) {
        free(powers);
        powers = NULL;
    }
}

void fft_spectrum_update() {    
    if (write_to_fft_buffer) {
        return;
    }

    if (kiss_cfg == NULL) {
        // display error ig
        printf("ERROR: fft spectrum!\n");
        return;
    }
    
    float average = 0;
    for (int i = 0; i < NUM_SAMP; i++) {
        average += fft_in[i];
    }
    average /= NUM_SAMP;
    // subtract DC offset and apply windowing
    for (int i = 0; i < NUM_SAMP; i++) {
        fft_in[i] -= average;
    }

    kiss_fftr(kiss_cfg, fft_in, fft_out);
    write_to_fft_buffer = true; // this should be right under fft calculation

    float max_power = 0;
    int max_idx = 0;
    // spectrum is symmetrical around NUM_SAMP/2, signal theory
    for (int i = 0; i < NUM_FFT_SCREENS*NUM_COLS; i++)
    {
        powers[i] = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
        if (powers[i] > max_power)
        {
            max_power = powers[i];
            max_idx = i;
        }
    }

    float reference_power = max_power;
    if (reference_power <= 5.f) {
        reference_power = 5.f;
    }

    for (int n = 0; n < NUM_FFT_SCREENS; n++) {
        for (int r = 0; r < NUM_ROWS; r++)
        {
            for (int c = 0; c < NUM_COLS; c++)
            {
                int fft_index = c + n*NUM_COLS;

                if (powers[fft_index] >= r*reference_power/NUM_ROWS) {
                    ws2812_write_screen_pixel(fft_screen_strip[n], NUM_ROWS-1 - r, c, rgb2rgb_t_f(50, 0, 0));
                } else {
                    ws2812_write_screen_pixel(fft_screen_strip[n], NUM_ROWS-1 - r, c, rgb2rgb_t_f(0, 0, 0));
                }
            }
        }
    }

    // printf("max f: %.2f\n", max_freq);
}
