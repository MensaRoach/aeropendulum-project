"""
Melodic Techno Cyberpunk Visualizer for IMU Music.
Inspired by Afterlife, Anyma, Printworks London, and Awakenings.

Visual Elements:
- Infinite Cyber-Grid Tunnel (rushing neon perspective grid at 124 BPM)
- Central 3D Holographic Cyber-Monolith that rotates with IMU pose
- Volumetric Overhead Sweeping Laser Fans & Beam Cannons
- Laser Strobe Shockwaves & Warp-Speed Particle Trails on Kick/Clap drops
- Resonant Electric Laser Waveform modulated by Filter Cutoff & Noise
- Futuristic Techno HUD with BPM, 16-step Sequencer, and Filter Gauge
"""

import math
import random
import time
import numpy as np

try:
    import pygame
    from pygame.locals import *
except ImportError:
    pass

import OpenGL as _OpenGL
_OpenGL.ERROR_CHECKING = False
from OpenGL.GL import *
from OpenGL.GLU import *

WIN_W, WIN_H = 1280, 720
FPS_CAP = 60

# Cyberpunk Melodic Techno Palettes (Primary, Accent, Laser)
TECHNO_PALETTES = [
    # Afterlife Cyan / Deep Indigo / Electric Blue
    ((0.0, 0.95, 1.0), (0.1, 0.2, 0.9), (0.9, 1.0, 1.0)),
    # Acid Cyber Green / Deep Purple
    ((0.1, 1.0, 0.4), (0.7, 0.0, 1.0), (0.8, 1.0, 0.9)),
    # Neon Magenta / Crimson / Gold
    ((1.0, 0.05, 0.6), (0.9, 0.5, 0.0), (1.0, 0.9, 0.8)),
    # Monochrome Laser White / Cold Blue
    ((0.8, 0.9, 1.0), (0.15, 0.3, 0.6), (1.0, 1.0, 1.0))
]

class WarpStarfield:
    """Warp-speed cyber stardust streaming towards the camera."""
    def __init__(self, num_stars=800):
        self.num = num_stars
        self.stars = [] # [x, y, z, speed, length]
        for _ in range(num_stars):
            x = random.uniform(-14.0, 14.0)
            y = random.uniform(-8.0, 8.0)
            z = random.uniform(-25.0, 5.0)
            spd = random.uniform(12.0, 28.0)
            length = random.uniform(0.3, 0.8)
            self.stars.append([x, y, z, spd, length])

    def update_and_draw(self, dt, boost_speed, col):
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        glLineWidth(1.8)
        
        glBegin(GL_LINES)
        for s in self.stars:
            s[2] += (s[3] + boost_speed * 30.0) * dt
            if s[2] > 6.0:
                s[2] = -25.0
                s[0] = random.uniform(-14.0, 14.0)
                s[1] = random.uniform(-8.0, 8.0)
            
            # Fade based on distance
            alpha = max(0.0, min(1.0, (s[2] + 25.0) / 15.0)) * 0.75
            glColor4f(col[0], col[1], col[2], alpha)
            glVertex3f(s[0], s[1], s[2])
            glVertex3f(s[0], s[1], s[2] - s[4] * (1.0 + boost_speed * 2.0))
        glEnd()

