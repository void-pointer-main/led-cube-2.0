#include "mpu6050_helper.h"

#include "hardware/i2c.h"
#include <stdio.h>
#include <math.h>

#define SCL_PIN 21 // PICO_DEFAULT_I2C_SCL_PIN
#define SDA_PIN 20 // PICO_DEFAULT_I2C_SDA_PIN
#define i2c_hardware i2c0 //i2c_default // i2c0, i2c1

#define ACC_RANGE_2G__LSB_PER_G 16384.0 // default on reset
#define ACC_RANGE_4G__LSB_PER_G 8192.0
#define ACC_RANGE_8G__LSB_PER_G 4096.0
#define ACC_RANGE_16G__LSB_PER_G 2048.0

#define GYRO_RANGE_250__LSB_PER_DEG_PER_S 131.0 // default on reset
#define GYRO_RANGE_500__LSB_PER_DEG_PER_S 65.5
#define GYRO_RANGE_1000__LSB_PER_DEG_PER_S 32.8
#define GYRO_RANGE_2000__LSB_PER_DEG_PER_S 16.4

#define POWER_MANAGEMENT_REG 107
#define DEVICE_RESET 128

#define GYRO_CONFIG_REG 27
// setting up the full scale
#define GYRO_FS_SEL 3

#define GYRO_SCALE_250_DEG_PER_S 0
#define GYRO_SCALE_500_DEG_PER_S 1
#define GYRO_SCALE_1000_DEG_PER_S 2
#define GYRO_SCALE_2000_DEG_PER_S 3

#define ACC_CONFIG_REG 28
// setting up the full scale
#define ACCEL_FS_SEL 3

#define ACC_SCALE_2G 0
#define ACC_SCALE_4G 1
#define ACC_SCALE_8G 2
#define ACC_SCALE_16G 3

#define ACC_CONFIG_2_REG 29 
// for setting up the DLPF
#define ACCEL_FCHOICE_B 3
#define A_DLPF_CFG 1

#define ACC_SCALE ACC_SCALE_2G
#define ACC_LSB_PER_G ACC_RANGE_2G__LSB_PER_G

#define GYRO_SCALE GYRO_SCALE_500_DEG_PER_S
#define GYRO_LSB_PER_DEG_PER_S GYRO_RANGE_500__LSB_PER_DEG_PER_S


#define CALIBRATION_SAMPLE_CNT 200

static int mpu_addr = 0x68;

float acc_offset[3] = {0};
float gyro_offset[3] = {0};

void axis_remap(float axis[3]);

void mpu6050_init() {
    i2c_init(i2c_hardware, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    mpu6050_reset();
}

void mpu6050_reset() {
    mpu6050_write_reg(POWER_MANAGEMENT_REG, DEVICE_RESET);
    sleep_ms(100);
    mpu6050_write_reg(POWER_MANAGEMENT_REG, 0x00); // waking from sleep // THIS NEEDS TO BE IN FRONT OF THE CONFIGS FOR SOME REASON I MESSED UP

    mpu6050_write_reg(GYRO_CONFIG_REG, GYRO_SCALE << GYRO_FS_SEL);

    mpu6050_write_reg(ACC_CONFIG_REG, ACC_SCALE << ACCEL_FS_SEL);

    // not using filtering, to capture tap spikes
    // mpu6050_write_reg(ACC_CONFIG_2_REG, ACCEL_FCHOICE_B | A_DLPF_CFG); 
    mpu6050_write_reg(ACC_CONFIG_2_REG, 0x00);

    sleep_ms(100);
}

void mpu6050_read_acc(float acc[3]) {
    uint8_t buffer[6];

    uint8_t reg = 0x3B;
    i2c_write_blocking(i2c_default, mpu_addr, &reg, 1, true);
    i2c_read_blocking(i2c_default, mpu_addr, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        acc[i] = (int16_t)(buffer[i * 2] << 8 | buffer[(i * 2) + 1]) / ACC_LSB_PER_G;
    }

    axis_remap(acc);

    for (int i = 0; i < 3; i++) {
        acc[i] -= acc_offset[i];
    }
}

void mpu6050_read_gyro(float gyro[3]) {
    uint8_t buffer[6];

    uint8_t val = 0x43;
    i2c_write_blocking(i2c_default, mpu_addr, &val, 1, true);
    i2c_read_blocking(i2c_default, mpu_addr, buffer, 6, false); 

    for (int i = 0; i < 3; i++) {
        gyro[i] = (int16_t)(buffer[i * 2] << 8 | buffer[(i * 2) + 1]) / GYRO_LSB_PER_DEG_PER_S;
    }
    
    axis_remap(gyro);

    for (int i = 0; i < 3; i++) {
        gyro[i] -= gyro_offset[i];
    }
}

void mpu6050_calibrate(float acc_expected_during_calibration[3]) {
    float acc[3] = {0}, gyro[3] = {0};
    float tmp_acc_offset[3] = {0};
    float tmp_gyro_offset[3] = {0};
    for (int i = 0; i < CALIBRATION_SAMPLE_CNT; i++) {
        for (int j = 0; j < 3; j++) {
            mpu6050_read_acc(acc);
            mpu6050_read_gyro(gyro);
            tmp_acc_offset[j] += acc[j] - acc_expected_during_calibration[j];
            tmp_gyro_offset[j] += gyro[j];  
        }

        sleep_ms(1);
    }

    for (int j = 0; j < 3; j++) {
        acc_offset[j] = tmp_acc_offset[j] / CALIBRATION_SAMPLE_CNT;
        gyro_offset[j] = tmp_gyro_offset[j] / CALIBRATION_SAMPLE_CNT;
    }
}

// finds axis that is 'down' for calibration purposes
void mpu6050_find_axis(float expected_accs[3]) {
    float max_value = 0.f;
    float acc[3];
    int axis;

    mpu6050_read_acc(acc);
    
    for (int j = 0; j < 3; j++) {
        expected_accs[j] = 0.f;
        if (fabsf(acc[j]) > max_value) {
            max_value = fabsf(acc[j]);
            axis = j;
        }
    }
    if (acc[axis] > 0.f) {
        expected_accs[axis] = 1.f;
    } else {
        expected_accs[axis] = -1.f;
    }
}

void mpu6050_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c_default, mpu_addr, buf, 2, false);
}

void axis_remap(float axis[3]) {
    /* X := -bZ, Y := -bX, Z := bY */
    float tmp = axis[0];
    axis[0] = -axis[2];
    axis[2] = axis[1];
    axis[1] = -tmp;
}
