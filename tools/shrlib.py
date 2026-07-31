"""Shared helpers for the audio-analysis tools.

Single definition for common IO/DSP primitives and the two parsers used by the
compiled-core clients, retired-effect counterfactuals, and reference analyzer:
  - signal IO + metrics (load/slice/env/attack/decay/centroid/rolloff/
    spread/f0/bands/rms/peak/crest/chirp/energy_conc)
  - parse_annotations: annotated S1/S2 landmark files (docs/references/timestamps/*.txt)
  - parse_constants: synthesis constants from src/core/Constants.hpp

Metric rationale and the preferred-metric taxonomy live in docs/MEASUREMENT_METHODS.md.
"""

from __future__ import annotations

import re
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, hilbert, sosfiltfilt

SR = 48000  # analysis signals are 48 kHz mono; native stereo core renders must select channel 0 explicitly


def analysis_channel(rendered: np.ndarray) -> np.ndarray:
    """Select channel zero from a native-layout core render without downmixing."""
    rendered = np.asarray(rendered)
    if rendered.ndim != 2 or rendered.shape[1] == 0:
        raise ValueError("core render must have shape (frames, channels)")
    return rendered[:, 0]


def a_weight_gain(f: np.ndarray) -> np.ndarray:
    """Linear IEC-61672 A-weighting gain per frequency, with zero gain at DC.

    Safe for spectral weighting. For time-domain levels call `a_weight` on the
    whole signal before slicing windows.
    """
    f = np.asarray(f, dtype=float)
    f2 = f * f
    ra = (12194.0**2 * f2**2) / (
        (f2 + 20.6**2) * np.sqrt((f2 + 107.7**2) * (f2 + 737.9**2)) * (f2 + 12194.0**2)
    )
    with np.errstate(divide="ignore"):
        a_db = 20.0 * np.log10(ra) + 2.00
    g = 10.0 ** (a_db / 20.0)
    g[f <= 0] = 0.0
    return g


def a_weight(x: np.ndarray, sr: int) -> np.ndarray:
    """A-weighted signal for time-domain levels. Filter the whole signal, then
    slice windows.

    Applying the rfft/irfft round trip to a short window circularly wraps the
    filter tail into its onset and invalidates the breath ruler.
    """
    spectrum = np.fft.rfft(x)
    freq = np.fft.rfftfreq(len(x), 1.0 / sr)
    return np.fft.irfft(spectrum * a_weight_gain(freq), n=len(x))


# Breath-swing Measurement
# Shared by rhythm_offline and ref_analyze so engine and reference use one ruler.

BREATH_WIN_MS = 75.0  # Fixed S1-onset window ending before S2.
BREATH_GROUP_FRAC = 0.30  # Compare the top and bottom 30% by lung inflation.

# Cycle count is necessary but not sufficient; phase coverage is checked separately.
MIN_BREATH_CYCLES = 30.0

# Maximum group-median deviation from uniform phase coverage.
BREATH_MEDIAN_TOL = 0.02


def breath_groups(inflations: np.ndarray, group_frac: float = BREATH_GROUP_FRAC):
    """Return expiration and inspiration indices by inflation quantile."""
    group_n = max(1, int(np.ceil(group_frac * len(inflations))))
    order = np.argsort(inflations)
    return order[:group_n], order[-group_n:]


def breath_group_ratio(
    values: np.ndarray, inflations: np.ndarray, group_frac: float = BREATH_GROUP_FRAC
) -> float:
    """Return inspiration median / expiration median."""
    expiration, inspiration = breath_groups(inflations, group_frac)
    return float(np.median(values[inspiration]) / np.median(values[expiration]))


def breath_swing_db(
    aw_rms: np.ndarray, inflations: np.ndarray, group_frac: float = BREATH_GROUP_FRAC
) -> float:
    """Return the breath loudness swing in dB.

    Negative means inspiration is quieter. This is the `BreathAmpDepth` ruler.
    """
    return float(20.0 * np.log10(breath_group_ratio(aw_rms, inflations, group_frac)))


def breath_cycles(n_beats: int, hr: float, resp_rate: float) -> float:
    """Return the respiratory cycles covered by a beat run."""
    return n_beats / (hr / resp_rate)


