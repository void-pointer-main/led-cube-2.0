#include "snake.h"
#include "ws2812_helper.h"
#include "mpu6050_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_SNAKE_LEN (NUM_PIXELS*NUM_SCREENS)
#define SNAKE_LOOP_FREQUENCY_DIVIDER 125 // loop time is going to be 4 ms, 125 times slower gives 0.5 s

#define TWIST_DETECTION_THRESHOLD 300 // degrees per second
#define TWIST_REJECTION_TIME_US (300*1000)

#define BLOCK_SIZE 2 // only 1 or two makes sense
#define HALF_ROW_MAX (NUM_ROWS/BLOCK_SIZE-1)
#define HALF_COL_MAX (NUM_COLS/BLOCK_SIZE-1)

typedef struct {
        uint screen_num;
        int half_row;
        int half_col;
} snake_segment_t;

snake_segment_t snake_segment[MAX_SNAKE_LEN];
snake_segment_t fruit;
uint snake_length;
rgb_t snake_color;
rgb_t snake_head_color;
rgb_t fruit_color;

int snake_dir[2];
int snake_dir_previous[2];
int snake_dir_update[2];

bool gyro_direction_evaluation(float gyro[3], uint screen_num, int new_snake_dir[2], int old_snake_dir[2]);
void write_square_block(snake_segment_t *seg, rgb_t color, size_t size);

// a 'get next snake head position' function would be useful, in hindsight.

void snake_init() {
    srand(time_us_32());
    snake_color = rgb2rgb_t_f(5, 7, 2);
    snake_head_color = rgb2rgb_t_f(10, 30, 10);
    fruit_color = rgb2rgb_t_f(55, 5, 3);
    snake_length = 2;
    snake_segment[0] = (snake_segment_t){TOP, 2, 1};
    snake_segment[1] = (snake_segment_t){TOP, 3, 1};
    snake_dir_update[0] = snake_dir[0] = 1;
    snake_dir_update[1] = snake_dir[1] = 0;

    // random fruit
    fruit.screen_num = TOP;
    fruit.half_row = 1;
    fruit.half_col = 0;

    for (int k = 0; k < NUM_SCREENS; k++) {
        ws2812_blank_screen(k);
    }
    write_square_block(&(snake_segment[0]), snake_color, BLOCK_SIZE);
    write_square_block(&(snake_segment[1]), snake_color, BLOCK_SIZE);

    write_square_block(&fruit, fruit_color, BLOCK_SIZE);
}

void snake_release() {}

