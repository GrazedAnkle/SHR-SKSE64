"""Steady-state rhythm layer for beat-to-beat analysis.

Adds the sinus steady-state branch missing from engine_offline.py: it converts a
fixed HR, exertion, and contractility into per-beat parameters and passes them to
engine_offline.synth_beat.

Scope:
  - BaseIBI RSA, preceding-RR Frank-Starling, respiratory-phase advance, and
    optional per-beat vigor jitter
  - sinus steady state at one operating point; no simulation dynamics, PVCs,
    compensatory pauses, runs, or PEP hysteresis

Deterministic RSA, Frank-Starling, and DSP match the engine exactly. Vigor jitter
matches only distributionally because C++ and NumPy use different RNGs.

Measurement rationale and comparison rules live in docs/MEASUREMENT_METHODS.md.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

import shrlib
from shrlib import SR
import engine_offline as eo


_C = eo._C


def ref8_peak_cv_target() -> tuple[float, float] | None:
    """Return ref8's peak-group S1 peak CV and jackknife SE.

    Returns None if the measurements.json leaf is absent.
    """
    path = Path(__file__).resolve().parents[1] / "docs" / "references" / "measurements.json"
    try:
        import json
        group = json.loads(path.read_text(encoding="utf-8"))["ref8"]["groups"]["peak (~180)"]
        return float(group["s1_peak_cv"]), float(group["s1_peak_cv_se"])
    except (OSError, KeyError, ValueError):
        return None


def base_ibi(hr: float, resp_phase: float, exertion: float) -> float:
    """Return RhythmEngine BaseIBI with RSA fading to zero at maximal exertion."""
    nominal = 60.0 / hr
    rsa = _C["RSAAmplitudeRest"] * (1.0 - exertion)
    jitter = rsa * np.sin(2.0 * np.pi * resp_phase)
    return max(0.1, nominal * (1.0 - jitter))


def beat_sequence(path: str | Path, hr: float, contractility: float, exertion: float,
                  n_beats: int = 24, vigor_sigma: float | None = None, seed: int = 0,
                  src_highpass: float | None = None, resp_rate: float | None = None,
                  breath_depth: float | None = None,
                  synth_options: dict | None = None) -> dict:
    """Render a steady-state sinus run and return {audio, beats, ...}.

    Mirrors RhythmEngine fire ordering: a beat uses the IBI set by the previous
    beat, Frank-Starling reads the preceding interval, and the next IBI and
    respiratory phase advance from the current phase. `beats` contains per-beat
    audio, IBI, Frank-Starling, contractility, respiratory phase, and inflation.
    """
    if vigor_sigma is None:
        vigor_sigma = _C["VigorJitterScale"]  # match the engine default; pass a value to override/disable
    s1, s2 = eo.prep_source(path, src_highpass)
    rng = np.random.default_rng(seed)
    synth_options = dict(synth_options or {})

    target_rate, target_depth = shrlib.ventilation_targets(_C, exertion)
    resp_rate = target_rate if resp_rate is None else resp_rate
    breath_depth = target_depth if breath_depth is None else breath_depth
    nominal_ibi = 60.0 / hr
    fs_min, fs_max = _C["FrankStarlingMin"], _C["FrankStarlingMax"]

    resp_phase = 0.0
    preceding_rr = nominal_ibi
    next_ibi = base_ibi(hr, resp_phase, exertion)

    beats = []
    for _ in range(n_beats):
        effective_ibi = next_ibi
        frank_starling = float(np.clip(preceding_rr / nominal_ibi, fs_min, fs_max))

        # Mirror the engine's instantaneous vigor draw: preserve excursions above the mean, clamp the
        # artifact-prone upper tail, and keep the result nonnegative. State contractility is an input here.
        c_beat = contractility
        if vigor_sigma > 0.0:
            draw = min(_C["VigorJitterMaxSigma"], rng.standard_normal())
            c_beat = max(0.0, contractility * (1.0 + vigor_sigma * draw))

        audio = eo.synth_beat(
            s1, s2, hr, c_beat, frank_starling, resp_phase, exertion,
            breath_depth=breath_depth, **synth_options
        )
        beats.append({"audio": audio, "ibi": effective_ibi, "frank_starling": frank_starling,
                      "contractility": c_beat, "resp_phase": resp_phase,
                      "lung_inflation": float(np.sin(np.pi * resp_phase))})

        next_ibi = base_ibi(hr, resp_phase, exertion) # IBI to the following beat
        preceding_rr = effective_ibi
        resp_phase = (resp_phase + resp_rate / 60.0 * next_ibi) % 1.0

    audio = np.concatenate([b["audio"] for b in beats])
    return {"audio": audio, "beats": beats, "hr": hr, "exertion": exertion,
            "contractility": contractility, "resp_rate": resp_rate,
            "breath_depth": breath_depth, "vigor_sigma": vigor_sigma}


def _s1_window(beat_audio: np.ndarray, hr: float, dur_ms: float) -> np.ndarray:
    """Return a dur_ms onset-aligned S1 slice matching reference windows.

    Uses the first 10% crossing of the running-maximum envelope, as in
    shrlib.rise_10_90_ms. shrlib.onset_peak_idx uses the last crossing, which can
    reset at a near-peak null and discard the rise. synth_beat starts S1 at index
    zero, so no preceding tail invalidates the first-crossing rule.
    """
    systole = np.clip(eo.SYS_INT - hr * eo.SYS_SLOPE, eo.SYS_MIN, eo.SYS_MAX)
    region = beat_audio[:int(systole * SR)]
    env = shrlib.env_analytic(region, SR)
    pk = int(np.argmax(env))
    onset = 0
    if pk > 0 and env[pk] > 0:
        climb = np.maximum.accumulate(env[:pk + 1])
        above = np.flatnonzero(climb >= 0.10 * env[pk])
        onset = int(above[0]) if len(above) else 0
    return region[onset:onset + int(dur_ms * 1e-3 * SR)]


# Shared breath-ruler contract. Keep it separate from the onset-aligned `--dur-ms` S1 metrics.
BREATH_WIN_MS = shrlib.BREATH_WIN_MS
MIN_BREATH_CYCLES = shrlib.MIN_BREATH_CYCLES
BREATH_MEDIAN_TOL = shrlib.BREATH_MEDIAN_TOL
breath_groups = shrlib.breath_groups
breath_group_ratio = shrlib.breath_group_ratio
breath_swing_db = shrlib.breath_swing_db
breath_cycles = shrlib.breath_cycles
breath_group_median_targets = shrlib.breath_group_median_targets
breath_swing_problems = shrlib.breath_swing_problems


def _fixed_s1_window(beat_audio: np.ndarray) -> np.ndarray:
    """Fixed S1-start window, ending well before S2, matching the ref20 breath calibration."""
    return beat_audio[:int(BREATH_WIN_MS * 1e-3 * SR)]


# Executable composite-estimator contract; docs/MEASUREMENT_METHODS.md owns the shared rationale.
#
#   window     'onset+dur_ms'  S1-onset-aligned slice, width = the caller's dur_ms  (_s1_window)
#              'beat+fixed'    beat-start slice, width = BREATH_WIN_MS              (_fixed_s1_window)
#              'sequence'      a statistic over the per-beat series; no audio window of its own
#   width      whether the value moves with window width. Peak-derived values are invariant once the peak
#              is inside the window; RMS- and centroid-derived values are sensitive and require matched
#              widths across comparisons.
#   converges  the sampling process the value must be swept over before it is trusted; None = no
#              periodic sampling underneath, so a modest beat count is fine.
ESTIMATOR_METADATA = {
    "peak":                  {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "crest":                 {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "centroid":              {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "peak_cv":               {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "peak_cv_slow":          {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "peak_cv_fast":          {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "centroid_cv":           {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "peak_mean":             {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "crest_mean_db":         {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "aw_swing_db":           {"window": "beat+fixed",   "width": "sensitive", "converges": "breath-cycles"},
    "centroid_ratio":        {"window": "beat+fixed",   "width": "sensitive", "converges": "breath-cycles"},
    "inflation_exp_median":  {"window": "sequence",     "width": "invariant", "converges": "breath-cycles"},
    "inflation_insp_median": {"window": "sequence",     "width": "invariant", "converges": "breath-cycles"},
    "breath_cycles":         {"window": "sequence",     "width": "invariant", "converges": None},
    "breath_problems":       {"window": "sequence",     "width": "invariant", "converges": None},
}


def measure_sequence(seq: dict, dur_ms: float = 128.0, warmup: int = 4) -> dict:
    """Return per-beat S1 metrics and their coefficients of variation.

    Skips `warmup` beats so RSA and filling reach steady state. Peak CV supports
    within-signal spread analysis; cross-recording levels remain confounded.
    """
    # Breath metrics include warmup beats. A-weight the whole run once, then slice fixed windows; applying
    # the filter per short window circularly wraps its tail (shrlib.a_weight).
    weighted_run = shrlib.a_weight(np.concatenate([b["audio"] for b in seq["beats"]]), SR)
    starts = np.concatenate([[0], np.cumsum([len(b["audio"]) for b in seq["beats"][:-1]])])
    breath_win = int(BREATH_WIN_MS * 1e-3 * SR)

    peaks, crests, cents = [], [], []
    breath_cents, aw_rms, inflations = [], [], []
    for i, (b, start) in enumerate(zip(seq["beats"], starts)):
        s1 = _s1_window(b["audio"], seq["hr"], dur_ms)
        breath_cents.append(shrlib.centroid(_fixed_s1_window(b["audio"]), SR))
        aw_rms.append(shrlib.rms(weighted_run[start:start + breath_win]))
        inflations.append(b["lung_inflation"])
        if i >= warmup:
            peaks.append(shrlib.peak(s1))
            crests.append(shrlib.crest(s1))
            # Onset-aligned, like the ref8 brightness target this is printed against. The breath
            # ruler's beat-start window is a different anchor and belongs to `breath_cents` only.
            cents.append(shrlib.centroid(s1, SR))
    peaks, crests, cents = np.array(peaks), np.array(crests), np.array(cents)
    breath_cents, aw_rms, inflations = np.array(breath_cents), np.array(aw_rms), np.array(inflations)

    def cv(x: np.ndarray) -> float:
        return float(np.std(x) / np.mean(x)) if np.mean(x) else 0.0

    slow, fast = _slow_fast(peaks)
    expiration, inspiration = breath_groups(inflations)

    # Both cycle count and phase coverage are mandatory. A rigid phase lock cannot be repaired by more
    # beats; callers must reject every returned problem.
    problems = breath_swing_problems(inflations, seq["hr"], seq["resp_rate"], shrlib.SINE)
    return {"peak": peaks, "crest": crests, "centroid": cents,
            "peak_cv": cv(peaks), "peak_cv_slow": slow, "peak_cv_fast": fast,
            "centroid_cv": cv(cents),
            "peak_mean": float(peaks.mean()), "crest_mean_db": float(20 * np.log10(crests.mean())),
            "aw_swing_db": breath_swing_db(aw_rms, inflations),
            "centroid_ratio": breath_group_ratio(breath_cents, inflations),
            "inflation_exp_median": float(np.median(inflations[expiration])),
            "inflation_insp_median": float(np.median(inflations[inspiration])),
            "breath_cycles": breath_cycles(len(inflations), seq["hr"], seq["resp_rate"]),
            "breath_problems": problems}


def breath_period_beats(a: np.ndarray) -> int:
    """Return the dominant 2..len/3-beat period by autocorrelation."""
    a = np.asarray(a, float)
    s = a - a.mean()
    ac = np.correlate(s, s, "full")[len(s) - 1:]
    hi = max(3, len(a) // 3)
    return 2 + int(np.argmax(ac[2:hi])) if hi > 2 else 3


def _slow_fast(a: np.ndarray, period: int | None = None, n_harm: int = 2) -> tuple[float, float]:
    """Split per-beat CV into respiratory and residual components.

    Fits linear drift plus `n_harm` harmonics of the breath period. This avoids a
    successive-difference high-pass misreading short breath periods as fast
    variation. Returns (cv_slow, cv_fast).
    """
    a = np.asarray(a, float)
    n = len(a)
    if n < 6 or a.mean() == 0:
        return 0.0, 0.0
    p = period or breath_period_beats(a)
    t = np.arange(n)
    cols = [np.ones(n), t]                                      # DC + linear drift (excluded from slow)
    for h in range(1, n_harm + 1):
        cols += [np.cos(2 * np.pi * h * t / p), np.sin(2 * np.pi * h * t / p)]
    X = np.column_stack(cols)
    coef, *_ = np.linalg.lstsq(X, a, rcond=None)
    harm = X[:, 2:] @ coef[2:]                                  # breath component (zero-mean)
    resid = a - X @ coef
    mean = a.mean()
    return float(harm.std() / mean), float(resid.std() / mean)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source")
    ap.add_argument("--hr", type=float, default=180)
    ap.add_argument("--contractility", type=float, default=1.0)
    ap.add_argument("--exertion", type=float, default=1.0)
    ap.add_argument("--resp-rate", type=float, default=None,
                    help="override the steady-state RR target (breaths/min)")
    ap.add_argument("--breath-depth", type=float, default=None,
                    help="override normalized tidal depth [0,1]")
    ap.add_argument("--beats", type=int, default=24)
    ap.add_argument("--vigor-sigma", type=float, default=None, help="per-beat contractility jitter std "
                    "(scaled by contractility); default = VigorJitterScale from Constants.hpp; 0 = disable")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--dur-ms", type=float, default=128.0, help="S1 window (match the reference group)")
    ap.add_argument("--set", action="append", metavar="NAME=VALUE", default=[],
                    help="override a Constants.hpp value for this run; repeatable")
    ap.add_argument("--legacy-tail-mode", choices=eo.LEGACY_TAIL_MODES, default="off",
                    help="late-S1 audit control; fixed/dry-end re-enable retired tail variants")
    ap.add_argument("--legacy-tamer", action="store_true",
                    help="late-S1 audit control: re-enable the retired secondary-lobe tamer")
    ap.add_argument("--out", help="optional wav of the beat run")
    a = ap.parse_args()
    eo.apply_overrides(a.set)

    seq = beat_sequence(
        a.source, a.hr, a.contractility, a.exertion, a.beats, a.vigor_sigma, a.seed,
        resp_rate=a.resp_rate, breath_depth=a.breath_depth,
        synth_options={"tail_mode": a.legacy_tail_mode, "tamer_enabled": a.legacy_tamer}
    )
    m = measure_sequence(seq, a.dur_ms)
    beats_per_breath = a.hr / seq["resp_rate"]
    print(f"HR{a.hr:.0f} c{a.contractility:.2f} ex{a.exertion:.2f} "
          f"vigor_sigma{seq['vigor_sigma']:.2f} respRate{seq['resp_rate']:.1f} "
          f"depth{seq['breath_depth']:.2f} beats/breath{beats_per_breath:.2f} "
          f"tail={a.legacy_tail_mode} tamer={'on' if a.legacy_tamer else 'off'} "
          f"({a.beats} beats, {len(m['peak'])} measured)")
    # Print the live leaf and its uncertainty as context; data_citations.toml records why it is not a target.
    target = ref8_peak_cv_target()
    note = (f"   (ref8 peak {target[0]*100:.1f}% +/-{target[1]*100:.1f}% SE over 8 beats; NOT a calibration"
            f" target yet - see data_citations.toml)" if target else "")
    print(f"  S1 peak-amp: mean {m['peak_mean']:.3f}  CV {m['peak_cv']*100:.1f}%{note}")
    # No brightness-CV target: centroid CV is width-sensitive and needs an explicitly matched window.
    print(f"  S1 brightness (centroid): CV {m['centroid_cv']*100:.1f}%   (within-signal only - "
          f"width-sensitive, no valid cross-recording target)")
    print(f"  S1 crest: {m['crest_mean_db']:.1f} dB mean")
    targets = breath_group_median_targets(shrlib.SINE)  # the ENGINE's inflation curve
    status = "UNSOUND - do not calibrate on this" if m["breath_problems"] else "sound"
    print(f"  breath top/bottom 30%: A-weighted swing {m['aw_swing_db']:.2f} dB  "
          f"centroid ratio {m['centroid_ratio']:.3f}  [{status}]")
    print(f"    over {m['breath_cycles']:.1f} respiratory cycles; inflation-group medians "
          f"{m['inflation_exp_median']:.3f}/{m['inflation_insp_median']:.3f} "
          f"vs {targets[0]:.3f}/{targets[1]:.3f} under uniform phase coverage")
    for problem in m["breath_problems"]:
        print(f"  WARNING: {problem}", file=sys.stderr)
    print("  per-beat peak-amp: " + " ".join(f"{p:.3f}" for p in m["peak"]))
    if a.out:
        import soundfile as sf
        sf.write(a.out, np.clip(seq["audio"], -1, 1), SR, subtype="PCM_16")
        print(f"  wrote {a.out}  {len(seq['audio'])/SR:.2f}s")
