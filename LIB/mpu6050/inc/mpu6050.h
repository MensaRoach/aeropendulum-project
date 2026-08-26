/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    mpu6050.h
 * @brief   InvenSense MPU-6050 six-axis IMU driver.
 *
 * Bus access goes through ::i2c_transport_t, so this driver is independent of
 * the MCU and of how the bus is implemented.
 *
 * @par Usage
 * @code
 * mpu6050_t imu;
 * mpu6050_config_t cfg = {
 *     .i2c_addr       = IMU_ADDR,
 *     .accel_fs       = MPU6050_ACCEL_FS_4G,
 *     .gyro_fs        = MPU6050_GYRO_FS_500DPS,
 *     .dlpf           = MPU6050_DLPF_BW_MID,
 *     .sample_rate_hz = 250,
 *     .timeout_ms     = 10,
 * };
 *
 * mpu6050_init(&imu, &transport, &cfg);
 * mpu6050_probe(&imu);
 * mpu6050_calibrate_gyro(&imu, 500);
 *
 * mpu6050_sample_raw_t raw;
 * mpu6050_sample_t sample;
 * if (mpu6050_read_raw(&imu, &raw) == DRV_OK)
 * {
 *     mpu6050_convert(&imu, &raw, &sample);
 * }
 * @endcode
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#include "drv_status.h"
#include "i2c_transport.h"

/** @brief Bytes in one accelerometer + temperature + gyroscope burst read. */
#define MPU6050_BURST_LEN 14


/** @brief Accelerometer full-scale range. */
typedef enum
{
    MPU6050_ACCEL_FS_2G,
    MPU6050_ACCEL_FS_4G,
    MPU6050_ACCEL_FS_8G,
    MPU6050_ACCEL_FS_16G
} mpu6050_accel_fs_t;

/** @brief Gyroscope full-scale range. */
typedef enum
{
    MPU6050_GYRO_FS_250DPS,
    MPU6050_GYRO_FS_500DPS,
    MPU6050_GYRO_FS_1000DPS,
    MPU6050_GYRO_FS_2000DPS
} mpu6050_gyro_fs_t;

/**
 * @brief Digital low-pass filter bandwidth, widest to narrowest.
 *
 * Also determines the gyroscope output rate, which sets the base frequency the
 * sample-rate divider works from.
 */
typedef enum
{
    MPU6050_DLPF_OFF,
    MPU6050_DLPF_BW_HIGHEST,
    MPU6050_DLPF_BW_HIGH,
    MPU6050_DLPF_BW_MID,
    MPU6050_DLPF_BW_LOW,
    MPU6050_DLPF_BW_LOWEST
} mpu6050_dlpf_t;

/** @brief Device configuration, applied by ::mpu6050_init. */
typedef struct
{
    uint8_t i2c_addr;              /**< 7-bit address; selected by the AD0 pin. */
    mpu6050_accel_fs_t accel_fs;
    mpu6050_gyro_fs_t gyro_fs;
    mpu6050_dlpf_t dlpf;
    uint16_t sample_rate_hz;       /**< Requested rate; quantised by an integer divider. */
    uint32_t timeout_ms;           /**< Applied to every blocking transfer. */
} mpu6050_config_t;

/** @brief One sample in device counts, bias-corrected. */
typedef struct
{
    int16_t accel[3];
    int16_t temp;
    int16_t gyro[3];
} mpu6050_sample_raw_t;

/** @brief One sample in physical units. */
typedef struct
{
    float accel_g[3];
    float gyro_dps[3];
    float temp_c;
} mpu6050_sample_t;

/**
 * @brief Gyroscope zero-rate offset.
 *
 * Held in device counts so it can be subtracted before scaling. Gyroscope bias
 * drifts with temperature, so @c temp_c_at_cal records the conditions the
 * offset was measured under.
 */
typedef struct
{
    int16_t gyro_bias[3];
    float temp_c_at_cal;
    bool valid;
} mpu6050_calibration_t;

/** @brief Device instance. */
typedef struct
{
    i2c_transport_t bus;
    mpu6050_config_t cfg;
    mpu6050_calibration_t cal;

    /** @cond INTERNAL */
    uint8_t rx_buf[MPU6050_BURST_LEN];
    volatile bool sample_pending;
    volatile drv_status_t last_async_status;
    volatile uint32_t dropped_samples;
    bool initialised;
    /** @endcond */
} mpu6050_t;

/**
 * @brief Configure the device and prepare the handle for use.
 *
 * Does not verify that a device is present; call ::mpu6050_probe for that. On
 * failure the device may be partially configured, so a retry must repeat the
 * whole sequence.
 *
 * @param dev Device handle to initialise.
 * @param bus Transport to use. Copied into the handle.
 * @param cfg Configuration to apply. Copied into the handle.
 */
drv_status_t mpu6050_init(mpu6050_t *dev, const i2c_transport_t *bus,
                          const mpu6050_config_t *cfg);

/**
 * @brief Verify the device identity register.
 *
 * @retval DRV_OK      Expected device found.
 * @retval DRV_ERR_ID  A device responded but reported a different identity.
 * @retval DRV_ERR_NACK Nothing responded at the configured address.
 */
drv_status_t mpu6050_probe(mpu6050_t *dev);

/**
 * @brief Measure and store the gyroscope zero-rate offset.
 *
 * The device must be stationary for the duration. Motion during calibration is
 * rejected rather than averaged in.
 *
 * @param dev     Device handle.
 * @param samples Number of readings to average.
 * @retval DRV_ERR_STATE The device moved; the stored offset is unchanged.
 */
drv_status_t mpu6050_calibrate_gyro(mpu6050_t *dev, uint16_t samples);

/** @brief Read one sample, blocking until it completes. */
drv_status_t mpu6050_read_raw(mpu6050_t *dev, mpu6050_sample_raw_t *out);

/**
 * @brief Convert device counts to physical units using the configured ranges.
 *
 * Pure function; performs no bus access.
 */
void mpu6050_convert(const mpu6050_t *dev, const mpu6050_sample_raw_t *raw,
                     mpu6050_sample_t *out);

/**
 * @brief Begin a non-blocking read.
 *
 * @retval DRV_ERR_STATE The transport provides no async path.
 * @retval DRV_ERR_BUSY  A transfer is already in flight.
 */
drv_status_t mpu6050_start_read(mpu6050_t *dev);

/** @brief Whether a sample started by ::mpu6050_start_read has completed. */
bool mpu6050_sample_ready(const mpu6050_t *dev);

/**
 * @brief Take the completed sample.
 *
 * @retval DRV_ERR_BUSY No sample is ready yet.
 */
drv_status_t mpu6050_get_sample(mpu6050_t *dev, mpu6050_sample_raw_t *out);

/**
 * @brief Samples that completed but were overwritten before being read.
 *
 * A non-zero and rising count means the consumer is not keeping up with the
 * configured sample rate.
 */
uint32_t mpu6050_dropped_samples(const mpu6050_t *dev);

#endif /* MPU6050_H */
