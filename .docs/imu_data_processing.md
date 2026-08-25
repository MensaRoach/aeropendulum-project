# IMU Data Processing Pipeline

This document traces every transformation applied to the raw MPU6050 data — from the bytes arriving over USART to the roll, pitch, yaw, and telemetry values rendered on screen — and explains the mathematical and physical reasoning behind each step.

> [!IMPORTANT]
> **Status: reference, describing firmware that no longer exists.**
>
> Written before the MPU6050 driver was deleted for a from-scratch rewrite. Sections 1.1 and 1.2
> describe firmware behaviour in the present tense that is **not currently on the board** — the
> driver in `DRIVERS/` is an unimplemented scaffold. The host-side half (§1.3 onward) is still
> live: `tools/` implements exactly this, and the wire format described here is the contract a
> replacement driver has to match or else update `tools/` alongside it.
>
> The mathematics in §2–§9 is independent of the firmware and remains correct.
>
> **Spoiler warning.** [driver_development_plan.md](driver_development_plan.md) treats this file as
> a spoiler source for the IMU track: it gives away the device address, several register addresses
> including the DLPF register, the burst-read start register, and the scaling constants. If you are
> working through that plan, §1.1 and §2 answer questions you were meant to derive from the
> register map.

---

## 1. Raw Data Acquisition

### 1.1 On the MCU

The MPU6050 is an I²C slave at address `0x68` (7-bit) / `0xD0` (8-bit write).
The firmware performs a burst-read of **14 bytes** starting at register `0x3B` (`ACCEL_XOUT_H`):

| Byte offset | Register     | Content            |
|:-----------:|:-------------|:-------------------|
| 0–1         | `0x3B–0x3C`  | Accelerometer X    |
| 2–3         | `0x3D–0x3E`  | Accelerometer Y    |
| 4–5         | `0x3F–0x40`  | Accelerometer Z    |
| 6–7         | `0x41–0x42`  | Temperature        |
| 8–9         | `0x43–0x44`  | Gyroscope X        |
| 10–11       | `0x45–0x46`  | Gyroscope Y        |
| 12–13       | `0x47–0x48`  | Gyroscope Z        |

