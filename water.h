#ifndef WATER_H
#define WATER_H

/* Simulate as a second order system */

void water_pendulum_init();
void water_pendulum_release();
void water_pendulum_update(float acc[3]);

/* Simply follow gravity */

void water_horizon_init();
void water_horizon_release();
void water_horizon_update(float acc[3]);

#endif