# Coverage formulas are curve-specific. SINE is the engine curve; RAISED_COSINE
# reconstructs BR= landmarks. See docs/MEASUREMENT_METHODS.md.
SINE = "sine"
RAISED_COSINE = "raised_cosine"


def _inflation_at_phase(phase: np.ndarray, curve: str) -> np.ndarray:
    """Return lung inflation [0,1] at respiratory phase [0,1)."""
    if curve == SINE:
        return np.sin(np.pi * phase)
    if curve == RAISED_COSINE:
        return 0.5 * (1.0 - np.cos(2.0 * np.pi * phase))
    raise ValueError(f"unknown inflation curve {curve!r} (expected {SINE!r} or {RAISED_COSINE!r})")


def _inflation_quantile(q: np.ndarray | float, curve: str) -> np.ndarray | float:
    """Return the inflation quantile under uniform respiratory phase.

    SINE: sin(pi*phase) is symmetric about phase 0.5, so P(Y<=y) = (2/pi)*arcsin(y) and Q(q) = sin(pi*q/2).
    RAISED_COSINE: monotone on each half-cycle, so P(Y<=y) = arccos(1-2y)/pi and Q(q) = sin^2(pi*q/2).
    """
    if curve == SINE:
        return np.sin(0.5 * np.pi * q)
    if curve == RAISED_COSINE:
        return np.sin(0.5 * np.pi * q) ** 2
    raise ValueError(f"unknown inflation curve {curve!r} (expected {SINE!r} or {RAISED_COSINE!r})")


def breath_group_median_targets(curve: str, group_frac: float = BREATH_GROUP_FRAC) -> tuple[float, float]:
    """Return expiration/inspiration medians under uniform phase coverage.

    Engine and reference curves have different targets and cannot be graded against
    each other.

    The bottom-`group_frac` group's median is the (group_frac/2) quantile and the top group's the
    (1 - group_frac/2) one. At 30%: SINE gives 0.2334/0.9724, RAISED_COSINE gives 0.0545/0.9455.
    """
    q = np.array([0.5 * group_frac, 1.0 - 0.5 * group_frac])
    lo, hi = _inflation_quantile(q, curve)
    return float(lo), float(hi)


def breath_group_bias(
    n_beats: int, beats_per_breath: float, curve: str, n_phases: int = 721
) -> tuple[float, float]:
    """Return SD and worst group-median phase-sampling error.

    Values are fractions of the metric's breath swing. Integrates over unknown
    start phase for a rigid 1/beats_per_breath increment; bias vanishes at integer
    spans and is largest at fractional spans.
    """
    phase0 = np.linspace(0.0, 1.0, n_phases, endpoint=False)
    phase = (phase0[:, None] + np.arange(n_beats)[None, :] / beats_per_breath) % 1.0
    inflation = _inflation_at_phase(phase, curve)
    # the value a fully-covered run converges to: the median of the inflation MARGINAL
    full = float(_inflation_quantile(0.5, curve))
    error = np.median(inflation, axis=1) - full
    return float(error.std()), float(np.abs(error).max())


def breath_swing_problems(
    inflations: np.ndarray, hr: float, resp_rate: float, curve: str, group_frac: float = BREATH_GROUP_FRAC
) -> list[str]:
    """Return reasons a breath swing must not be trusted.

    Requires enough cycles and representative group-median phase coverage. Engine
    and BR=-reconstructed curves have different targets. More beats cannot repair
    a rigid phase lock; callers must reject every returned problem.
    """
    problems: list[str] = []
    cycles = breath_cycles(len(inflations), hr, resp_rate)
    too_short = cycles < MIN_BREATH_CYCLES
    if too_short:
        problems.append(
            f"only {cycles:.1f} respiratory cycles covered (< {MIN_BREATH_CYCLES:.0f}); "
            f"the swing has not converged - raise the beat count"
        )

    expiration, inspiration = breath_groups(inflations, group_frac)
    realized = (float(np.median(inflations[expiration])), float(np.median(inflations[inspiration])))
    targets = breath_group_median_targets(curve, group_frac)
    deviation = max(abs(r - t) for r, t in zip(realized, targets))
    if deviation > BREATH_MEDIAN_TOL:
        # Report the observation; name only the cause actually ruled in. A short run is under-covered for
        # the boring reason. A LONG run that is still under-covered is a phase lock - the one no beat
        # count can fix.
        cause = (
            "raise the beat count first, then re-check - a run this short is under-covered for the "
            "boring reason"
            if too_short
            else f"MORE BEATS WILL NOT FIX THIS: at {hr / resp_rate:.3f} beats/breath the phase advance "
            f"revisits too few phases, and it is a rigid rotation whenever RSA is off (exertion 1). "
            f"Move the operating point off the lock, or measure where RSA is live (exertion < 1)"
        )
        problems.append(
            f"respiratory phase is under-covered: inflation-group medians "
            f"{realized[0]:.3f}/{realized[1]:.3f} vs {targets[0]:.3f}/{targets[1]:.3f} expected under "
            f"uniform coverage (deviation {deviation:.3f} > {BREATH_MEDIAN_TOL}). {cause}"
        )
    return problems


