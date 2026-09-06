#include "tap_detection_helper.h"

#include <stdio.h>
#include "my_utils.h"

/* Tap detection */
#define REJECTION_TIME_US (70*1000)
#define DOUBLE_TAP_DETECTION_TIME_US (300*1000)
#define ACC_DIFF_THRESHOLD 0.5f
#define ACC_DIFF_THRESHOLD_INVERSION_CATCH 0.9f

tap_t tap;

bool evaluate_acc_data_for_taps(float acc, tap_t *tap) {
    static uint64_t previous_tap_time_us = 0;
    uint64_t current_time_us = time_us_64();

    static float previous_acc = 0;
    static float diff_buffer[ACC_DIFF_BUF_LEN];

    static bool tap_in_waiting = false;
    static int tap_in_waiting_polarity = POLARITY_POSITIVE;
    static uint64_t tap_count_start_time_us = 0;

    float latest_diff = acc - previous_acc;
    // printf("%.2f\n", latest_diff);
    previous_acc = acc;
    shift_into_buffer(diff_buffer, ACC_DIFF_BUF_LEN, sizeof(float), &latest_diff);

    if (current_time_us - previous_tap_time_us < REJECTION_TIME_US) {
        return false;
    }

    if (tap_in_waiting && current_time_us - previous_tap_time_us > DOUBLE_TAP_DETECTION_TIME_US) {
        tap_in_waiting = false;
        tap->type = TAP_SINGLE;
        tap->polarity = tap_in_waiting_polarity;
        return true;
    }

    int new_tap_polarity;
    if (check_for_tap(diff_buffer, &new_tap_polarity)) {
        if (!tap_in_waiting) {
            tap_in_waiting = true;
            tap_in_waiting_polarity = new_tap_polarity;
            previous_tap_time_us = current_time_us;
            return false;
        }
        // implicitly:
        // current_time_us - previous_tap_time_us <= DOUBLE_TAP_DETECTION_TIME_US
        // tap_in_waiting == true
        
        previous_tap_time_us = current_time_us;

        if (new_tap_polarity != tap_in_waiting_polarity) {
            tap_in_waiting = false; // I chose to reject in case of opposite taps
            return false;
        }
        // tap_in_waiting == true && new_tap_polarity == tap_in_waiting_polarity
        tap_in_waiting = false;
        tap->type = TAP_DOUBLE;
        tap->polarity = tap_in_waiting_polarity;
        return true;
    }

    return false;
}

bool check_for_tap(float diff_buffer[ACC_DIFF_BUF_LEN], int *polarity) {
    // there should not be any static variables in this function
    bool tap_detected = false;

    if (diff_buffer[0] >= ACC_DIFF_THRESHOLD) {
        *polarity = POLARITY_POSITIVE;
        for (int j = 0; j < ACC_DIFF_BUF_LEN; j++) {
            // printf("%.2f\n", diff_buffer[j]);
            if (diff_buffer[j] <= -ACC_DIFF_THRESHOLD_INVERSION_CATCH) {
                *polarity = POLARITY_NEGATIVE;
                break;
            }
        }
        // for (int j = 0; j < ACC_DIFF_BUF_LEN; j++) {
            // printf("%.2f\n", diff_buffer[j]);
        // }
        tap_detected = true;
    } else if (diff_buffer[0] <= -ACC_DIFF_THRESHOLD) {
        *polarity = POLARITY_NEGATIVE;
        for (int j = 0; j < ACC_DIFF_BUF_LEN; j++) {
            // printf("%.2f\n", diff_buffer[j]);
            if (diff_buffer[j] >= ACC_DIFF_THRESHOLD_INVERSION_CATCH) {
                *polarity = POLARITY_POSITIVE;
                break;
            }
        }
        // for (int j = 0; j < ACC_DIFF_BUF_LEN; j++) {
            // printf("%.2f\n", diff_buffer[j]);
        // }
        tap_detected = true;
    }

    return tap_detected;
}