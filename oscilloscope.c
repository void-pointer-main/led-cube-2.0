#include "oscilloscope.h"

#include <math.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#include "ws2812_helper.h"

#define ADC_CLOCK 48000000

#define NUM_SHOWN_SAMPLES 32
#define OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS 4
#define SKIP_INDEX_N 8
#define NUM_SAMP (32*(SKIP_INDEX_N+2))
float adc_clock_divs[OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS] = {ADC_CLOCK/SKIP_INDEX_N/2000.f, ADC_CLOCK/SKIP_INDEX_N/4000.f, ADC_CLOCK/SKIP_INDEX_N/8000.f, ADC_CLOCK/SKIP_INDEX_N/16000.f};//, ADC_CLOCK/SKIP_INDEX_N/32000.f};
uint16_t oscilloscope_colors[OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS][3] = {{215, 40, 0}, {128, 127, 0}, {15, 240, 0}, {2, 23, 210}};//, {50, 5, 200}};
int oscilloscope_adc_clock_div_index = 0;

#define TOP_BOTTOM_LEVEL_LIMIT 2.5f

float oscilloscope_scaled_trigger_levels[OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS] = {3.f, 3.f, 3.f, 3.f};

#define TOP_SCREEN_INTENSITY_MULT 1
#define TOP_SCREEN_INTENSITY_DIV 16
#define q 0xFF0000
uint32_t top_screen_pixel_map[NUM_ROWS][NUM_COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, q, q, 0, 0, 0, 0, 0},
    {q, 0, 0, q, 0, 0, 0, 0},
    {0, 0, 0, 0, q, 0, 0, q},
    {0, 0, 0, 0, 0, q, q, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

#define OSCILLOSCOPE_INTENSITY_MULT 255
#define OSCILLOSCOPE_INTENSITY_DIV 1024

#define OSCILLOSCOPE_NUM_SCREENS 4
uint oscilloscope_screens[OSCILLOSCOPE_NUM_SCREENS] = {FRONT, RIGHT, BACK, LEFT};
uint16_t oscilloscope_data_buffer[NUM_SAMP] = {0};
float oscilloscope_data_scaled[NUM_SAMP] = {0};

#define LEVEL_FILTER_COEF 0.4f

uint dma_chan = -1;
dma_channel_config dma_cfg;

void xiaolin_wu_draw(float x0, float x1, float y0, float y1);
void plot(int x, int y, uint8_t intensity);
float scale_to_display(float y, float top_level, float bottom_level);
void scale_to_display_in_place(float *y, float top_level, float bottom_level);
void swap_floats(float *a, float *b);

// unused
// void draw_trigger_cursor();

void oscilloscope_init() {
    adc_gpio_init(26 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // I do not have the patience to deal with errors
        false);

    oscilloscope_adc_clock_div_index = 0;
    adc_set_clkdiv(adc_clock_divs[oscilloscope_adc_clock_div_index]);

    dma_chan = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);

    channel_config_set_dreq(&dma_cfg, DREQ_ADC);

    for (int i = 0; i < NUM_SAMP; i++) {
        oscilloscope_data_buffer[i] = 0;
    }

    ws2812_blank_screen(BOTTOM);
    // setting up the top screen
    for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
            ws2812_write_screen_pixel(TOP, r, c, hex2rgb_t_f_modified_intensity(top_screen_pixel_map[r][c], TOP_SCREEN_INTENSITY_MULT, TOP_SCREEN_INTENSITY_DIV));
        }
    }
}

void oscilloscope_release() {
    if (dma_chan >= 0) {
        adc_run(false);
        adc_fifo_drain();
        dma_channel_cleanup(dma_chan);
        dma_channel_unclaim(dma_chan);
        dma_chan = -1;
    }
}

