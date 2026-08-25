/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file    drv_status.h
 * @brief   Common result type returned by every driver entry point.
 */

#ifndef DRV_STATUS_H
#define DRV_STATUS_H

/**
 * @brief Driver result code.
 *
 * Each value corresponds to a distinct recovery action. A caller that only
 * learns "it failed" cannot choose between retrying, resetting the bus, and
 * shutting down, so drivers never return a plain boolean.
 */
typedef enum
{
    DRV_OK = 0,      /**< Operation completed successfully. */
    DRV_ERR_PARAM,   /**< Invalid argument. Caller bug; not recoverable. */
    DRV_ERR_BUSY,    /**< Resource in use. Retry later. */
    DRV_ERR_TIMEOUT, /**< Deadline expired. Bus likely stuck; run recovery. */
    DRV_ERR_NACK,    /**< Not acknowledged. Device absent or wrong address. */
    DRV_ERR_BUS,     /**< Arbitration lost, bus error, or overrun. */
    DRV_ERR_STATE,   /**< Called out of order, or operation unavailable. */
    DRV_ERR_ID       /**< Identity register did not match. Wrong device. */
} drv_status_t;

#endif /* DRV_STATUS_H */
