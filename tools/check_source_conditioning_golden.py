"""Capture and verify the compiled core's golden source-conditioning output.

Freezes the compiled core's source-conditioning stages (from the SHRSourceConditioningFixture exe) as a
committed manifest, then re-verifies a fresh core run against it. This is the full-waveform, per-stage
regression oracle for offline source conditioning, anchored to the compiled core. The Catch suite pins
the conditioning contract (frame counts, attack region, joint peak); this golden pins the exact
conditioned samples.

The Release-Clang build is deterministic, so per-stage SHA-256 of the raw float bytes is the gate;
``peak_abs``/``rms`` are review aids compared at a loose tolerance.

    python tools/check_source_conditioning_golden.py            # verify against the manifest
    python tools/check_source_conditioning_golden.py --capture  # (re)generate after an intended change
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
GOLDEN = ROOT / "tests" / "golden" / "source_conditioning.json"

# Fixed pipeline stages emitted by SHRSourceConditioningFixture: slice -> high-pass -> joint normalize.
STAGES = (
    "sliced_s1",
    "sliced_s2",
    "highpass_s1",
    "highpass_s2",
    "normalized_s1",
    "normalized_s2",
)

STAT_ATOL = 1.0e-9


def _metadata(path: Path) -> dict[str, int]:
    return {
        name: int(value)
        for name, value in (line.split() for line in path.read_text(encoding="utf-8").splitlines())
    }


def _stage_entry(path: Path, frames: int, channels: int) -> dict[str, object]:
    raw = path.read_bytes()
    samples = np.frombuffer(raw, dtype="<f4")
    if samples.size != frames * channels:
        raise AssertionError(
            f"{path.name}: expected {frames * channels} samples, found {samples.size}"
        )
    peak_abs = float(np.max(np.abs(samples), initial=0.0))
    rms = float(np.sqrt(np.mean(np.square(samples, dtype=np.float64)))) if samples.size else 0.0
    return {
        "frames": frames,
        "sha256": hashlib.sha256(raw).hexdigest(),
        "peak_abs": round(peak_abs, 10),
        "rms": round(rms, 10),
    }


def _render(fixture: Path, source: Path, output: Path) -> dict[str, object]:
    subprocess.run([str(fixture), str(source), str(output)], check=True)
    metadata = _metadata(output / "metadata.txt")
    channels = metadata["channel_count"]
    stages: dict[str, object] = {}
    for stage in STAGES:
        frames = metadata["s1_frames"] if stage.endswith("_s1") else metadata["s2_frames"]
        stages[stage] = _stage_entry(output / f"{stage}.f32", frames, channels)
    return {
        "_comment": (
            "Golden source-conditioning output from the compiled shr_core (Release-Clang, deterministic). "
            "Regenerate with: python tools/check_source_conditioning_golden.py --capture"
        ),
        "sample_rate": metadata["sample_rate"],
        "channel_count": channels,
        "attack_start": metadata["attack_start"],
        "attack_peak": metadata["attack_peak"],
        "stages": stages,
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
        "--fixture",
        type=Path,
        default=ROOT / "build" / "release-clang" / "SHRSourceConditioningFixture.exe",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav",
    )
    parser.add_argument(
        "--capture",
        action="store_true",
        help="Write the manifest instead of verifying against it.",
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="shr-source-golden-") as tmp:
        manifest = _render(args.fixture.resolve(), args.source.resolve(), Path(tmp))

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
