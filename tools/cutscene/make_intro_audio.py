#!/usr/bin/env python3
"""Synthesise the crash intro's soundtrack: assets/cutscene/intro.ogg.

    tools/sprite_forge/.venv/bin/python tools/cutscene/make_intro_audio.py

One track for the whole intro, timed to ArenaIntro.cpp (the times below
must match its constants):

    1.2 s   a distress siren, far off behind the camera
    ~7.1 s  the ship passes the camera: the siren and the fire roar peak,
            sweep left to right and drop in pitch (Doppler: the sound is
            simulated from a moving source, delayed by its distance)
    12.7 s  cut to black: the crash, a deep boom with metal and debris
    to 16.8 s the boom rings out under the fade-in

Plain numpy; ffmpeg encodes the OGG. Everything is made here, nothing is
sampled, so there is nothing to credit.
"""

import math
import os
import subprocess
import sys
import wave

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "assets", "cutscene", "intro.ogg")

RATE = 44100
LENGTH = 16.8
SHIP_PASS = 7.1     # closest to the camera (ArenaIntro.cpp SHIP_PASS)
CRASH = 12.7        # the cut to black (ArenaIntro.cpp CRASH)
SIREN_START = 1.2

# The ship in "audio metres": it comes from behind the listener, passes on
# the right and falls away ahead. A slow speed of sound makes the Doppler
# drop big enough to hear over a short pass.
SPEED = 30.0        # m/s
SOUND = 170.0       # m/s
PASS_OFFSET = 3.5   # m to the side at the closest point
REF_DISTANCE = 6.0  # loudness 1 at this distance

rng = np.random.default_rng(11)
t = np.arange(int(RATE * LENGTH)) / RATE


def ship_position(time):
    """Listener at the origin facing -z; x is right."""
    along = -SPEED * (time - SHIP_PASS)          # + behind, - ahead
    # It drifts in from the right and slides toward the middle as it falls.
    side = PASS_OFFSET + 0.08 * np.maximum(0, SHIP_PASS - time) * SPEED * 0.25
    return side, along


def emission_time(time):
    """Solve tau + distance(tau) / SOUND = time for each output sample."""
    tau = time.copy()
    for _ in range(30):
        x, z = ship_position(tau)
        tau = time - np.sqrt(x * x + z * z) / SOUND
    return tau


def lowpass(signal, cutoff):
    spec = np.fft.rfft(signal)
    freqs = np.fft.rfftfreq(len(signal), 1.0 / RATE)
    spec *= 1.0 / (1.0 + (freqs / cutoff) ** 4)
    return np.fft.irfft(spec, len(signal))


def bandpass(signal, low, high):
    spec = np.fft.rfft(signal)
    freqs = np.fft.rfftfreq(len(signal), 1.0 / RATE)
    spec *= 1.0 / (1.0 + (low / np.maximum(freqs, 1.0)) ** 4)
    spec *= 1.0 / (1.0 + (freqs / high) ** 4)
    return np.fft.irfft(spec, len(signal))


def smoothstep(a, b, x):
    u = np.clip((x - a) / (b - a), 0.0, 1.0)
    return u * u * (3 - 2 * u)


def siren_wave(tau):
    """A two-tone distress wail at emission time tau: phase from the
    integral of its frequency, so a moving source bends it smoothly."""
    cycle = 1.1  # seconds per wee-woo
    phase_in_cycle = (tau % cycle) / cycle
    # Glide between the two tones rather than step: more alarm, less doorbell.
    glide = 0.5 - 0.5 * np.cos(2 * math.pi * phase_in_cycle)
    freq = 620.0 + 330.0 * glide
    # Integrate frequency over emission time.
    dtau = np.diff(tau, prepend=tau[0])
    phase = 2 * math.pi * np.cumsum(freq * dtau)
    # A hard, horn-like timbre: odd harmonics plus a little buzz.
    wave_ = (np.sin(phase) + 0.45 * np.sin(3 * phase) + 0.22 * np.sin(5 * phase)
             + 0.12 * np.sin(7 * phase) + 0.15 * np.sin(2 * phase))
    # Slight warble: the speaker is damaged.
    wave_ *= 1.0 + 0.12 * np.sin(2 * math.pi * 7.0 * tau)
    return wave_ * 0.35


def place(mono, distance, pan, head_shadow=True):
    """Distance loudness, air (highs fade with distance) and equal-power pan."""
    # Softer than 1/r, so the far-off siren is faint but heard from the start.
    gain = np.minimum(1.0, (REF_DISTANCE / np.maximum(distance, 0.5)) ** 0.75)
    near = lowpass(mono, 9000.0)
    far = lowpass(mono, 1400.0)
    airmix = np.clip(distance / 120.0, 0, 1)
    sig = (near * (1 - airmix) + far * airmix) * gain
    left = sig * np.cos((pan + 1) * math.pi / 4)
    right = sig * np.sin((pan + 1) * math.pi / 4)
    return np.stack([left, right], axis=1)


