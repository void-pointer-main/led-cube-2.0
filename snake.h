#ifndef SNAKE_H
#define SNAKE_H

#include "pico/stdlib.h"

void snake_init();
void snake_release();
void snake_update(float gyro[3]);

#endif
