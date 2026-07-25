"""Verify the offline core binding (shr_pybind) reproduces the golden beat-renderer output.

This is the binding's acceptance test: it drives the compiled ``shr_pybind`` from Python over the same
fixture specs used elsewhere and checks that every stage matches ``tests/golden/beat_render.json``
bit-for-bit. A pass proves the Python access path returns exactly what the plugin's renderer produces,
so callers can migrate off the NumPy render mirror. WAV parsing stays in Python (soundfile), mirroring
the plugin's decode-then-render split.

Build the module first with tools/build_pybind.py.

    python tools/check_pybind_golden.py
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


def _sha(array: np.ndarray) -> str:
    # Match the fixture exe's raw little-endian float32 byte order that the golden was hashed from.
    return hashlib.sha256(np.ascontiguousarray(array, dtype="<f4").tobytes()).hexdigest()


def check(module_dir: Path) -> None:
    sys.path.insert(0, str(module_dir))
    try:
        import shr_pybind
    except ImportError as error:
        sys.exit(
            f"cannot import shr_pybind from {module_dir} ({error}); "
            f"build it with: python tools/build_pybind.py"
        )

    golden = json.loads(GOLDEN.read_text(encoding="utf-8"))["fixtures"]
    samples, rate = sf.read(SOURCE, dtype="int16", always_2d=True)
    source = shr_pybind.prepare_source(np.ascontiguousarray(samples), rate)

    problems: list[str] = []
    for name, spec in FIXTURES.items():
        if name not in golden:
            problems.append(f"{name}: fixture not present in golden manifest")
            continue
        stages = shr_pybind.trace_beat_render(
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
        for stage in STAGES:
            array = stages[stage]
            expected = golden[name][stage]
            if array.shape[0] != expected["frames"]:
                problems.append(
                    f"{name}/{stage}: frames {array.shape[0]} != golden {expected['frames']}"
                )
            if _sha(array) != expected["sha256"]:
                problems.append(f"{name}/{stage}: sha256 mismatch vs golden")

    if problems:
        print("shr_pybind DIVERGED from golden:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        sys.exit("binding does not reproduce the golden beat renderer")

    print(f"shr_pybind reproduces golden beat renderer ({len(FIXTURES)} fixtures, {len(STAGES)} stages):")
    for name in FIXTURES:
        print(f"  {name}: OK")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=ROOT / "build" / "pybind",
        help="Directory containing the built shr_pybind*.pyd (default: build/pybind).",
    )
    args = parser.parse_args()
    check(args.module_dir.resolve())


if __name__ == "__main__":
    main()
