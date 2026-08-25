#ifndef DRV_STATUS_H
#define DRV_STATUS_H

// ==============================================================================
// Shared driver result type
// ==============================================================================
// Every driver entry point in DRIVERS/ returns one of these instead of a bool.
//
// The reason is recovery, not tidiness: a caller that only learns "it failed"
// cannot choose between retrying, running bus recovery, and disarming. Each
// value below is here because it implies a *different* response. If a new value
// does not, it does not belong in this enum.

typedef enum
{
    DRV_OK = 0,

    DRV_ERR_PARAM,    // Caller passed a null pointer, zero length, bad config.
                      // -> A bug in the caller. Not recoverable at runtime.

    DRV_ERR_BUSY,     // A transfer is already in flight on this bus/device.
                      // -> Back off and retry on the next loop iteration.

    DRV_ERR_TIMEOUT,  // Deadline expired waiting on a hardware flag.
                      // -> The bus is likely wedged. Run recovery.

    DRV_ERR_NACK,     // Address or data byte was not acknowledged.
                      // -> The device is absent or the address is wrong.
                      //    Recovery will not help; re-probe instead.

    DRV_ERR_BUS,      // Arbitration lost, bus error, or overrun/underrun.
                      // -> Run recovery, then re-init the device.

    DRV_ERR_STATE,    // Called out of order (e.g. before a successful init),
                      // or the operation is not implemented yet.

    DRV_ERR_ID        // Identity register did not match the expected value.
                      // -> Wrong device on that address, or a wiring fault.
} drv_status_t;

#endif // DRV_STATUS_H
