/* FFT library from https://github.com/mborgerding/kissfft by Mark Borgerding */

#ifndef FFT_ADC_DMA_HELPER_H
#define FFT_ADC_DMA_HELPER_H

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "kiss_fftr.h"

#ifndef CAPTURE_CHANNEL
#define CAPTURE_CHANNEL 0
#endif

void fft_adc_dma_init();
void fft_adc_dma_release();
void fft_adc_dma_update();

#endif