# Rise/body HF contrast. See rise_body_contrast.
HF_BAND_HZ = (150.0, 2000.0)
CONTRAST_RISE_MS = 30.0
CONTRAST_BODY_MS = 60.0

# Executable metric contract; docs/MEASUREMENT_METHODS.md owns the shared taxonomy and rationale.
METRIC_METADATA = {
    "env": {"anchor": "none", "valid": "within-signal"},
    "env_analytic": {"anchor": "none", "valid": "within-signal"},
    "onset_peak_idx": {"anchor": "threshold", "valid": "within-signal"},
    "attack_ms": {"anchor": "threshold", "valid": "within-signal"},
    "rise_10_90_ms": {
        "anchor": "threshold",
        "valid": "cross-signal",
        "invalid": ("pre-onset-component-near-10pct",),
    },
    "decay_ms": {"anchor": "threshold", "valid": "within-signal"},
    "centroid": {"anchor": "none", "valid": "within-signal"},
    "rolloff": {"anchor": "threshold", "valid": "within-signal"},
    "spread": {"anchor": "none", "valid": "within-signal"},
    "f0": {"anchor": "peak", "valid": "within-signal"},
    "bands": {"anchor": "none", "valid": "within-signal"},
    "rms": {"anchor": "none", "valid": "within-signal"},
    "peak": {"anchor": "peak", "valid": "within-signal"},
    "crest": {"anchor": "peak", "valid": "within-signal"},
    "chirp": {"anchor": "none", "valid": "within-signal"},
    "energy_conc": {"anchor": "none", "valid": "within-signal"},
    "hf_band": {"anchor": "none", "valid": "within-signal"},
    "lobe_count": {"anchor": "peak", "valid": "within-signal"},
    "hf_temporal_skew": {
        "anchor": "none",
        "valid": "cross-signal",
        "role": "hf-lead-lag-diagnostic",
        "window": "complete-s1-support",
        "aggregation": "group-median-for-references",
        "invalid": ("truncated-s1", "material-nonlinear-distortion"),
    },
    "rise_body_contrast": {"anchor": "peak", "valid": "within-signal"},
    "a_weight": {"anchor": "none", "valid": "cross-signal"},
}


# Signal IO


def load(path: str | Path) -> tuple[np.ndarray, int]:
    """Load WAV channel 0 as a mono float signal and return its sample rate."""
    x, sr = sf.read(str(path), always_2d=True)
    return x[:, 0].astype(float), sr


def sl(x: np.ndarray, sr: int, a: float, b: float) -> np.ndarray:
    """Return signal x from a to b seconds."""
    return x[int(a * sr) : int(b * sr)]


# Metric battery


def env(s: np.ndarray, sr: int, ms: float = 0.5) -> np.ndarray:
    """Rectified-and-box-smoothed envelope.

    `ms` must span at least one F0 period or half-cycle zeroes remain. Use
    `env_analytic` for timing. The annotator supplies its own beat-scale smoother.
    """
    n = max(1, int(ms * 1e-3 * sr))
    return np.convolve(np.abs(s), np.ones(n) / n, "same")


