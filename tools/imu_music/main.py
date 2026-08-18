#!/usr/bin/env python3
"""
Melodic Techno IMU Synthesizer & Visualizer

A high-energy, 124 BPM Melodic Techno experience (Afterlife / Tale of Us / Anyma style).
- 16th-note driving melodic bassline and arpeggios
- Resonant filter sweeps controlled by IMU Pitch
- Sequence / harmony progression selected by IMU Roll
- 909 Techno Kicks on Upward Jerk & Snappy Techno Claps on Downward Jerk
- Infinite rushing neon cyber-grid tunnel, 3D rotating cyber-monolith, and sweeping laser beams
"""

import argparse
import sys
import threading
import time

try:
    import pygame
    from pygame.locals import QUIT, KEYDOWN, K_ESCAPE, K_q, K_SPACE, K_r, DOUBLEBUF, OPENGL
except ImportError:
    sys.exit("Missing pygame-ce. Run: pip install pygame-ce")

from imu_reader import IMU, run_serial_reader
from synth import Synth
from visuals import Visualizer, WIN_W, WIN_H, FPS_CAP

def main():
    ap = argparse.ArgumentParser(description="Melodic Techno IMU Synthesizer & Visualizer")
    ap.add_argument("-p", "--port", required=True, help="Serial port (e.g. COM3 or COM8)")
    ap.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    ap.add_argument("--snap", action="store_true", default=True, help="Start in 16th-note Arpeggiator mode (default: True)")
    args = ap.parse_args()

    # 1. Initialize IMU & Serial background thread
    imu = IMU()
    stop_event = threading.Event()
    ser_thread = threading.Thread(
        target=run_serial_reader,
        args=(args.port, args.baud, imu, stop_event),
        daemon=True
    )
    ser_thread.start()

    # 2. Initialize Melodic Techno Synthesizer
    synth = Synth()
    synth.set_snap(args.snap)
    synth.start()

    # 3. Initialize Pygame & OpenGL Visualizer
    pygame.init()
    pygame.display.set_mode((WIN_W, WIN_H), DOUBLEBUF | OPENGL)
    pygame.display.set_caption("Melodic Techno IMU — Afterlife Cyber Visualizer")
    
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas,courier new,monospace", 15)
    
    viz = Visualizer()
    viz.init_gl()

    running = True
    last_time = time.perf_counter()
    
    try:
        while running:
            now = time.perf_counter()
            dt = now - last_time
            last_time = now

            for ev in pygame.event.get():
                if ev.type == QUIT:
                    running = False
                elif ev.type == KEYDOWN:
                    if ev.key in (K_ESCAPE, K_q):
                        running = False
                    elif ev.key == K_SPACE:
                        # Toggle between 16th-note Arpeggiator and Continuous Analog Lead
                        synth.set_snap(not synth.snap)
                    elif ev.key == K_r:
                        imu.reset_yaw()

            # Read latest IMU telemetry
            snap_data = imu.snap()
            roll, pitch, yaw = snap_data[0], snap_data[1], snap_data[2]
            gyro_mag = snap_data[9]
            accel_mag = snap_data[10]
            kick_triggered = snap_data[13]
            snare_triggered = snap_data[14]

            # Update Melodic Techno Synthesizer
            synth.update_imu(roll, pitch, gyro_mag, accel_mag)
            if kick_triggered:
                synth.trigger_kick()
            if snare_triggered:
                synth.trigger_snare()

            # Render Cyberpunk Visuals
            viz.render(
                roll=roll,
                pitch=pitch,
                yaw=yaw,
                kick_hit=kick_triggered,
                snare_hit=snare_triggered,
                gyro_mag=gyro_mag,
                is_snap=synth.snap,
                prog_idx=synth.prog_idx,
                prog_name=synth.prog_name,
                cutoff_norm=synth.filter_cutoff_norm,
                cutoff_hz=synth.current_cutoff_hz,
                beat_pulse=synth.beat_pulse,
                dt=dt
            )
            
            # Draw HUD
            viz.draw_hud(
                font=font,
                rfps=clock.get_fps(),
                is_snap=synth.snap,
                prog_name=synth.prog_name,
                cutoff_hz=synth.current_cutoff_hz,
                gyro_mag=gyro_mag,
                volume=synth.current_volume,
                beat_pulse=synth.beat_pulse
            )
            
            pygame.display.flip()
            clock.tick(FPS_CAP)

    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        synth.stop()
        ser_thread.join(timeout=1.0)
        pygame.quit()

if __name__ == "__main__":
    main()
