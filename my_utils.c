#include "my_utils.h"

#include <string.h>
#include <math.h>

void shift_into_buffer(void *buffer, size_t len, size_t element_size, void *new_element) {
    char *buf = (char *)buffer;

    memmove(buf + element_size, buf, (len-1) * element_size);
    memcpy(buf, new_element, element_size);
}

float dot_product(float vec1[3], float vec2[3]) {
    return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2]; 
}

float magnitude(float vec[3]) {
    return sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
}

void normalize(float vec[3]) {
    float l = sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    scalar_mult(vec, 1/magnitude(vec));
}

void scalar_mult(float vec[3], float scalar) {
    for (int i = 0; i < 3; i++) {
        vec[i] *= scalar;
    }
}