def env_analytic(s: np.ndarray, sr: int, ms: float = 1.0) -> np.ndarray:
    """Return a lightly smoothed analytic envelope without half-cycle ripple.

    Use for lobe timing. `ms` only tames sample-rate noise and is not load-bearing.
    """
    e = np.abs(hilbert(s))
    n = max(1, int(ms * 1e-3 * sr))
    return np.convolve(e, np.ones(n) / n, "same") if n > 1 else e


def onset_peak_idx(s: np.ndarray, sr: int, frac: float = 0.1) -> tuple[int, int]:
    """Return analytic-envelope onset and peak sample indices.

    Onset is the last pre-peak crossing of `frac`, matching the C++ S1 compressor.
    A near-peak null can reset it, so compare only like lobe structures. Use
    `rise_10_90_ms` across signals.
    """
    e = env_analytic(s, sr)
    pk = int(np.argmax(e))
    if e[pk] <= 0:
        return 0, 0
    below = np.flatnonzero(e[:pk] < frac * e[pk])
    return (int(below[-1]) + 1 if len(below) else 0), pk


def attack_ms(s: np.ndarray, sr: int) -> float:
    """Return analytic-envelope 10%-to-peak time in ms.

    Relative-threshold and last-crossing dependent. A near-peak null resets it; never compare unlike lobe
    structures. Use `rise_10_90_ms` across signals.
    """
    o, pk = onset_peak_idx(s, sr)
    return np.nan if pk <= o else (pk - o) / sr * 1e3


def rise_10_90_ms(s: np.ndarray, sr: int) -> float:
    """Return null-immune 10%-to-90% rise time in ms.

    Uses the running-maximum analytic envelope. Pre-onset energy above 10% can
    start the clock early, so anchor the input at the S1 annotation.
    """
    e = env_analytic(s, sr)
    pk = int(np.argmax(e))
    if pk <= 0 or e[pk] <= 0:
        return np.nan
    climb = np.maximum.accumulate(e[: pk + 1])
    i10 = np.flatnonzero(climb >= 0.10 * e[pk])
    i90 = np.flatnonzero(climb >= 0.90 * e[pk])
    if not len(i10) or not len(i90):
        return np.nan
    return (int(i90[0]) - int(i10[0])) / sr * 1e3


def decay_ms(s: np.ndarray, sr: int) -> float:
    """Return peak-to-first-10% analytic-envelope decay time in ms."""
    e = env_analytic(s, sr)
    pk = int(np.argmax(e))
    if e[pk] <= 0:
        return np.nan
    thr = 0.1 * e[pk]
    below = np.flatnonzero(e[pk:] < thr)
    d1 = pk + int(below[0]) if len(below) else len(e) - 1
    return (d1 - pk) / sr * 1e3


def spec(s: np.ndarray, sr: int, n: int = 1 << 15) -> tuple[np.ndarray, np.ndarray]:
    w = s * np.hanning(len(s))
    S = np.abs(np.fft.rfft(w, n))
    f = np.fft.rfftfreq(n, 1 / sr)
    return f, S


def centroid(s: np.ndarray, sr: int, fmax: float = 1200) -> float:
    """Return magnitude-weighted spectral centroid below fmax.

    Power weighting over-emphasizes the loud ~50 Hz fundamental and hides the
    brightness shifts being measured. Narrow valve transients use a separate
    A-weighted HF metric.
    """
    f, S = spec(s, sr)
    m = f <= fmax
    w = S[m]
    return np.nan if w.sum() == 0 else float((f[m] * w).sum() / w.sum())


def rolloff(s: np.ndarray, sr: int, frac: float = 0.85, fmax: float = 1200) -> float:
    f, S = spec(s, sr)
    m = f <= fmax
    p = np.cumsum(S[m] ** 2)
    if p[-1] == 0:
        return np.nan
    return float(f[m][np.searchsorted(p, frac * p[-1])])


def spread(s: np.ndarray, sr: int, fmax: float = 1200) -> float:
    """Return magnitude-weighted spectral spread below fmax."""
    f, S = spec(s, sr)
    m = f <= fmax
    w = S[m]
    if w.sum() == 0:
        return np.nan
    c = (f[m] * w).sum() / w.sum()
    return float(np.sqrt(((f[m] - c) ** 2 * w).sum() / w.sum()))


