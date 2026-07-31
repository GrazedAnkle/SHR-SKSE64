#!/usr/bin/env python3
"""Measure S2 "valve-clap" brightness and S1 envelope timing from references.

Reports:
  - S1 10%->90% rise and peak->10% decay times
  - S2 attack, band energy, and brightness relative to S1
  - tail sensitivity of full-window and peak-anchored S2 rulers

Reads docs/references/timestamps/<n>.txt and the matching original/<n>.wav through
shrlib (channel 0). Spectra are averaged by annotation group so ventilation and
noise variation wash out, and are only comparable within a recording. The clap
metrics use a 2 kHz ceiling instead of the 1.2 kHz ceiling used by committed
centroid measurements.

Usage:
    python tools/measure_clap.py 11      # all groups in timestamps/11.txt vs original/11.wav
    python tools/measure_clap.py 11 8 15
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

import shrlib
from shrlib import a_weight_gain
from shrlib import SR

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "docs" / "references" / "timestamps"
ORIG_DIR = ROOT / "docs" / "references" / "original"

FFT_N = 2048  # ~43 ms: ~50 Hz resolution while remaining within one sound.
BAND_MAX_HZ = 2000.0
HF_SPLIT_HZ = 150.0  # S2 clap boundary; references put ~37-38% of S2 energy above it.
# Separates the clap energy from the low body.
BANDS = [(40, 100), (100, 150), (150, 250), (250, 400), (400, 700), (700, 1200)]
ONSET_FFT_MS = 12  # Capture the S2 onset transient instead of the body.


def avg_spectrum(signal: np.ndarray, centers: list[float], nfft: int) -> tuple[np.ndarray, np.ndarray]:
    """Return the mean spectrum of nfft-sample Hann windows at `centers`."""
    acc = None
    freqs = None
    for c in centers:
        a = max(0, int(c) - nfft // 2)
        seg = signal[a : a + nfft]
        if len(seg) < nfft:
            seg = np.concatenate([seg, np.zeros(nfft - len(seg))])
        freqs, mag = shrlib.spec(seg, SR, nfft)
        acc = mag if acc is None else acc + mag
    if acc is None:
        return np.zeros(0), np.zeros(0)
    return freqs, acc / len(centers)


def centroid_hf(f: np.ndarray, S: np.ndarray) -> tuple[float, float]:
    """Return centroid and HF fraction over 0 < f <= BAND_MAX_HZ."""
    m = (f > 0) & (f <= BAND_MAX_HZ)
    fb, Sb = f[m], S[m]
    den = float(Sb.sum())
    if den == 0:
        return 0.0, 0.0
    return float((fb * Sb).sum() / den), float(Sb[fb >= HF_SPLIT_HZ].sum() / den)


def band_energy(f: np.ndarray, S: np.ndarray) -> list[float]:
    """Return each band's fraction of magnitude below BAND_MAX_HZ."""
    tot = float(S[(f > 0) & (f <= BAND_MAX_HZ)].sum()) or 1.0
    return [float(S[(f >= lo) & (f < hi)].sum() / tot) for lo, hi in BANDS]


def onset_nfft() -> int:
    """Return the smallest power of two covering the S2 onset window."""
    n = 1
    while n < int(ONSET_FFT_MS * 0.001 * SR):
        n *= 2
    return n


# A-weighted, peak-anchored S2 ruler. The committed pipeline uses a different metric.

PEAK_WIN_MS = 18.0  # Capture the clap and body without the soft LF tail.
TAIL_PROBE_MS = 90.0  # Test invariance against ref11's ~90 ms tail.


def s2_peak_index(signal: np.ndarray, s2a: float, s2b: float) -> int:
    """Return the absolute sample index of the S2 envelope peak within [s2a, s2b]."""
    seg = shrlib.sl(signal, SR, s2a, s2b)
    if len(seg) == 0:
        return int(s2a * SR)
    return int(s2a * SR) + int(np.argmax(shrlib.env(seg, SR)))


def aw_peak_metrics(signal: np.ndarray, center: int) -> tuple[float, float]:
    """Return A-weighted centroid and HF fraction around the S2 envelope peak.

    Peak anchoring makes the window independent of s2b. The data is Hann-windowed
    and zero-padded to FFT_N.
    """
    half = int(PEAK_WIN_MS * 1e-3 * SR / 2)
    seg = signal[max(0, center - half) : center + half]
    if len(seg) < 2:
        return np.nan, np.nan
    mag = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), FFT_N))
    f = np.fft.rfftfreq(FFT_N, 1 / SR)
    m = (f > 0) & (f <= BAND_MAX_HZ)
    fb, Sw = f[m], mag[m] * a_weight_gain(f[m])
    den = float(Sw.sum())
    if den == 0:
        return np.nan, np.nan
    return float((fb * Sw).sum() / den), float(Sw[fb >= HF_SPLIT_HZ].sum() / den)


def full_window_centroid(signal: np.ndarray, s2a: float, s2b: float) -> float:
    """Magnitude centroid over the full annotated [s2a, s2b] window, as in measure_group."""
    return shrlib.centroid(shrlib.sl(signal, SR, s2a, s2b), SR)


