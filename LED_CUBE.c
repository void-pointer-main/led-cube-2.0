#include <stdio.h>
#include <math.h>
#include "pico/multicore.h"

// adc capture channel (GPIO26)
#define CAPTURE_CHANNEL 0

#include "my_utils.h"

#include "mpu6050_helper.h"
#include "advanced_tap_detection_helper.h"
#include "ws2812_helper.h"
#include "tap_hint.h"

#include "fft_spectrum.h"
#include "water.h"
#include "snake.h"
#include "matrix_rain.h"
#include "oscilloscope.h"

#define TIME_STEP_US 4000
#define EXTRA_ACC_SAMPLING_TIME 2000

enum states {
    MATRIX_RAIN,
    WATER_HORIZON,
    WATER_PENDULUM,
    SNAKE_3D,
    FFT,
    OSCILLOSCOPE,
    NUM_STATES
};

void init_state(int state);
void release_state(int state);

int main() {
    stdio_init_all();

    multicore_launch_core1(core1_entry);

    sleep_ms(500); // wait for mpu to calibrate

    ws2812_init();

    uint64_t previous_time_us = 0;

    int state = MATRIX_RAIN;
    init_state(state);

    mpu_data_t mpu_data;
    for (int i = 0; i < 3; i++) {
        mpu_data.acc[i] = 0.f;
        mpu_data.gyro[i] = 0.f;
    }

    while (1) {
        tap_t tap;
        if(queue_try_remove(&tap_queue, &tap)) {
            if (tap.double_tap) {
                release_state(state);
                state = (state + 1) % NUM_STATES;
                printf("state: %d\n", state);
                init_state(state);
            } else if (state == OSCILLOSCOPE) {
                oscilloscope_change_sampling_frequency_index(1);
            }
        }

        
        for (int i = 0; i < MPU_DATA_QUEUE_LEN; i++) (queue_try_remove(&mpu_data_queue, &mpu_data)); // make sure that the queue doesn't fill up and introduce latency

        /* Each mode has access to the ws2812 interface, and writes to the pixel arrays as needed.
        The actual LEDs are updated with ws2812_display_screens(). */

        switch (state) {
            case FFT:
                fft_spectrum_update();
                break;
            case SNAKE_3D:
                snake_update(mpu_data.gyro);
                break;
            case MATRIX_RAIN:
                matrix_rain_update();
                break;
            case WATER_PENDULUM:
                water_pendulum_update(mpu_data.acc);
                break;
            case WATER_HORIZON:
                water_horizon_update(mpu_data.acc);
                break;
            case OSCILLOSCOPE:
                oscilloscope_update();
                break;
            default:
                break;
        }

        ws2812_display_screens();

        while (time_us_64() - previous_time_us < TIME_STEP_US);
        previous_time_us = time_us_64();
    }
}

void init_state(int state) {
    switch (state) {
        case FFT:
            fft_spectrum_init();
            break;
        case SNAKE_3D:
            snake_init();
            break;
        case MATRIX_RAIN:
            matrix_rain_init();
            break;
        case WATER_PENDULUM:
            water_pendulum_init();
            break;
        case WATER_HORIZON:
            water_horizon_init();
            break;
        case OSCILLOSCOPE:
            oscilloscope_init();
            break;
        default:
            break;
    }
}

void release_state(int state) {
    switch (state) {
        case FFT:
            fft_spectrum_release();
            break;
        case SNAKE_3D:
            snake_release();
            break;
        case MATRIX_RAIN:
            matrix_rain_release();
            break;
        case WATER_PENDULUM:
            water_pendulum_release();
            break;
        case WATER_HORIZON:
            water_horizon_release();
            break;
        case OSCILLOSCOPE:
            oscilloscope_release();
            break;
        default:
            break;
    }
}
