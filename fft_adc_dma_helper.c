#include "fft_adc_dma_helper.h"
#include <math.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "ws2812_helper.h"

#include "hann_windows.h"

#define NUM_SAMP 128 // must be divisible by two, not only for FFT but also for DMA reasons, also >= 64
#define hann_window hann_window_128 // not used, no perceivable effect

#define FSAMP 8000
#define CLOCK_DIV (48000000.f/FSAMP)

#define DMA_STEP_SIZE 64

typedef struct {
    int16_t buffer[NUM_SAMP + DMA_STEP_SIZE];
    uint read_index;
    uint write_index;
} rotating_buffer_t;

typedef struct {
    volatile bool enabled;
    bool first_run;

    /* DMA stuff */
    dma_channel_config dma_cfg;
    uint dma_chan;
    rotating_buffer_t rotating_buffer;

    /* ADC stuff */
    uint capture_channel;

    /* KISS fft stuff */
    // uint16_t capture_buf[NUM_SAMP];
    kiss_fft_scalar fft_in[NUM_SAMP]; // kiss_fft_scalar is a float
    kiss_fft_cpx fft_out[NUM_SAMP];
    kiss_fftr_cfg kiss_fft_cfg;

} fft_adc_dma_cfg_t;

#define MIN_REFERENCE_POWER 0.5f

static float freqs[NUM_SAMP];
static float powers[NUM_SAMP];
static float f_max;
static float f_res;
#define NUM_FFT_SCREENS 4
static const int fft_screen_strip[NUM_FFT_SCREENS] = {FRONT, RIGHT, BACK, LEFT};
volatile bool write_to_fft_buffer = true;

static fft_adc_dma_cfg_t fft_adc_dma_cfg;

#define FFT_SCREEN_INTENSITY_MULT 1
#define FFT_SCREEN_INTENSITY_DIV 4
// #define q 0x000A04
// #define Q 0x010A00
// #define w 0x080A00
// const uint32_t fft_top_screen[NUM_ROWS][NUM_COLS] = {
//     {0, 0, 0, 0, 0, 0, 0, 0},
//     {q, q, q, Q, Q, w, w, w},
//     {q, 0, 0, Q, 0, 0, w, 0},
//     {q, q, 0, Q, Q, 0, w, 0},
//     {q, 0, 0, Q, 0, 0, w, 0},
//     {q, 0, 0, Q, 0, 0, w, 0},
//     {q, 0, 0, Q, 0, 0, w, 0},
//     {0, 0, 0, 0, 0, 0, 0, 0}
// };
#define q 0x000AFE
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

void __isr fft_adc_dma_handler();

void fft_adc_dma_init()
{
    // setting up the top screen
    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            ws2812_write_screen_pixel(TOP, r, c, hex2rgb_t_f_modified_intensity(fft_top_screen[r][c], FFT_SCREEN_INTENSITY_MULT, FFT_SCREEN_INTENSITY_DIV));
        }
    }
    ws2812_blank_screen(BOTTOM);

    adc_gpio_init(26 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // I do not have the patience to deal with errors
        false);

    adc_set_clkdiv(CLOCK_DIV);
    
    fft_adc_dma_cfg.dma_chan = dma_claim_unused_channel(true);
    fft_adc_dma_cfg.dma_cfg = dma_channel_get_default_config(fft_adc_dma_cfg.dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&(fft_adc_dma_cfg.dma_cfg), DMA_SIZE_16);
    channel_config_set_read_increment(&(fft_adc_dma_cfg.dma_cfg), false);
    channel_config_set_write_increment(&(fft_adc_dma_cfg.dma_cfg), true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&(fft_adc_dma_cfg.dma_cfg), DREQ_ADC);

    dma_channel_set_irq0_enabled(fft_adc_dma_cfg.dma_chan, true);
    fft_adc_dma_cfg.enabled = true;
    fft_adc_dma_cfg.first_run = true;

    irq_set_exclusive_handler(DMA_IRQ_0, fft_adc_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    fft_adc_dma_cfg.rotating_buffer.read_index = 0;
    fft_adc_dma_cfg.rotating_buffer.write_index = 0;

    fft_adc_dma_cfg.kiss_fft_cfg = kiss_fftr_alloc(NUM_SAMP, false, NULL, NULL);

    // calculate frequencies of each bin
    f_max = FSAMP;
    f_res = f_max / NUM_SAMP;
    for (int i = 0; i < NUM_SAMP; i++)
    {
        freqs[i] = f_res * i;
    }

    // start DMA and ADC
    fft_adc_dma_handler();
}

