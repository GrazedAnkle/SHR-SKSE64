"""The golden beat-renderer domain: what to freeze and how to compare it.

Drives the offline core binding (shr_pybind) over the committed fixture specs and describes every
beat-render stage as a manifest. The manifest is the regression oracle for offline beat rendering: it
detects any unintended change to core beat-render output, and proves the Python access path returns
exactly what the plugin's renderer produces. WAV parsing stays in Python (soundfile), mirroring the
plugin's decode-then-render split.

The Release-Clang build is deterministic (bit-identical across runs), so the per-stage SHA-256 of the
raw float bytes is the authoritative gate. ``peak_abs`` and ``rms`` are stored alongside as
human-readable review aids and are compared at a loose tolerance.

One golden domain; tools/golden_registry.py describes the surface it exposes.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

import numpy as np
import soundfile as sf

from beat_render_fixtures import FIXTURES, STAGES  # committed fixture specs + stage names

ROOT = Path(__file__).resolve().parent.parent

ID = "beat-renderer"
MANIFEST = ROOT / "tests" / "golden" / "beat_render.json"

SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"

# peak_abs / rms are informational; sha256 is the real gate. Tolerate float-repr drift on recompute.
STAT_ATOL = 1.0e-9


def _stage_entry(array: np.ndarray) -> dict[str, object]:
    # Match the raw little-endian float32 byte order the golden was hashed from.
    raw = np.ascontiguousarray(array, dtype="<f4").tobytes()
    samples = np.frombuffer(raw, dtype="<f4")
    peak_abs = float(np.max(np.abs(samples), initial=0.0))
    rms = float(np.sqrt(np.mean(np.square(samples, dtype=np.float64)))) if samples.size else 0.0
    return {
        "frames": int(array.shape[0]),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "peak_abs": round(peak_abs, 10),
        "rms": round(rms, 10),
    }


def build(module) -> dict[str, object]:
    samples, rate = sf.read(SOURCE, dtype="int16", always_2d=True)
    source = module.prepare_source(np.ascontiguousarray(samples), rate)

    fixtures: dict[str, object] = {}
    channels = 0
    for name, spec in FIXTURES.items():
        stages = module.trace_beat_render(
            source,
            ibi=spec.ibi,
            systole_duration=spec.systole_duration,
            s1_amplitude=spec.s1_amplitude,
            s2_amplitude=spec.s2_amplitude,
            s1_resample_ratio=spec.s1_resample_ratio,
            s2_resample_ratio=spec.s2_resample_ratio,
            lowpass_cutoff_hz=spec.lowpass_cutoff_hz,
            onset_compression=spec.onset_compression,
            kind=spec.kind,
        )
        fixtures[name] = {stage: _stage_entry(stages[stage]) for stage in STAGES}
        channels = int(stages[STAGES[0]].shape[1])

    return {
        "_comment": (
            "Golden beat-renderer output from the compiled shr_core (Release-Clang, deterministic). "
            "Regenerate with: python tools/capture_goldens.py beat-renderer"
        ),
        "sample_rate": int(rate),
        "channel_count": channels,
        "fixtures": fixtures,
    }


def diff(expected: dict, actual: dict) -> list[str]:
    problems: list[str] = []
    for scalar in ("sample_rate", "channel_count"):
        if expected.get(scalar) != actual.get(scalar):
            problems.append(f"{scalar}: golden {expected.get(scalar)} != core {actual.get(scalar)}")

    exp_fx: dict = expected.get("fixtures", {})
    act_fx: dict = actual.get("fixtures", {})
    missing = sorted(set(exp_fx) - set(act_fx))
    added = sorted(set(act_fx) - set(exp_fx))
    if missing:
        problems.append(f"fixtures in golden but not emitted by core: {', '.join(missing)}")
    if added:
        problems.append(f"fixtures emitted by core but absent from golden: {', '.join(added)}")

    for name in sorted(set(exp_fx) & set(act_fx)):
        for stage in STAGES:
            exp = exp_fx[name].get(stage)
            act = act_fx[name].get(stage)
            if exp is None or act is None:
                problems.append(f"{name}/{stage}: present in only one of golden/core")
                continue
            if exp["frames"] != act["frames"]:
                problems.append(
                    f"{name}/{stage}: frames golden {exp['frames']} != core {act['frames']}"
                )
            if exp["sha256"] != act["sha256"]:
                problems.append(
                    f"{name}/{stage}: sha256 mismatch "
                    f"(peak_abs golden {exp['peak_abs']} vs core {act['peak_abs']}, "
                    f"rms golden {exp['rms']} vs core {act['rms']})"
                )
            for field in ("peak_abs", "rms"):
                if abs(float(exp[field]) - float(act[field])) > STAT_ATOL:
                    problems.append(
                        f"{name}/{stage}: {field} golden {exp[field]} != core {act[field]}"
                    )
    return problems


def summary(manifest: dict) -> list[str]:
    fixtures: dict = manifest["fixtures"]
    lines = [f"{len(fixtures)} fixtures, {len(STAGES)} stages each"]
    for name, stages in fixtures.items():
        peak = max(stage["peak_abs"] for stage in stages.values())
        lines.append(f"{name}: {len(stages)} stages, max peak_abs = {peak:.6g}")
    return lines