def moving_source():
    tau = emission_time(t)
    x, z = ship_position(tau)
    distance = np.sqrt(x * x + z * z)
    pan = np.clip(x / np.maximum(distance, 1e-3), -1, 1) * 0.9
    # Behind the listener the source sits in front's mirror; damp it a little.
    behind = np.clip(z / np.maximum(distance, 1e-3), 0, 1)

    siren = siren_wave(tau) * smoothstep(SIREN_START, SIREN_START + 2.5, t)
    siren *= 1.0 - 0.35 * behind
    # The fire roar: brown noise read at emission time, so it bends too.
    noise = np.cumsum(rng.standard_normal(len(t)))
    noise -= lowpass(noise, 8.0)
    noise /= np.abs(noise).max()
    idx = np.clip((tau * RATE).astype(int), 0, len(t) - 1)
    roar = bandpass(noise, 60.0, 2500.0)[idx] * 1.6
    hiss = bandpass(rng.standard_normal(len(t)), 1500.0, 9000.0)[idx] * 0.18
    fire = (roar + hiss) * smoothstep(SHIP_PASS - 3.5, SHIP_PASS - 0.7, t)
    stereo = place(siren + fire, distance, pan)
    # The whoosh: a short burst of air as it passes, wide and close.
    whoosh_env = np.exp(-((t - SHIP_PASS - 0.05) / 0.22) ** 2)
    whoosh = bandpass(rng.standard_normal(len(t)), 200.0, 5000.0) * whoosh_env * 0.55
    sweep = np.clip((t - SHIP_PASS + 0.35) / 0.7, 0, 1) * 2 - 1
    stereo[:, 0] += whoosh * np.cos((sweep + 1) * math.pi / 4)
    stereo[:, 1] += whoosh * np.sin((sweep + 1) * math.pi / 4)
    # Everything from the ship is gone at the cut.
    stereo *= (1.0 - smoothstep(CRASH - 0.02, CRASH + 0.04, t))[:, None]
    return stereo


def crash():
    out = np.zeros((len(t), 2))
    s = np.maximum(t - CRASH, 0.0)
    on = (t >= CRASH).astype(float)
    # The impact: a falling sub-bass thump.
    f = 70.0 * np.exp(-s * 3.0) + 32.0
    phase = 2 * math.pi * np.cumsum(f) / RATE
    boom = np.sin(phase) * np.exp(-s * 1.4) * on * 1.3
    # The blast: a noise burst that closes down as it rolls away.
    burst = rng.standard_normal(len(t))
    blast = (lowpass(burst, 3000.0) * np.exp(-s * 9.0)
             + lowpass(burst, 400.0) * np.exp(-s * 1.6) * 1.4) * on
    # Metal: inharmonic clangs, a few scattered hits.
    metal = np.zeros(len(t))
    for when, base, amp in ((0.0, 210.0, 0.5), (0.18, 340.0, 0.3), (0.42, 155.0, 0.35),
                            (0.75, 470.0, 0.18), (1.1, 260.0, 0.15)):
        u = np.maximum(t - CRASH - when, 0.0)
        hit = (t >= CRASH + when)
        tone = sum(np.sin(2 * math.pi * base * r * u) / (k + 1)
                   for k, r in enumerate((1.0, 2.76, 5.4, 8.93)))
        metal += tone * np.exp(-u * 4.5) * hit * amp
    # Debris: crackles thinning out over two seconds.
    crackle = np.zeros(len(t))
    for _ in range(140):
        when = CRASH + 0.05 + rng.exponential(0.7)
        if when > LENGTH - 0.2:
            continue
        i = int(when * RATE)
        n = int(RATE * 0.012)
        crackle[i:i + n] += rng.standard_normal(n) * np.exp(-np.arange(n) / (n / 4)) * \
            0.5 * math.exp(-(when - CRASH) * 1.2)
    crackle = bandpass(crackle, 800.0, 7000.0)
    mono = boom + blast + metal * 0.5
    out[:, 0] = mono + crackle * rng.uniform(0.3, 1.0)
    out[:, 1] = mono + np.roll(crackle, 211)
    # A cheap room: a few late echoes, darker each time.
    tail = np.zeros_like(out)
    for delay, gain in ((0.083, 0.35), (0.149, 0.28), (0.231, 0.2), (0.367, 0.14)):
        d = int(delay * RATE)
        tail[d:, 0] += out[:-d, 1] * gain
        tail[d:, 1] += out[:-d, 0] * gain
    tail[:, 0] = lowpass(tail[:, 0], 1800.0)
    tail[:, 1] = lowpass(tail[:, 1], 1800.0)
    return out + tail


def main():
    mix = moving_source() + crash() * 0.6
    # Fades at both ends, then a soft limit so the boom does not clip.
    mix *= smoothstep(0.0, 0.3, t)[:, None] * (1 - smoothstep(LENGTH - 1.5, LENGTH, t))[:, None]
    mix = np.tanh(mix * 0.9)
    mix /= max(1.0, np.abs(mix).max() / 0.95)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    wav = OUT[:-4] + ".wav"
    with wave.open(wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((mix * 32767).astype("<i2").tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis",
                    "-q:a", "5", OUT], check=True)
    os.remove(wav)
    print("wrote", os.path.relpath(OUT, REPO), f"{LENGTH:.1f} s")


if __name__ == "__main__":
    sys.exit(main())
