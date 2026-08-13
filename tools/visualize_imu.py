#!/usr/bin/env python3
"""
3D IMU Pose Visualizer

Reads COBS-framed MPU6050 telemetry from serial and renders the IMU's
orientation in real-time using OpenGL.  A complementary filter fuses
accelerometer and gyroscope data into stable roll / pitch / yaw.

Controls
--------
    R       Reset yaw to zero
    Q / Esc Quit

Usage
-----
    python visualize_imu.py -p COM3
    python visualize_imu.py -p /dev/ttyACM0 -b 115200
"""

import argparse
import math
import struct
import sys
import threading
import time
from collections import deque

# ── Dependency gate ───────────────────────────────────────────────────────

try:
    import pygame
    from pygame.locals import (
        QUIT, KEYDOWN, K_ESCAPE, K_q, K_r, DOUBLEBUF, OPENGL,
    )
except ImportError:
    sys.exit("Missing: pygame-ce  →  pip install pygame-ce")

# Disable PyOpenGL error-checking for speed (must be set before GL import)
import OpenGL as _OpenGL

_OpenGL.ERROR_CHECKING = False

try:
    from OpenGL.GL import *   # noqa: F403
    from OpenGL.GLU import *  # noqa: F403
except ImportError:
    sys.exit("Missing: PyOpenGL  →  pip install PyOpenGL")

try:
    import serial
except ImportError:
    sys.exit("Missing: pyserial  →  pip install pyserial")

try:
    from cobs import cobs as _cobs
except ImportError:
    sys.exit("Missing: cobs  →  pip install cobs")


# ━━ Protocol constants ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PKT_FMT  = "<7h"                           # (ax,ay,az,temp,gx,gy,gz)
PKT_SIZE = struct.calcsize(PKT_FMT)        # 14 bytes
ACC_S    = 16384.0                         # LSB/g   at ±2 g
GYR_S    = 131.0                           # LSB/dps at ±250 °/s
R2D      = 180.0 / math.pi

# ━━ Window ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

WIN_W, WIN_H = 1280, 720
FPS_CAP      = 60


# ═══════════════════════════════════════════════════════════════════════════
#  IMU orientation estimator (complementary filter)
# ═══════════════════════════════════════════════════════════════════════════

class IMU:
    """Thread-safe complementary-filter orientation estimator."""

    __slots__ = (
        "_lk", "roll", "pitch", "yaw",
        "ax", "ay", "az", "gx", "gy", "gz",
        "temp", "alive", "hz", "_a",
    )

    def __init__(self, alpha: float = 0.98):
        self._lk = threading.Lock()
        self.roll = self.pitch = self.yaw = 0.0
        self.ax = self.ay = 0.0
        self.az = 1.0
        self.gx = self.gy = self.gz = 0.0
        self.temp = 0.0
        self.alive = False
        self.hz = 0.0
        self._a = alpha

    # Called from the serial-reader thread
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

        a = self._a
        with self._lk:
            if 0.0 < dt < 0.5:
                self.roll  = a * (self.roll  + gx * dt) + (1.0 - a) * ra
                self.pitch = a * (self.pitch + gy * dt) + (1.0 - a) * pa
                self.yaw  += gz * dt
            else:
                # First sample or time-gap — seed from accelerometer
                self.roll, self.pitch = ra, pa

            self.ax, self.ay, self.az = ax, ay, az
            self.gx, self.gy, self.gz = gx, gy, gz
            self.temp = v[3] / 340.0 + 36.53
            self.alive = True

    # Called from the render thread
    def snap(self) -> tuple:
        with self._lk:
            return (
                self.roll, self.pitch, self.yaw,
                self.ax,   self.ay,    self.az,
                self.gx,   self.gy,    self.gz,
                self.temp, self.alive,  self.hz,
            )

    def reset_yaw(self) -> None:
        with self._lk:
            self.yaw = 0.0


# ═══════════════════════════════════════════════════════════════════════════
#  Serial reader (background thread)
# ═══════════════════════════════════════════════════════════════════════════

def _reader(port: str, baud: int, imu: IMU, stop: threading.Event) -> None:
    try:
        ser = serial.Serial(
            port, baud,
            bytesize=8, parity="E", stopbits=1, timeout=0.05,
        )
    except Exception as exc:
        print(f"[serial] {exc}", file=sys.stderr)
        return
    print(f"[serial] {port} @ {baud} (8E1)")

    buf = bytearray()
    t0 = time.perf_counter()
    ts: deque = deque(maxlen=200)

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


# ═══════════════════════════════════════════════════════════════════════════
#  OpenGL scene
# ═══════════════════════════════════════════════════════════════════════════

_q = None  # GLU quadric (created once in _init_gl)


