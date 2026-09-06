#ifndef ADC_DMA_SAMPLER_HELPER_H
#define ADC_DMA_SAMPLER_HELPER_H

#include "pico/stdlib.h"

enum {
    OK,
    ERROR_MALLOC,
    ERROR_DMA_CHAN,
};

int adc_dma_sampler_init(float clkdiv, float *transfer_buf, size_t transfer_buf_len, volatile bool *write_to_transfer_buf);
void adc_dma_sampler_deinit();

#endif
