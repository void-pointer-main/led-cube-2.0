#ifndef MPU6050_HELPER_H
#define MPU6050_HEPLER_H

#include "pico/stdlib.h"

enum axis_enum {
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};

void mpu6050_init();
void mpu6050_reset();
void mpu6050_read_acc(float acc[3]);
void mpu6050_read_gyro(float gyro[3]);
void mpu6050_calibrate(float acc_expected_during_calibration[3]);
void mpu6050_find_axis(float expected_accs[3]);
// Not stupid-proof
void mpu6050_write_reg(uint8_t reg, uint8_t val);

#endif
