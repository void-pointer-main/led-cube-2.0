#ifndef ADVANCED_TAP_DETECTION_HELPER_H
#define ADVANCED_TAP_DETECTION_HELPER_H

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#define TAP_QUEUE_LEN 3
#define MPU_DATA_QUEUE_LEN 3

typedef enum {
    SINGLE,
    DOUBLE
} tap_type_t;

typedef struct {
    bool double_tap;
} tap_t;

typedef struct {
    float acc[3];
    float gyro[3];
} mpu_data_t;

extern queue_t tap_queue;
extern queue_t mpu_data_queue;

void core1_entry();

#endif