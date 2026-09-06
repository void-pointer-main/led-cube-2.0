#ifndef TAP_DETECTION_HELPER_H
#define TAP_DETECTION_HELPER_H

#include "pico/stdlib.h"

#define ACC_DIFF_BUF_LEN 5 // diff buffer will serve for catching inversions

enum {
    POLARITY_POSITIVE,
    POLARITY_NEGATIVE
};

enum {
    TAP_SINGLE,
    TAP_DOUBLE
};

typedef struct{
    int polarity;
    int type;
} tap_t;

extern tap_t tap;

bool evaluate_acc_data_for_taps(float acc, tap_t *tap);
bool check_for_tap(float diff_buffer[ACC_DIFF_BUF_LEN], int *polarity);
int64_t extra_acc_sampling_callback(alarm_id_t id, void *data);

#endif