class LaserShockwave:
    """Expanding neon laser frames and strobes for techno drum drops."""
    def __init__(self):
        self.frames = [] # [z, size, life, max_life, r, g, b, speed]
        self.flash = 0.0

    def trigger_kick(self, col):
        self.flash = 0.85
        self.frames.append([-22.0, 3.5, 1.2, 1.2, col[0], col[1], col[2], 26.0])
        self.frames.append([-22.0, 2.0, 0.9, 0.9, 1.0, 1.0, 1.0, 32.0])

    def trigger_snare(self, col):
        self.flash = 0.65
        self.frames.append([-18.0, 4.0, 0.8, 0.8, 1.0, 0.1, 0.7, 34.0])
        self.frames.append([-18.0, 2.5, 0.6, 0.6, 0.2, 0.9, 1.0, 40.0])

    def update_and_draw(self, dt):
        self.flash = max(0.0, self.flash - dt * 4.0)
        
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        
        # 1. Fullscreen / Tunnel Laser Strobe Flash
        if self.flash > 0.02:
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            glLoadIdentity()
            glOrtho(0, WIN_W, 0, WIN_H, -1, 1)
            glMatrixMode(GL_MODELVIEW)
            glPushMatrix()
            glLoadIdentity()
            
            glBegin(GL_QUADS)
            glColor4f(0.8, 0.95, 1.0, self.flash * 0.35)
            glVertex2f(0, 0)
            glVertex2f(WIN_W, 0)
            glVertex2f(WIN_W, WIN_H)
            glVertex2f(0, WIN_H)
            glEnd()
            
            glPopMatrix()
            glMatrixMode(GL_PROJECTION)
            glPopMatrix()
            glMatrixMode(GL_MODELVIEW)

        # 2. Expanding Geometric Laser Gates rushing down tunnel
        new_frames = []
        for f in self.frames:
            z, size, life, max_life, cr, cg, cb, spd = f
            life -= dt
            z += spd * dt
            if life > 0 and z < 8.0:
                alpha = (life / max_life) ** 1.3
                glLineWidth(3.0)
                glColor4f(cr, cg, cb, alpha * 0.9)
                
                glBegin(GL_LINE_LOOP)
                # Octagonal laser portal
                for i in range(8):
                    ang = i * 2.0 * math.pi / 8.0 + math.pi / 8.0
                    gx = size * math.cos(ang) * 1.5
                    gy = size * math.sin(ang)
                    glVertex3f(gx, gy, z)
                glEnd()
                
                new_frames.append([z, size * 1.03, life, max_life, cr, cg, cb, spd])
        self.frames = new_frames

