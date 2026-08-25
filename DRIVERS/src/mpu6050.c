/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    mpu6050.c
 * @brief   InvenSense MPU-6050 six-axis IMU driver.
 */

#include "mpu6050.h"

/* -------------------------------------------------------------------------- */
/* Register map                                                               */
/* -------------------------------------------------------------------------- */

/* Register addresses and bit definitions belong here. */

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

/** @brief Read @p len bytes starting at @p reg using the configured address. */
static drv_status_t MPU6050_ReadRegs(mpu6050_t *dev, uint8_t reg,
                                     uint8_t *buf, uint16_t len);

/** @brief Write a single register. */
static drv_status_t MPU6050_WriteReg(mpu6050_t *dev, uint8_t reg, uint8_t value);

/** @brief Accelerometer counts per g for the configured range. */
static float MPU6050_AccelScale(mpu6050_accel_fs_t fs);

/** @brief Gyroscope counts per degree per second for the configured range. */
static float MPU6050_GyroScale(mpu6050_gyro_fs_t fs);

/** @brief Async completion hook passed to the transport. */
static void MPU6050_OnTransferDone(void *user_ctx, drv_status_t status);

static drv_status_t MPU6050_ReadRegs(mpu6050_t *dev, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    (void)dev;
    (void)reg;
    (void)buf;
    (void)len;
    return DRV_ERR_STATE;
}

static drv_status_t MPU6050_WriteReg(mpu6050_t *dev, uint8_t reg, uint8_t value)
{
    (void)dev;
    (void)reg;
    (void)value;
    return DRV_ERR_STATE;
}

static float MPU6050_AccelScale(mpu6050_accel_fs_t fs)
{
    (void)fs;
    return 0.0f;
}

static float MPU6050_GyroScale(mpu6050_gyro_fs_t fs)
{
    (void)fs;
    return 0.0f;
}

static void MPU6050_OnTransferDone(void *user_ctx, drv_status_t status)
{
    (void)user_ctx;
    (void)status;
}

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

drv_status_t mpu6050_init(mpu6050_t *dev, const i2c_transport_t *bus,
                          const mpu6050_config_t *cfg)
{
    (void)dev;
    (void)bus;
    (void)cfg;
    (void)MPU6050_WriteReg;
    return DRV_ERR_STATE;
}

drv_status_t mpu6050_probe(mpu6050_t *dev)
{
    (void)dev;
    (void)MPU6050_ReadRegs;
    return DRV_ERR_STATE;
}

drv_status_t mpu6050_calibrate_gyro(mpu6050_t *dev, uint16_t samples)
{
    (void)dev;
    (void)samples;
    return DRV_ERR_STATE;
}

/* -------------------------------------------------------------------------- */
/* Blocking acquisition                                                       */
/* -------------------------------------------------------------------------- */

drv_status_t mpu6050_read_raw(mpu6050_t *dev, mpu6050_sample_raw_t *out)
{
    (void)dev;
    (void)out;
    return DRV_ERR_STATE;
}

void mpu6050_convert(const mpu6050_t *dev, const mpu6050_sample_raw_t *raw,
                     mpu6050_sample_t *out)
{
    (void)dev;
    (void)raw;
    (void)out;
    (void)MPU6050_AccelScale;
    (void)MPU6050_GyroScale;
}

/* -------------------------------------------------------------------------- */
/* Non-blocking acquisition                                                   */
/* -------------------------------------------------------------------------- */

drv_status_t mpu6050_start_read(mpu6050_t *dev)
{
    (void)dev;
    (void)MPU6050_OnTransferDone;
    return DRV_ERR_STATE;
}

bool mpu6050_sample_ready(const mpu6050_t *dev)
{
    (void)dev;
    return false;
}

drv_status_t mpu6050_get_sample(mpu6050_t *dev, mpu6050_sample_raw_t *out)
{
    (void)dev;
    (void)out;
    return DRV_ERR_STATE;
}

uint32_t mpu6050_dropped_samples(const mpu6050_t *dev)
{
    (void)dev;
    return 0;
}
