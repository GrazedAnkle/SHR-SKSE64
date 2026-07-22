#!/usr/bin/env python3
"""Render a reproducible temporal-anchor review view for one annotated sound lobe.

Audacity remains the playback and annotation editor. This view shows the exact envelopes and temporal
anchors used by shrlib beside a fixed-parameter spectrogram. The spectrogram is contextual: its window
smears time and must not be used to overrule waveform/envelope onset anchors.

Example (the committed source S1 annotation):
  python tools/plot_metric_anchors.py \
      contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav \
      --start 0.050 --end 0.145 --out build/source-s1-anchors.png
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from scipy.signal import spectrogram

import shrlib


DEFAULT_FMAX_HZ = 1000.0
DEFAULT_CONTEXT_MS = 30.0
DEFAULT_STFT_MS = 32.0
DEFAULT_HOP_MS = 4.0
DEFAULT_DB_FLOOR = -60.0


def _first_crossing(values: np.ndarray, threshold: float) -> int:
    found = np.flatnonzero(values >= threshold)
    return int(found[0]) if len(found) else 0


def metric_anchors(lobe: np.ndarray, sr: int) -> dict[str, object]:
    """Return the sample indices and envelopes drawn by the review view."""
    envelope = shrlib.env_analytic(lobe, sr)
    peak = int(np.argmax(envelope)) if len(envelope) else 0
    last_10, _ = shrlib.onset_peak_idx(lobe, sr)
    running = np.maximum.accumulate(envelope[:peak + 1]) if len(envelope) else np.array([])
    rise_10 = _first_crossing(running, 0.10 * envelope[peak]) if len(running) else 0
    rise_90 = _first_crossing(running, 0.90 * envelope[peak]) if len(running) else 0
    lobe_envelope, lobes = shrlib._lobe_peak_indices(lobe, sr)
    return {
        "envelope": envelope,
        "running": running,
        "peak": peak,
        "last_10": last_10,
        "rise_10": rise_10,
        "rise_90": rise_90,
        "lobe_envelope": lobe_envelope,
        "lobes": lobes,
    }


def _spectrogram(signal: np.ndarray, sr: int, display_start: float, display_end: float,
                 stft_ms: float, hop_ms: float, fmax_hz: float):
    nperseg = max(16, int(round(stft_ms * 1e-3 * sr)))
    hop = max(1, int(round(hop_ms * 1e-3 * sr)))
    noverlap = min(nperseg - 1, nperseg - hop)
    # Supply half a window beyond the displayed context where the file permits, so the visible edge
    # columns are not created from an already-cropped review window.
    i0 = max(0, int(np.floor(display_start * sr)) - nperseg // 2)
    i1 = min(len(signal), int(np.ceil(display_end * sr)) + nperseg // 2)
    x = signal[i0:i1]
    if len(x) < nperseg:
        nperseg = len(x)
        noverlap = min(noverlap, max(0, nperseg - 1))
    nfft = 1 << max(1, (nperseg - 1).bit_length())
    freq, time, magnitude = spectrogram(
        x, fs=sr, window="hann", nperseg=nperseg, noverlap=noverlap, nfft=nfft,
        detrend=False, scaling="spectrum", mode="magnitude",
    )
    time = time + i0 / sr
    mask_f = freq <= fmax_hz
    mask_t = (time >= display_start) & (time <= display_end)
    magnitude = magnitude[np.ix_(mask_f, mask_t)]
    peak = float(magnitude.max()) if magnitude.size else 0.0
    relative_db = 20.0 * np.log10(np.maximum(magnitude / peak, 1e-6)) if peak > 0 else np.full_like(magnitude, -120.0)
    return freq[mask_f], time[mask_t], relative_db, nperseg / sr * 1e3, hop / sr * 1e3


def plot_anchor_view(wav: str | Path, start: float, end: float, out: str | Path | None = None,
                     show: bool = False, fmax_hz: float = DEFAULT_FMAX_HZ,
                     context_ms: float = DEFAULT_CONTEXT_MS, stft_ms: float = DEFAULT_STFT_MS,
                     hop_ms: float = DEFAULT_HOP_MS, db_floor: float = DEFAULT_DB_FLOOR,
                     title: str | None = None) -> None:
    """Plot a spectrogram above the exact waveform/envelope temporal anchors for `[start, end]`."""
    import matplotlib.pyplot as plt

    wav = Path(wav)
    signal, sr = shrlib.load(wav)
    if not (0.0 <= start < end <= len(signal) / sr):
        raise ValueError(f"annotation [{start:.6f}, {end:.6f}] is outside 0..{len(signal) / sr:.6f} s")
    if fmax_hz <= 0.0 or context_ms < 0.0 or stft_ms <= 0.0 or hop_ms <= 0.0:
        raise ValueError("fmax, STFT window, and hop must be positive; context must be nonnegative")

    display_start = max(0.0, start - context_ms * 1e-3)
    display_end = min(len(signal) / sr, end + context_ms * 1e-3)
    i0, i1 = int(round(start * sr)), int(round(end * sr))
    lobe = signal[i0:i1]
    anchors = metric_anchors(lobe, sr)

    freq, stft_time, spec_db, actual_stft_ms, actual_hop_ms = _spectrogram(
        signal, sr, display_start, display_end, stft_ms, hop_ms, fmax_hz)
    fig = plt.figure(figsize=(14, 8), layout="constrained")
    grid = fig.add_gridspec(2, 2, height_ratios=(1.0, 1.2), width_ratios=(1.0, 0.025),
                            hspace=0.08, wspace=0.03)
    ax_spec = fig.add_subplot(grid[0, 0])
    ax_wave = fig.add_subplot(grid[1, 0], sharex=ax_spec)
    ax_colorbar = fig.add_subplot(grid[0, 1])
    ax_spacer = fig.add_subplot(grid[1, 1])
    ax_spacer.axis("off")
    mesh = ax_spec.pcolormesh(stft_time, freq, spec_db, shading="auto", cmap="magma",
                              vmin=db_floor, vmax=0.0)
    colorbar = fig.colorbar(mesh, cax=ax_colorbar)
    colorbar.set_label("dB relative to view peak")
    ax_spec.set_ylabel("frequency (Hz)")
    ax_spec.set_ylim(0.0, fmax_hz)
    ax_spec.text(0.01, 0.97, "context only — temporal anchors come from the envelope below",
                 transform=ax_spec.transAxes, va="top", ha="left", color="white", fontsize=9,
                 bbox={"facecolor": "black", "alpha": 0.45, "edgecolor": "none", "pad": 3})

    d0, d1 = int(np.floor(display_start * sr)), int(np.ceil(display_end * sr))
    view = signal[d0:d1]
    view_time = (np.arange(len(view)) + d0) / sr
    wave_scale = max(float(np.max(np.abs(lobe))), 1e-12)
    ax_wave.plot(view_time, view / wave_scale, color="0.35", lw=0.75, label="waveform / lobe peak")

    lobe_time = (np.arange(len(lobe)) + i0) / sr
    envelope = anchors["envelope"]
    env_scale = max(float(np.max(envelope)), 1e-12)
    ax_wave.plot(lobe_time, envelope / env_scale, color="tab:blue", lw=1.5,
                 label="analytic envelope (1 ms)")
    running = anchors["running"]
    peak = int(anchors["peak"])
    ax_wave.plot(lobe_time[:peak + 1], running / env_scale, color="tab:green", lw=1.1, ls="--",
                 label="running maximum")
    lobe_envelope = anchors["lobe_envelope"]
    lobe_env_scale = max(float(np.max(lobe_envelope)), 1e-12)
    ax_wave.plot(lobe_time, lobe_envelope / lobe_env_scale, color="tab:purple", lw=1.0, ls=":",
                 label="lobe detector envelope (5 ms)")

    def at(sample: int) -> float:
        return start + sample / sr

    # The declared annotation/window is the only manually supplied anchor.
    for x, label in ((start, "annotation start"), (end, "annotation end")):
        ax_spec.axvline(x, color="white", lw=1.2, ls="--", alpha=0.9)
        ax_wave.axvline(x, color="black", lw=1.2, ls="--", alpha=0.8, label=label)

    anchor_styles = (
        (int(anchors["last_10"]), "last 10% crossing", "tab:orange", "-."),
        (int(anchors["rise_10"]), "running-max 10%", "tab:green", "-"),
        (int(anchors["rise_90"]), "running-max 90%", "tab:cyan", "-"),
        (peak, "envelope peak", "tab:red", "-"),
    )
    for sample, label, color, style in anchor_styles:
        x = at(sample)
        ax_spec.axvline(x, color=color, lw=1.0, ls=style, alpha=0.85)
        ax_wave.axvline(x, color=color, lw=1.15, ls=style, alpha=0.9, label=label)

    lobes = np.asarray(anchors["lobes"], dtype=int)
    for j, sample in enumerate(lobes):
        x = at(int(sample))
        ax_spec.axvline(x, color="tab:purple", lw=0.8, ls=":", alpha=0.65)
        ax_wave.axvline(x, color="tab:purple", lw=0.8, ls=":", alpha=0.65,
                        label="detected lobe peak" if j == 0 else None)
        ax_wave.plot(x, lobe_envelope[sample] / lobe_env_scale, "o", color="tab:purple", ms=4)

    ax_wave.hlines((0.10, 0.90), start, at(peak), colors=("tab:orange", "tab:cyan"),
                   linestyles=":", linewidths=0.8, alpha=0.8)
    peak_time = at(peak)
    rise_start = peak_time - shrlib.CONTRAST_RISE_MS * 1e-3
    body_end = peak_time + shrlib.CONTRAST_BODY_MS * 1e-3
    for ax in (ax_spec, ax_wave):
        ax.axvspan(rise_start, peak_time, color="tab:orange", alpha=0.08)
        ax.axvspan(peak_time, body_end, color="tab:blue", alpha=0.06)

    n_lobes, tallest, runner = shrlib.lobe_count(lobe, sr)
    rise_ms = shrlib.rise_10_90_ms(lobe, sr)
    attack_ms = shrlib.attack_ms(lobe, sr)
    heading = title or wav.name
    fig.suptitle(
        f"{heading}  [{start:.6f}, {end:.6f}] s\n"
        f"rise10–90 {rise_ms:.2f} ms   last10→peak {attack_ms:.2f} ms   "
        f"lobes {n_lobes} (tallest #{tallest}, runner {runner:.2f})   |   "
        f"spectrogram: Hann {actual_stft_ms:.1f} ms, hop {actual_hop_ms:.1f} ms, "
        f"0–{fmax_hz:g} Hz, {db_floor:g}..0 dB",
        fontsize=11)
    ax_wave.set_xlim(display_start, display_end)
    ax_wave.set_ylim(-1.12, 1.18)
    ax_wave.set_xlabel("file time (s)")
    ax_wave.set_ylabel("normalized amplitude")
    ax_wave.grid(True, alpha=0.2)
    handles, labels = ax_wave.get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    ax_wave.legend(unique.values(), unique.keys(), loc="upper right", fontsize=8, ncol=2,
                   framealpha=0.92)
    ax_spec.tick_params(axis="x", labelbottom=False)
    if out is not None:
        out = Path(out)
        out.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out, dpi=140)
        print(f"wrote {out}")
    if show:
        plt.show()
    plt.close(fig)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("wav", help="WAV containing the sound lobe")
    ap.add_argument("--start", required=True, type=float, help="annotation/window start in file seconds")
    ap.add_argument("--end", required=True, type=float, help="annotation/window end in file seconds")
    ap.add_argument("--out", help="write the review view to this image path")
    ap.add_argument("--show", action="store_true", help="also open the interactive Matplotlib window")
    ap.add_argument("--fmax", type=float, default=DEFAULT_FMAX_HZ,
                    help=f"spectrogram upper frequency in Hz (default: {DEFAULT_FMAX_HZ:g})")
    ap.add_argument("--context-ms", type=float, default=DEFAULT_CONTEXT_MS,
                    help=f"context shown before/after the annotation (default: {DEFAULT_CONTEXT_MS:g} ms)")
    ap.add_argument("--stft-ms", type=float, default=DEFAULT_STFT_MS,
                    help=f"Hann-window duration (default: {DEFAULT_STFT_MS:g} ms)")
    ap.add_argument("--hop-ms", type=float, default=DEFAULT_HOP_MS,
                    help=f"spectrogram hop duration (default: {DEFAULT_HOP_MS:g} ms)")
    ap.add_argument("--db-floor", type=float, default=DEFAULT_DB_FLOOR,
                    help=f"relative spectrogram display floor (default: {DEFAULT_DB_FLOOR:g} dB)")
    ap.add_argument("--title", help="optional figure title in place of the WAV filename")
    args = ap.parse_args()
    if not args.out and not args.show:
        ap.error("choose --out and/or --show")
    plot_anchor_view(args.wav, args.start, args.end, args.out, args.show, args.fmax,
                     args.context_ms, args.stft_ms, args.hop_ms, args.db_floor, args.title)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
