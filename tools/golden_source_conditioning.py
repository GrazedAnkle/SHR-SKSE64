"""The golden source-conditioning domain: what to freeze and how to compare it.

Drives the offline core binding (shr_pybind) over the shortened heartbeat source and describes its
per-stage source conditioning (slice -> high-pass -> joint normalize) as a manifest. This is the
full-waveform, per-stage regression oracle for offline source conditioning, anchored to the compiled
core. The Catch suite pins the conditioning contract (frame counts, attack region, joint peak); this
golden pins the exact conditioned samples.

The Release-Clang build is deterministic, so per-stage SHA-256 of the raw float bytes is the gate;
``peak_abs``/``rms`` are review aids compared at a loose tolerance.

One golden domain; tools/golden_registry.py describes the surface it exposes.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent

ID = "source-conditioning"
MANIFEST = ROOT / "tests" / "golden" / "source_conditioning.json"

SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"

# Fixed conditioning stages emitted by trace_source_conditioning: slice -> high-pass -> joint normalize.
STAGES = (
    "sliced_s1",
    "sliced_s2",
    "highpass_s1",
    "highpass_s2",
    "normalized_s1",
    "normalized_s2",
)

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
    trace = module.trace_source_conditioning(np.ascontiguousarray(samples), rate)
    return {
        "_comment": (
            "Golden source-conditioning output from the compiled shr_core (Release-Clang, deterministic). "
            "Regenerate with: python tools/capture_goldens.py source-conditioning"
        ),
        "sample_rate": int(trace["sample_rate"]),
        "channel_count": int(trace["channel_count"]),
        "attack_start": int(trace["attack_start"]),
        "attack_peak": int(trace["attack_peak"]),
        "stages": {stage: _stage_entry(trace[stage]) for stage in STAGES},
    }


def diff(expected: dict, actual: dict) -> list[str]:
    problems: list[str] = []
    for scalar in ("sample_rate", "channel_count", "attack_start", "attack_peak"):
        if expected.get(scalar) != actual.get(scalar):
            problems.append(f"{scalar}: golden {expected.get(scalar)} != core {actual.get(scalar)}")

    exp_stages: dict = expected.get("stages", {})
    act_stages: dict = actual.get("stages", {})
    for stage in STAGES:
        exp = exp_stages.get(stage)
        act = act_stages.get(stage)
        if exp is None or act is None:
            problems.append(f"{stage}: present in only one of golden/core")
            continue
        if exp["frames"] != act["frames"]:
            problems.append(f"{stage}: frames golden {exp['frames']} != core {act['frames']}")
        if exp["sha256"] != act["sha256"]:
            problems.append(
                f"{stage}: sha256 mismatch "
                f"(peak_abs golden {exp['peak_abs']} vs core {act['peak_abs']}, "
                f"rms golden {exp['rms']} vs core {act['rms']})"
            )
        for field in ("peak_abs", "rms"):
            if abs(float(exp[field]) - float(act[field])) > STAT_ATOL:
                problems.append(f"{stage}: {field} golden {exp[field]} != core {act[field]}")
    return problems


def summary(manifest: dict) -> list[str]:
    lines = [f"{len(STAGES)} stages"]
    for stage in STAGES:
        entry = manifest["stages"][stage]
        lines.append(f"{stage}: {entry['frames']} frames, peak_abs = {entry['peak_abs']:.6g}")
    lines.append(f"baseline attack region: [{manifest['attack_start']}, {manifest['attack_peak']}]")
    return lines
