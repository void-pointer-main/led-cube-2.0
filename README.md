# Led Cube 2.0
Made for the subject 'Laboratory of Industrial Electronics and Sensors' at the CTU. Inspired by this [older project](https://maglab.fel.cvut.cz/workshop/led-cube/) made for the same subject.
It's somewhat amateurish, but the final toy is fun to play with.

# Hardware
The cube is made out of 6 8x8 WS2812B panels. The MCU is a Pi Pico 2. The included sensors are an MPU6050 and a SPH8878BLR5H-1 MEMS microphone.

# Modes
The cube has a couple of modes; some of them use the sensors to do stuff. Switching between modes is achieved by double tapping the cube.

1. 'Screensaver' (code rain from the matrix)
2. Artificial horizon
3. Water pendulum (basically the same as 2. but with some second order effects)
4. Snake3D
5. FFT applied to readings from the microphone sensor (using the [KISSFFT](https://github.com/mborgerding/kissfft) library)
6. Simple oscilloscope using the microphone sensor

# Demo
Sorry for the bad whistling accompanying the FFT and oscilloscope demonstrations.

https://github.com/user-attachments/assets/ddfb163b-e75d-4161-8866-4ce88aee37da


