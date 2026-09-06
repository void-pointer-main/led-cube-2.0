#include "adc_dma_sampler_helper.h"

#include <stdlib.h>
#include <stdio.h>
#include "pico/malloc.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#define CAPTURE_CHANNEL 0
#define DMA_STEP_SIZE 64 // determines how many new samples we collect for each handler call

typedef struct {
    uint read_index;
    uint write_index;
    int16_t *buf;
} rotating_buffer_t;

typedef struct {
    volatile bool enabled;
    bool first_run;

    volatile bool *write_to_transfer_buf;
    float *transfer_buf;

    dma_channel_config dma_cfg;
    uint dma_chan;
    rotating_buffer_t rotating_buffer;

    size_t num_samp;
} adc_dma_cfg_t;

adc_dma_cfg_t adc_dma_cfg;

void __isr adc_dma_handler();

int adc_dma_sampler_init(float clkdiv, float *transfer_buf, size_t transfer_buf_len, volatile bool *write_to_transfer_buf) {

    int16_t *tmp = malloc((transfer_buf_len + DMA_STEP_SIZE) * sizeof(int16_t));
    if (tmp == NULL) {
        adc_dma_cfg.rotating_buffer.buf = NULL;
        return ERROR_MALLOC;
    }
    adc_dma_cfg.rotating_buffer.buf = tmp;

    adc_dma_cfg.num_samp = transfer_buf_len;
    adc_dma_cfg.transfer_buf = transfer_buf; // We assume that allocation is handled by caller.
    adc_dma_cfg.write_to_transfer_buf = write_to_transfer_buf;

    adc_gpio_init(26 + CAPTURE_CHANNEL);
    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // I do not have the patience to deal with errors
        false);
    adc_set_clkdiv(clkdiv);
    
    adc_dma_cfg.dma_chan = dma_claim_unused_channel(false);
    if (adc_dma_cfg.dma_chan == -1) {
        adc_dma_sampler_deinit();
        return ERROR_DMA_CHAN;
    }

    adc_dma_cfg.dma_cfg = dma_channel_get_default_config(adc_dma_cfg.dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&(adc_dma_cfg.dma_cfg), DMA_SIZE_16);
    channel_config_set_read_increment(&(adc_dma_cfg.dma_cfg), false);
    channel_config_set_write_increment(&(adc_dma_cfg.dma_cfg), true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&(adc_dma_cfg.dma_cfg), DREQ_ADC);

    dma_channel_set_irq0_enabled(adc_dma_cfg.dma_chan, true);
    adc_dma_cfg.enabled = true;
    adc_dma_cfg.first_run = true;

    irq_set_exclusive_handler(DMA_IRQ_0, adc_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    adc_dma_cfg.rotating_buffer.read_index = 0;
    adc_dma_cfg.rotating_buffer.write_index = 0;

    // start DMA and ADC
    adc_dma_handler();
    return OK;
}

void adc_dma_sampler_deinit() {
    adc_dma_cfg.enabled = false;
    if (adc_dma_cfg.dma_chan >= 0) {
        // dma_channel_wait_for_finish_blocking(fft_adc_dma_cfg.dma_chan);
        dma_channel_cleanup(adc_dma_cfg.dma_chan);
        dma_channel_unclaim(adc_dma_cfg.dma_chan);
        adc_dma_cfg.dma_chan = -1;
    }
    
    if (adc_dma_cfg.rotating_buffer.buf != NULL) {
        free(adc_dma_cfg.rotating_buffer.buf);
        adc_dma_cfg.rotating_buffer.buf = NULL;
    }
}

void __isr adc_dma_handler()
{
    if (!adc_dma_cfg.enabled) {
        adc_dma_cfg.first_run = true; // it is actually correct for first_run to be set to true inside the handler, because the handler executes at a potentially different rate to the update function.
        adc_run(false);
        adc_fifo_drain();
        dma_channel_acknowledge_irq0(adc_dma_cfg.dma_chan);
        return;
    }

    if (adc_dma_cfg.first_run)
    {
        adc_dma_cfg.first_run = false;

        dma_channel_configure(adc_dma_cfg.dma_chan, &(adc_dma_cfg.dma_cfg),
                              adc_dma_cfg.rotating_buffer.buf,
                              &adc_hw->fifo,
                              adc_dma_cfg.num_samp,
                              true
        );
        dma_channel_acknowledge_irq0(adc_dma_cfg.dma_chan);
        adc_run(true);

        // preloading indicies
        adc_dma_cfg.rotating_buffer.read_index = adc_dma_cfg.num_samp;
        adc_dma_cfg.rotating_buffer.write_index = DMA_STEP_SIZE;

        return;
    }

    adc_dma_cfg.rotating_buffer.read_index += DMA_STEP_SIZE;
    adc_dma_cfg.rotating_buffer.write_index += DMA_STEP_SIZE;

    if (adc_dma_cfg.rotating_buffer.read_index >= adc_dma_cfg.num_samp + DMA_STEP_SIZE)
    {
        adc_dma_cfg.rotating_buffer.read_index = 0;
    }
    if (adc_dma_cfg.rotating_buffer.write_index >= adc_dma_cfg.num_samp + DMA_STEP_SIZE)
    {
        adc_dma_cfg.rotating_buffer.write_index = 0;
    }

    dma_channel_configure(adc_dma_cfg.dma_chan, &(adc_dma_cfg.dma_cfg),
                          adc_dma_cfg.rotating_buffer.buf + adc_dma_cfg.rotating_buffer.write_index, // dst
                          &adc_hw->fifo,                                                                        // src
                          DMA_STEP_SIZE,                                                                        // transfer count
                          true                                                                                  // start immediately
    );

    dma_channel_acknowledge_irq0(adc_dma_cfg.dma_chan);

    // copying data from rotating buffer into transfer_buffer
    if (*(adc_dma_cfg.write_to_transfer_buf)) {
        int i = 0;
        int read = adc_dma_cfg.rotating_buffer.read_index;
        while (i < adc_dma_cfg.num_samp)
        {
            adc_dma_cfg.transfer_buf[i++] = adc_dma_cfg.rotating_buffer.buf[read++] * 3.3 / 4096 - 1.65;
            if (read >= adc_dma_cfg.num_samp)
                read = 0;
        }
        *(adc_dma_cfg.write_to_transfer_buf) = false;
    }
}
