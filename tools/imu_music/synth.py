"""
Melodic Techno Synthesizer Engine for IMU Music.
Inspired by Afterlife, Tale of Us, Anyma, Stephan Bodzin, and Boris Brejcha.

Features:
- 124 BPM Hypnotic 16th-note Melodic Arpeggiator & Bassline
- Resonant Filter Sweeps modulated by IMU Pitch (closed dark sub -> open soaring lead)
- Dual Detuned Sawtooth + Sub-Oscillator (Prophet / Moog lead)
- Sidechain pumping compression emulation for rolling techno groove
- 909-style Punchy Techno Kick (Upward Jerk) & Snappy Techno Clap (Downward Jerk)
- Snapped Mode: Hypnotic minor arpeggio sequence (D Minor / A Minor / F Major / C Major)
- Continuous Mode: Gliding analog lead with portamento & filter resonance
"""

import math
import random
import threading
import numpy as np
import sounddevice as sd

SAMPLE_RATE = 44100
BLOCK_SIZE = 512
BPM = 124.0
STEP_DURATION = 60.0 / (BPM * 4.0)  # 16th note in seconds (~0.121s)

# Pre-generate noise buffer for claps, hi-hats, and analog hiss
NOISE_LEN = SAMPLE_RATE * 2
_raw_noise = [random.uniform(-1.0, 1.0) for _ in range(NOISE_LEN)]
NOISE_TABLE = np.array(_raw_noise, dtype=np.float32)

# Melodic Techno Minor Scales & Arpeggio Sequences (Frequencies in Hz)
# 4 progression sections: Dm -> Bb -> F -> C
PROGRESSIONS = [
    # D Minor Techno Arp (D2, D3, F3, A3, D4, C4, A3, F3, D3, A3, F3, G3, A3, D4, C4, E4)
    ("D Minor (Afterlife)", [73.42, 146.83, 174.61, 220.00, 293.66, 261.63, 220.00, 174.61,
                             146.83, 220.00, 174.61, 196.00, 220.00, 293.66, 261.63, 329.63]),
    # Bb Major Techno Arp (Bb1, F2, Bb2, D3, F3, A3, F3, D3, Bb2, D3, F3, G3, A3, D4, C4, A3)
    ("Bb Major (Hypnotic)", [58.27, 87.31, 116.54, 146.83, 174.61, 220.00, 174.61, 146.83,
                             116.54, 146.83, 174.61, 196.00, 220.00, 293.66, 261.63, 220.00]),
    # F Major / Dm7 Arp (F2, C3, F3, A3, C4, E4, C4, A3, F3, A3, C4, D4, E4, C4, A3, G3)
    ("F Major (Euphoria)",  [87.31, 130.81, 174.61, 220.00, 261.63, 329.63, 261.63, 220.00,
                             174.61, 220.00, 261.63, 293.66, 329.63, 261.63, 220.00, 196.00]),
    # C Major / Am7 Arp (C2, G2, C3, E3, G3, B3, G3, E3, C3, E3, G3, A3, B3, G3, E3, D3)
    ("C Dark (Peak Time)",  [65.41, 98.00, 130.81, 164.81, 196.00, 246.94, 196.00, 164.81,
                             130.81, 164.81, 196.00, 220.00, 246.94, 196.00, 164.81, 146.83])
]