void oscilloscope_update() {
    // debugging
    // static int cntr = 0;
    // if (cntr > 100) {
    //     while(1);
    // }
    // cntr++;
    static float top_level = 1.f;
    static float bottom_level = -1.f;

    if (dma_channel_is_busy(dma_chan)) {
        return;
    }
    
    adc_run(false);
    adc_fifo_drain();
    adc_set_clkdiv(adc_clock_divs[oscilloscope_adc_clock_div_index]);

    int average = 0;
    for (int i = 0; i < NUM_SAMP; i++) {
        average += oscilloscope_data_buffer[i];
    }
    average /= NUM_SAMP;

    for (int i = 0; i < NUM_SAMP; i++) {
        oscilloscope_data_scaled[i] = (oscilloscope_data_buffer[i]-average)/10.f;
    }

    float max_value = 0.f;
    float min_value = 0.f;
    for (int i = 0; i < NUM_SAMP; i++) {
        if (oscilloscope_data_scaled[i] > max_value) {
            max_value = oscilloscope_data_scaled[i];
        } else if (oscilloscope_data_scaled[i] < min_value) {
            min_value = oscilloscope_data_scaled[i];
        }
    }

    top_level = LEVEL_FILTER_COEF*max_value + (1-LEVEL_FILTER_COEF)*top_level;
    bottom_level = LEVEL_FILTER_COEF*min_value + (1-LEVEL_FILTER_COEF)*bottom_level;

    if (top_level < TOP_BOTTOM_LEVEL_LIMIT) {
        top_level = TOP_BOTTOM_LEVEL_LIMIT;
    }
    if (bottom_level > -TOP_BOTTOM_LEVEL_LIMIT) {
        bottom_level = -TOP_BOTTOM_LEVEL_LIMIT;
    }

    for (int i = 0; i < NUM_SAMP; i++) {
        scale_to_display_in_place(&oscilloscope_data_scaled[i], top_level, bottom_level);
    }

    /* Trigger Logic */
    int trigger_index = 0;
    bool trigger_set = false;
    bool signal_appeared_below_triger = false;
    for (int i = 1; i < NUM_SAMP-1; i++) {
        // we want to find the rising edge of the signal
        if (!signal_appeared_below_triger && oscilloscope_data_scaled[i-1] < oscilloscope_scaled_trigger_levels[oscilloscope_adc_clock_div_index]) {
            signal_appeared_below_triger = true;
        }
        if (!trigger_set && oscilloscope_data_scaled[i] >= oscilloscope_scaled_trigger_levels[oscilloscope_adc_clock_div_index]
            && signal_appeared_below_triger) {
            trigger_index = i;
            trigger_set = true;
            break;
        }
    }

    for (int k = 0; k < OSCILLOSCOPE_NUM_SCREENS; k++) {
        ws2812_blank_screen(oscilloscope_screens[k]);
    }
    
    for (int i = 0; i <= NUM_SHOWN_SAMPLES-1; i++) {
        float x0 = i*1.f;
        float x1 = x0+1;
        if (i*SKIP_INDEX_N + trigger_index >= 0 && (i+1)*SKIP_INDEX_N + trigger_index < NUM_SAMP) { // check bounds
            float y0 = oscilloscope_data_scaled[i*SKIP_INDEX_N + trigger_index];
            float y1 = oscilloscope_data_scaled[(i+1)*SKIP_INDEX_N + trigger_index]; // watch out for top index!
            xiaolin_wu_draw(x0, x1, y0, y1);
        }
    }
    // printf("%d\n", oscilloscope_adc_clock_div_index);
    // putchar('\n');

    // lots of debugging

    // printf("%.2f, %.2f\n", top_level, bottom_level);
    // putchar('\n');
    // putchar('\n');
    // for (int i = 0; i < NUM_SAMP; i+=1) {
    //     // printf("%.2f\n", scale_to_display(oscilloscope_data_scaled[i], top_level, bottom_level));
    //     printf("%.2f\n", oscilloscope_data_scaled[i]);
    // }

    // restart DMA
    dma_channel_configure(dma_chan, &dma_cfg,
                            oscilloscope_data_buffer, // dst
                            &adc_hw->fifo,                          // src
                            NUM_SAMP,                                  // transfer count
                            true                                    // start immediately
    );
    dma_channel_acknowledge_irq0(dma_chan);
    adc_run(true);
}

