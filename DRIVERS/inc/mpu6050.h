#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>

#include "drv_status.h"
#include "i2c_transport.h"

// ==============================================================================
// MPU6050 device driver
// ==============================================================================
// Knows the register map and the scaling. Knows nothing about STM32, LL, DMA or
// which I2C peripheral it sits on -- everything it needs arrives through the
// i2c_transport_t handed to mpu6050_init().
//
// Rule: if this file ever gains an `#include "stm32..."` or an `#include
// "bus_i2c.h"`, the layering has collapsed and the driver is no longer portable
// or testable off-target.
//
// ------------------------------------------------------------------------------
// Register values are deliberately absent
// ------------------------------------------------------------------------------
// No register addresses, bit encodings, scale factors, or the device address
// appear in this header or in mpu6050.c. Those come from the InvenSense REGISTER
// MAP document (a separate document from the product specification in
// .docs/IMU/MPU6050-DataSheet.pdf -- see plan item I0; you still need to obtain
// it). Filling them in is the exercise.
//
// The enums below name the options without encoding them. The set of available
// full-scale ranges is on the front page of the product specification, so naming
// them spoils nothing; the bit patterns that select them are yours to look up.

#define MPU6050_BURST_LEN 14   // 7 x int16: accel XYZ, temp, gyro XYZ.
                               // Matches the telemetry frame the host tools in
                               // tools/ already expect. See CLAUDE.md.

// ------------------------------------------------------------------------------
// Configuration  (plan item I2)
// ------------------------------------------------------------------------------
// Everything here changes the numbers coming out of the device, so all of it is
// caller-visible rather than hard-coded inside mpu6050_init().
//
// While filling these in, work through the other half of I2: which reset-default
// values would the driver inherit by accident for anything NOT listed here?
typedef enum
{
    MPU6050_ACCEL_FS_2G,
    MPU6050_ACCEL_FS_4G,
    MPU6050_ACCEL_FS_8G,
    MPU6050_ACCEL_FS_16G
} mpu6050_accel_fs_t;

typedef enum
{
    MPU6050_GYRO_FS_250DPS,
    MPU6050_GYRO_FS_500DPS,
    MPU6050_GYRO_FS_1000DPS,
    MPU6050_GYRO_FS_2000DPS
} mpu6050_gyro_fs_t;

// The digital low-pass filter setting also changes the gyro's internal output
// rate, which in turn changes what sample-rate divider values mean. Those two
// settings are coupled; treat them as one decision, not two.
typedef enum
{
    MPU6050_DLPF_OFF,
    MPU6050_DLPF_BW_HIGHEST,
    MPU6050_DLPF_BW_HIGH,
    MPU6050_DLPF_BW_MID,
    MPU6050_DLPF_BW_LOW,
    MPU6050_DLPF_BW_LOWEST
} mpu6050_dlpf_t;

typedef struct
{
    uint8_t i2c_addr;             // 7-bit, right-aligned. See i2c_transport.h on
                                  // the address convention, and the register map
                                  // for the value and how AD0 selects between the
                                  // two possible addresses.
    mpu6050_accel_fs_t accel_fs;
    mpu6050_gyro_fs_t gyro_fs;
    mpu6050_dlpf_t dlpf;
    uint16_t sample_rate_hz;      // Requested. The achievable rate is quantised
                                  // by an integer divider, so the driver should
                                  // report what it actually programmed rather
                                  // than let the caller assume it got this.
    uint32_t timeout_ms;          // Applied to every blocking transfer.
} mpu6050_config_t;

// ------------------------------------------------------------------------------
// Samples
// ------------------------------------------------------------------------------
// Raw and scaled are separate types on purpose (Phase 0: "raw counts or physical
// units"). Keeping both means:
//   - telemetry can send raw int16 and preserve the existing wire format, so
//     tools/ keeps working and frames stay small;
//   - scaling is a pure function that can be tested on the host;
//   - the caller is never forced to pay for float conversion it does not need.
//
// Temperature is carried through rather than dropped. Gyro bias drifts strongly
// with temperature, so this is the signal that answers plan item I4's question of
// how a stored calibration is known to still be valid.
typedef struct
{
    int16_t accel[3];
    int16_t temp;
    int16_t gyro[3];
} mpu6050_sample_raw_t;

typedef struct
{
    float accel_g[3];
    float gyro_dps[3];
    float temp_c;
} mpu6050_sample_t;

