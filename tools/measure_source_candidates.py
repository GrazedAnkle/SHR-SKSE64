#!/usr/bin/env python3
"""Rank heart-sound recordings for use as a SYNTHESIS SOURCE sample (not analysis).

For a source we want the opposite of an analysis clip: a clean, isolated, BRIGHT
resting beat. This reports, per file:
  - format (sample rate, channels, duration)
  - S1 spectral centroid (Hz), measured in a window around the loudest transient
  - HF energy fraction: share of spectral energy above HF_SPLIT_HZ - the material the
    inspiratory low-pass muffle actually has to act on (the whole reason for a brighter source)
  - noise floor and a crude SNR (peak-window RMS vs a low-percentile window RMS)
  - crest factor and clipping fraction in the loud beat - QUALITY guards. Distortion and
    saturation ADD high-frequency harmonics, so they inflate centroid/HF: a high HF number is
    only "bright" if the beat is also CLEAN. A saturated beat has low crest (flat-topped peaks)
    and/or a non-trivial clip fraction. Treat high HF + low crest / nonzero clip% as suspect.

This is the dedicated SOURCE-screening centroid (2 kHz ceiling, HF fraction), distinct from the
magnitude centroid in shrlib that owns the committed measurements. Runs in the analysis venv
(numpy + soundfile via shrlib); samples are scaled to int16 magnitude so the clip guard reads
in full-scale units. Usage:
    python tools/measure_source_candidates.py docs/references/clips/*.wav
    python tools/measure_source_candidates.py docs/references/clips
"""

from __future__ import annotations

import glob
import os
import sys

import numpy as np
import soundfile as sf

import shrlib

HF_SPLIT_HZ = 200.0  # "high frequency" boundary for the muffle's working material
BAND_MAX_HZ = 2000.0  # ignore content above this (heart sounds are low; cuts hiss bias)
FFT_N = 4096  # analysis window
WIN_MS = 40  # RMS window for the noise-floor scan
CLIP_THRESH = 32000.0  # ~0.977 of int16 full scale


def spectrum_at(mono: np.ndarray, center: int, sr: int) -> tuple[np.ndarray, np.ndarray]:
    """Hann-windowed magnitude spectrum (f, |S|) over FFT_N samples centred on `center`."""
    a = max(0, center - FFT_N // 2)
    seg = mono[a : a + FFT_N]
    if len(seg) < FFT_N:
        seg = np.concatenate([seg, np.zeros(FFT_N - len(seg))])
    return shrlib.spec(seg, sr, FFT_N)


def centroid_and_hf(f: np.ndarray, S: np.ndarray) -> tuple[float, float]:
    """Magnitude centroid (Hz) and energy fraction above HF_SPLIT_HZ, over 0 < f <= BAND_MAX_HZ."""
    m = (f > 0) & (f <= BAND_MAX_HZ)
    fb, Sb = f[m], S[m]
    den = float(Sb.sum())
    if den == 0:
        return 0.0, 0.0
    return float((fb * Sb).sum() / den), float(Sb[fb >= HF_SPLIT_HZ].sum() / den)


def rms_windows(mono: np.ndarray, sr: int) -> tuple[np.ndarray, np.ndarray]:
    """RMS and centre index of each non-overlapping WIN_MS window across the signal."""
    w = max(1, int(WIN_MS * 0.001 * sr))
    starts = np.arange(0, len(mono) - w, w)
    if starts.size == 0:
        return np.zeros(0), np.zeros(0, dtype=int)
    frames = mono[starts[:, None] + np.arange(w)[None, :]]
    return np.sqrt((frames**2).mean(axis=1)), starts + w // 2


def measure(path: str) -> dict:
    info = sf.info(path)
    dur = info.frames / info.samplerate if info.samplerate else 0.0
    base = dict(name=os.path.basename(path), sr=info.samplerate, ch=info.channels, dur=dur)
    mono, sr = shrlib.load(path)
    mono = mono * 32768.0  # int16 magnitude, so CLIP_THRESH reads in full-scale units

    rms, centers = rms_windows(mono, sr)
    if rms.size == 0:
        return {**base, "err": "too short"}
    noise = float(np.sort(rms)[max(0, rms.size // 10)])  # 10th-percentile window RMS
    pk = int(np.argmax(rms))
    peak_rms, peak_center = float(rms[pk]), int(centers[pk])
    snr = 20 * np.log10(peak_rms / noise) if noise > 0 else float("inf")

    f, S = spectrum_at(mono, peak_center, sr)
    cen, hf = centroid_and_hf(f, S)

    # Quality guards within +/-60ms of the loud transient.
    hw = int(0.06 * sr)
    seg = mono[max(0, peak_center - hw) : peak_center + hw]
    speak = float(np.abs(seg).max()) if seg.size else 0.0
    srms = float(np.sqrt((seg**2).mean())) if seg.size else 1.0
    crest = 20 * np.log10(speak / srms) if srms > 0 else 0.0
    clip_frac = float(np.count_nonzero(np.abs(mono) >= CLIP_THRESH)) / len(mono)

    return {
        **base,
        "centroid": cen,
        "hf": hf,
        "snr": snr,
        "noise": noise,
        "peak": peak_rms,
        "crest": crest,
        "clip": clip_frac,
        "err": None,
    }


def expand(args: list[str]) -> list[str]:
    files: list[str] = []
    for a in args:
        if os.path.isdir(a):
            files += sorted(glob.glob(os.path.join(a, "*.wav")))
        else:
            files += sorted(glob.glob(a))
    return files


def main() -> int:
    files = expand(sys.argv[1:] or ["docs/references/clips"])
    if not files:
        print("no .wav files found")
        return 1
    rows = [measure(f) for f in files]
    ok = [r for r in rows if not r["err"]]
    # Rank by a simple desirability: bright (centroid) AND clean (snr) AND has HF to work with.
    ok.sort(key=lambda r: (r["centroid"], r["hf"], r["snr"]), reverse=True)

    print(f"{'file':<40} {'fmt':<12} {'dur':>6}  {'cen':>6} {'HF>':>5} {'SNR':>6} {'crest':>6} {'clip':>6}")
    print(
        f"{'':<40} {'':<12} {'(s)':>6}  {'(Hz)':>6} {str(int(HF_SPLIT_HZ)) + 'Hz':>5} {'(dB)':>6} {'(dB)':>6} {'%':>6}"
    )
    print("-" * 98)
    for r in ok:
        print(
            f"{r['name']:<40} {str(r['sr']) + 'Hz/' + str(r['ch']) + 'ch':<12} {r['dur']:>6.1f}  "
            f"{r['centroid']:>6.1f} {r['hf'] * 100:>4.0f}% {r['snr']:>6.1f} {r['crest']:>6.1f} {r['clip'] * 100:>5.2f}%"
        )
    for r in rows:
        if r["err"]:
            print(
                f"{r['name']:<40} {str(r['sr']) + 'Hz/' + str(r['ch']) + 'ch':<12} {r['dur']:>6.1f}  -- {r['err']}"
            )
    print("\ncen   = S1 spectral centroid (higher = brighter)")
    print(f"HF>   = fraction of spectral energy above {int(HF_SPLIT_HZ)}Hz (what the muffle can act on)")
    print("SNR   = peak-window RMS over 10th-percentile-window RMS (higher = cleaner)")
    print("crest = peak/RMS in the loud beat (LOW = compressed/saturated; clean transients ~15-25dB)")
    print("clip  = fraction of samples near full scale (>0 suggests clipping/saturation)")
    print("\nNOTE: high HF with LOW crest or nonzero clip% is suspect - distortion fakes brightness.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
