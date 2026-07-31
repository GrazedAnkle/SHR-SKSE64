"""The named beat-render operating points, and the stage names the trace returns.

Sole definition: the beat-renderer golden manifest and the audition reel are both built from these
specs, so any change here is caught by the golden. The C++ suite asserts renderer properties from its
own local specs and does not mirror this table.

The values are regression inputs, not physiological claims. Changing one is a retune, and the manifest
is regenerated with ``python tools/capture_goldens.py beat-renderer`` so the diff is reviewable.
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
    # The two edge fixtures below carry provisional values. They guard the renderer's most nonlinear
    # branches, not a validated operating point. Their extreme-end amplitudes are extrapolated (the
    # corpus has no max-effort reference); the extreme-value audit (ROADMAP "maintenance candidates")
    # may retune them.
    #
    # extreme-vigor: vigor-jitter ceiling. Peak is Vigor 1.0 -> s1_amplitude 10^(0.55*1.0) = 3.548;
    # the VigorJitterMaxSigma(2.5) * VigorJitterScale(0.17) clamp lifts effective vigor to 1.425, so
    # s1_amplitude = 10^(0.55*1.425) ~= 6.079, with onset_compression at AttackCompressMax. Timing is
    # borrowed from peak. This is the only fixture that drives ApplySoftKnee hard into saturation.
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
    # truncation: synthetic robustness case, not a physiological rate. ibi(0.12) < systole_duration
    # drops the S2 window to zero (MixTransducerInput's s2Window==0 path) and forces S1's s1Cap /
    # totalFrames clamp to bind. No other fixture reaches the S2-absent branch.
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
