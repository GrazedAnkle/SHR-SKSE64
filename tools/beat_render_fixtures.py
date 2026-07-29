"""Shared beat-render fixture specs and stage names.

Hand-transcribed from ``tests/BeatRenderFixtures.hpp``; edit both files together. Drift is caught in one
direction only: a value changed here fails the beat-renderer golden, while one changed there fails
nothing, because the manifest is generated from this file. WI-038 owns removing the transcription.
"""
from __future__ import annotations

from dataclasses import dataclass

# Fixed by the BeatRenderTrace domains; ordered source -> transmission -> transducer-input -> output.
STAGES = (
    "source_s1",
    "source_s2",
    "transmitted_s1",
    "transmitted_s2",
    "transducer_input",
    "output",
)


@dataclass(frozen=True)
class BeatRenderSpec:
    """Direct float-renderer controls matching C++ RenderSpec."""

    ibi: float
    systole_duration: float
    s1_amplitude: float
    s2_amplitude: float
    s1_resample_ratio: float
    s2_resample_ratio: float
    lowpass_cutoff_hz: float
    onset_compression: float
    kind: str = "sinus"


FIXTURES: dict[str, BeatRenderSpec] = {
    "rest": BeatRenderSpec(
        ibi=60.0 / 79.0,
        systole_duration=0.32991,
        s1_amplitude=1.0,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.0,
    ),
    "peak": BeatRenderSpec(
        ibi=60.0 / 177.0,
        systole_duration=0.130,
        s1_amplitude=3.5481339,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=2.5,
    ),
    "recovery": BeatRenderSpec(
        ibi=0.6,
        systole_duration=0.244,
        s1_amplitude=2.25,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.8,
    ),
    "inspiration": BeatRenderSpec(
        ibi=60.0 / 79.0,
        systole_duration=0.32991,
        s1_amplitude=0.53,
        s2_amplitude=0.53,
        s1_resample_ratio=0.86,
        s2_resample_ratio=0.86,
        lowpass_cutoff_hz=112.0,
        onset_compression=1.0,
    ),
    "pvc": BeatRenderSpec(
        ibi=0.735,
        systole_duration=0.19932,
        s1_amplitude=0.55125,
        s2_amplitude=0.60,
        s1_resample_ratio=0.90,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.0,
        kind="pvc",
    ),
    # Provisional edge fixtures (see tests/BeatRenderFixtures.hpp): extreme-vigor drives the soft-knee into
    # saturation, truncation collapses the S2 window to zero.
    "extreme-vigor": BeatRenderSpec(
        ibi=60.0 / 177.0,
        systole_duration=0.130,
        s1_amplitude=6.079,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=2.5,
    ),
    "truncation": BeatRenderSpec(
        ibi=0.12,
        systole_duration=0.130,
        s1_amplitude=1.0,
        s2_amplitude=1.0,
        s1_resample_ratio=1.0,
        s2_resample_ratio=1.0,
        lowpass_cutoff_hz=450.0,
        onset_compression=1.0,
    ),
}