void snake_update(float gyro[3]) {
    static int counter = 0;
    bool dead = false;
    static uint64_t death_screen_start_time_us;
    uint64_t current_time_us = time_us_64();

    /* input state machine */
    if (gyro_direction_evaluation(gyro, snake_segment[0].screen_num, snake_dir_update, NULL)) {
        if (snake_dir_update[0] != -snake_dir_previous[0] && snake_dir_update[1] != -snake_dir_previous[1]) {
            snake_dir[0] = snake_dir_update[0];
            snake_dir[1] = snake_dir_update[1];
        }
    }

    if (counter++ < SNAKE_LOOP_FREQUENCY_DIVIDER) {
        return;
    }
    counter = 0;

    snake_dir_previous[0] = snake_dir[0];
    snake_dir_previous[1] = snake_dir[1];

    /* 1) fruit detection */

    if (snake_segment[0].screen_num == fruit.screen_num
        && snake_segment[0].half_row == fruit.half_row
        && snake_segment[0].half_col == fruit.half_col) {
        snake_length += 1; // we do not have to initialise additional segment, it is handled by the position update
        // generate next fruit
        int r1 = rand();
        int r2 = rand();
        int r3 = rand();
        for (int k = 0; k < NUM_SCREENS; k++) {
            fruit.screen_num = (r1+k) % NUM_SCREENS;
            for (int r = 0; r < HALF_ROW_MAX; r++) {
                fruit.half_row = (r2+r) % HALF_ROW_MAX;
                for (int c = 0; c < HALF_COL_MAX; c++) {
                    fruit.half_col = (r3+c) % HALF_COL_MAX;
                    bool fruit_under_snake = false;
                    for (int s = 0; s < snake_length; s++) {
                        if (fruit.screen_num == snake_segment[s].screen_num
                            && fruit.half_row == snake_segment[s].half_row
                            && fruit.half_col == snake_segment[s].half_col) {
                            fruit_under_snake = true;
                            break;
                        }
                    }
                    if (!fruit_under_snake) {
                        goto exit_loops;
                    }
                }
            }
        }
        exit_loops:;
        // printf("fruit: s%d, r%d, c%d\n", fruit.screen_num, fruit.half_row, fruit.half_col);
    }

    /* 2) position update */

    // for erasing the trailing end of the snake
    snake_segment_t prev_snake_end = snake_segment[snake_length-1];

    // tail logic
    for (int i = snake_length-1; i > 0; i--) {
        snake_segment[i] = snake_segment[i-1];
    }

    snake_segment[0].half_row += snake_dir[0];
    snake_segment[0].half_col += snake_dir[1];

    if (snake_segment[0].half_row < 0) {
        switch(snake_segment[0].screen_num) {
            case FRONT:
                snake_segment[0].screen_num = TOP;
                snake_segment[0].half_row = HALF_ROW_MAX;
                // no change in half_col
                // no change in dir
                break;
            case BACK:
                snake_segment[0].screen_num = TOP;
                snake_segment[0].half_row = 0;
                snake_segment[0].half_col = HALF_COL_MAX - snake_segment[0].half_col;
                snake_dir[0] = 1;
                snake_dir_previous[0] = 1;
                break;
            case LEFT:
                snake_segment[0].screen_num = TOP;
                snake_segment[0].half_row = snake_segment[0].half_col;
                snake_segment[0].half_col = 0;
                snake_dir[0] = 0;
                snake_dir[1] = 1;
                snake_dir_previous[0] = 0;
                snake_dir_previous[1] = 1;
                break;
            case RIGHT:
                snake_segment[0].screen_num = TOP;
                snake_segment[0].half_row = HALF_COL_MAX - snake_segment[0].half_col;
                snake_segment[0].half_col = HALF_COL_MAX;
                snake_dir[0] = 0;
                snake_dir[1] = -1;
                snake_dir_previous[0] = 0;
                snake_dir_previous[1] = -1;
                break;
            case TOP:
                snake_segment[0].screen_num = BACK;
                snake_segment[0].half_row = 0;
                snake_segment[0].half_col = HALF_COL_MAX - snake_segment[0].half_col;
                snake_dir[0] = 1;
                snake_dir_previous[0] = 1;
                break;
            case BOTTOM:
                snake_segment[0].screen_num = FRONT;
                snake_segment[0].half_row = HALF_ROW_MAX;
                snake_segment[0].half_col = snake_segment[0].half_col;
                // no change in dir
                break;
        }
    } else if (snake_segment[0].half_row > HALF_ROW_MAX) {
        switch(snake_segment[0].screen_num) {
            case FRONT:
                snake_segment[0].screen_num = BOTTOM;
                snake_segment[0].half_row = 0;
                // no change in half_col
                // no change in dir
                break;
            case BACK:
                snake_segment[0].screen_num = BOTTOM;
                snake_segment[0].half_row = HALF_ROW_MAX;
                snake_segment[0].half_col = HALF_COL_MAX - snake_segment[0].half_col;
                snake_dir[0] = -1;
                snake_dir_previous[0] = -1;
                break;
            case LEFT:
                snake_segment[0].screen_num = BOTTOM;
                snake_segment[0].half_row = HALF_COL_MAX - snake_segment[0].half_col;
                snake_segment[0].half_col = 0;
                snake_dir[0] = 0;
                snake_dir[1] = 1;
                snake_dir_previous[0] = 0;
                snake_dir_previous[1] = 1;
                break;
            case RIGHT:
                snake_segment[0].screen_num = BOTTOM;
                snake_segment[0].half_row = snake_segment[0].half_col;
                snake_segment[0].half_col = HALF_COL_MAX;
                snake_dir[0] = 0;
                snake_dir[1] = -1;
                snake_dir_previous[0] = 0;
                snake_dir_previous[1] = -1;
                break;
            case TOP:
                snake_segment[0].screen_num = FRONT;
                snake_segment[0].half_row = 0;
                // no change in half_col
                // no change in dir
                break;
            case BOTTOM:
                snake_segment[0].screen_num = BACK;
                snake_segment[0].half_row = HALF_ROW_MAX;
                snake_segment[0].half_col = HALF_COL_MAX - snake_segment[0].half_col;
                snake_dir[0] = -1;
                snake_dir_previous[0] = -1;
                break;
        }
    }
    if (snake_segment[0].half_col < 0) {
        switch(snake_segment[0].screen_num) {
            case FRONT:
                snake_segment[0].screen_num = LEFT;
                // no change in half_row
                snake_segment[0].half_col = HALF_COL_MAX;
                // no change in dir
                break;
            case BACK:
                snake_segment[0].screen_num = RIGHT;
                // no change in half_row
                snake_segment[0].half_col = HALF_COL_MAX;
                // no change in dir
                break;
            case LEFT:
                snake_segment[0].screen_num = BACK;
                // no change in half_row
                snake_segment[0].half_col = HALF_COL_MAX;
                // no change in dir
                break;
            case RIGHT:
                snake_segment[0].screen_num = FRONT;
                // no change in half_row
                snake_segment[0].half_col = HALF_COL_MAX;
                // no change in dir
                break;
            case TOP:
                snake_segment[0].screen_num = LEFT;
                snake_segment[0].half_col = snake_segment[0].half_row;
                snake_segment[0].half_row = 0;
                snake_dir[0] = 1;
                snake_dir[1] = 0;
                snake_dir_previous[0] = 1;
                snake_dir_previous[1] = 0;
                break;
            case BOTTOM:
                snake_segment[0].screen_num = LEFT;
                snake_segment[0].half_col = HALF_ROW_MAX - snake_segment[0].half_row;
                snake_segment[0].half_row = HALF_ROW_MAX;
                snake_dir[0] = -1;
                snake_dir[1] = 0;
                snake_dir_previous[0] = -1;
                snake_dir_previous[1] = 0;
                break;
        }
    } else if (snake_segment[0].half_col > HALF_COL_MAX) {
        switch(snake_segment[0].screen_num) {
            case FRONT:
                snake_segment[0].screen_num = RIGHT;
                // no change in half_row
                snake_segment[0].half_col = 0;
                // no change in dir
                break;
            case BACK:
                snake_segment[0].screen_num = LEFT;
                // no change in half_row
                snake_segment[0].half_col = 0;
                // no change in dir
                break;
            case LEFT:
                snake_segment[0].screen_num = FRONT;
                // no change in half_row
                snake_segment[0].half_col = 0;
                // no change in dir
                break;
            case RIGHT:
                snake_segment[0].screen_num = BACK;
                // no change in half_row
                snake_segment[0].half_col = 0;
                // no change in dir
                break;
            case TOP:
                snake_segment[0].screen_num = RIGHT;
                snake_segment[0].half_col = HALF_ROW_MAX - snake_segment[0].half_row;
                snake_segment[0].half_row = 0;
                snake_dir[0] = 1;
                snake_dir[1] = 0;
                snake_dir_previous[0] = 1;
                snake_dir_previous[1] = 0;
                break;
            case BOTTOM:
                snake_segment[0].screen_num = RIGHT;
                snake_segment[0].half_col = snake_segment[0].half_row;
                snake_segment[0].half_row = HALF_ROW_MAX;
                snake_dir[0] = -1;
                snake_dir[1] = 0;
                snake_dir_previous[0] = -1;
                snake_dir_previous[1] = 0;
                break;
        }
    }

    /* 3) collision detection */


    for (int s = 2; s < snake_length; s++) {
        if (snake_segment[0].screen_num == snake_segment[s].screen_num
            && snake_segment[0].half_row == snake_segment[s].half_row
            && snake_segment[0].half_col == snake_segment[s].half_col) {
            // game over
            snake_init();
            return;
        }
    }

    /* 4) screen write */

    for (int k = 0; k < NUM_SCREENS; k++) {
        ws2812_blank_screen(k);
    }

    // we do not have to erase fruit
    write_square_block(&fruit, fruit_color, BLOCK_SIZE);
    
    // snake
    write_square_block(&(snake_segment[0]), snake_head_color, BLOCK_SIZE);
    for (int s = 1; s < snake_length; s++) {
        write_square_block(&(snake_segment[s]), snake_color, BLOCK_SIZE);
    }
}