class Synth:
    def __init__(self):
        self.running = False
        self.stream = None
        self.lock = threading.Lock()
        
        # Mode & progression
        self.snap = True  # Default to driving techno arpeggiator!
        self.prog_idx = 0
        self.prog_name = PROGRESSIONS[0][0]
        
        # Sequencer clock
        self.time = 0.0
        self.step_idx = 0
        self.current_note_freq = 146.83
        
        # Dual detuned oscillators
        self.phase1 = 0.0
        self.phase2 = 0.0
        self.phase_sub = 0.0
        
        # Continuous mode lead frequency
        self.cont_freq = 146.83
        self.target_cont_freq = 146.83
        
        # Filter & Techno controls
        self.filter_cutoff_norm = 0.5  # Modulated by Pitch (0.0 to 1.0)
        self.resonance = 0.65
        self.filter_state_l = 0.0
        self.filter_state_r = 0.0
        self.sidechain_duck = 0.0
        
        # Noise pointer
        self.noise_ptr = 0
        
        # Drum triggers (seconds timestamps)
        self.kick_t = -1.0
        self.snare_t = -1.0
        
        # Telemetry for visualizer
        self.current_volume = 0.0
        self.sub_bass_energy = 0.0
        self.current_cutoff_hz = 1000.0
        self.beat_pulse = 0.0

    def set_snap(self, snap: bool):
        with self.lock:
            self.snap = snap

    def trigger_kick(self):
        with self.lock:
            self.kick_t = self.time

    def trigger_snare(self):
        with self.lock:
            self.snare_t = self.time

    def update_imu(self, roll: float, pitch: float, gyro_mag: float, accel_mag: float):
        """Map IMU movements to Melodic Techno parameters."""
        # Roll (-90 to +90) selects progression chord or pitch
        r_norm = float(np.clip((roll + 90.0) / 180.0, 0.0, 1.0))
        # Pitch (-90 to +90) controls the iconic Techno Resonant Filter Sweep!
        p_norm = float(np.clip((pitch + 90.0) / 180.0, 0.0, 1.0))
        
        with self.lock:
            # Pitch directly opens/closes the resonant filter (150 Hz sub to 8500 Hz screaming lead)
            self.filter_cutoff_norm = p_norm
            # Exponential filter curve for natural ear response
            self.current_cutoff_hz = 150.0 * (55.0 ** p_norm)
            
            if self.snap:
                # Select 1 of 4 melodic techno chord progressions
                p_idx = int(r_norm * len(PROGRESSIONS))
                p_idx = min(p_idx, len(PROGRESSIONS) - 1)
                self.prog_idx = p_idx
                self.prog_name = PROGRESSIONS[p_idx][0]
            else:
                # Continuous Mode: Lead frequency glides from D2 (73Hz) up to D5 (587Hz)
                self.target_cont_freq = 73.42 * (2.0 ** (r_norm * 3.0))
                self.prog_name = f"Continuous Lead ({self.target_cont_freq:.1f} Hz)"

    def _audio_callback(self, outdata, frames, time_info, status):
        with self.lock:
            is_snap = self.snap
            prog_idx = self.prog_idx
            target_cont_f = self.target_cont_freq
            cutoff_norm = self.filter_cutoff_norm
            kick_t = self.kick_t
            snare_t = self.snare_t
            t_start = self.time

        # Block time array
        dt_block = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
        t_arr = t_start + dt_block
        
        # Noise buffer slice
        n_start = self.noise_ptr
        n_end = n_start + frames
        if n_end <= NOISE_LEN:
            noise_slice = NOISE_TABLE[n_start:n_end]
            self.noise_ptr = n_end % NOISE_LEN
        else:
            p1 = NOISE_TABLE[n_start:NOISE_LEN]
            p2 = NOISE_TABLE[0:n_end - NOISE_LEN]
            noise_slice = np.concatenate((p1, p2))
            self.noise_ptr = n_end - NOISE_LEN

        # Determine note frequency for this block
        if is_snap:
            # 16-step Melodic Techno Arpeggiator
            seq = PROGRESSIONS[prog_idx][1]
            current_step = int((t_start / STEP_DURATION) % 16)
            self.step_idx = current_step
            # Short envelope decay per 16th note for rhythmic pluck
            step_phase = (t_start % STEP_DURATION) / STEP_DURATION
            note_freq = seq[current_step]
            # Pluck envelope: sharp attack, exponential decay
            pluck_env = np.exp(-4.5 * np.linspace(step_phase, step_phase + frames / (SAMPLE_RATE * STEP_DURATION), frames))
            pluck_env = np.clip(pluck_env, 0.05, 1.0)
            self.beat_pulse = float(pluck_env[0])
        else:
            # Continuous analog lead glide
            self.cont_freq += (target_cont_f - self.cont_freq) * 0.15
            note_freq = self.cont_freq
            pluck_env = np.ones(frames, dtype=np.float32) * 0.8
            self.beat_pulse = 0.5

        # --- Dual Detuned Sawtooth Oscillators + Sub Bass ---
        detune_hz = 0.85  # Fat stereo unison detune
        freq_l = note_freq - detune_hz
        freq_r = note_freq + detune_hz
        freq_sub = note_freq * 0.5
        
        # Phase progression
        inc_l = 2.0 * np.pi * freq_l / SAMPLE_RATE
        inc_r = 2.0 * np.pi * freq_r / SAMPLE_RATE
        inc_sub = 2.0 * np.pi * freq_sub / SAMPLE_RATE
        
        phases_l = self.phase1 + np.cumsum(np.full(frames, inc_l, dtype=np.float32))
        phases_r = self.phase2 + np.cumsum(np.full(frames, inc_r, dtype=np.float32))
        phases_sub = self.phase_sub + np.cumsum(np.full(frames, inc_sub, dtype=np.float32))
        
        self.phase1 = phases_l[-1] % (2.0 * np.pi)
        self.phase2 = phases_r[-1] % (2.0 * np.pi)
        self.phase_sub = phases_sub[-1] % (2.0 * np.pi)
        
        # Band-limited-like sawtooth approximation (sum of 4 harmonics)
        saw_l = (np.sin(phases_l) + 0.5 * np.sin(phases_l * 2) + 0.25 * np.sin(phases_l * 3) + 0.125 * np.sin(phases_l * 4)) * 0.35
        saw_r = (np.sin(phases_r) + 0.5 * np.sin(phases_r * 2) + 0.25 * np.sin(phases_r * 3) + 0.125 * np.sin(phases_r * 4)) * 0.35
        # Warm sub sine
        sub_wave = np.sin(phases_sub) * 0.28
        
        # Mix synth body with pluck envelope
        synth_l = (saw_l + sub_wave) * pluck_env
        synth_r = (saw_r + sub_wave) * pluck_env
        
        # --- Resonant Lowpass Filter (Techno Filter Sweep) ---
        # 1-pole / 2-pole recursive smoothing for filter cutoff
        alpha = float(np.clip(cutoff_norm * 0.85 + 0.05, 0.05, 0.92))
        # Vectorized simple lowpass simulation
        filtered_l = np.empty(frames, dtype=np.float32)
        filtered_r = np.empty(frames, dtype=np.float32)
        fl = self.filter_state_l
        fr = self.filter_state_r
        for i in range(frames):
            fl += alpha * (synth_l[i] - fl)
            fr += alpha * (synth_r[i] - fr)
            filtered_l[i] = fl
            filtered_r[i] = fr
        self.filter_state_l = fl
        self.filter_state_r = fr
        
        # Add resonance bite when filter opens
        res_bite = (synth_l - filtered_l) * (cutoff_norm * 0.35)
        out_l = filtered_l + res_bite
        out_r = filtered_r + res_bite

        # --- Techno Hi-Hat / Shaker Groove (16th note offbeat) ---
        hat_env = np.zeros(frames, dtype=np.float32)
        hat_step = int((t_start / STEP_DURATION) % 4)
        if hat_step in (1, 3):  # Offbeat 16ths
            h_phase = (t_start % STEP_DURATION) / STEP_DURATION
            hat_env = np.exp(-22.0 * np.linspace(h_phase, h_phase + frames / (SAMPLE_RATE * STEP_DURATION), frames)) * 0.12
        out_l += noise_slice * hat_env * 0.7
        out_r += noise_slice * hat_env * 1.0

        # --- Punchy 909 Techno Kick (Upward Jerk) ---
        sub_peak = 0.0
        if kick_t >= 0:
            dt_k = t_arr - kick_t
            mask_k = (dt_k >= 0) & (dt_k < 0.42)
            if np.any(mask_k):
                dt_k_act = dt_k[mask_k]
                # Punchy techno pitch drop: 180 Hz punch down to 42 Hz sub thump
                k_freq = 180.0 * np.exp(-32.0 * dt_k_act) + 42.0
                k_phase = 2.0 * np.pi * np.cumsum(k_freq) / SAMPLE_RATE
                k_wave = np.sin(k_phase) * np.exp(-6.5 * dt_k_act)
                # Transient click
                k_click = noise_slice[mask_k] * np.exp(-90.0 * dt_k_act) * 0.35
                kick_sig = (k_wave + k_click) * 0.85
                
                # Sidechain ducking on synth during kick
                duck = np.exp(-8.0 * dt_k_act)
                out_l[mask_k] *= (1.0 - duck * 0.65)
                out_r[mask_k] *= (1.0 - duck * 0.65)
                
                out_l[mask_k] += kick_sig
                out_r[mask_k] += kick_sig
                sub_peak = float(np.max(duck))

        # --- Snappy Techno Clap / Snare (Downward Jerk) ---
        if snare_t >= 0:
            dt_s = t_arr - snare_t
            mask_s = (dt_s >= 0) & (dt_s < 0.32)
            if np.any(mask_s):
                dt_s_act = dt_s[mask_s]
                # Pre-delay multi-clap burst + noise tail
                clap_env = np.exp(-18.0 * dt_s_act)
                clap_sig = noise_slice[mask_s] * clap_env * 0.55
                # Tonal body (210 Hz)
                clap_tone = np.sin(2.0 * np.pi * 210.0 * dt_s_act) * np.exp(-30.0 * dt_s_act) * 0.25
                snare_out = clap_sig + clap_tone
                out_l[mask_s] += snare_out * 0.95
                out_r[mask_s] += snare_out * 1.05

        # --- Analog Master Saturation (Punchy Club Limiter) ---
        out_l = np.tanh(out_l * 1.65) * 0.82
        out_r = np.tanh(out_r * 1.65) * 0.82

        self.time += frames / SAMPLE_RATE
        self.current_volume = float(np.max(np.abs(out_l)))
        self.sub_bass_energy = sub_peak
        
        outdata[:, 0] = out_l
        outdata[:, 1] = out_r

    def start(self):
        self.stream = sd.OutputStream(
            samplerate=SAMPLE_RATE, 
            channels=2, 
            blocksize=BLOCK_SIZE,
            callback=self._audio_callback
        )
        self.stream.start()
        self.running = True

    def stop(self):
        if self.stream:
            self.stream.stop()
            self.stream.close()
        self.running = False
