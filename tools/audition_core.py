"""Build a stereo audition reel with the compiled core renderer.

This is the canonical offline audition path. It preserves the renderer's native
channel layout in the written WAV; numerical analysis remains explicitly
channel-zero, matching ``shrlib.load`` and the core source-attack landmark.

The named operating points are the committed direct-render fixtures used by the
beat-render golden. They are regression inputs, not physiological claims.

Build ``shr_pybind`` first:

    python tools/build_pybind.py
    python tools/audition_core.py --out build/audition/core.wav
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf

import core_offline
from beat_render_fixtures import FIXTURES
from shrlib import analysis_channel


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = (
    ROOT
    / "contrib"
    / "Distribution"
    / "Sound"
    / "fx"
    / "SHR_HeartBeat"
    / "HeartBeat_Shortened.wav"
)
DEFAULT_FIXTURES = ("rest", "peak", "inspiration", "recovery", "pvc")


def side_to_mid_db(rendered: np.ndarray) -> float | None:
    """Return stereo side/mid RMS in dB, or None for a non-stereo render."""
    rendered = np.asarray(rendered, dtype=np.float64)
    if rendered.ndim != 2 or rendered.shape[1] != 2:
        return None
    mid = 0.5 * (rendered[:, 0] + rendered[:, 1])
    side = 0.5 * (rendered[:, 0] - rendered[:, 1])
    mid_rms = float(np.sqrt(np.mean(mid * mid)))
    side_rms = float(np.sqrt(np.mean(side * side)))
    if mid_rms == 0.0:
        return None
    if side_rms == 0.0:
        return float("-inf")
    return float(20.0 * np.log10(side_rms / mid_rms))


def render_fixture(module, source, name: str) -> np.ndarray:
    """Render one committed direct-render fixture through ``shr_core``."""
    spec = FIXTURES[name]
    return module.render_beat(
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


def build_reel(
    module,
    source_path: Path,
    fixture_names: list[str],
    beats: int,
    gap_seconds: float,
) -> tuple[np.ndarray, int, list[dict[str, float | int | str | None]]]:
    """Render the requested sections and return native-layout audio plus a section report."""
    pcm, sample_rate = sf.read(source_path, dtype="int16", always_2d=True)
    source = module.prepare_source(np.ascontiguousarray(pcm), sample_rate)

    sections: list[np.ndarray] = []
    report: list[dict[str, float | int | str | None]] = []
    gap = np.zeros((int(round(gap_seconds * sample_rate)), pcm.shape[1]), dtype=np.float32)
    cursor = 0
    for index, name in enumerate(fixture_names):
        beat = np.asarray(render_fixture(module, source, name), dtype=np.float32)
        section = np.concatenate([beat] * beats, axis=0)
        sections.append(section)
        analysis = analysis_channel(section)
        report.append(
            {
                "name": name,
                "start_seconds": cursor / sample_rate,
                "duration_seconds": len(section) / sample_rate,
                "channels": int(section.shape[1]),
                "channel_zero_peak": float(np.max(np.abs(analysis), initial=0.0)),
                "side_to_mid_db": side_to_mid_db(section),
            }
        )
        cursor += len(section)
        if index != len(fixture_names) - 1:
            sections.append(gap)
            cursor += len(gap)

    return np.concatenate(sections, axis=0), int(sample_rate), report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=ROOT / "build" / "pybind",
        help="directory containing shr_pybind (default: build/pybind)",
    )
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--out", type=Path, required=True, help="native-layout PCM16 audition WAV")
    parser.add_argument(
        "--fixture",
        action="append",
        choices=tuple(FIXTURES),
        help="fixture section to include; repeatable (default: rest, peak, inspiration, recovery, pvc)",
    )
    parser.add_argument("--beats", type=int, default=4, help="repetitions per fixture section")
    parser.add_argument("--gap", type=float, default=0.75, help="silence between sections (seconds)")
    args = parser.parse_args()

    if args.beats <= 0:
        parser.error("--beats must be positive")
    if args.gap < 0.0:
        parser.error("--gap must be nonnegative")
    fixture_names = args.fixture or list(DEFAULT_FIXTURES)

    module = core_offline.load_binding(args.module_dir)
    reel, sample_rate, report = build_reel(
        module,
        args.source,
        fixture_names,
        args.beats,
        args.gap,
    )
    args.out.parent.mkdir(parents=True, exist_ok=True)
    sf.write(args.out, reel, sample_rate, subtype="PCM_16")

    print(
        f"wrote native-layout core audition: {args.out} "
        f"({sample_rate} Hz, {reel.shape[1]} channels, {len(reel) / sample_rate:.2f} s)"
    )
    print("measurement policy: channel 0 (no implicit downmix)")
    for section in report:
        side = section["side_to_mid_db"]
        side_text = "n/a" if side is None else f"{side:.1f} dB"
        print(
            f"  {section['name']}: start {section['start_seconds']:.2f}s, "
            f"duration {section['duration_seconds']:.2f}s, "
            f"ch0 peak {section['channel_zero_peak']:.3f}, side/mid {side_text}"
        )


if __name__ == "__main__":
    main()
