"""Capture and verify the compiled core's golden beat-renderer output.

Drives the offline core binding (shr_pybind) over the committed fixture specs and freezes every
beat-render stage as a committed manifest, then re-verifies a fresh core run against it. The manifest
is the regression oracle for offline beat rendering: it detects any unintended change to core
beat-render output, and proves the Python access path returns exactly what the plugin's renderer
produces. WAV parsing stays in Python (soundfile), mirroring the plugin's decode-then-render split.

The Release-Clang build is deterministic (bit-identical across runs), so the per-stage SHA-256 of the
raw float bytes is the authoritative gate. ``peak_abs`` and ``rms`` are stored alongside as
human-readable review aids and are compared at a loose tolerance.

Build the module first with tools/build_pybind.py.

    python tools/check_beat_renderer_golden.py            # verify against the committed manifest
    python tools/check_beat_renderer_golden.py --capture  # (re)generate the manifest after a change
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
GOLDEN = ROOT / "tests" / "golden" / "beat_render.json"
SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"

sys.path.insert(0, str(ROOT / "tools"))
from beat_render_fixtures import FIXTURES, STAGES  # committed fixture specs + stage names

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


def _render(module) -> dict[str, object]:
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
            "Regenerate with: python tools/check_beat_renderer_golden.py --capture"
        ),
        "sample_rate": int(rate),
        "channel_count": channels,
        "fixtures": fixtures,
    }


def _diff(expected: dict, actual: dict) -> list[str]:
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
        fixtures: dict = manifest["fixtures"]  # type: ignore[assignment]
        print(f"Wrote {GOLDEN.relative_to(ROOT)} ({len(fixtures)} fixtures, {len(STAGES)} stages each)")
        return

    if not GOLDEN.exists():
        sys.exit(f"golden manifest missing: {GOLDEN.relative_to(ROOT)} (run with --capture)")
    expected = json.loads(GOLDEN.read_text(encoding="utf-8"))
    problems = _diff(expected, manifest)
    if problems:
        print("Compiled core beat renderer DIVERGED from golden:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        sys.exit("golden mismatch (regenerate with --capture only if the change is intended)")

    fixtures = expected["fixtures"]
    print(f"Compiled core beat renderer matches golden ({len(fixtures)} fixtures):")
    for name, stages in fixtures.items():
        peak = max(stage["peak_abs"] for stage in stages.values())
        print(f"  {name}: {len(stages)} stages, max peak_abs = {peak:.6g}")


if __name__ == "__main__":
    main()