Each value is a **16-bit signed integer** (two's complement), transmitted **MSB first** by the MPU6050.

The firmware reassembles them manually:

```c
mpu_data.accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
```

### 1.2 Framing with COBS

The 14-byte `MPU6050_Data_t` struct is encoded using **Consistent Overhead Byte Stuffing (COBS)** before transmission over USART2. COBS eliminates all `0x00` bytes from the payload so that `0x00` can serve as an unambiguous **frame delimiter**.

The on-wire format is:

```
[COBS-encoded payload (≤ 15 bytes)] [0x00]
```

The overhead is at most 1 byte per 254 bytes of input; for our 14-byte payload the encoded output is 15 bytes worst-case, plus the trailing `0x00`.

### 1.3 On the Host

The Python tool accumulates serial bytes until it encounters `0x00`, then COBS-decodes the accumulated buffer and unpacks the result:

```python
decoded = cobs.decode(bytes(buffer))          # Remove COBS framing
values  = struct.unpack("<7h", decoded)       # 7 × int16, little-endian
# values = (ax, ay, az, temp, gx, gy, gz)
```

> [!NOTE]
> The MCU stores the struct in memory as little-endian (ARM Cortex-M4 is little-endian), so the host unpacks with `<` (little-endian). This is *not* the same as the MPU6050's MSB-first I²C register order — the firmware already swapped the byte order during the `(buffer[0] << 8) | buffer[1]` assembly step.

---

## 2. Scaling Raw Values to Physical Units

### 2.1 Accelerometer

The MPU6050's ADC output depends on the configured full-scale range.
We use **±2 g**, which gives a sensitivity of:

$$
S_a = 16384 \ \frac{\text{LSB}}{g}
$$

Conversion to gravitational units:

$$
a_x = \frac{\text{raw}_{a_x}}{16384} \quad [g]
$$

The same applies to $a_y$ and $a_z$. A value of `+16384` means +1 g (9.81 m/s²).

| Full-Scale Range | Sensitivity (LSB/g) |
|:----------------:|:-------------------:|
| ±2 g             | 16384               |
| ±4 g             | 8192                |
| ±8 g             | 4096                |
| ±16 g            | 2048                |

### 2.2 Gyroscope

We use **±250 °/s**, giving a sensitivity of:

$$
S_g = 131 \ \frac{\text{LSB}}{°/s}
$$

Conversion to degrees per second:

$$
\omega_x = \frac{\text{raw}_{g_x}}{131} \quad [°/s]
$$

| Full-Scale Range | Sensitivity (LSB/(°/s)) |
|:----------------:|:-----------------------:|
| ±250 °/s         | 131                     |
| ±500 °/s         | 65.5                    |
| ±1000 °/s        | 32.8                    |
| ±2000 °/s        | 16.4                    |

### 2.3 Temperature

The MPU6050's on-die temperature sensor has a fixed formula from the datasheet (register `0x41–0x42`):

$$
T_{°C} = \frac{\text{raw}_{\text{temp}}}{340} + 36.53
$$

This measures the **die temperature**, not the ambient temperature. It is useful for temperature-compensated calibration but not as an environment thermometer.

---

## 3. Orientation from Accelerometer (Static Angles)

### 3.1 Physical Principle

When the sensor is **stationary** (or moving at constant velocity), the only force measured by the accelerometer is gravity. The gravity vector in the sensor's body frame directly encodes the sensor's tilt relative to Earth's vertical.

If the gravity vector in the body frame is $(a_x, a_y, a_z)$ in units of *g*, then for a level sensor at rest: $(a_x, a_y, a_z) = (0, 0, 1)$.

### 3.2 Roll (rotation about X-axis)

Roll is the angle of the gravity vector projected onto the **Y–Z plane**:

$$
\phi_{\text{accel}} = \text{atan2}(a_y, \ a_z)
$$

```
        Z (up)
        ^
        |  /  ← gravity projected onto Y-Z
        | / φ (roll angle)
        |/____> Y
```

When the sensor tilts around its X-axis, $a_y$ and $a_z$ change while $a_x$ stays near zero. The `atan2` function correctly handles all four quadrants and the singularity at $a_z = 0$.

### 3.3 Pitch (rotation about Y-axis)

Pitch is the angle of the gravity vector projected onto the **X–Z plane**, but normalized to avoid coupling with roll:

$$
\theta_{\text{accel}} = \text{atan2}\!\left(-a_x, \ \sqrt{a_y^2 + a_z^2}\right)
$$

The denominator $\sqrt{a_y^2 + a_z^2}$ is the magnitude of the gravity component orthogonal to X. Using this instead of just $a_z$ makes the pitch estimate independent of roll angle, avoiding gimbal-lock-like artifacts at large roll values.

### 3.4 Why No Yaw?

Yaw is rotation about the **vertical axis** (aligned with gravity). Since gravity is parallel to this axis, rotating around it does not change the accelerometer reading at all. Therefore:

> **An accelerometer alone cannot determine yaw.** It is fundamentally unobservable from gravity measurements.

### 3.5 Limitations of Accelerometer-Only Angles

- **Vibration and linear acceleration** corrupt the measurement. Any non-gravitational acceleration (motor thrust, movement) adds to the reading and distorts the estimated tilt.
- **Noisy at high frequency.** The accelerometer signal has broadband noise, making the angle estimate jittery.
- **Correct on average.** Over time, the accelerometer gives an absolute reference for tilt because gravity is constant. It does not drift.

---

## 4. Orientation from Gyroscope (Integration)

### 4.1 Physical Principle

A gyroscope measures **angular velocity** $\omega$ (°/s). To obtain an angle, we integrate over time:

$$
\phi_{\text{gyro}}(t) = \phi_{\text{gyro}}(t - \Delta t) + \omega_x \cdot \Delta t
$$

$$
\theta_{\text{gyro}}(t) = \theta_{\text{gyro}}(t - \Delta t) + \omega_y \cdot \Delta t
$$

$$
\psi_{\text{gyro}}(t) = \psi_{\text{gyro}}(t - \Delta t) + \omega_z \cdot \Delta t
$$

where $\Delta t$ is the time elapsed since the previous sample. In the tool, $\Delta t$ is measured using `time.perf_counter()` on the host side.

### 4.2 Strengths

- **Smooth and responsive.** Gyro integration gives clean, high-bandwidth angle tracking that responds instantly to rotation.
- **Immune to linear acceleration.** Unlike the accelerometer, the gyroscope is not affected by motor thrust or vibration (at first order).

### 4.3 The Drift Problem

Gyro integration has a critical flaw: **any constant bias in the gyroscope reading accumulates without bound.**

If the gyro has a DC bias error $\epsilon$ (in °/s), then after time $T$:

$$
\text{drift} = \epsilon \cdot T
$$

The MPU6050's typical zero-rate output tolerance is ±20 °/s at ±250 °/s full-scale. Even a tiny residual bias of 0.1 °/s produces **6° of drift per minute** and **360° per hour**. This makes pure gyro integration useless for long-term orientation.

---

## 5. The Complementary Filter

### 5.1 Motivation

We have two flawed estimates:

| Source          | Short-term | Long-term | Noise    |
|:----------------|:-----------|:----------|:---------|
| Accelerometer  | Bad (noisy, affected by vibration) | Good (gravity is constant) | High-frequency |
| Gyroscope      | Good (smooth, responsive) | Bad (drifts) | Low-frequency (bias) |

They have **complementary error spectra**: the accelerometer is accurate at low frequencies (steady-state), while the gyroscope is accurate at high frequencies (fast changes). A complementary filter combines both by applying a **high-pass filter** to the gyro and a **low-pass filter** to the accelerometer.

### 5.2 The Formula

$$
\phi(t) = \alpha \cdot \underbrace{\left[\phi(t-\Delta t) + \omega_x \cdot \Delta t\right]}_{\text{gyro prediction}} + (1 - \alpha) \cdot \underbrace{\phi_{\text{accel}}(t)}_{\text{accel correction}}
$$

The same formula is applied independently for pitch:

$$
\theta(t) = \alpha \cdot \left[\theta(t-\Delta t) + \omega_y \cdot \Delta t\right] + (1 - \alpha) \cdot \theta_{\text{accel}}(t)
$$

For yaw, **no accelerometer correction exists**, so it is pure gyro integration:

$$
\psi(t) = \psi(t-\Delta t) + \omega_z \cdot \Delta t
$$

### 5.3 The α Coefficient

The coefficient $\alpha \in [0, 1]$ controls the trust balance:

- $\alpha \to 1$: trust the gyroscope more → smoother but more drift
- $\alpha \to 0$: trust the accelerometer more → noisier but drift-free

We use **$\alpha = 0.98$**, meaning ~98% of the estimate comes from gyro integration and ~2% from the accelerometer correction each sample.

The effective time constant of the filter is approximately:

$$
\tau \approx \frac{\alpha \cdot \Delta t}{1 - \alpha}
$$

At 100 Hz ($\Delta t = 0.01$ s) with $\alpha = 0.98$:

$$
\tau \approx \frac{0.98 \times 0.01}{0.02} = 0.49 \text{ s}
$$

This means gyro drift is corrected with a time constant of about half a second.

### 5.4 In Code

From `visualize_imu.py`:

```python
if 0.0 < dt < 0.5:
    self.roll  = a * (self.roll  + gx * dt) + (1.0 - a) * ra
    self.pitch = a * (self.pitch + gy * dt) + (1.0 - a) * pa
    self.yaw  += gz * dt
else:
    # First sample or time gap — seed from accelerometer
    self.roll, self.pitch = ra, pa
```

The `dt < 0.5` guard prevents absurd integration steps if the serial stream is interrupted (e.g., USB disconnect). In such cases, the filter resets to the accelerometer-only estimate.

---

## 6. Coordinate Frame Mapping

### 6.1 MPU6050 Body Frame

The MPU6050 datasheet defines (with the chip's text facing up, pin 1 at top-left):

- **X**: along the long edge of the chip (forward)
- **Y**: along the short edge (left)
- **Z**: out of the chip face (up)

### 6.2 OpenGL Model Frame

The 3D visualizer uses OpenGL's right-handed coordinate system where **Y is up**. The mapping from MPU frame to model frame is:

| MPU Axis | Model Axis | Role        |
|:--------:|:----------:|:------------|
| X        | X          | Board length |
| Y        | Z          | Board width  |
| Z        | Y          | Up (component side) |

Rotations follow accordingly:

| Euler Angle | MPU Axis | GL Rotation              |
|:-----------:|:--------:|:-------------------------|
| Roll        | Around X | `glRotatef(roll, 1,0,0)` |
| Pitch       | Around Y | `glRotatef(pitch, 0,0,1)`|
| Yaw         | Around Z | `glRotatef(yaw, 0,1,0)`  |

The acceleration vector is also remapped: MPU $(a_x, a_y, a_z)$ becomes model $(a_x, a_z, a_y)$.

---

## 7. Summary of Displayed Values

| Value | Source | Formula | Unit |
|:------|:-------|:--------|:-----|
| **Accel X/Y/Z** | Raw ÷ 16384 | $a = \text{raw} / 16384$ | g |
| **Gyro X/Y/Z** | Raw ÷ 131 | $\omega = \text{raw} / 131$ | °/s |
| **Temp** | Datasheet formula | $T = \text{raw}/340 + 36.53$ | °C |
| **Roll** | Complementary filter | $\alpha(\phi + \omega_x \Delta t) + (1-\alpha)\,\text{atan2}(a_y, a_z)$ | ° |
| **Pitch** | Complementary filter | $\alpha(\theta + \omega_y \Delta t) + (1-\alpha)\,\text{atan2}(-a_x, \sqrt{a_y^2+a_z^2})$ | ° |
| **Yaw** | Gyro integration only | $\psi + \omega_z \Delta t$ | ° |
| **Hz** | Packet timing | $(N-1) / (t_{\text{last}} - t_{\text{first}})$ | Hz |

---

## 8. Potential Accuracy Improvements (Software)

### 8.1 Kalman Filter / Extended Kalman Filter (EKF)

The complementary filter is a fixed-gain approximation. A **Kalman filter** optimally weights sensor inputs based on their noise characteristics, modeled as:

- **Process model**: orientation evolves by integrating gyro (with process noise $Q$ accounting for gyro bias drift).
- **Measurement model**: accelerometer provides a noisy observation of gravity (with measurement noise $R$).

The Kalman gain $K$ is computed dynamically each step, automatically adapting trust between gyro and accelerometer based on the current uncertainty estimate. This yields better accuracy during transient accelerations than a fixed $\alpha$.

### 8.2 Madgwick / Mahony Filters

These are purpose-built orientation filters for IMUs. The **Madgwick filter** uses gradient descent to minimize the error between the measured gravity vector and the expected gravity vector given the current quaternion estimate. The **Mahony filter** uses a proportional-integral (PI) controller on the orientation error. Both operate in **quaternion space**, avoiding gimbal lock entirely.

### 8.3 Gyroscope Bias Calibration

At startup, while the sensor is stationary, average the gyroscope readings over several hundred samples to estimate the DC bias:

$$
\text{bias}_x = \frac{1}{N}\sum_{i=1}^{N} \omega_{x,i}
$$

Then subtract this bias from all subsequent readings. This dramatically reduces drift.

### 8.4 Accelerometer Calibration (6-Point / Least-Squares)

Factory calibration is imperfect. A full calibration involves placing the sensor in 6 known orientations (each axis pointing up and down) and solving for scale factors and offsets:

$$
a_{\text{calibrated}} = S \cdot (a_{\text{raw}} - b)
$$

where $S$ is a 3×3 scale/cross-axis matrix and $b$ is the bias vector. This corrects for axis misalignment, gain errors, and zero-g offset.

### 8.5 Magnetometer Fusion (9-DOF)

Adding a magnetometer (e.g., via an MPU9250 or external HMC5883L) provides an **absolute yaw reference** from Earth's magnetic field. With all three sensors:

- **Accelerometer** → roll, pitch (absolute)
- **Magnetometer** → yaw (absolute)
- **Gyroscope** → all three axes (rate, for smoothing)

This is the standard approach for full 9-DOF (degrees of freedom) attitude estimation.

### 8.6 Using the MPU6050's Digital Low-Pass Filter (DLPF)

The MPU6050 has a configurable on-chip DLPF (register `0x1A`, bits 2:0). Lowering the bandwidth (e.g., to 20 Hz or 5 Hz) reduces high-frequency noise at the cost of increased latency. For the aeropendulum's low-frequency dynamics, a moderate DLPF setting could clean up the signal substantially.

### 8.7 Higher Sample Rate + MCU-Side Filtering

Currently, the host-side `time.perf_counter()` measures $\Delta t$, which is affected by USB latency jitter. Moving the complementary filter (or a Kalman filter) **onto the MCU** with a hardware timer providing precise $\Delta t$ would improve consistency. The MCU could also run at a higher internal sample rate (the MPU6050 supports up to 1 kHz) and send pre-filtered orientation data to the host.

### 8.8 Motion Detection / Dynamic Accel Rejection

During high accelerations (motor thrust), the accelerometer no longer measures only gravity. The filter can detect this by checking if $\|a\| \gg 1g$ and temporarily increasing $\alpha$ (trusting the gyro more) or disabling the accel correction entirely. This prevents the motor's thrust from corrupting the roll/pitch estimate.

---

## 9. Fundamental Hardware Limitations

These are inherent to MEMS IMU technology and **cannot be fully solved in software**.

### 9.1 Gyroscope Drift Is Inherent

MEMS gyroscopes have an irreducible **bias instability** — a slow random walk of the zero-rate output. The MPU6050's datasheet specifies:

> **Bias instability**: typ. 8 °/hr (at room temperature, ±250 °/s range)

No software filter can distinguish true rotation from this slow bias wander. Over long periods, yaw (and to a lesser extent roll/pitch, corrected by the accelerometer) will always drift. Higher-grade gyroscopes (fiber-optic, ring-laser) reduce this by orders of magnitude but cost thousands of dollars.

### 9.2 No Absolute Yaw Without External Reference

As discussed in §3.4, gravity provides no information about rotation around the vertical axis. Without a magnetometer, GPS, or visual reference, **yaw is fundamentally unobservable** from accelerometer + gyroscope alone. The yaw estimate will always drift over time.

### 9.3 Vibration Sensitivity

MEMS accelerometers are mechanical resonators — tiny proof masses on silicon springs. High-frequency vibration (from a motor or propeller) excites these structures and introduces **rectification errors**: the mean of a vibrating signal shifts due to nonlinear response. This produces a DC offset in the accelerometer that no linear filter can fully remove.

The MPU6050 does not have high-g vibration rejection. For applications with significant vibration (like an aeropendulum with a spinning propeller), this is a real concern. Mitigation is mechanical: mount the IMU on vibration-dampening material (foam, rubber grommets).

### 9.4 Temperature Sensitivity

The MPU6050's zero-rate output and sensitivity drift with temperature:

- **Gyro zero-rate change**: ±20 °/s over the full temperature range (−40°C to +85°C)
- **Accel zero-g offset change**: ±35 mg over the full range
- **Sensitivity drift**: ±3% for accelerometer, ±3% for gyroscope

The on-die temperature sensor exists specifically to enable temperature compensation, but the MPU6050 does **not** perform this internally — it must be implemented in software with per-unit characterization data, which is tedious and imperfect.

### 9.5 Cross-Axis Sensitivity

Due to manufacturing tolerances, the sensor axes are not perfectly orthogonal. The MPU6050 specifies:

- **Accelerometer cross-axis sensitivity**: ±2%
- **Gyroscope cross-axis sensitivity**: ±2%

This means a rotation purely about X will produce a small spurious reading on Y and Z. This can be partially corrected with a calibration matrix, but the remaining error is bounded by the sensor's mechanical precision.

### 9.6 ADC Quantization Noise

At ±2 g range, each LSB represents $\frac{1}{16384}$ g ≈ 0.06 mg. The RMS noise floor at this range is approximately **4 mg** (from the datasheet: 400 µg/√Hz at ~100 Hz bandwidth), which corresponds to:

$$
\text{Angle noise} \approx \text{atan2}(0.004, 1.0) \approx 0.23°
$$

This sets a fundamental floor on the accelerometer-derived angle resolution. No amount of filtering can recover information below this noise level (only averaging over time or using a better sensor helps).

### 9.7 Gimbal Lock (Euler Angles)

The pitch formula $\theta = \text{atan2}(-a_x, \sqrt{a_y^2+a_z^2})$ becomes numerically unstable near $\pm 90°$ pitch (sensor pointing straight up or down). At this singularity, roll and yaw become indistinguishable — this is the classic **gimbal lock** of Euler angle representations.

This is not a sensor limitation but a **mathematical limitation of the Euler angle representation** itself. Quaternion-based filters (Madgwick, Mahony, EKF with quaternion state) avoid this entirely.

### 9.8 Limited Bandwidth

The MPU6050's internal ADC runs at 1 kHz maximum (accelerometer) and 8 kHz (gyroscope). For the aeropendulum, this is adequate, but for high-speed applications (drone flight controllers, vibration analysis), this bandwidth is insufficient. Modern IMUs like the ICM-42688-P offer up to 32 kHz gyroscope ODR.

---

## References

- **MPU-6050 Register Map**: InvenSense document RM-MPU-6000A-00 (Rev. 4.2)
- **MPU-6050 Datasheet**: InvenSense PS-MPU-6000A-00 (Rev. 3.4)
- **Complementary Filter**: Shane Colton, "The Balance Filter" (MIT, 2007)
- **Madgwick Filter**: S.O.H. Madgwick, "An efficient orientation filter for inertial and inertial/magnetic sensor arrays" (2010)
- **STM32F401xE Reference Manual**: ST RM0368