void oscilloscope_change_sampling_frequency_index(int change) {
    oscilloscope_adc_clock_div_index += change + OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS;
    oscilloscope_adc_clock_div_index %= OSCILLOSCOPE_NUM_ADC_CLOCK_DIVS;
}

// basically https://www.youtube.com/watch?v=f3Rs20k-hcI
void xiaolin_wu_draw(float x0, float x1, float y0, float y1) {
    if (fabsf(y1 - y0) < fabsf(x1 - x0)) {
        if (x1 < x0) {
            swap_floats(&x0, &x1);
            swap_floats(&y0, &y1);
        }
        float dx = x1-x0;
        float dy = y1-y0;
        float gradient = 1.f;
        if (dx != 0.f) {
            gradient = dy/dx;
        }
        for (int i = 0; i < (int)dx + 1; i++) {
            float x = x0 + i;
            float y = y0 + i*gradient;
            int ix = (int)x;
            int iy = (int)y;
            float dist = y - iy;
            plot(ix, iy, (uint8_t)((1-dist)*OSCILLOSCOPE_INTENSITY_MULT));
            plot(ix, iy+1, (uint8_t)(dist*OSCILLOSCOPE_INTENSITY_MULT));
        }
    } else {
        if (y1 < y0) {
            swap_floats(&x0, &x1);
            swap_floats(&y0, &y1);
        }
        float dx = x1-x0;
        float dy = y1-y0;
        float gradient = 1.f;
        if (dy != 0.f) {
            gradient = dx/dy;
        }
        for (int i = 0; i < (int)dy + 1; i++) {
            float x = x0 + i*gradient;
            float y = y0 + i;
            int ix = (int)x;
            int iy = (int)y;
            float dist = x - ix;
            plot(ix, iy, (uint8_t)((1-dist)*OSCILLOSCOPE_INTENSITY_MULT));
            plot(ix+1, iy, (uint8_t)(dist*OSCILLOSCOPE_INTENSITY_MULT));
        }
    }

}

void plot(int x, int y, uint8_t intensity) {
    if (y < 0 || y >= NUM_ROWS) return;
    if (x < 0 || x >= NUM_SHOWN_SAMPLES) return;

    // printf("x %d, y %d, i %d\n", x, y, intensity);

    uint8_t red = (uint8_t)(oscilloscope_colors[oscilloscope_adc_clock_div_index][0]*intensity/OSCILLOSCOPE_INTENSITY_DIV);
    uint8_t green = (uint8_t)(oscilloscope_colors[oscilloscope_adc_clock_div_index][1]*intensity/OSCILLOSCOPE_INTENSITY_DIV);
    uint8_t blue = (uint8_t)(oscilloscope_colors[oscilloscope_adc_clock_div_index][2]*intensity/OSCILLOSCOPE_INTENSITY_DIV);

    rgb_t color = rgb2rgb_t_f(red, green, blue);
    ws2812_write_screen_pixel(oscilloscope_screens[x/NUM_COLS], y, x % NUM_COLS, color);
}

float scale_to_display(float y, float top_level, float bottom_level) {
    top_level+=2; // giving the signal a bit of breathing space
    bottom_level-=2;
    return 7 * ((-y + bottom_level) / (top_level-bottom_level) + 1);
}

void scale_to_display_in_place(float *y, float top_level, float bottom_level) {
    top_level+=2;
    bottom_level-=2;
    *y = 7 * ((-*y + bottom_level) / (top_level-bottom_level) + 1);
}

void swap_floats(float *a, float *b) {
    float tmp = *a;
    *a = *b;
    *b = tmp;
}
