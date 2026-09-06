#include "advanced_tap_detection_helper.h"

#include <stdio.h>
#include <math.h>
#include "my_utils.h"

#include "mpu6050_helper.h"

/* Tap detection */
#define REJECTION_TIME_US (100*1000)
#define DOUBLE_TAP_DETECTION_TIME_US (300*1000)
#define DOUBLE_TAP_MAX_COS_DIFF 0.6f
#define ACC_DIFF_THRESHOLD 0.5f
#define ACC_DIFF_BUF_LEN 5 // diff buffer will serve for catching inversions

#define SAMPLING_TIME_US 1000

queue_t tap_queue;
queue_t mpu_data_queue;

bool evaluate_data_for_taps(float acc[3], int *type);

void core1_entry() {

    queue_init(&tap_queue, sizeof(tap_t), TAP_QUEUE_LEN);
    queue_init(&mpu_data_queue, sizeof(mpu_data_t), MPU_DATA_QUEUE_LEN);

    printf("init mpu\n");
    sleep_ms(50);
    mpu6050_init();
    printf("worked\n");
    sleep_ms(50);
    {
        float acc_during_calibration[3];
        mpu6050_find_axis(acc_during_calibration);
        mpu6050_calibrate(acc_during_calibration);
    }

    uint64_t previous_time_us = 0;
    int loop_cnt = 0;

    float acc[3] = {0};
    float gyro_buf[4][3] = {0};
    
    while (1) {
        mpu6050_read_acc(acc);
        mpu6050_read_gyro(gyro_buf[loop_cnt]);

        int type;
        if (evaluate_data_for_taps(acc, &type)) {
            tap_t tap;
            tap.double_tap = type == DOUBLE ? true : false;
            queue_try_add(&tap_queue, &tap);
        }

        // other core loop runs at 1/4th the speed
        if (loop_cnt < 3) {
            loop_cnt++;
        } else {
            loop_cnt = 0;

            mpu_data_t mpu_data;
            for (int i = 0; i < 3; i++) {
                mpu_data.acc[i] = acc[i];
                // select the maximum angular velocity
                mpu_data.gyro[i] = 0.f;
                for (int j = 0; j < 4; j++) {
                    if (fabsf(mpu_data.gyro[i]) < fabsf(gyro_buf[j][i])) {
                        mpu_data.gyro[i] = gyro_buf[j][i];
                    }
                }
            }
            queue_try_add(&mpu_data_queue, &mpu_data);
        }

        while (time_us_64() - previous_time_us < SAMPLING_TIME_US);
        previous_time_us = time_us_64();
    }
}

bool evaluate_data_for_taps(float acc[3], int *type) {
    static uint64_t previous_time_us = 0;
    uint64_t current_time_us = time_us_64();

    static uint64_t previous_tap_time_us = 0;
    static bool waiting_for_next_tap = false;
    static float prev_diff_magnitude;

    static float previous_acc[3] = {0};
    
    float latest_diff[3] = {0};
    static float prev_tap_diff[3] = {0};

    for (int i = 0; i < 3; i++) {
        latest_diff[i] = acc[i] - previous_acc[i];
        previous_acc[i] = acc[i];
    }

    if (current_time_us - previous_tap_time_us < REJECTION_TIME_US) {
        return false;
    }

    if (waiting_for_next_tap && current_time_us - previous_tap_time_us > DOUBLE_TAP_DETECTION_TIME_US) {
        waiting_for_next_tap = false;
        // printf("Single_timeout, %llu\n",  current_time_us - previous_tap_time_us);
        return false;
    }

    float diff_magnitude = magnitude(latest_diff);

    if (diff_magnitude < ACC_DIFF_THRESHOLD) {
        return false;
    }    

    if (!waiting_for_next_tap) {
        // printf("Tap\n");
        waiting_for_next_tap = true;
        previous_tap_time_us = current_time_us;

        prev_diff_magnitude = diff_magnitude;
        for (int i = 0; i < 3; i++) {
            prev_tap_diff[i] = latest_diff[i];
        }

        *type = SINGLE; // every double tap will also produce a single tap in advance - this is a conscious choice, for reponsivity.
        return true;
    }

    waiting_for_next_tap = false;

    float dp = dot_product(latest_diff, prev_tap_diff);
    float cos_theta = dp/diff_magnitude/prev_diff_magnitude;

    printf("%.8f, %.8f\n", dp, cos_theta);

    if (cos_theta < DOUBLE_TAP_MAX_COS_DIFF) {
        return false;
    }

    *type = DOUBLE;
    return true;
}