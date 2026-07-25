"""Capture and verify the compiled core's golden source-conditioning output.

Drives the offline core binding (shr_pybind) over the shortened heartbeat source and freezes its
per-stage source conditioning (slice -> high-pass -> joint normalize) as a committed manifest, then
re-verifies a fresh core run against it. This is the full-waveform, per-stage regression oracle for
offline source conditioning, anchored to the compiled core. The Catch suite pins the conditioning
contract (frame counts, attack region, joint peak); this golden pins the exact conditioned samples.

The Release-Clang build is deterministic, so per-stage SHA-256 of the raw float bytes is the gate;
``peak_abs``/``rms`` are review aids compared at a loose tolerance.

Build the module first with tools/build_pybind.py.

    python tools/check_source_conditioning_golden.py            # verify against the manifest
    python tools/check_source_conditioning_golden.py --capture  # (re)generate after an intended change
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent
GOLDEN = ROOT / "tests" / "golden" / "source_conditioning.json"
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


def _render(module) -> dict[str, object]:
    samples, rate = sf.read(SOURCE, dtype="int16", always_2d=True)
    trace = module.trace_source_conditioning(np.ascontiguousarray(samples), rate)
    return {
        "_comment": (
            "Golden source-conditioning output from the compiled shr_core (Release-Clang, deterministic). "
            "Regenerate with: python tools/check_source_conditioning_golden.py --capture"
        ),
        "sample_rate": int(trace["sample_rate"]),
        "channel_count": int(trace["channel_count"]),
        "attack_start": int(trace["attack_start"]),
        "attack_peak": int(trace["attack_peak"]),
        "stages": {stage: _stage_entry(trace[stage]) for stage in STAGES},
    }


def _diff(expected: dict, actual: dict) -> list[str]:
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=ROOT / "build" / "pybind",
        help="Directory containing the built shr_pybind*.pyd (default: build/pybind).",
    )
    parser.add_argument(
        "--capture",
        action="store_true",
        help="Write the manifest instead of verifying against it.",
    )
    args = parser.parse_args()

    sys.path.insert(0, str(args.module_dir.resolve()))
    try:
        import shr_pybind
    except ImportError as error:
        sys.exit(
            f"cannot import shr_pybind from {args.module_dir} ({error}); "
            f"build it with: python tools/build_pybind.py"
        )

    manifest = _render(shr_pybind)

    if args.capture:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote {GOLDEN.relative_to(ROOT)} ({len(STAGES)} stages)")
        return

    if not GOLDEN.exists():
        sys.exit(f"golden manifest missing: {GOLDEN.relative_to(ROOT)} (run with --capture)")
    expected = json.loads(GOLDEN.read_text(encoding="utf-8"))
    problems = _diff(expected, manifest)
    if problems:
        print("Compiled core source conditioning DIVERGED from golden:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        sys.exit("golden mismatch (regenerate with --capture only if the change is intended)")

    print(f"Compiled core source conditioning matches golden ({len(STAGES)} stages):")
    for stage in STAGES:
        entry = expected["stages"][stage]
        print(f"  {stage}: {entry['frames']} frames, peak_abs = {entry['peak_abs']:.6g}")
    print(f"  baseline attack region: [{expected['attack_start']}, {expected['attack_peak']}]")


if __name__ == "__main__":
    main()