def _init_gl() -> None:
    global _q
    glClearColor(0.105, 0.11, 0.13, 1.0)
    glEnable(GL_DEPTH_TEST)
    glDepthFunc(GL_LEQUAL)
    glEnable(GL_CULL_FACE)
    glEnable(GL_NORMALIZE)
    glShadeModel(GL_SMOOTH)

    # Two-point lighting for depth
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glEnable(GL_LIGHT1)
    glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)

    glLightfv(GL_LIGHT0, GL_POSITION, (5.0, 8.0, 5.0, 0.0))
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  (0.70, 0.70, 0.70, 1.0))
    glLightfv(GL_LIGHT0, GL_AMBIENT,  (0.22, 0.22, 0.24, 1.0))
    glLightfv(GL_LIGHT1, GL_POSITION, (-3.0, 6.0, -4.0, 0.0))
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  (0.22, 0.22, 0.28, 1.0))

    _q = gluNewQuadric()
    gluQuadricNormals(_q, GLU_SMOOTH)


def _camera() -> None:
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(40.0, WIN_W / WIN_H, 0.1, 100.0)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    gluLookAt(4.5, 3.0, 4.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0)


# ── Grid ──────────────────────────────────────────────────────────────────

def _grid() -> None:
    glDisable(GL_LIGHTING)
    glBegin(GL_LINES)
    glColor3f(0.19, 0.19, 0.22)
    N = 8
    Y = -1.5
    for i in range(-N, N + 1):
        fi = float(i)
        glVertex3f(fi, Y, -N)
        glVertex3f(fi, Y,  N)
        glVertex3f(-N, Y, fi)
        glVertex3f( N, Y, fi)
    glEnd()
    glEnable(GL_LIGHTING)


# ── Arrows ────────────────────────────────────────────────────────────────

def _arrow(length: float, r: float, g: float, b: float) -> None:
    """Draw an arrow along +Z.  Cone is push/popped internally."""
    shaft = length - 0.18
    glColor3f(r, g, b)
    gluCylinder(_q, 0.022, 0.022, shaft, 8, 1)
    glPushMatrix()
    glTranslatef(0.0, 0.0, shaft)
    gluCylinder(_q, 0.055, 0.0, 0.18, 10, 1)
    glRotatef(180.0, 1.0, 0.0, 0.0)
    gluDisk(_q, 0.0, 0.055, 10, 1)
    glPopMatrix()


def _axes(length: float, s: float = 1.0) -> None:
    """Draw RGB axis arrows at the current origin (s = brightness)."""
    glDisable(GL_LIGHTING)
    # X — red
    glPushMatrix()
    glRotatef(90.0, 0.0, 1.0, 0.0)
    _arrow(length, 0.9 * s, 0.25 * s, 0.25 * s)
    glPopMatrix()
    # Y — green
    glPushMatrix()
    glRotatef(-90.0, 1.0, 0.0, 0.0)
    _arrow(length, 0.25 * s, 0.9 * s, 0.25 * s)
    glPopMatrix()
    # Z — blue
    glPushMatrix()
    _arrow(length, 0.25 * s, 0.25 * s, 0.9 * s)
    glPopMatrix()
    glEnable(GL_LIGHTING)


# ── PCB board model ───────────────────────────────────────────────────────

def _face(n: tuple, verts: list, clr: tuple) -> None:
    """Emit a single lit quad (call inside glBegin(GL_QUADS))."""
    glColor3f(*clr)
    glNormal3f(*n)
    for v in verts:
        glVertex3f(*v)


