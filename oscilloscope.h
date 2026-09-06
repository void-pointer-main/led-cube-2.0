#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include "pico/stdlib.h"

#ifndef CAPTURE_CHANNEL
#define CAPTURE_CHANNEL 0
#endif

void oscilloscope_init();
void oscilloscope_release();
void oscilloscope_update();
void oscilloscope_change_sampling_frequency_index(int change);
void oscilloscope_toggle_trigger_on_falling_edge();

#endif
