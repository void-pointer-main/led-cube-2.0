# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(LED_CUBE C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(LED_CUBE LED_CUBE.c
    my_utils.c
    mpu6050_helper.c
    tap_detection_helper.c
    kiss_fft.c
    kiss_fftr.c
    adc_dma_sampler_helper.c
    fft_spectrum.c
    ws2812_helper.c
    water.c
    snake.c
    matrix_rain.c
    oscilloscope.c
    tap_hint.c
    advanced_tap_detection_helper.c
)

pico_set_program_name(LED_CUBE "LED_CUBE")
pico_set_program_version(LED_CUBE "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(LED_CUBE 0)
pico_enable_stdio_usb(LED_CUBE 1)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(LED_CUBE ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Add the standard library to the build
target_link_libraries(LED_CUBE
    pico_stdlib
    pico_malloc
    hardware_i2c
    hardware_adc
    hardware_dma
    hardware_clocks
    hardware_pio
    pico_util
    pico_multicore
    )

# Add the standard include files to the build
target_include_directories(LED_CUBE PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(LED_CUBE)