def f0(s: np.ndarray, sr: int, lo: float = 25, hi: float = 140) -> float:
    """Parabolic-interpolated spectral peak in [lo, hi] Hz."""
    f, S = spec(s, sr, 1 << 16)
    m = (f >= lo) & (f <= hi)
    fi, Si = f[m], S[m]
    k = int(np.argmax(Si))
    if 0 < k < len(Si) - 1:
        a, b, c = Si[k - 1], Si[k], Si[k + 1]
        d = 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
        return float(fi[k] + d * (fi[1] - fi[0]))
    return float(fi[k])


def bands(s: np.ndarray, sr: int, edges: tuple[float, ...] = (20, 40, 80, 150, 300, 1200)) -> list[float]:
    """Energy fraction within each consecutive edge pair (of the total below the last edge)."""
    f, S = spec(s, sr)
    p = S**2
    tot = p[f <= edges[-1]].sum()
    return [
        float(p[(f >= lo) & (f < hi)].sum() / tot) if tot > 0 else np.nan for lo, hi in zip(edges, edges[1:])
    ]


def rms(s: np.ndarray) -> float:
    return float(np.sqrt(np.mean(s**2))) if len(s) else 0.0


def peak(s: np.ndarray) -> float:
    return float(np.max(np.abs(s))) if len(s) else 0.0


def crest(s: np.ndarray) -> float:
    r = rms(s)
    return float(peak(s) / r) if r > 0 else np.nan


def chirp(s: np.ndarray, sr: int) -> tuple[float, float]:
    """Return first-third and last-third centroids; down-chirp means first > last."""
    n = len(s) // 3
    if n < 10:
        return np.nan, np.nan
    return centroid(s[:n], sr), centroid(s[-n:], sr)


def energy_conc(s: np.ndarray, sr: int) -> list[float]:
    """Return energy fractions in the first 0-20, 20-40, and 40-80 ms."""
    e = s**2
    tot = e.sum()
    if tot == 0:
        return [np.nan] * 3

    def w(a, b):
        return float(e[int(a * 1e-3 * sr) : int(b * 1e-3 * sr)].sum() / tot)

    return [w(0, 20), w(20, 40), w(40, 80)]


def db(ratio: float) -> float:
    return 20.0 * np.log10(ratio + 1e-9)


# Rise/body HF contrast


def hf_band(x: np.ndarray, sr: int, lo: float = HF_BAND_HZ[0], hi: float = HF_BAND_HZ[1]) -> np.ndarray:
    """Apply a zero-phase fourth-order Butterworth bandpass to the whole signal.

    Whole-signal filtering keeps edge transients out of lobe onsets. Zero phase
    prevents group delay from shifting rise energy into the body window.
    """
    return sosfiltfilt(butter(4, [lo, hi], btype="band", fs=sr, output="sos"), x)


def _lobe_peak_indices(
    s: np.ndarray, sr: int, frac: float = 0.45, min_gap_ms: float = 15.0
) -> tuple[np.ndarray, np.ndarray]:
    """Return the exact smoothed envelope and peak indices used by `lobe_count`.

    Private shared machinery for the metric and its diagnostic view; callers should report
    `lobe_count`, not treat these indices as anatomical component labels.
    """
    from scipy.signal import find_peaks

    e = env_analytic(s, sr, ms=5.0)
    if e.max() <= 0:
        return e, np.array([], dtype=int)
    p, _ = find_peaks(e, height=frac * e.max(), distance=max(1, int(min_gap_ms * 1e-3 * sr)))
    if len(p) == 0:
        p = np.array([int(np.argmax(e))])
    return e, p


def lobe_count(
    s: np.ndarray, sr: int, frac: float = 0.45, min_gap_ms: float = 15.0
) -> tuple[int, int, float]:
    """Return lobe count, tallest index, and runner-up/tallest ratio.

    Gate peak-anchored metrics with this: when the runner-up approaches the tallest, `argmax` can flip
    lobes between beats. `hf_temporal_skew` can describe HF lead/lag across unlike structures only over
    complete S1 support and in the absence of material nonlinear distortion.
    """
    e, p = _lobe_peak_indices(s, sr, frac, min_gap_ms)
    if len(p) == 0:
        return 0, 0, 0.0
    pk = int(np.argmax(e))
    idx = int(np.where(p == pk)[0][0]) + 1 if pk in p else 1
    h = sorted(e[p], reverse=True)
    return len(p), idx, float(h[1] / h[0]) if len(h) > 1 else 0.0


