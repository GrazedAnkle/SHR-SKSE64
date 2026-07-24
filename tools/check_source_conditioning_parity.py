"""Compare every float source-conditioning stage between compiled core and NumPy."""
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import engine_offline as eo


def _metadata(path: Path) -> dict[str, int]:
    return {
        name: int(value)
        for name, value in (line.split() for line in path.read_text(encoding="utf-8").splitlines())
    }


def _read_stage(path: Path, frames: int, channels: int) -> np.ndarray:
    samples = np.fromfile(path, dtype="<f4")
    if samples.size != frames * channels:
        raise AssertionError(
            f"{path.name}: expected {frames * channels} samples, found {samples.size}"
        )
    return samples.reshape(frames, channels)


def check(fixture: Path, source: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="shr-source-parity-") as output:
        output_path = Path(output)
        subprocess.run([fixture, source, output_path], check=True)
        metadata = _metadata(output_path / "metadata.txt")
        cpp = {
            "sliced_s1": _read_stage(
                output_path / "sliced_s1.f32",
                metadata["s1_frames"],
                metadata["channel_count"],
            ),
            "sliced_s2": _read_stage(
                output_path / "sliced_s2.f32",
                metadata["s2_frames"],
                metadata["channel_count"],
            ),
            "highpass_s1": _read_stage(
                output_path / "highpass_s1.f32",
                metadata["s1_frames"],
                metadata["channel_count"],
            ),
            "highpass_s2": _read_stage(
                output_path / "highpass_s2.f32",
                metadata["s2_frames"],
                metadata["channel_count"],
            ),
            "normalized_s1": _read_stage(
                output_path / "normalized_s1.f32",
                metadata["s1_frames"],
                metadata["channel_count"],
            ),
            "normalized_s2": _read_stage(
                output_path / "normalized_s2.f32",
                metadata["s2_frames"],
                metadata["channel_count"],
            ),
        }

        numpy_stages = eo.prep_source_float_stages(source)
        max_deltas: dict[str, float] = {}
        for name, actual in cpp.items():
            expected = getattr(numpy_stages, name)
            tolerance = 0.0 if name.startswith("sliced_") else 2.0e-6
            np.testing.assert_allclose(actual, expected, rtol=0.0, atol=tolerance)
            max_deltas[name] = float(np.max(np.abs(actual - expected), initial=0.0))

        cpp_attack = (metadata["attack_start"], metadata["attack_peak"])
        if cpp_attack != numpy_stages.baseline_s1_attack:
            raise AssertionError(
                f"baseline S1 attack differs: C++ {cpp_attack}, "
                f"NumPy {numpy_stages.baseline_s1_attack}"
            )

    print("C++/NumPy float source conditioning matches")
    for name, delta in max_deltas.items():
        print(f"  {name}: max |delta| = {delta:.9g}")
    print(f"  baseline_s1_attack: {cpp_attack}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--fixture",
        type=Path,
        default=ROOT / "build" / "release-clang" / "SHRSourceConditioningFixture.exe",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=(
            ROOT /
            "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"
        ),
    )
    args = parser.parse_args()
    check(args.fixture.resolve(), args.source.resolve())


if __name__ == "__main__":
    main()
