"""Retired late-S1 counterfactual transforms.

These frozen transforms are not a renderer and never define current engine behavior. Callers obtain
the active post-onset source stage from ``shr_core``, apply one explicitly selected transform here, and
return the result to the compiled renderer for transmission, mixing, and limiting.
"""
from __future__ import annotations

import numpy as np

from shrlib import SR, analysis_channel


LEGACY_TAIL_RING_LEVEL = 0.15
LEGACY_TAIL_HZ = 46.0
LEGACY_TAIL_DECAY_MS = 45.0
LEGACY_TAIL_SPLICE_MS = 78.0
LEGACY_TAIL_RAMP_MS = 15.0
LEGACY_TAIL_EXTRA_MS = 140.0
LEGACY_LOBE_DECAY_MS = 22.0
LEGACY_LOBE_ENV_MS = 2.0
LEGACY_TAIL_MODES = ("fixed", "off", "dry-end")


def _as_native(source_stage: np.ndarray) -> tuple[np.ndarray, bool]:
    stage = np.asarray(source_stage, dtype=float)
    if stage.ndim == 1:
        return stage[:, None], True
    if stage.ndim != 2 or stage.shape[1] == 0:
        raise ValueError("source stage must have shape (frames, channels)")
    return stage, False


def _restore_layout(stage: np.ndarray, was_mono: bool) -> np.ndarray:
    return stage[:, 0] if was_mono else stage


def _box_smooth(signal: np.ndarray, half: int) -> np.ndarray:
    if half <= 0 or len(signal) == 0:
        return signal
    padded = np.pad(signal, (half, half), mode="edge")
    cumulative = np.insert(np.cumsum(padded), 0, 0.0)
    width = 2 * half + 1
    return (cumulative[width:] - cumulative[:-width]) / width


def tame_lobe(source_s1: np.ndarray, strength: float) -> np.ndarray:
    """Apply the retired post-peak envelope cap to an active core source stage."""
    stage, was_mono = _as_native(source_s1)
    if strength <= 0.0 or len(stage) < 4:
        return source_s1

    control = analysis_channel(stage)
    magnitude = np.abs(control)
    env_half = max(1, int(LEGACY_LOBE_ENV_MS * 1.0e-3 * SR * 0.5))
    envelope = _box_smooth(magnitude, env_half)
    peak = int(np.argmax(magnitude))
    decay = np.exp(-1.0 / (LEGACY_LOBE_DECAY_MS * 1.0e-3 * SR))
    gain = np.ones(len(stage))
    ceiling = envelope[peak]
    for frame in range(peak + 1, len(stage)):
        ceiling *= decay
        current = envelope[frame] if envelope[frame] > 1.0e-9 else 1.0
        gain[frame] = np.clip(
            1.0 - strength * (1.0 - min(1.0, ceiling / current)),
            0.0,
            1.0,
        )
    smoothed_gain = _box_smooth(gain, max(1, env_half // 2))
    return _restore_layout(stage * smoothed_gain[:, None], was_mono)


def _resonator(source: np.ndarray, f0: float, tau_ms: float) -> np.ndarray:
    omega = 2.0 * np.pi * f0 / SR
    radius = np.exp(-1.0 / (tau_ms * 1.0e-3 * SR))
    a1, a2, b0 = 2.0 * radius * np.cos(omega), -(radius * radius), 1.0 - radius
    output = np.zeros_like(source)
    y1 = np.zeros(source.shape[1])
    y2 = np.zeros(source.shape[1])
    for frame in range(len(source)):
        value = b0 * source[frame] + a1 * y1 + a2 * y2
        output[frame] = value
        y2, y1 = y1, value
    return output


def tail_splice_frame(dry_frames: int, baseline_dry_frames: int, mode: str = "fixed") -> int:
    """Return the retired fixed or dry-end-relative splice frame."""
    if mode not in LEGACY_TAIL_MODES:
        raise ValueError(f"unknown tail mode {mode!r}; expected one of {LEGACY_TAIL_MODES}")
    fixed = int(LEGACY_TAIL_SPLICE_MS * 1.0e-3 * SR)
    if mode != "dry-end":
        return fixed
    lead_frames = max(0, baseline_dry_frames - fixed)
    return max(0, dry_frames - lead_frames)


def apply_tail(
    source_s1: np.ndarray,
    mode: str = "off",
    *,
    baseline_dry_frames: int | None = None,
) -> np.ndarray:
    """Apply the retired resonator tail to an active core source stage."""
    if mode not in LEGACY_TAIL_MODES:
        raise ValueError(f"unknown tail mode {mode!r}; expected one of {LEGACY_TAIL_MODES}")
    if mode == "off":
        return source_s1

    stage, was_mono = _as_native(source_s1)
    baseline = len(stage) if baseline_dry_frames is None else baseline_dry_frames
    padding = int(LEGACY_TAIL_EXTRA_MS * 1.0e-3 * SR)
    extended = np.concatenate(
        [stage, np.zeros((padding, stage.shape[1]), dtype=stage.dtype)],
        axis=0,
    )
    ring = _resonator(extended, LEGACY_TAIL_HZ, LEGACY_TAIL_DECAY_MS)
    window = np.ones(len(extended))
    splice = tail_splice_frame(len(stage), baseline, mode)
    ramp = max(1, int(LEGACY_TAIL_RAMP_MS * 1.0e-3 * SR))
    window[:splice] = 0.0
    ramp_frames = min(ramp, max(0, len(window) - splice))
    window[splice:splice + ramp_frames] = (
        0.5 - 0.5 * np.cos(np.pi * np.arange(ramp_frames) / ramp)
    )
    ring *= window[:, None]
    ring_peak = np.max(np.abs(ring), initial=0.0)
    if ring_peak > 0.0:
        ring *= LEGACY_TAIL_RING_LEVEL * np.max(np.abs(stage)) / ring_peak
    return _restore_layout(extended + ring, was_mono)