def _board() -> None:
    """Draw the PCB box with an IC chip on top."""
    x, y, z = 1.0, 0.075, 0.7  # half-extents

    glBegin(GL_QUADS)
    # fmt: off
    # Top — component side (bright PCB green)
    _face(( 0, 1, 0), [(-x, y, z),( x, y, z),( x, y,-z),(-x, y,-z)], (.22,.60,.38))
    # Bottom — solder side (dark green)
    _face(( 0,-1, 0), [(-x,-y,-z),( x,-y,-z),( x,-y, z),(-x,-y, z)], (.10,.30,.16))
    # Front  (+Z)
    _face(( 0, 0, 1), [(-x,-y, z),( x,-y, z),( x, y, z),(-x, y, z)], (.16,.48,.30))
    # Back   (−Z)
    _face(( 0, 0,-1), [( x,-y,-z),(-x,-y,-z),(-x, y,-z),( x, y,-z)], (.16,.48,.30))
    # Right  (+X)
    _face(( 1, 0, 0), [( x,-y, z),( x,-y,-z),( x, y,-z),( x, y, z)], (.14,.44,.27))
    # Left   (−X)
    _face((-1, 0, 0), [(-x,-y,-z),(-x,-y, z),(-x, y, z),(-x, y,-z)], (.14,.44,.27))
    # fmt: on
    glEnd()

    # ── IC chip (small dark box on the top surface) ───────────────────
    cx, cy, cz = 0.22, 0.035, 0.22
    ty = y  # sits on the board top

    glBegin(GL_QUADS)
    # fmt: off
    _face(( 0, 1, 0), [(-cx,ty+cy, cz),( cx,ty+cy, cz),( cx,ty+cy,-cz),(-cx,ty+cy,-cz)], (.08,.08,.08))
    _face(( 0, 0, 1), [(-cx,ty,    cz),( cx,ty,    cz),( cx,ty+cy, cz),(-cx,ty+cy, cz)], (.06,.06,.06))
    _face(( 0, 0,-1), [( cx,ty,   -cz),(-cx,ty,   -cz),(-cx,ty+cy,-cz),( cx,ty+cy,-cz)], (.06,.06,.06))
    _face(( 1, 0, 0), [( cx,ty,    cz),( cx,ty,   -cz),( cx,ty+cy,-cz),( cx,ty+cy, cz)], (.07,.07,.07))
    _face((-1, 0, 0), [(-cx,ty,   -cz),(-cx,ty,    cz),(-cx,ty+cy, cz),(-cx,ty+cy,-cz)], (.07,.07,.07))
    # fmt: on
    glEnd()


# ── Acceleration vector (gold arrow in body frame) ────────────────────

def _accel_arrow(ax: float, ay: float, az: float) -> None:
    """Draw a gold arrow showing measured acceleration direction."""
    mag = math.sqrt(ax * ax + ay * ay + az * az)
    if mag < 0.01:
        return
    nx, ny, nz = ax / mag, ay / mag, az / mag
    length = min(mag, 3.0) * 1.2

    glPushMatrix()
    # Rotate +Z to the (nx, ny, nz) direction
    dot = nz  # dot product of +Z with direction
    if dot < 0.9999:
        if dot < -0.9999:
            glRotatef(180.0, 1.0, 0.0, 0.0)
        else:
            angle = math.acos(max(-1.0, min(1.0, dot))) * R2D
            al = math.sqrt(ny * ny + nx * nx)
            if al > 1e-6:
                glRotatef(angle, -ny / al, nx / al, 0.0)

    glDisable(GL_LIGHTING)
    _arrow(length, 1.0, 0.82, 0.18)
    glEnable(GL_LIGHTING)
    glPopMatrix()


# ═══════════════════════════════════════════════════════════════════════════
#  HUD  (2-D text overlay)
# ═══════════════════════════════════════════════════════════════════════════

def _blit(font, text: str, x: float, y: float,
          color: tuple = (220, 220, 220)) -> None:
    """Render a line of text as a temporary GL texture quad."""
    surf = font.render(text, True, color)
    data = pygame.image.tobytes(surf, "RGBA", True)
    tw, th = surf.get_size()

    tex = int(glGenTextures(1))
    glBindTexture(GL_TEXTURE_2D, tex)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data,
    )

    glEnable(GL_TEXTURE_2D)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    glColor4f(1.0, 1.0, 1.0, 1.0)

    glBegin(GL_QUADS)
    glTexCoord2f(0, 0); glVertex2f(x,      y)       # noqa: E702
    glTexCoord2f(1, 0); glVertex2f(x + tw, y)       # noqa: E702
    glTexCoord2f(1, 1); glVertex2f(x + tw, y + th)  # noqa: E702
    glTexCoord2f(0, 1); glVertex2f(x,      y + th)  # noqa: E702
    glEnd()

    glDisable(GL_TEXTURE_2D)
    glDeleteTextures(1, [tex])


