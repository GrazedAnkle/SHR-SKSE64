"""Capture and verify the compiled core's golden beat-renderer output.

This tool freezes the compiled core's own beat-renderer output (from the SHRBeatRendererFixture exe)
as a committed manifest, then re-verifies a fresh core run against it. The manifest is the regression
oracle for offline beat rendering: it detects any unintended change to core beat-render output. Its
sibling ``check_pybind_golden.py`` holds the shr_pybind binding to the same manifest.

The Release-Clang build is deterministic (bit-identical across runs), so the per-stage SHA-256 of the
raw float bytes is the authoritative gate. ``peak_abs`` and ``rms`` are stored alongside as
human-readable review aids and are compared at a loose tolerance.

    python tools/check_beat_renderer_golden.py            # verify against the committed manifest
    python tools/check_beat_renderer_golden.py --capture  # (re)generate the manifest after a change
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
GOLDEN = ROOT / "tests" / "golden" / "beat_render.json"

sys.path.insert(0, str(ROOT / "tools"))
# Stage names are fixed by BeatRenderTrace; fixture names are derived from the emitted metadata so
# this tool never duplicates the fixture list defined in tests/BeatRenderFixtures.hpp.
from beat_render_fixtures import STAGES

# peak_abs / rms are informational; sha256 is the real gate. Tolerate float-repr drift on recompute.
STAT_ATOL = 1.0e-9


def _metadata(path: Path) -> dict[str, int]:
    return {
        name: int(value)
        for name, value in (line.split() for line in path.read_text(encoding="utf-8").splitlines())
    }


def _fixture_names(metadata: dict[str, int]) -> list[str]:
    names: list[str] = []
    for key in metadata:
        for stage in STAGES:
            suffix = f"_{stage}_frames"
            if key.endswith(suffix):
                name = key[: -len(suffix)]
                if name not in names:
                    names.append(name)
    return names


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
    manifest: dict[str, object] = {
        "_comment": (
            "Golden beat-renderer output from the compiled shr_core (Release-Clang, deterministic). "
            "Regenerate with: python tools/check_beat_renderer_golden.py --capture"
        ),
        "sample_rate": metadata["sample_rate"],
        "channel_count": metadata["channel_count"],
        "fixtures": {},
    }
    channels = metadata["channel_count"]
    fixtures: dict[str, object] = manifest["fixtures"]  # type: ignore[assignment]
    for name in _fixture_names(metadata):
        stages: dict[str, object] = {}
        for stage in STAGES:
            frames = metadata[f"{name}_{stage}_frames"]
            stages[stage] = _stage_entry(output / f"{name}_{stage}.f32", frames, channels)
        fixtures[name] = stages
    return manifest


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
        "--fixture",
        type=Path,
        default=ROOT / "build" / "release-clang" / "SHRBeatRendererFixture.exe",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=(
            ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"
        ),
    )
    parser.add_argument(
        "--capture",
        action="store_true",
        help="Write the manifest instead of verifying against it.",
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="shr-renderer-golden-") as tmp:
        manifest = _render(args.fixture.resolve(), args.source.resolve(), Path(tmp))

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
