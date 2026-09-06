#include "water.h"
#include "ws2812_helper.h"
#include "my_utils.h"

#define TIMESTEP 0.004f // seconds

// arbitrary constants tuned to make the result look good
#define MASS .2f
#define DAMPING_COEFF 0.6
#define PENDULUM_LENGTH 1.f
#define GRAVITY 30.f

#define PEDNULUM_BLUE_INTENSITY 40

float pendulum_position[3];
float pendulum_velocity[3];
float pendulum_acceleration[3];

#define HORIZON_RED_INTENSITY 10
#define HORIZON_GREEN_INTENSITY 1
#define HORIZON_BLUE_INTENSITY 10

#define ACC_EXP_FILTER_COEF 0.2
float acc_exp_filtered[3] = {0};

void water_pendulum_init() {
    for (int i = 0; i < 3; i++) {
        pendulum_position[i] = 0;
        pendulum_velocity[i] = 0;
        pendulum_acceleration[i] = 0;
    }
    pendulum_position[1] = DAMPING_COEFF;
}

void water_pendulum_release() {}

void water_pendulum_update(float acc[3]) {
    float gravity[3];

    for (int i = 0; i < 3; i++) {
        gravity[i] = acc[i];
    }
    scalar_mult(gravity, GRAVITY);

    // coefficiton for constraint force, derived from constraints of the system (pendulum of fixed length)
    float lambda = (dot_product(pendulum_velocity, pendulum_velocity) - dot_product(pendulum_position, gravity))/(PENDULUM_LENGTH*PENDULUM_LENGTH);

    // euler integration
    for (int i = 0; i < 3; i++) {
        pendulum_acceleration[i] = gravity[i] - DAMPING_COEFF/MASS*pendulum_velocity[i] + lambda/MASS*pendulum_position[i];
        pendulum_velocity[i] += pendulum_acceleration[i] * TIMESTEP;
        pendulum_position[i] += pendulum_velocity[i] * TIMESTEP;
    }

    // prevent position drifting away
    normalize(pendulum_position);
    scalar_mult(pendulum_position, PENDULUM_LENGTH);

    // make sure velocity stays tangent to position
    float e = dot_product(pendulum_position, pendulum_velocity);
    for (int i = 0; i < 3; i++) {
        pendulum_velocity[i] -= e/(PENDULUM_LENGTH*PENDULUM_LENGTH) * pendulum_position[i];
    }

    // printf("%.2f, %.2f, %.2f\n", pendulum_position[0], pendulum_position[1], pendulum_position[2]);

    // screen projections
    for (int k = 0; k < NUM_SCREENS; k++) {
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                float dp = dot_product(pendulum_position, screens[k].pixel_vectors[r][c]);

                // anti-aliasing the edge
                uint8_t pixel_blue_intensity = 0;
                if (-dp > 0) {
                    if (-dp < 1) {
                        pixel_blue_intensity = (uint8_t)(PEDNULUM_BLUE_INTENSITY*(-dp));
                    } else {
                        pixel_blue_intensity = PEDNULUM_BLUE_INTENSITY;
                    }
                }
                
                ws2812_write_screen_pixel(k, r, c,  rgb2rgb_t_f(0, 0, pixel_blue_intensity));
            }
        }
    }
}

void water_horizon_init() {
    for (int i = 0; i < 3; i++) {
        acc_exp_filtered[i] = 0;
    }
}
void water_horizon_release() {}

void water_horizon_update(float acc[3]) {
    for (int i = 0; i < 3; i++) {
        acc_exp_filtered[i] = acc[i] * ACC_EXP_FILTER_COEF + acc_exp_filtered[i] * (1-ACC_EXP_FILTER_COEF);
    }

    for (int k = 0; k < NUM_SCREENS; k++) {
        for (int r = 0; r < NUM_ROWS; r++) {
            for (int c = 0; c < NUM_COLS; c++) {
                float dp = dot_product(acc_exp_filtered, screens[k].pixel_vectors[r][c]);

                // // anti-aliasing the edge
                uint8_t pixel_red_intensity = 0;
                uint8_t pixel_green_intensity = 0;
                uint8_t pixel_blue_intensity = 0;
                if (-dp > 0.f) {
                    pixel_red_intensity = HORIZON_RED_INTENSITY;
                    pixel_green_intensity = HORIZON_GREEN_INTENSITY;
                } else {
                    pixel_blue_intensity = HORIZON_BLUE_INTENSITY;
                }

                ws2812_write_screen_pixel(k, r, c,  rgb2rgb_t_f(pixel_red_intensity, pixel_green_intensity, pixel_blue_intensity));
            }
        }
    }
}