def _hud(font, snap: tuple, rfps: float) -> None:
    """Draw the heads-up display with telemetry data."""
    roll, pitch, yaw, ax, ay, az, gx, gy, gz, temp, alive, hz = snap

    # Switch to 2-D orthographic projection
    glMatrixMode(GL_PROJECTION)
    glPushMatrix()
    glLoadIdentity()
    glOrtho(0, WIN_W, 0, WIN_H, -1, 1)
    glMatrixMode(GL_MODELVIEW)
    glPushMatrix()
    glLoadIdentity()
    glDisable(GL_DEPTH_TEST)
    glDisable(GL_LIGHTING)

    LH = 22  # line height
    x0 = 16
    y0 = WIN_H - 30

    # Connection status
    if alive:
        _blit(font, f"\u25cf Connected   {hz:.0f} Hz", x0, y0, (90, 210, 120))
    else:
        _blit(font, "\u25cb Waiting for data\u2026", x0, y0, (210, 160, 80))
    y0 -= LH + 6

    val = (215, 215, 220)
    dim = (150, 150, 160)

    _blit(font,
          f"Roll {roll:+7.1f}\u00b0   Pitch {pitch:+7.1f}\u00b0   Yaw {yaw:+7.1f}\u00b0",
          x0, y0, val)
    y0 -= LH
    _blit(font,
          f"Accel  {ax:+6.3f}  {ay:+6.3f}  {az:+6.3f} g",
          x0, y0, dim)
    y0 -= LH
    _blit(font,
          f"Gyro   {gx:+7.1f}  {gy:+7.1f}  {gz:+7.1f} \u00b0/s",
          x0, y0, dim)
    y0 -= LH
    _blit(font, f"Temp   {temp:.1f} \u00b0C", x0, y0, dim)

    # Bottom-right: controls
    _blit(font, "[R] Reset Yaw   [Q] Quit", WIN_W - 296, 16, (100, 100, 110))
    # Bottom-left: render FPS
    _blit(font, f"{rfps:.0f} fps", 16, 16, (100, 100, 110))

    # Restore 3-D state
    glEnable(GL_LIGHTING)
    glEnable(GL_DEPTH_TEST)
    glMatrixMode(GL_PROJECTION)
    glPopMatrix()
    glMatrixMode(GL_MODELVIEW)
    glPopMatrix()


# ═══════════════════════════════════════════════════════════════════════════
#  Entry point
# ═══════════════════════════════════════════════════════════════════════════

def main() -> None:
    ap = argparse.ArgumentParser(
        description="3D IMU orientation visualizer for MPU6050 telemetry.",
    )
    ap.add_argument(
        "-p", "--port", required=True,
        help="Serial port (e.g. COM3 or /dev/ttyACM0)",
    )
    ap.add_argument(
        "-b", "--baud", type=int, default=115200,
        help="Baud rate (default: 115200)",
    )
    ap.add_argument(
        "-a", "--alpha", type=float, default=0.98,
        help="Complementary-filter coefficient 0–1 (default: 0.98)",
    )
    args = ap.parse_args()

    # ── IMU + serial thread ───────────────────────────────────────────────
    imu  = IMU(alpha=args.alpha)
    stop = threading.Event()

    thr = threading.Thread(
        target=_reader,
        args=(args.port, args.baud, imu, stop),
        daemon=True,
    )
    thr.start()

    # ── Pygame / OpenGL window ────────────────────────────────────────────
    pygame.init()
    pygame.display.set_mode((WIN_W, WIN_H), DOUBLEBUF | OPENGL)
    pygame.display.set_caption("IMU 3D Visualizer — Aeropendulum")
    clock = pygame.time.Clock()
    font  = pygame.font.SysFont("consolas,courier new,monospace", 15)

    _init_gl()

    # ── Render loop ───────────────────────────────────────────────────────
    running = True
    while running:
        # Events
        for ev in pygame.event.get():
            if ev.type == QUIT:
                running = False
            elif ev.type == KEYDOWN:
                if ev.key in (K_ESCAPE, K_q):
                    running = False
                elif ev.key == K_r:
                    imu.reset_yaw()

        snap = imu.snap()
        roll, pitch, yaw = snap[0], snap[1], snap[2]
        ax_g, ay_g, az_g = snap[3], snap[4], snap[5]

        # ── Draw 3-D scene ────────────────────────────────────────────────
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        _camera()

        _grid()
        _axes(1.5, s=0.35)  # dim world-frame axes

        # Board orientation
        #   MPU6050 frame → OpenGL model frame mapping:
        #     MPU-X → model-X,  MPU-Y → model-Z,  MPU-Z → model-Y (up)
        #   Euler rotation order (intrinsic): yaw → pitch → roll
        glPushMatrix()
        glRotatef(yaw,   0.0, 1.0, 0.0)   # yaw   around model-Y  (MPU-Z)
        glRotatef(pitch, 0.0, 0.0, 1.0)   # pitch  around model-Z  (MPU-Y)
        glRotatef(roll,  1.0, 0.0, 0.0)   # roll   around model-X  (MPU-X)

        _board()
        _axes(1.8, s=1.0)  # bright body-frame axes

        # Acceleration vector (map MPU (x,y,z) → model (x,z,y))
        _accel_arrow(ax_g, az_g, ay_g)

        glPopMatrix()

        # ── 2-D HUD ──────────────────────────────────────────────────────
        _hud(font, snap, clock.get_fps())

        pygame.display.flip()
        clock.tick(FPS_CAP)

    # ── Cleanup ───────────────────────────────────────────────────────────
    stop.set()
    thr.join(timeout=2)
    if _q is not None:
        gluDeleteQuadric(_q)
    pygame.quit()


if __name__ == "__main__":
    main()