def _energy_temporal_centroid(s: np.ndarray) -> float:
    """Return the energy-weighted temporal centroid in samples, or NaN for zero energy."""
    weights = s**2
    total = weights.sum()
    if total <= 0:
        return np.nan
    return float((np.arange(len(s)) * weights).sum() / total)


def hf_temporal_skew(lobe: np.ndarray, hf_slice: np.ndarray) -> float:
    """Return normalized HF-minus-broadband temporal centroid.

    Negative means HF energy leads the S1's total energy; positive means it trails. The calculation has
    no inferred peak or threshold anchor, but the caller-supplied window is part of the ruler: it must
    contain the complete S1, and comparisons must use the same window fraction of actual S1 duration.
    Material clipping or saturation manufactures time-localized HF and invalidates physiological
    interpretation. Use this as a lead/lag diagnostic and late-HF-wash detector, not as a perceptual
    sharpness ordering, drive proxy, or tuning setpoint.
    """
    if len(lobe) != len(hf_slice):
        raise ValueError("lobe and hf_slice must describe the same window")
    if len(lobe) == 0:
        return np.nan
    broadband_centroid = _energy_temporal_centroid(lobe)
    hf_centroid = _energy_temporal_centroid(hf_slice)
    if np.isnan(broadband_centroid) or np.isnan(hf_centroid):
        return np.nan
    return float((hf_centroid - broadband_centroid) / len(lobe))


def rise_body_contrast(
    lobe: np.ndarray,
    hf_signal: np.ndarray,
    sr: int,
    t0: float,
    s2_onset: float | None = None,
    rise_ms: float = CONTRAST_RISE_MS,
    body_ms: float = CONTRAST_BODY_MS,
) -> tuple[float, float, float]:
    """Return contrast, rise density, and body density for one S1 lobe.

    Contrast divides mean 150-2000 Hz energy before the peak by mean energy after
    it. Density values normalize those means by squared lobe peak amplitude.
    Windows are fixed and peak-anchored; unequal or threshold-defined windows are
    invalid.

    `t0` locates the lobe inside the already-filtered whole signal. Returns NaN if the rise is out of
    bounds or S2 would contaminate the body. Direction indicator only: report group median and spread,
    confirm by ear, and compare only like lobe structures (`lobe_count`). For unlike structures,
    `hf_temporal_skew` is limited to complete, materially undistorted S1 windows and describes HF
    lead/lag rather than perceptual sharpness.
    """
    nan3 = (np.nan, np.nan, np.nan)
    pk = int(np.argmax(env_analytic(lobe, sr)))
    i0 = int(round(t0 * sr))
    n_rise, n_body = int(rise_ms * 1e-3 * sr), int(body_ms * 1e-3 * sr)
    if i0 + pk - n_rise < 0 or i0 + pk + n_body > len(hf_signal):
        return nan3
    if s2_onset is not None and i0 + pk + n_body > int(round(s2_onset * sr)):
        return nan3
    y = hf_signal[i0 + pk - n_rise : i0 + pk + n_body]
    rise = float(np.mean(y[:n_rise] ** 2))
    body = float(np.mean(y[n_rise:] ** 2))
    a2 = peak(lobe) ** 2
    if body <= 0 or a2 <= 0:
        return nan3
    return rise / body, rise / a2, body / a2


_TS_RE = re.compile(r"(\d+):(\d+(?:\.\d+)?)")
_S1_RE = re.compile(r"S1=([\d:.]+)-([\d:.]+)")
_S2_RE = re.compile(r"S2=([\d:.]+)-([\d:.]+)")
_BR_RE = re.compile(r"BR=([\d:.]+)-([\d:.]+)")
_VENT_RE = re.compile(r"vent:\s*b\d+\s+(expiration|inspiration)")
_EX_RE = re.compile(r"EX=([\d:.]+)-([\d:.]+)")
_SCOPE_RE = re.compile(r"scope:\s*(\w+)")


