# Aeropendulum Feasibility Analysis

This document provides a feasibility analysis for building an aeropendulum based on the components found in the project's `.docs` directory. An aeropendulum is a pendulum actuated by a propeller, often used as an educational testbed for embedded systems and control theory (e.g., PID or LQR control).

## Overview of Components

The following components were identified:

1.  **MCU**: STM32F401RE (Nucleo-64 MB1136)
2.  **IMU**: MPU6050
3.  **Motor**: SpeedyBee 1404 V3 4600KV
4.  **Propeller**: HQProp T3.5x2x3GR (3.5 inch, 2 inch pitch, tri-blade)
5.  **ESC**: LANRC 45A BLHeli (2S-6S)

---

## Component Analysis & Feasibility

### 1. MCU: STM32F401RE (Nucleo-64)
*   **Feasibility: Excellent**
*   **Analysis:** This ARM Cortex-M4 microcontroller running at 84 MHz with a floating-point unit (FPU) is perfectly suited for control systems. It has ample hardware timers for generating precise PWM or DShot signals to control the ESC, and it provides hardware I2C/SPI for fast communication with the IMU. The FPU will significantly accelerate floating-point math required for IMU filtering (e.g., complementary or Kalman filter) and PID control calculations.

### 2. IMU: MPU6050
*   **Feasibility: Good (with caveats)**
*   **Analysis:** A very standard and accessible 6-DOF IMU (accelerometer + gyroscope). It is fully capable of providing the necessary orientation data (roll/pitch) to determine the pendulum's angle. 
*   **Caveats:** As noted in the `imu_data_processing.md` document, the MPU6050 is highly sensitive to high-frequency vibrations, which can cause rectification errors (DC offset). Since the sensor will be mounted on the same structure as a spinning motor and propeller, **mechanical vibration dampening (e.g., foam tape, rubber grommets) is absolutely critical**. Additionally, moving the IMU filtering to run directly on the STM32 (rather than the host PC) will reduce latency and improve control loop performance.

### 3. Motor & Propeller
*   **Motor:** SpeedyBee 1404 V3 4600KV
*   **Propeller:** HQProp T3.5x2x3GR
*   **Feasibility: Very Good**
*   **Analysis:** This motor/propeller combination is typical for sub-250g micro-drones. A 1404 motor spinning a 3.5" propeller at 4600KV will generate substantial thrust on a 2S or 3S LiPo battery (likely in the range of 200g-400g of thrust). This is more than enough force to swing a lightweight pendulum arm. The fast response time of a small brushless motor is also beneficial for tight control loops.

### 4. ESC: LANRC 45A BLHeli (2S-6S)
*   **Feasibility: Feasible (Over-specced)**
*   **Analysis:** BLHeli firmware allows for modern, fast digital protocols like DShot, which the STM32 can easily generate, avoiding the need to calibrate analog PWM endpoints. It supports 2S to 6S batteries.
*   **Caveat:** A 45A ESC is massively over-specced for a 1404 motor, which will realistically draw around 10A-15A at peak load. While it is larger and heavier than necessary, weight is less of a concern on a stationary aeropendulum base compared to a free-flying drone, so this will work perfectly fine.

---

## Missing Components & Considerations

To complete the build, you will also need to consider the following:

1.  **Power Supply:** Since you want to avoid LiPo batteries and need a cheap wall-plug solution, here are the best options that can deliver the required ~10A+ peak current at a suitable voltage (12V is ideal, mimicking a 3S LiPo):
    *   **Repurposed ATX PC Power Supply (Recommended):** You can often find old desktop PC power supplies for free or very cheap. They have a 12V rail (yellow wires) capable of delivering massive current (20A+). You just need to short the green wire to a black ground wire to turn it on.
    *   **12V LED Strip Power Supply:** Metal cage-style or brick-style power supplies rated for 12V at 10A or 15A are widely available online for $10–$20.
    *   **Used Xbox 360/Xbox One Power Brick:** These output 12V at high current (often up to 14A–16A) and can be found very cheaply at used game stores or thrift shops.
    *(Note: Avoid old laptop chargers. While they are usually 19V, they often only supply 3A-4.5A, and a fast motor spin-up could trip their overcurrent protection.)*
2.  **Voltage Regulation:** The Nucleo board and MPU6050 require 3.3V/5V logic power. You will need a way to step down the motor voltage (e.g., using a BEC/buck converter) or power the Nucleo board separately via USB.
3.  **Mechanical Structure:** You'll need a low-friction pivot/hinge for the pendulum, a rigid arm to mount the motor/prop, and a sturdy base to prevent the entire assembly from tipping over when the motor thrusts.
4.  **Slip Ring (Optional):** If you plan for the pendulum to make full 360-degree rotations, you will need a slip ring to route power and signals to the motor/IMU without twisting the wires.

## Conclusion

The project is **highly feasible**. The selected components are extremely capable of building a high-performance aeropendulum. The main technical challenge will be tuning the control loop and mitigating the mechanical vibrations from the motor so they do not overwhelm the MPU6050 accelerometer readings.
