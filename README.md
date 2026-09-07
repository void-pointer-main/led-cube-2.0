# Led Cube 2.0
Made for the subject 'Laboratory of Industrial Electronics and Sensors' at the CTU. Inspired by this [older project](https://maglab.fel.cvut.cz/workshop/led-cube/) made for the same subject.
The firmware is written in C, using the Pi Pico SDK.
It's somewhat amateurish, but fun to play with.

# Hardware
The cube is made out of 6 8x8 WS2812B panels. The MCU is a Pi Pico 2. The included sensors are an MPU6050 and a SPH8878BLR5H-1 MEMS microphone.

# Demo
Sorry for the bad whistling accompanying the FFT and oscilloscope demonstrations.

https://github.com/user-attachments/assets/ddfb163b-e75d-4161-8866-4ce88aee37da

# Modes
The cube has a couple of modes; some of them use the sensors to do stuff. Switching between modes is achieved by double tapping the cube.

### 1. Code rain
A simple but interesting screen saver. Guaranteed to improve programming skills of nearby persons.

### 2. Artificial horizon
Uses the accelerometer to determine the horizon. The colour of a pixel is determined by examining the sign of the scalar product of the gravity vector and the pixel's relative position vector. 

### 3. Water pendulum (my favourite)
Simulation of a fixed length pendulum in 3d, projected to the pixels to give the impression of a water level.

### 4. Snake3D
Implementation of the classic snake game on the surface of a cube. The player changes the direction of the snake by sharply twisting the cube. It's interesting, because you can't see the whole game space at once. This often leads to you getting trapped by yourself.

### 5. FFT
Uses the [KISSFFT](https://github.com/mborgerding/kissfft) library to calculate the FFT from the microphone data. The sampling frequency of the ADC is set to 8 KHz. In total 128 frequency bins are calculated, only the first 32 get displayed on the cube. The KISSFFT library is plenty fast for this application - calculating a 1024 bin FFT (using floats) takes under 0.7 ms on the RP2350.

### 6. Simple oscilloscope
Implementation of a simple oscilloscope to demonstrate the waveform of a sound. The trigger logic is very simple, so it only works with simple sine waves (generated, for example, by whistling).
Different sampling rates can be selected with single taps, indicated by different colours.