bool gyro_direction_evaluation(float gyro[3], uint screen_num, int new_snake_dir[2], int old_snake_dir[2]) {
    int half_row_axis;
    int half_col_axis;
    int half_row_axis_sign;
    int half_col_axis_sign;

    static uint64_t previous_twist_time_us = 0;
    uint64_t current_time_us = time_us_64();

    if (current_time_us - previous_twist_time_us < TWIST_REJECTION_TIME_US) {
        return false;
    }

    switch (screen_num) {
        case FRONT:
            half_row_axis = AXIS_Y;
            half_row_axis_sign = 1;
            half_col_axis = AXIS_Z;
            half_col_axis_sign = 1;
            break;
        case BACK:
            half_row_axis = AXIS_Y;
            half_row_axis_sign = -1;
            half_col_axis = AXIS_Z;
            half_col_axis_sign = 1;
            break;
        case LEFT:
            half_row_axis = AXIS_X;
            half_row_axis_sign = 1;
            half_col_axis = AXIS_Z;
            half_col_axis_sign = 1;
            break;
        case RIGHT:
            half_row_axis = AXIS_X;
            half_row_axis_sign = -1;
            half_col_axis = AXIS_Z;
            half_col_axis_sign = 1;
            break;
        case TOP:
            half_row_axis = AXIS_Y;
            half_row_axis_sign = 1;
            half_col_axis = AXIS_X;
            half_col_axis_sign = -1;
            break;
        default:
        case BOTTOM:
            half_row_axis = AXIS_Y;
            half_row_axis_sign = 1;
            half_col_axis = AXIS_X;
            half_col_axis_sign = 1;
            break;
    }

    if (fabsf(gyro[half_row_axis]) >= TWIST_DETECTION_THRESHOLD || fabsf(gyro[half_col_axis]) >= TWIST_DETECTION_THRESHOLD) {
        if (fabsf(gyro[half_col_axis]) > fabsf(gyro[half_row_axis])) {
            new_snake_dir[0] = 0;
            new_snake_dir[1] = gyro[half_col_axis] > 0 ? half_col_axis_sign : -half_col_axis_sign;
        } else {
            new_snake_dir[0] = gyro[half_row_axis] > 0 ? half_row_axis_sign : -half_row_axis_sign;
            new_snake_dir[1] = 0;
        }

        previous_twist_time_us = current_time_us; // registering the previous twist timestamp here is a conscious choice.
        return true;
    }

    return false;
}

void write_square_block(snake_segment_t *seg, rgb_t color, size_t size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            ws2812_write_screen_pixel(seg->screen_num, 2*(seg->half_row)+i, 2*(seg->half_col)+j, color);
        }
    }
}