// ------------------------------------------------------------------------------
// Calibration  (plan item I4)
// ------------------------------------------------------------------------------
// Gyro bias only, held in RAW COUNTS so it can be subtracted before scaling.
//
// There is deliberately no accel_bias field. Accelerometer bias cannot be
// separated from gravity in a single static orientation -- estimating it needs
// the six-face procedure and a combined scale-and-offset model. A field that
// looks implemented but is only ever estimated from one orientation is worse than
// no field at all. Add it when the six-face routine exists, not before.
typedef struct
{
    int16_t gyro_bias[3];
    float temp_c_at_cal;   // Ambient at calibration time. Compare against live
                           // temperature to decide whether this is still valid.
    bool valid;
} mpu6050_calibration_t;

// ------------------------------------------------------------------------------
// Device handle
// ------------------------------------------------------------------------------
typedef struct
{
    // --- set by mpu6050_init() ---------------------------------------------
    i2c_transport_t bus;
    mpu6050_config_t cfg;
    mpu6050_calibration_t cal;

    // --- private -----------------------------------------------------------
    // Owned by the transport while an async read is in flight. mpu6050_get_sample()
    // must copy out of it, never hand out a pointer into it: a second async read
    // started before the caller finishes reading would overwrite it mid-parse.
    uint8_t rx_buf[MPU6050_BURST_LEN];

    volatile bool sample_pending;
    volatile drv_status_t last_async_status;

    // Incremented when a completed sample is overwritten before it was read.
    // This is plan item I3's "how would that be detected rather than just assumed
    // away" -- a silently dropped sample is indistinguishable from a working
    // system until the control loop starts behaving strangely.
    volatile uint32_t dropped_samples;

    bool initialised;
} mpu6050_t;

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------
// Copies the transport and config into the handle and applies the configuration
// to the device. Does NOT probe -- call mpu6050_probe() first if you want to know
// whether anything is actually on that address.
//
// init() contract: on failure `initialised` stays false and every other entry
// point returns DRV_ERR_STATE. The device may be left partially configured, so a
// retry must re-run the whole sequence rather than resume it.
drv_status_t mpu6050_init(mpu6050_t *dev,
                          const i2c_transport_t *bus,
                          const mpu6050_config_t *cfg);

// Reads the identity register and compares it against the expected value.
// Returns DRV_ERR_ID on mismatch, DRV_ERR_NACK if nothing answers the address.
//
// This exists because it is the first rung of the plan's IMU verification ladder.
// If a test app has to reach around the API to check WHO_AM_I, the API is wrong.
drv_status_t mpu6050_probe(mpu6050_t *dev);

// ------------------------------------------------------------------------------
// Calibration
// ------------------------------------------------------------------------------
// Averages `samples` readings and stores the gyro bias. The caller is responsible
// for the device actually being still -- which raises I4's real question: how is
// stillness *established* rather than assumed? Consider rejecting the result if
// the accelerometer magnitude wanders or the gyro variance is too high, and
// returning DRV_ERR_STATE instead of silently storing a bias measured mid-motion.
drv_status_t mpu6050_calibrate_gyro(mpu6050_t *dev, uint16_t samples);

// ------------------------------------------------------------------------------
// Blocking read
// ------------------------------------------------------------------------------
// One burst read. Bias is applied here so that raw output is already corrected.
drv_status_t mpu6050_read_raw(mpu6050_t *dev, mpu6050_sample_raw_t *out);

// Pure function: raw counts -> physical units, using the ranges in dev->cfg.
// No bus access, no side effects, no failure mode.
void mpu6050_convert(const mpu6050_t *dev,
                     const mpu6050_sample_raw_t *raw,
                     mpu6050_sample_t *out);

// ------------------------------------------------------------------------------
// Non-blocking read
// ------------------------------------------------------------------------------
// Three-step cycle: start -> poll ready -> take the sample.
//
// The shape mirrors the blocking path closely enough that swapping between them
// is a call-site change, not a rewrite -- which is the point of deciding the API
// shape up front (Phase 0).
//
// Returns DRV_ERR_STATE if the transport offers no async path (read_async == NULL),
// and DRV_ERR_BUSY if a transfer is already in flight.
drv_status_t mpu6050_start_read(mpu6050_t *dev);

bool mpu6050_sample_ready(const mpu6050_t *dev);

// Copies the completed sample out and clears the pending flag. Returns
// DRV_ERR_BUSY if no sample is ready yet, or the transfer's own error status if
// it failed.
//
// Note for the implementation: reading rx_buf after checking a volatile flag is
// not automatically ordered against it. On Cortex-M4 there is no data cache, so
// this is a compiler-reordering concern rather than a coherency one -- but a
// barrier between the flag check and the buffer read is still the correct thing.
drv_status_t mpu6050_get_sample(mpu6050_t *dev, mpu6050_sample_raw_t *out);

uint32_t mpu6050_dropped_samples(const mpu6050_t *dev);

#endif // MPU6050_H
