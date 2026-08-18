"""
IMU Reader and Processing Module
Reads COBS-framed MPU6050 telemetry from serial, processes orientation
using a complementary filter, calculates movement energy, and detects
robust jerk/acceleration spikes for drum triggers with cooldown.
"""

import math
import struct
import sys
import threading
import time
from collections import deque

try:
    import serial
except ImportError:
    sys.exit("Missing: pyserial  ->  pip install pyserial")

try:
    from cobs import cobs as _cobs
except ImportError:
    sys.exit("Missing: cobs  ->  pip install cobs")

PKT_FMT  = "<7h"                           # (ax,ay,az,temp,gx,gy,gz)
PKT_SIZE = struct.calcsize(PKT_FMT)        # 14 bytes
ACC_S    = 16384.0                         # LSB/g   at ±2 g
GYR_S    = 131.0                           # LSB/dps at ±250 °/s
R2D      = 180.0 / math.pi

class IMU:
    """Thread-safe complementary-filter orientation estimator with jerk detection."""

    __slots__ = (
        "_lk", "roll", "pitch", "yaw",
        "ax", "ay", "az", "gx", "gy", "gz",
        "alive", "hz", "_a",
        "drum_trigger_up", "drum_trigger_down",
        "_last_az", "_last_drum_time",
        "gyro_mag", "accel_mag"
    )

    def __init__(self, alpha: float = 0.98):
        self._lk = threading.Lock()
        self.roll = self.pitch = self.yaw = 0.0
        self.ax = self.ay = 0.0
        self.az = 1.0
        self.gx = self.gy = self.gz = 0.0
        self.gyro_mag = 0.0
        self.accel_mag = 1.0
        self.alive = False
        self.hz = 0.0
        self._a = alpha
        
        # Jerk detection state
        self.drum_trigger_up = False
        self.drum_trigger_down = False
        self._last_az = 1.0
        self._last_drum_time = 0.0

    def reset_yaw(self):
        with self._lk:
            self.yaw = 0.0

    def feed(self, v: tuple, dt: float) -> None:
        ax = v[0] / ACC_S
        ay = v[1] / ACC_S
        az = v[2] / ACC_S
        gx = v[4] / GYR_S
        gy = v[5] / GYR_S
        gz = v[6] / GYR_S

        # Accel-only angle estimates
        ra = math.atan2(ay, az) * R2D
        pa = math.atan2(-ax, math.sqrt(ay * ay + az * az)) * R2D

        # Movement dynamics
        g_mag = math.sqrt(gx * gx + gy * gy + gz * gz)
        a_mag = math.sqrt(ax * ax + ay * ay + az * az)

        now = time.perf_counter()
        a = self._a
        with self._lk:
            if 0.0 < dt < 0.5:
                self.roll  = a * (self.roll  + gx * dt) + (1.0 - a) * ra
                self.pitch = a * (self.pitch + gy * dt) + (1.0 - a) * pa
                self.yaw  += gz * dt
            else:
                self.roll, self.pitch = ra, pa

            # Detect sudden acceleration (jerk) on Z axis with cooldown
            if dt > 0 and (now - self._last_drum_time > 0.18):
                jerk_z = (az - self._last_az) / dt
                
                # Dynamic threshold: sharp jerk trigger
                if jerk_z > 35.0 or (az - self._last_az > 0.6):
                    self.drum_trigger_up = True
                    self._last_drum_time = now
                elif jerk_z < -35.0 or (az - self._last_az < -0.6):
                    self.drum_trigger_down = True
                    self._last_drum_time = now

            self._last_az = az
            self.ax, self.ay, self.az = ax, ay, az
            self.gx, self.gy, self.gz = gx, gy, gz
            self.gyro_mag = g_mag
            self.accel_mag = a_mag
            self.alive = True

    def snap(self) -> tuple:
        """Returns the current state and clears the drum triggers."""
        with self._lk:
            up, down = self.drum_trigger_up, self.drum_trigger_down
            self.drum_trigger_up = False
            self.drum_trigger_down = False
            return (
                self.roll, self.pitch, self.yaw,
                self.ax,   self.ay,    self.az,
                self.gx,   self.gy,    self.gz,
                self.gyro_mag, self.accel_mag,
                self.alive, self.hz,
                up, down
            )

def run_serial_reader(port: str, baud: int, imu: IMU, stop: threading.Event) -> None:
    try:
        ser = serial.Serial(
            port, baud,
            bytesize=8, parity="E", stopbits=1, timeout=0.05,
        )
    except Exception as exc:
        print(f"[serial] {exc}", file=sys.stderr)
        return
    print(f"[serial] connected {port} @ {baud}")

    buf = bytearray()
    t0 = time.perf_counter()
    ts = deque(maxlen=200)

    while not stop.is_set():
        try:
            data = ser.read(max(ser.in_waiting, 1))
            for b in data:
                if b == 0:
                    if buf:
                        try:
                            dec = _cobs.decode(bytes(buf))
                            if len(dec) == PKT_SIZE:
                                now = time.perf_counter()
                                imu.feed(struct.unpack(PKT_FMT, dec), now - t0)
                                t0 = now
                                ts.append(now)
                                if len(ts) > 1:
                                    span = ts[-1] - ts[0]
                                    if span > 0.0:
                                        imu.hz = (len(ts) - 1) / span
                        except _cobs.DecodeError:
                            pass
                    buf.clear()
                else:
                    buf.append(b)
                    if len(buf) > 256:
                        buf.clear()
        except serial.SerialException:
            if not stop.is_set():
                time.sleep(0.2)

    ser.close()
    print("[serial] closed")