def measure(num: str) -> None:
    tpath = TS_DIR / f"{num}.txt"
    wpath = ORIG_DIR / f"{num}.wav"
    if not (tpath.exists() and wpath.exists()):
        print(f"ref{num}: missing {tpath} or {wpath}")
        return
    signal, sr = shrlib.load(wpath)
    if sr != SR:
        print(f"ref{num}: {wpath.name} is {sr}Hz, not {SR}")
        return
    onfft = onset_nfft()
    for label, beats in shrlib.parse_annotations(tpath).items():
        # Sample the S1/S2 body at one-quarter duration and the clap at S2 onset.
        s1c = [(s1a + (s1b - s1a) * 0.25) * SR for s1a, s1b, s2a, s2b in beats]
        s2c = [(s2a + (s2b - s2a) * 0.25) * SR for s1a, s1b, s2a, s2b in beats]
        s2onset = [s2a * SR for s1a, s1b, s2a, s2b in beats]
        s1f, s1sp = avg_spectrum(signal, s1c, FFT_N)
        s2f, s2sp = avg_spectrum(signal, s2c, FFT_N)
        onf, s2on = avg_spectrum(signal, s2onset, onfft)
        s1cen, s1hf = centroid_hf(s1f, s1sp)
        s2cen, s2hf = centroid_hf(s2f, s2sp)
        oncen, onhf = centroid_hf(onf, s2on)
        s1segs = [shrlib.sl(signal, SR, s1a, s1b) for s1a, s1b, s2a, s2b in beats]
        s2segs = [shrlib.sl(signal, SR, s2a, s2b) for s1a, s1b, s2a, s2b in beats]
        att1 = float(np.nanmedian([shrlib.rise_10_90_ms(s, SR) for s in s1segs]))
        dec1 = float(np.nanmedian([shrlib.decay_ms(s, SR) for s in s1segs]))
        att2 = float(np.nanmedian([shrlib.rise_10_90_ms(s, SR) for s in s2segs]))
        dec2 = float(np.nanmedian([shrlib.decay_ms(s, SR) for s in s2segs]))
        bands1 = band_energy(s1f, s1sp)
        bands2 = band_energy(s2f, s2sp)
        hr = shrlib.beats_hr(beats)

        print(f"\n=== ref{num}: {label}  ({len(beats)} beats, HR~{hr:.0f}) ===")
        print(
            f"  S1: centroid {s1cen:5.0f}Hz  HF>{int(HF_SPLIT_HZ)} {s1hf * 100:4.0f}%   attack {att1:5.1f}ms  decay {dec1:5.1f}ms"
        )
        print(
            f"  S2: centroid {s2cen:5.0f}Hz  HF>{int(HF_SPLIT_HZ)} {s2hf * 100:4.0f}%   attack {att2:5.1f}ms  decay {dec2:5.1f}ms"
        )
        print(
            f"  S2/S1 centroid {s2cen / s1cen:4.2f}x   S2-onset transient: centroid {oncen:5.0f}Hz  HF>{int(HF_SPLIT_HZ)} {onhf * 100:4.0f}%"
        )
        hdr = "  band%   " + " ".join(f"{lo}-{hi}".rjust(8) for lo, hi in BANDS)
        print(hdr)
        print("   S1     " + " ".join(f"{e * 100:7.1f}%" for e in bands1))
        print("   S2     " + " ".join(f"{e * 100:7.1f}%" for e in bands2))

        # Verify peak-ruler tail immunity against the full-window ruler.
        peaks = [s2_peak_index(signal, s2a, s2b) for s1a, s1b, s2a, s2b in beats]
        aw = [aw_peak_metrics(signal, c) for c in peaks]
        aw_cen = float(np.nanmedian([c for c, _ in aw]))
        aw_hf = float(np.nanmedian([h for _, h in aw]))
        full = float(np.nanmedian([full_window_centroid(signal, s2a, s2b) for s1a, s1b, s2a, s2b in beats]))
        full_tail = float(
            np.nanmedian(
                [
                    full_window_centroid(signal, s2a, s2b + TAIL_PROBE_MS * 1e-3)
                    for s1a, s1b, s2a, s2b in beats
                ]
            )
        )
        # Re-run the peak ruler on the extended annotation; its anchor should not move.
        aw_cen_tail = float(
            np.nanmedian(
                [
                    aw_peak_metrics(signal, s2_peak_index(signal, s2a, s2b + TAIL_PROBE_MS * 1e-3))[0]
                    for s1a, s1b, s2a, s2b in beats
                ]
            )
        )
        print(
            f"  S2 ruler: A-weighted peak-anchored centroid {aw_cen:5.0f}Hz  HF>{int(HF_SPLIT_HZ)} {aw_hf * 100:4.0f}%"
        )
        print(
            f"    tail-immunity (+{int(TAIL_PROBE_MS)}ms s2b):  "
            f"full-window {full:5.0f}->{full_tail:5.0f}Hz ({full_tail - full:+.0f})   "
            f"peak-anchored {aw_cen:5.0f}->{aw_cen_tail:5.0f}Hz ({aw_cen_tail - aw_cen:+.0f})"
        )


def main() -> int:
    nums = sys.argv[1:] or ["11", "8", "15"]
    for n in nums:
        measure(n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