class Visualizer:
    def __init__(self):
        self.t = 0.0
        self.warp_stars = WarpStarfield(800)
        self.lasers = LaserShockwave()
        
        # Smooth orientation angles
        self.v_roll = 0.0
        self.v_pitch = 0.0
        self.v_yaw = 0.0
        
        self.prog_idx = 0
        self.tunnel_pos = 0.0

    def init_gl(self):
        glClearColor(0.015, 0.015, 0.03, 1.0)
        glEnable(GL_DEPTH_TEST)
        glDepthFunc(GL_LEQUAL)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glEnable(GL_LINE_SMOOTH)
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST)

    def set_camera(self):
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(60.0, WIN_W / WIN_H, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        
        # Camera fixed along tunnel with slight cockpit banking on Roll
        roll_rad = math.radians(self.v_roll * 0.25)
        up_x = -math.sin(roll_rad)
        up_y = math.cos(roll_rad)
        gluLookAt(0.0, 0.0, 4.5, 0.0, 0.0, -15.0, up_x, up_y, 0.0)

    def draw_cyber_grid(self, primary_col, accent_col, cutoff_norm, beat_pulse):
        """Draws infinite perspective cyber-grid on floor and ceiling rushing backwards."""
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        
        grid_speed = 12.0  # Speed of tunnel motion
        self.tunnel_pos = (self.tunnel_pos + grid_speed * 0.016) % 2.0
        
        glLineWidth(1.5)
        
        # 1. Floor & Ceiling Longitudinal Lines
        glBegin(GL_LINES)
        num_lanes = 12
        for i in range(-num_lanes, num_lanes + 1):
            lx = i * 0.9
            # Color fade along Z depth
            alpha = (0.25 + 0.35 * cutoff_norm) * (1.0 if i % 2 == 0 else 0.5)
            
            # Floor lanes
            glColor4f(primary_col[0], primary_col[1], primary_col[2], alpha)
            glVertex3f(lx, -2.4, 4.0)
            glColor4f(primary_col[0], primary_col[1], primary_col[2], 0.0)
            glVertex3f(lx * 0.2, -2.4, -28.0)
            
            # Ceiling lanes
            glColor4f(accent_col[0], accent_col[1], accent_col[2], alpha * 0.7)
            glVertex3f(lx, 2.4, 4.0)
            glColor4f(accent_col[0], accent_col[1], accent_col[2], 0.0)
            glVertex3f(lx * 0.2, 2.4, -28.0)
        glEnd()
        
        # 2. Horizontal Grid Crossbars (Moving forward)
        glBegin(GL_LINES)
        num_bars = 24
        for b in range(num_bars):
            bz = 3.0 - (b * 1.3 + self.tunnel_pos * 1.3)
            if bz < -26.0:
                continue
            z_fade = max(0.0, min(1.0, (bz + 26.0) / 18.0)) * (0.35 + 0.45 * beat_pulse)
            
            # Floor crossbar
            glColor4f(primary_col[0], primary_col[1], primary_col[2], z_fade)
            w = 11.0 * (1.0 - (bz / -28.0) * 0.8)
            glVertex3f(-w, -2.4, bz)
            glVertex3f(w, -2.4, bz)
            
            # Ceiling crossbar
            glColor4f(accent_col[0], accent_col[1], accent_col[2], z_fade * 0.6)
            glVertex3f(-w, 2.4, bz)
            glVertex3f(w, 2.4, bz)
        glEnd()

    def draw_cyber_monolith(self, primary_col, laser_col, cutoff_norm, beat_pulse):
        """Draws central rotating 3D futuristic hologram monolith / wireframe polyhedron."""
        glPushMatrix()
        glTranslatef(0.0, 0.0, -3.5)
        
        # Rotate with IMU pose!
        glRotatef(self.v_yaw, 0.0, 1.0, 0.0)
        glRotatef(self.v_pitch, 1.0, 0.0, 0.0)
        glRotatef(self.v_roll, 0.0, 0.0, 1.0)
        
        # Scale breathing with techno beat pulse & filter cutoff
        scale = (1.2 + 0.3 * beat_pulse + 0.4 * cutoff_norm)
        glScalef(scale, scale, scale)
        
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        
        # Outer Icosahedron / Diamond wireframe
        glLineWidth(2.2)
        glColor4f(primary_col[0], primary_col[1], primary_col[2], 0.85)
        
        # Diamond vertices
        r = 1.0
        h = 1.6
        pts = [
            (0, h, 0), (r, 0, 0), (0, 0, r), (-r, 0, 0), (0, 0, -r), (0, -h, 0)
        ]
        edges = [
            (0,1),(0,2),(0,3),(0,4),
            (5,1),(5,2),(5,3),(5,4),
            (1,2),(2,3),(3,4),(4,1)
        ]
        
        glBegin(GL_LINES)
        for e in edges:
            v1, v2 = pts[e[0]], pts[e[1]]
            glVertex3f(v1[0], v1[1], v1[2])
            glVertex3f(v2[0], v2[1], v2[2])
        glEnd()
        
        # Inner glowing energy core (Pulsing Octahedron)
        glLineWidth(1.5)
        glColor4f(laser_col[0], laser_col[1], laser_col[2], 0.95)
        core_s = 0.5 + 0.3 * math.sin(self.t * 6.0)
        glBegin(GL_LINES)
        for e in edges:
            v1, v2 = pts[e[0]], pts[e[1]]
            glVertex3f(v1[0] * core_s, v1[1] * core_s, v1[2] * core_s)
            glVertex3f(v2[0] * core_s, v2[1] * core_s, v2[2] * core_s)
        glEnd()
        
        glPopMatrix()

    def draw_laser_cannons(self, laser_col, cutoff_norm, gyro_mag):
        """Sweeping overhead laser beams and fan cannons (Techno Festival Style)."""
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        
        num_lasers = 6
        glLineWidth(2.5)
        
        glBegin(GL_LINES)
        for i in range(num_lasers):
            pct = (i / (num_lasers - 1)) - 0.5  # -0.5 to 0.5
            src_x = pct * 10.0
            src_y = 2.4
            src_z = -1.0
            
            # Laser sweep angle modulated by tempo and filter cutoff
            sweep_angle = math.sin(self.t * 3.5 + i * 0.8) * (0.6 + 0.8 * cutoff_norm)
            target_x = src_x + math.sin(sweep_angle) * 14.0
            target_y = -2.4
            target_z = -22.0
            
            alpha = (0.55 + 0.45 * cutoff_norm)
            glColor4f(laser_col[0], laser_col[1], laser_col[2], alpha)
            glVertex3f(src_x, src_y, src_z)
            glColor4f(laser_col[0], laser_col[1], laser_col[2], 0.0)
            glVertex3f(target_x, target_y, target_z)
        glEnd()

    def draw_electric_waveform(self, primary_col, laser_col, cutoff_norm):
        """Draws vibrant, high-energy laser audio waveform running horizontally."""
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE)
        
        segments = 160
        glLineWidth(2.8)
        
        glBegin(GL_LINE_STRIP)
        for i in range(segments + 1):
            pct = (i / segments) * 2.0 - 1.0  # -1.0 to 1.0
            x = pct * 7.5
            z = -3.2
            
            # Dynamic multi-frequency techno laser waveform with noise bite
            freq1 = 8.0 + cutoff_norm * 14.0
            w1 = math.sin(pct * freq1 + self.t * 12.0)
            w2 = math.cos(pct * 24.0 - self.t * 18.0) * 0.4
            noise_bite = math.sin(pct * 65.0 + self.t * 28.0) * (0.15 + cutoff_norm * 0.25)
            
            # Envelope (faded edges)
            env = (1.0 - abs(pct) ** 2)
            y = -1.6 + (w1 + w2 + noise_bite) * (0.35 + 0.65 * cutoff_norm) * env
            
            glColor4f(laser_col[0], laser_col[1], laser_col[2], 0.85 * env)
            glVertex3f(x, y, z)
        glEnd()

    def draw_hud(self, font, rfps, is_snap, prog_name, cutoff_hz, gyro_mag, volume, beat_pulse):
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0, WIN_W, 0, WIN_H, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
        glDisable(GL_DEPTH_TEST)
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

        def blit_text(text, x, y, color):
            surf = font.render(text, True, color)
            data = pygame.image.tobytes(surf, "RGBA", True)
            tw, th = surf.get_size()
            tex = int(glGenTextures(1))
            glBindTexture(GL_TEXTURE_2D, tex)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, data)
            
            glEnable(GL_TEXTURE_2D)
            glColor4f(1.0, 1.0, 1.0, 1.0)
            glBegin(GL_QUADS)
            glTexCoord2f(0, 0); glVertex2f(x,      y)
            glTexCoord2f(1, 0); glVertex2f(x + tw, y)
            glTexCoord2f(1, 1); glVertex2f(x + tw, y + th)
            glTexCoord2f(0, 1); glVertex2f(x,      y + th)
            glEnd()
            glDisable(GL_TEXTURE_2D)
            glDeleteTextures(1, [tex])

        # Header Title
        mode_tag = "MELODIC TECHNO ARPEGGIATOR (124 BPM)" if is_snap else "CONTINUOUS ANALOG GLIDE"
        blit_text(f"GENRE: {mode_tag}   [SPACE: Mode]", 24, WIN_H - 32, (0, 240, 255))
        blit_text(f"SEQUENCE: {prog_name}", 24, WIN_H - 56, (255, 230, 100))
        
        # Real-time filter sweep gauge
        filter_bars = int(min(24, max(1, (math.log(cutoff_hz / 150.0 + 1.0) / 4.0) * 24)))
        gauge_str = "█" * filter_bars + "░" * (24 - filter_bars)
        blit_text(f"FILTER CUTOFF: [{gauge_str}] {cutoff_hz:.0f} Hz  (Tilt Pitch)", 24, WIN_H - 80, (180, 220, 255))

        # Bottom telemetry
        blit_text(f"FPS: {rfps:.0f}  •  TEMPO: 124 BPM  •  Motion: {gyro_mag:.0f}°/s  •  Out: {volume:.2f}", 24, 20, (120, 150, 180))
        blit_text("UP Jerk: 909 Kick  •  DOWN Jerk: Techno Clap  •  [R] Reset Yaw  •  [Q/ESC] Quit", WIN_W - 580, 20, (120, 150, 180))

        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopMatrix()

    def render(self, roll, pitch, yaw, kick_hit, snare_hit, gyro_mag, is_snap, prog_idx, prog_name, cutoff_norm, cutoff_hz, beat_pulse, dt):
        self.t += dt
        self.prog_idx = prog_idx
        
        # Smooth IMU angles
        self.v_roll += (roll - self.v_roll) * 8.0 * dt
        self.v_pitch += (pitch - self.v_pitch) * 8.0 * dt
        self.v_yaw += (yaw - self.v_yaw) * 8.0 * dt

        palette = TECHNO_PALETTES[prog_idx % len(TECHNO_PALETTES)]
        primary_col, accent_col, laser_col = palette

        # Drum triggers
        if kick_hit:
            self.lasers.trigger_kick(primary_col)
        if snare_hit:
            self.lasers.trigger_snare(accent_col)

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        self.set_camera()
        
        # 1. Warp Speed Cyber Stardust
        self.warp_stars.update_and_draw(dt, cutoff_norm * 0.6 + (1.0 if kick_hit else 0.0), primary_col)
        
        # 2. Infinite Cyber-Grid Tunnel
        self.draw_cyber_grid(primary_col, accent_col, cutoff_norm, beat_pulse)
        
        # 3. Central 3D Cyber-Monolith
        self.draw_cyber_monolith(primary_col, laser_col, cutoff_norm, beat_pulse)
        
        # 4. Sweeping Overhead Laser Cannons
        self.draw_laser_cannons(laser_col, cutoff_norm, gyro_mag)
        
        # 5. Electric Audio Waveform
        self.draw_electric_waveform(primary_col, laser_col, cutoff_norm)
        
        # 6. Laser Shockwaves & Flash
        self.lasers.update_and_draw(dt)