def mmss(s: str) -> float:
    """Parse MM:SS(.mmm) into seconds."""
    m = _TS_RE.fullmatch(s.strip())
    if not m:
        raise ValueError(f"bad timestamp {s!r}")
    return int(m.group(1)) * 60 + float(m.group(2))


def fmt_mmss(t: float) -> str:
    """Format seconds as the annotation timestamp MM:SS.mmm.

    Millisecond precision matches the annotation files. Minutes are zero-padded
    to two digits but widen past 99 min if ever needed.
    """
    m, s = divmod(round(t, 3), 60)
    return f"{int(m):02d}:{s:06.3f}"


def parse_breaths(path: str | Path) -> list[tuple[float, float]]:
    """Parse `BR=<trough>-<peak>` landmarks into seconds.

    The two landmarks encode:
      - period: trough -> next trough
      - InspirationFraction: (peak - trough) / period
      - per-beat inflation through `breath_inflation`

    Depth is not encoded and cannot be inferred from the muffle it is meant to
    predict. It requires an independent channel such as audible airflow or
    remains unknown. See docs/MEASUREMENT_METHODS.md.
    """
    breaths: list[tuple[float, float]] = []
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line.startswith("BR="):
            continue
        m = _BR_RE.search(line)
        if not m:
            continue
        trough, peak = mmss(m.group(1)), mmss(m.group(2))
        if trough >= peak:
            print(f"  warn: skipping malformed breath in {Path(path).name}: {line}")
            continue
        breaths.append((trough, peak))
    return sorted(breaths)


def parse_exclusions(path: str | Path, scope: str | None = None) -> list[tuple[float, float]]:
    """Parse `EX=<start>-<end> (scope: <s>) (reason)` spans into seconds.

    Exclusions are a blacklist; groups are the semantic whitelist:
      - `scope: all`    the audio is corrupt or otherwise unusable
      - `scope: breath` the audio is usable but respiratory phase is undefined

    State changes such as recovery, posture, or breathing pattern belong in
    groups rather than exclusions.
    """
    spans: list[tuple[float, float]] = []
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line.startswith("EX="):
            continue
        m = _EX_RE.search(line)
        if not m:
            continue
        a, b = mmss(m.group(1)), mmss(m.group(2))
        if a >= b:
            print(f"  warn: skipping malformed exclusion in {Path(path).name}: {line}")
            continue
        scope_match = _SCOPE_RE.search(line)
        span_scope = scope_match.group(1) if scope_match else "all"
        if scope is None or span_scope == "all" or span_scope == scope:
            spans.append((a, b))
    return sorted(spans)


def excluded(times: np.ndarray, spans: list[tuple[float, float]]) -> np.ndarray:
    """Return a mask for times inside any excluded span."""
    mask = np.zeros(np.shape(times), dtype=bool)
    for a, b in spans:
        mask |= (np.asarray(times) >= a) & (np.asarray(times) <= b)
    return mask


# TODO: This function should be removed and all users moved to the new BR annotation format.
def parse_vent_tags(path: str | Path) -> list[tuple[float, str]]:
    """Parse legacy per-beat vent tags into (S1 onset, phase).

    ref20's tags approximate the top and bottom 30% by muffle rather than cycle
    extrema; treating them as extrema would move the ruler by ~2.8 dB. Prefer BR=
    landmarks for continuous phase.
    """
    tags: list[tuple[float, str]] = []
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line.startswith("S1="):
            continue
        beat, vent = _S1_RE.search(line), _VENT_RE.search(line)
        if beat and vent:
            tags.append((mmss(beat.group(1)), vent.group(1)))
    return tags


