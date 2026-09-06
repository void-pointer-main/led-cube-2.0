#ifndef MY_UTILS_H
#define MY_UTILS_H

#include "pico/stdlib.h"

void shift_into_buffer(void *buffer, size_t len, size_t element_size, void *new_element);
float dot_product(float vec1[3], float vec2[3]);
float magnitude(float vec[3]);
void normalize(float vec[3]);
void scalar_mult(float vec[3], float scalar);

#endif