void fft_adc_dma_release()
{
    fft_adc_dma_cfg.enabled = false;
    if (fft_adc_dma_cfg.dma_chan >= 0) {
        // dma_channel_wait_for_finish_blocking(fft_adc_dma_cfg.dma_chan);
        dma_channel_cleanup(fft_adc_dma_cfg.dma_chan);
        dma_channel_unclaim(fft_adc_dma_cfg.dma_chan);
        fft_adc_dma_cfg.dma_chan = -1;

        kiss_fft_free(fft_adc_dma_cfg.kiss_fft_cfg);
    }
}

void fft_adc_dma_update() {
    if (write_to_fft_buffer) {
        return;
    }
    
    float average = 0;
    for (int i = 0; i < NUM_SAMP; i++) {
        average += fft_adc_dma_cfg.fft_in[i];
    }
    average /= NUM_SAMP;
    // subtract DC offset and apply windowing
    for (int i = 0; i < NUM_SAMP; i++) {
        fft_adc_dma_cfg.fft_in[i] -= average;
        // fft_adc_dma_cfg.fft_in[i] *= hann_window[i];// no difference if not zoomed in
    }

    kiss_fftr(fft_adc_dma_cfg.kiss_fft_cfg, fft_adc_dma_cfg.fft_in, fft_adc_dma_cfg.fft_out);
    write_to_fft_buffer = true; // this should be right under fft calculation

    float max_power = 0;
    int max_idx = 0;
    // spectrum is symmetrical around NUM_SAMP/2, signal theory
    for (int i = 0; i < NUM_FFT_SCREENS*NUM_COLS; i++)
    {
        powers[i] = fft_adc_dma_cfg.fft_out[i].r * fft_adc_dma_cfg.fft_out[i].r + fft_adc_dma_cfg.fft_out[i].i * fft_adc_dma_cfg.fft_out[i].i;
        if (powers[i] > max_power)
        {
            max_power = powers[i];
            max_idx = i;
        }
    }
    float max_freq = freqs[max_idx];

    // printf("%.2f\n", max_power);
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

void __isr fft_adc_dma_handler() {
    if (!fft_adc_dma_cfg.enabled)
    {
        fft_adc_dma_cfg.first_run = true;
        adc_run(false);
        adc_fifo_drain();
        dma_channel_acknowledge_irq0(fft_adc_dma_cfg.dma_chan);
        return;
    }

    if (fft_adc_dma_cfg.first_run)
    {
        fft_adc_dma_cfg.first_run = false;

        dma_channel_configure(fft_adc_dma_cfg.dma_chan, &(fft_adc_dma_cfg.dma_cfg),
                              fft_adc_dma_cfg.rotating_buffer.buffer,
                              &adc_hw->fifo,
                              NUM_SAMP,
                              true
        );
        dma_channel_acknowledge_irq0(fft_adc_dma_cfg.dma_chan);
        adc_run(true);

        // preloading indicies
        fft_adc_dma_cfg.rotating_buffer.read_index = NUM_SAMP;
        fft_adc_dma_cfg.rotating_buffer.write_index = DMA_STEP_SIZE;

        return;
    }

    fft_adc_dma_cfg.rotating_buffer.read_index += DMA_STEP_SIZE;
    fft_adc_dma_cfg.rotating_buffer.write_index += DMA_STEP_SIZE;

    if (fft_adc_dma_cfg.rotating_buffer.read_index >= NUM_SAMP + DMA_STEP_SIZE)
    {
        fft_adc_dma_cfg.rotating_buffer.read_index = 0;
    }
    if (fft_adc_dma_cfg.rotating_buffer.write_index >= NUM_SAMP + DMA_STEP_SIZE)
    {
        fft_adc_dma_cfg.rotating_buffer.write_index = 0;
    }

    dma_channel_configure(fft_adc_dma_cfg.dma_chan, &(fft_adc_dma_cfg.dma_cfg),
                          fft_adc_dma_cfg.rotating_buffer.buffer + fft_adc_dma_cfg.rotating_buffer.write_index, // dst
                          &adc_hw->fifo,                                                                        // src
                          DMA_STEP_SIZE,                                                                        // transfer count
                          true                                                                                  // start immediately
    );

    dma_channel_acknowledge_irq0(fft_adc_dma_cfg.dma_chan);

    // copying data from rotating buffer
    if (write_to_fft_buffer) {
        int i = 0;
        int read = fft_adc_dma_cfg.rotating_buffer.read_index;
        while (i < NUM_SAMP)
        {
            fft_adc_dma_cfg.fft_in[i++] = fft_adc_dma_cfg.rotating_buffer.buffer[read++] * 3.3 / 4096 - 1.65;
            if (read >= NUM_SAMP)
                read = 0;
        }
        write_to_fft_buffer = false;
    }
}