def breath_inflation(times: np.ndarray, breaths: list[tuple[float, float]]) -> np.ndarray:
    """Return normalized lung inflation at `times` from BR= landmarks.

    Each half-cycle uses a raised cosine, preserving the annotated inspiration
    and expiration durations. Inflation is 0 at troughs and 1 at peaks. Times
    outside the annotated span return NaN rather than extrapolating.
    """
    times = np.asarray(times, dtype=float)
    inflation = np.full(times.shape, np.nan)
    for i, (trough, peak) in enumerate(breaths):
        next_trough = breaths[i + 1][0] if i + 1 < len(breaths) else None

        rising = (times >= trough) & (times <= peak)
        u = (times[rising] - trough) / (peak - trough)
        inflation[rising] = 0.5 * (1.0 - np.cos(np.pi * u))

        if next_trough is not None and next_trough > peak:
            falling = (times > peak) & (times < next_trough)
            u = (times[falling] - peak) / (next_trough - peak)
            inflation[falling] = 0.5 * (1.0 + np.cos(np.pi * u))
    return inflation


def parse_annotations(path: str | Path) -> dict[str, list]:
    """Parse an S1/S2 annotation file into ordered groups.

    Returns an ordered dict {group_label: [(s1a, s1b, s2a, s2b), ...]}.
    A line beginning with `S1=` is a beat in the current group. Any other
    non-blank line starts a new group with its text (minus an optional trailing
    colon) as the label. Beats with inconsistent ordering (e.g. an annotation
    typo) are skipped with a warning, and groups left with no valid beats are
    dropped.

    `BR=` respiratory landmarks are a separate, beat-independent layer and are
    skipped here. They are read with `parse_breaths`.
    """
    groups: dict[str, list] = {}
    order: list[str] = []
    label = "default"
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("BR="):
            continue
        if line.startswith("S1="):
            m1, m2 = _S1_RE.search(line), _S2_RE.search(line)
            if not (m1 and m2):
                continue
            beat = (mmss(m1.group(1)), mmss(m1.group(2)), mmss(m2.group(1)), mmss(m2.group(2)))
            if not (beat[0] < beat[1] <= beat[2] < beat[3]):
                print(f"  warn: skipping malformed beat in {Path(path).name}: {line}")
                continue
            if label not in groups:
                groups[label] = []
                order.append(label)
            groups[label].append(beat)
        else:
            label = line.rstrip(":").strip()
    return {k: groups[k] for k in order if groups.get(k)}


def beats_to_segments(signal: np.ndarray, sr: int, beats: list) -> list:
    """Convert beat landmarks into (S1, S2, diastole) signal slices.

    Diastole runs from S2 end to the next S1 onset, capped at 300 ms. The final
    beat uses 100 ms past S2.
    """
    segs = []
    for i, (s1a, s1b, s2a, s2b) in enumerate(beats):
        nxt = beats[i + 1][0] if i + 1 < len(beats) else s2b + 0.10
        dia_end = min(nxt, s2b + 0.30)
        segs.append((sl(signal, sr, s1a, s1b), sl(signal, sr, s2a, s2b), sl(signal, sr, s2b, dia_end)))
    return segs


def beats_hr(beats: list) -> float:
    """Return median-IBI HR from S1 onsets, or NaN with fewer than two beats.

    Uses the median so a dropped or degenerate annotation, which would leave a
    ~2x gap, does not pull the estimate down.
    """
    onsets = [b[0] for b in beats]
    if len(onsets) < 2:
        return np.nan
    return float(60.0 / np.median(np.diff(onsets)))


_CONST_RE = re.compile(
    r"constexpr" + r"\s+" + r"(?:float|double|int|std::uint32_t|std::int32_t)"
    r"\s+" + r"(\w+)" + r"\s*" + r"="
    r"\s*" + r"([^;]+);"
)


def parse_constants(hpp_path: str | Path) -> dict[str, float]:
    """Parse numeric `constexpr` definitions into {name: value}.

    Handles literal-suffix `F`, line comments after the value, and expressions
    that reference earlier constants. Non-numeric or unparseable definitions are
    skipped.
    """
    text = Path(hpp_path).read_text(encoding="utf-8")
    ns: dict[str, float] = {}
    for name, expr in _CONST_RE.findall(text):
        expr = expr.split("//")[0].strip()
        # Strip C++ float/long suffixes from numeric literals.
        expr = re.sub(r"(?<=[0-9.])[fFlL](?![A-Za-z0-9_])", "", expr)
        try:
            ns[name] = eval(expr, {"__builtins__": {}}, ns)  # noqa: S307 - trusted repo source
        except Exception:
            continue  # Skip non-numeric constants.
    return ns
