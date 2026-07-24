"""Compare every float beat-renderer stage between compiled core and NumPy."""
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


FIXTURES = {
    "rest": eo.FloatBeatRenderSpec(
        ibi=60.0 / 79.0,
        systole_duration=0.32991,
        s1_amplitude=1.0,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.0,
    ),
    "peak": eo.FloatBeatRenderSpec(
        ibi=60.0 / 177.0,
        systole_duration=0.130,
        s1_amplitude=3.5481339,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=2.5,
    ),
    "recovery": eo.FloatBeatRenderSpec(
        ibi=0.6,
        systole_duration=0.244,
        s1_amplitude=2.25,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.8,
    ),
    "inspiration": eo.FloatBeatRenderSpec(
        ibi=60.0 / 79.0,
        systole_duration=0.32991,
        s1_amplitude=0.53,
        s2_amplitude=0.53,
        s1_resample_ratio=0.86,
        s2_resample_ratio=0.86,
        lowpass_cutoff_hz=112.0,
        onset_compression=1.0,
    ),
    "pvc": eo.FloatBeatRenderSpec(
        ibi=0.735,
        systole_duration=0.19932,
        s1_amplitude=0.55125,
        s2_amplitude=0.60,
        s1_resample_ratio=0.90,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.0,
        is_pvc=True,
    ),
}

STAGES = (
    "source_s1",
    "source_s2",
    "transmitted_s1",
    "transmitted_s2",
    "transducer_input",
    "output",
)


def _metadata(path: Path) -> dict[str, int]:
    return {
        name: int(value)
        for name, value in (line.split() for line in path.read_text(encoding="utf-8").splitlines())
    }


def _read_stage(
    path: Path,
    frames: int,
    channels: int,
) -> np.ndarray:
    samples = np.fromfile(path, dtype="<f4")
    if samples.size != frames * channels:
        raise AssertionError(
            f"{path.name}: expected {frames * channels} samples, found {samples.size}"
        )
    return samples.reshape(frames, channels)


def check(fixture: Path, source: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="shr-renderer-parity-") as output:
        output_path = Path(output)
        subprocess.run([fixture, source, output_path], check=True)
        metadata = _metadata(output_path / "metadata.txt")
        source_stages = eo.prep_source_float_stages(source)

        max_deltas: dict[str, dict[str, float]] = {}
        for name, render in FIXTURES.items():
            expected = eo.render_beat_float_stages(source_stages, render)
            max_deltas[name] = {}
            for stage in STAGES:
                actual = _read_stage(
                    output_path / f"{name}_{stage}.f32",
                    metadata[f"{name}_{stage}_frames"],
                    metadata["channel_count"],
                )
                reference = getattr(expected, stage)
                np.testing.assert_allclose(actual, reference, rtol=0.0, atol=2.0e-6)
                max_deltas[name][stage] = float(
                    np.max(np.abs(actual - reference), initial=0.0)
                )

    print("C++/NumPy float beat renderer matches")
    for name, stages in max_deltas.items():
        print(f"  {name}:")
        for stage, delta in stages.items():
            print(f"    {stage}: max |delta| = {delta:.9g}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--fixture",
        type=Path,
        default=ROOT / "build" / "release-clang" / "SHRBeatRendererFixture.exe",
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
