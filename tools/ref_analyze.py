#!/usr/bin/env python3
"""Measure annotated heart-sound reference recordings.

Single-file mode prints the full per-group metric battery. `--all` writes
docs/references/measurements.json, embedding the hand-authored reference-state
ledger beside each measurement scope. `--md-table` regenerates the corresponding
REFERENCE_ANALYSIS rows, and `--check` compares the committed JSON with a fresh
measurement plus ledger merge.

Per-group values are beat medians. Only timing, pitch, and within-recording
ratios transfer across recordings; absolute level and centroid are
capture-dependent.

Examples:
  # one recording, table for every annotated group:
  python tools/ref_analyze.py docs/references/original/8.wav docs/references/timestamps/8.txt
  # compare the engine against ref8's peak group (engine beats sliced by the systole law):
  python tools/ref_analyze.py docs/references/original/8.wav docs/references/timestamps/8.txt \
      --group peak --engine build/fresh/engine_177_c1.wav --hr 177 -c 1.0
  # audit truncation/padding sensitivity relative to each hand-annotated S1 duration:
  python tools/ref_analyze.py docs/references/original/14.wav docs/references/timestamps/14.txt \
      --hf-skew-window-sweep
  # regenerate the committed measurements file from every annotation under docs/references/timestamps/:
  python tools/ref_analyze.py --all
  # markdown rows for REFERENCE_ANALYSIS, or a drift check vs the committed JSON:
  python tools/ref_analyze.py --all --md-table
  python tools/ref_analyze.py --check
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

import shrlib
from reference_states import attach_reference_states, group_state_path
from shrlib import SR

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "docs" / "references" / "timestamps"
ORIG_DIR = ROOT / "docs" / "references" / "original"
DEFAULT_JSON = ROOT / "docs" / "references" / "measurements.json"
STATE_LEDGER = ROOT / "docs" / "references" / "state_ledger.toml"

# Physical asset 12 is a time-zeroed clip of recording 13. Keep its compact
# annotation/audio pair as an authoring convenience, but expose one logical
# reference in generated data so source/session counts cannot treat it as an
# independent recording. The offset was checked against the accompanying video.
DERIVED_CLIPS = {
    "ref12": {
        "parent": "ref13",
        "parent_start_s": 4 * 60 + 2.912,
        "alignment_tolerance_ms": 1.0,
    },
}

_C = shrlib.parse_constants(ROOT / "src" / "core" / "Constants.hpp")
SYS_INT, SYS_SLOPE = _C["SystoleIntercept"], _C["SystoleSlope"]
SYS_MIN, SYS_MAX = _C["SystoleMin"], _C["SystoleMax"]
S1_SYS_FRAC, S2_WIN_FRAC = _C["S1SystoleFraction"], _C["S2WindowFraction"]


def qs2_pred_ms(hr: float) -> float:
    """Return the unclamped Constants.hpp QS2 prediction in ms."""
    return (SYS_INT - hr * SYS_SLOPE) * 1e3


# Per-beat metrics aggregated by group median. Landmarks = (s1a, s1b, s2a, s2b).
def _systole_ms(s1, s2, dia, lm) -> float:
    return (lm[2] - lm[0]) * 1e3


_BEAT_METRICS = {
    "systole_ms": _systole_ms,
    "s1_dur_ms": lambda s1, s2, d, lm: (lm[1] - lm[0]) * 1e3,
    "s2_dur_ms": lambda s1, s2, d, lm: (lm[3] - lm[2]) * 1e3,
    "s1_attack_ms": lambda s1, s2, d, lm: shrlib.rise_10_90_ms(s1, SR),
    "s1_decay_ms": lambda s1, s2, d, lm: shrlib.decay_ms(s1, SR),
    "s2_attack_ms": lambda s1, s2, d, lm: shrlib.rise_10_90_ms(s2, SR),
    "s2_decay_ms": lambda s1, s2, d, lm: shrlib.decay_ms(s2, SR),
    "s1_f0_hz": lambda s1, s2, d, lm: shrlib.f0(s1, SR),
    "s2_f0_hz": lambda s1, s2, d, lm: shrlib.f0(s2, SR),
    "s1_centroid_hz": lambda s1, s2, d, lm: shrlib.centroid(s1, SR),
    "s2_centroid_hz": lambda s1, s2, d, lm: shrlib.centroid(s2, SR),
    "s2s1_centroid_ratio": lambda s1, s2, d, lm: shrlib.centroid(s2, SR) / shrlib.centroid(s1, SR),
    "s1_rolloff85_hz": lambda s1, s2, d, lm: shrlib.rolloff(s1, SR),
    "s1_spread_hz": lambda s1, s2, d, lm: shrlib.spread(s1, SR),
    "s1_crest": lambda s1, s2, d, lm: shrlib.crest(s1),
    "s2s1_loud": lambda s1, s2, d, lm: shrlib.rms(s2) / shrlib.rms(s1) if shrlib.rms(s1) else np.nan,
    "s2s1_loud_db": lambda s1, s2, d, lm: shrlib.db(shrlib.rms(s2) / (shrlib.rms(s1) + 1e-9)),
    "s2s1_peak_db": lambda s1, s2, d, lm: shrlib.db(shrlib.peak(s2) / (shrlib.peak(s1) + 1e-9)),
    "dia_s1_db": lambda s1, s2, d, lm: shrlib.db(shrlib.rms(d) / (shrlib.rms(s1) + 1e-9)),
    "s1_e0_20ms": lambda s1, s2, d, lm: shrlib.energy_conc(s1, SR)[0],
    "s1_e20_40ms": lambda s1, s2, d, lm: shrlib.energy_conc(s1, SR)[1],
    "s1_e40_80ms": lambda s1, s2, d, lm: shrlib.energy_conc(s1, SR)[2],
}

_BAND_LABELS = ["20_40", "40_80", "80_150", "150_300", "300_1200"]
_HF_SKEW_AUDIT_FRACTIONS = (0.60, 0.70, 0.80, 0.90, 1.00, 1.10, 1.25)


def measure_group(signal: np.ndarray, beats: list) -> dict:
    """Return the median metric battery for one annotation group.

    The rise/body HF-contrast metrics need the whole-file band signal (filtered once, so no filter
    edge transient lands in the onset) plus each beat's landmarks, so they are accumulated here
    rather than in _BEAT_METRICS, whose callables see only the sliced segments.
    """
    segs = shrlib.beats_to_segments(signal, SR, beats)
    hf = shrlib.hf_band(signal, SR)
    acc: dict[str, list] = {k: [] for k in _BEAT_METRICS}
    con: dict[str, list] = {
        k: []
        for k in (
            "s1_contrast",
            "s1_hf_dens_rise",
            "s1_hf_dens_body",
            "s1_hf_skew",
            "s1_lobes",
            "s1_lobe_runnerup",
        )
    }
    s1b = {lab: [] for lab in _BAND_LABELS}
    s2b = {lab: [] for lab in _BAND_LABELS}
    for (s1, s2, dia), lm in zip(segs, beats):
        for k, fn in _BEAT_METRICS.items():
            acc[k].append(fn(s1, s2, dia, lm))
        c, dr, db_ = shrlib.rise_body_contrast(s1, hf, SR, t0=lm[0], s2_onset=lm[2])
        con["s1_contrast"].append(c)
        con["s1_hf_dens_rise"].append(dr)
        con["s1_hf_dens_body"].append(db_)
        i0 = int(round(lm[0] * SR))
        con["s1_hf_skew"].append(shrlib.hf_temporal_skew(s1, hf[i0 : i0 + len(s1)]))
        n_lobes, _, runner = shrlib.lobe_count(s1, SR)
        con["s1_lobes"].append(n_lobes)
        con["s1_lobe_runnerup"].append(runner)
        for lab, v in zip(_BAND_LABELS, shrlib.bands(s1, SR)):
            s1b[lab].append(v)
        for lab, v in zip(_BAND_LABELS, shrlib.bands(s2, SR)):
            s2b[lab].append(v)
    out = {"n_beats": len(beats), "hr": shrlib.beats_hr(beats)}
    out["qs2_pred_ms"] = qs2_pred_ms(out["hr"])
    for k, vals in acc.items():
        out[k] = float(np.nanmedian(vals))
    out["systole_residual_ms"] = out["systole_ms"] - out["qs2_pred_ms"]
    for k, vals in con.items():
        out[k] = float(np.nanmedian(vals))
    # Contrast spread is large enough that the metric is directional, not a setpoint.
    # Store the range with the median.
    cv = [v for v in con["s1_contrast"] if v == v]
    out["s1_contrast_min"] = float(np.min(cv)) if cv else np.nan
    out["s1_contrast_max"] = float(np.max(cv)) if cv else np.nan
    out["s1_contrast_n"] = len(cv)
    out["s1_bands"] = {lab: float(np.nanmedian(s1b[lab])) for lab in _BAND_LABELS}
    out["s2_bands"] = {lab: float(np.nanmedian(s2b[lab])) for lab in _BAND_LABELS}

    # Emit beat-to-beat spread and uncertainty as leaves. Peak CV is window-width-invariant once the peak
    # is inside the window, but cross-recording level spread remains AGC/state-confounded. Centroid CV is
    # width-sensitive and requires matched windows; see rhythm_offline.ESTIMATOR_METADATA.
    peaks = np.array([shrlib.peak(s1) for s1, _, _ in segs])
    cents = np.array([shrlib.centroid(s1, SR) for s1, _, _ in segs])

    def cv(a: np.ndarray) -> float:
        return float(a.std() / a.mean()) if len(a) and a.mean() else float("nan")

    out["s1_peak_cv"] = cv(peaks)
    out["s1_centroid_cv"] = cv(cents)
    if len(peaks) >= 4:  # leave-one-beat-out SE, so the CI is visible
        drops = np.array([cv(np.delete(peaks, k)) for k in range(len(peaks))])
        n = len(drops)
        out["s1_peak_cv_se"] = float(np.sqrt((n - 1) / n * ((drops - drops.mean()) ** 2).sum()))
    return out


def _hf_skew_window_sweep(
    signal: np.ndarray,
    hf_signal: np.ndarray,
    beats: list,
    fractions: tuple[float, ...] = _HF_SKEW_AUDIT_FRACTIONS,
) -> dict[float, float]:
    """Return group-median HF skew across fractions of each beat's annotated S1 duration.

    This is an audit helper, not a family of alternative rulers. Fraction 1.0 is the complete-S1
    measurement. Shorter fractions demonstrate truncation sensitivity; longer fractions demonstrate
    padding/context sensitivity and are capped before S2.
    """
    out = {}
    for fraction in fractions:
        values = []
        for s1a, s1b, s2a, _ in beats:
            i0 = int(round(s1a * SR))
            annotated_frames = int(round((s1b - s1a) * SR))
            before_s2_frames = int(round((s2a - s1a) * SR))
            frames = min(int(round(fraction * annotated_frames)), before_s2_frames)
            if frames <= 0:
                continue
            values.append(shrlib.hf_temporal_skew(signal[i0 : i0 + frames], hf_signal[i0 : i0 + frames]))
        out[fraction] = float(np.nanmedian(values)) if values else np.nan
    return out


def print_hf_skew_window_sweep(signal: np.ndarray, groups: dict[str, list]) -> None:
    """Print the reproducible complete-S1 window audit for annotated groups."""
    hf_signal = shrlib.hf_band(signal, SR)
    fractions = _HF_SKEW_AUDIT_FRACTIONS
    print("HF lead/lag window audit")
    print("  fractions are W / annotated S1 duration; 1.00 is the valid complete-S1 ruler")
    print("  values below/above 1.00 expose truncation/padding sensitivity only")
    print("  fraction " + " ".join(f"{fraction:>7.2f}" for fraction in fractions))
    for label, beats in groups.items():
        measured = _hf_skew_window_sweep(signal, hf_signal, beats, fractions)
        print(f"  {label} (n={len(beats)})")
        print("           " + " ".join(f"{measured[fraction]:+7.3f}" for fraction in fractions))


def engine_beats(signal: np.ndarray, hr: float, n: int = 6, skip: int = 2) -> list:
    """Slice an engine render into (s1, s2, dia) beats via the systole/window laws (no annotation).

    Engine beats are deterministic, so we synthesize landmarks from the same laws the engine uses:
    S1 = [t, t + S1SystoleFraction*systole], S2 = [t+systole, t+systole + S2WindowFraction*window].
    """
    ibi = 60.0 / hr
    systole = float(np.clip(SYS_INT - hr * SYS_SLOPE, SYS_MIN, SYS_MAX))
    beats = []
    for k in range(skip, skip + n):
        t = k * ibi
        s1_end = t + S1_SYS_FRAC * systole
        s2_start = t + systole
        s2_end = s2_start + S2_WIN_FRAC * (ibi - systole)
        beats.append((t, s1_end, s2_start, s2_end))
    return beats


def ref_id(annot_path: str | Path) -> str:
    """docs/references/timestamps/8.txt -> 'ref8'."""
    return "ref" + Path(annot_path).stem


def measure_breath(signal: np.ndarray, annot: Path, groups: dict) -> dict | None:
    """Return breath-modulation leaves using the shared shrlib ruler.

    A-weights the whole signal before slicing fixed S1-onset windows. Per-window
    filtering would circularly convolve and bias recordings with LF drift.

    Prefers `BR=` landmarks for continuous inflation, top/bottom-30% grouping,
    respiratory rate, and inspiration fraction. Falls back to coarse `vent:`
    phase tags.

    Returns None without phase data; annotation groups do not imply representative
    breath-phase coverage.
    """
    onsets = np.array([beat[0] for beats in groups.values() for beat in beats])
    onsets = onsets[~shrlib.excluded(onsets, shrlib.parse_exclusions(annot, scope="breath"))]
    if len(onsets) == 0:
        return None
    weighted = shrlib.a_weight(signal, SR)
    width = int(shrlib.BREATH_WIN_MS * 1e-3 * SR)
    levels = np.array([shrlib.rms(weighted[int(t * SR) : int(t * SR) + width]) for t in onsets])
    centroids = np.array([shrlib.centroid(signal[int(t * SR) : int(t * SR) + width], SR) for t in onsets])

    breaths = shrlib.parse_breaths(annot)
    if breaths:
        inflation = shrlib.breath_inflation(onsets, breaths)
        covered = ~np.isnan(inflation)
        if covered.sum() < 4:
            return None
        periods = np.diff([trough for trough, _ in breaths])
        inspirations = np.array([peak - trough for trough, peak in breaths[:-1]])
        result = {
            "method": "landmarks",
            "n_beats": int(covered.sum()),
            "n_cycles": len(breaths),
            "aw_swing_db": shrlib.breath_swing_db(levels[covered], inflation[covered]),
            "centroid_ratio": shrlib.breath_group_ratio(centroids[covered], inflation[covered]),
            "resp_rate_bpm": float(60.0 / periods.mean()) if len(periods) else float("nan"),
            "inspiration_fraction": float((inspirations / periods).mean()) if len(periods) else float("nan"),
        }
        # Record leave-one-breath-out uncertainty and coverage against the
        # reference's raised-cosine inflation curve, not the engine's sine curve.
        full = result["aw_swing_db"]
        drops = []
        for k in range(len(breaths)):
            sub = breaths[:k] + breaths[k + 1 :]
            f = shrlib.breath_inflation(onsets, sub)
            m = ~np.isnan(f)
            if m.sum() >= 4:
                drops.append(shrlib.breath_swing_db(levels[m], f[m]))
        if len(drops) >= 3:
            drops = np.array(drops)
            n = len(drops)
            result["jackknife_se_db"] = float(np.sqrt((n - 1) / n * ((drops - drops.mean()) ** 2).sum()))
        expiration, inspiration = shrlib.breath_groups(inflation[covered])
        realized = (
            float(np.median(inflation[covered][expiration])),
            float(np.median(inflation[covered][inspiration])),
        )
        targets = shrlib.breath_group_median_targets(shrlib.RAISED_COSINE)
        result["coverage_deviation"] = float(max(abs(r - t) for r, t in zip(realized, targets)))
        result["covered"] = result["coverage_deviation"] <= shrlib.BREATH_MEDIAN_TOL
        tags = shrlib.parse_vent_tags(annot)
        if tags:
            index = {round(t, 3): i for i, t in enumerate(onsets)}
            ins = [index[round(t, 3)] for t, p in tags if p == "inspiration" and round(t, 3) in index]
            exp = [index[round(t, 3)] for t, p in tags if p == "expiration" and round(t, 3) in index]
            if ins and exp:
                result["aw_swing_db_vent_tags"] = float(
                    20.0 * np.log10(np.median(levels[ins]) / np.median(levels[exp]))
                )
        return result

    tags = shrlib.parse_vent_tags(annot)
    if len(tags) < 4:
        return None
    index = {round(t, 3): i for i, t in enumerate(onsets)}
    inspiration = [index[round(t, 3)] for t, phase in tags if phase == "inspiration" and round(t, 3) in index]
    expiration = [index[round(t, 3)] for t, phase in tags if phase == "expiration" and round(t, 3) in index]
    if not inspiration or not expiration:
        return None
    return {
        "method": "vent-tags",
        "n_beats": len(inspiration) + len(expiration),
        "aw_swing_db": float(20.0 * np.log10(np.median(levels[inspiration]) / np.median(levels[expiration]))),
        "centroid_ratio": float(np.median(centroids[inspiration]) / np.median(centroids[expiration])),
    }


SYSTOLE_LAW_KEY = "systole_law"

# Exclude non-steady, non-free-breathing groups explicitly. ref11 contains deep
# inspiration and breath-hold manipulations. ref14 is a post-hold release transient
# confounded by excitation; PEP/Valsalva-release shortening is outside this law.
SYSTOLE_FIT_EXCLUDE = {
    ("ref11", "deeper inspiration (~79)"),
    ("ref11", "breath hold peak inspiration (~71)"),
    ("ref14", "high, post-hold release (~158)"),
}


def fit_systole_law(data: dict) -> dict:
    """Fit `systole = intercept - slope*HR` without reading Constants.hpp.

    This leaf calibrates the systole constants. Reading those constants here would
    make both sides of the calibration move together. Per-group `qs2_pred_ms` and
    `systole_residual_ms` do read them, so they are display values rather than
    calibration targets.

    Emitting the fit as a leaf keeps the target reproducible from committed groups.
    """
    rows = [
        (rid, label, g["hr"], g["systole_ms"])
        for rid, ref in data.items()
        if isinstance(ref, dict) and "groups" in ref
        for label, g in ref["groups"].items()
        if (rid, label) not in SYSTOLE_FIT_EXCLUDE
        and g.get("hr") == g.get("hr")
        and g.get("systole_ms") == g.get("systole_ms")
    ]
    if len(rows) < 3:
        return {}
    hr = np.array([r[2] for r in rows])
    systole = np.array([r[3] for r in rows])
    n = len(rows)

    design = np.vstack([np.ones(n), -hr]).T
    (intercept, slope), *_ = np.linalg.lstsq(design, systole, rcond=None)
    residual = systole - (intercept - slope * hr)
    resid_std = float(residual.std(ddof=2))
    cov = (float(residual @ residual) / (n - 2)) * np.linalg.inv(design.T @ design)
    se_intercept, se_slope = (float(v) for v in np.sqrt(np.diag(cov)))

    # Bind at mean HR, where the line is better determined than its extrapolated
    # HR=0 intercept. The prediction SE there is resid_std/sqrt(n).
    mean_hr = float(hr.mean())
    return {
        "fit_intercept_ms": float(intercept),
        "fit_slope_ms_per_bpm": float(slope),
        "fit_intercept_se_ms": se_intercept,
        "fit_slope_se_ms_per_bpm": se_slope,
        "residual_std_ms": resid_std,
        "mean_hr": mean_hr,
        "pred_at_mean_hr_ms": float(intercept - slope * mean_hr),
        "pred_se_at_mean_hr_ms": resid_std / np.sqrt(n),
        "n_groups": n,
        "hr_min": float(hr.min()),
        "hr_max": float(hr.max()),
        "state": {
            "kind": "mixed_reference_states",
            "members": [group_state_path(rid, label) for rid, label, _, _ in rows],
        },
    }


def merge_derived_clips(data: dict) -> None:
    """Merge derived asset measurements into their logical parent references.

    Clip-local timebases and annotations remain visible as provenance. Breath
    summaries are keyed by group because the exercise and recovery windows are
    separate physiological scopes within the parent recording.
    """
    for clip_id, lineage in DERIVED_CLIPS.items():
        if clip_id not in data:
            continue
        parent_id = lineage["parent"]
        if parent_id not in data:
            raise ValueError(f"{clip_id}: derived-clip parent {parent_id!r} was not measured")
        clip = data.pop(clip_id)
        parent = data[parent_id]
        overlaps = set(clip.get("groups", {})) & set(parent.get("groups", {}))
        if overlaps:
            raise ValueError(f"{clip_id}: group name collision in {parent_id}: {sorted(overlaps)}")

        clip_meta = {
            "source": clip["source"],
            "annotations": clip["annotations"],
            "parent_start_s": lineage["parent_start_s"],
            "alignment_tolerance_ms": lineage["alignment_tolerance_ms"],
        }
        parent.setdefault("derived_clips", {})[clip_id] = clip_meta
        for label, group in clip.get("groups", {}).items():
            group["provenance"] = {"derived_clip": clip_id, **clip_meta}
            parent["groups"][label] = group

        breath_scopes = parent.setdefault("breath_scopes", {})
        parent_breath = parent.pop("breath", None)
        if parent_breath is not None:
            if len(parent["groups"]) - len(clip.get("groups", {})) != 1:
                raise ValueError(f"{parent_id}: parent breath summary needs an unambiguous group")
            parent_group = next(label for label in parent["groups"] if label not in clip.get("groups", {}))
            breath_scopes[parent_group] = parent_breath
        clip_breath = clip.get("breath")
        if clip_breath is not None:
            if len(clip.get("groups", {})) != 1:
                raise ValueError(f"{clip_id}: clip breath summary needs an unambiguous group")
            clip_group = next(iter(clip["groups"]))
            breath_scopes[clip_group] = clip_breath


def measure_all() -> dict:
    """Measure every timestamp file, then merge clips into logical references."""
    data = {}
    for annot in sorted(TS_DIR.glob("*.txt"), key=lambda p: int(p.stem) if p.stem.isdigit() else p.stem):
        wav = ORIG_DIR / f"{annot.stem}.wav"
        if not wav.exists():
            print(f"  warn: no recording for {annot.name} ({wav.name} missing), skipping")
            continue
        signal, sr = shrlib.load(wav)
        if sr != SR:
            print(f"  warn: {wav.name} is {sr}Hz, not {SR}, skipping")
            continue
        groups = shrlib.parse_annotations(annot)
        entry = {
            "source": str(wav.relative_to(ROOT).as_posix()),
            "annotations": str(annot.relative_to(ROOT).as_posix()),
            "groups": {label: measure_group(signal, beats) for label, beats in groups.items()},
        }
        breath = measure_breath(signal, annot, groups)
        if breath is not None:
            entry["breath"] = breath
        data[ref_id(annot)] = entry
    merge_derived_clips(data)
    law = fit_systole_law(data)
    if law:
        data[SYSTOLE_LAW_KEY] = law
    attach_reference_states(data, STATE_LEDGER)
    return data


# Columns mirrored into REFERENCE_ANALYSIS.md's "Annotated S1/S2 measurements" table.
_TABLE_COLS = [
    ("HR", "hr", "{:.0f}"),
    ("systole", "systole_ms", "{:.0f}"),
    ("(QS2 pred)", "qs2_pred_ms", "({:.0f})"),
    ("S1 dur", "s1_dur_ms", "{:.0f}"),
    ("S2 dur", "s2_dur_ms", "{:.0f}"),
    ("S2/S1 loud", "s2s1_loud", "{:.2f}"),
    ("S1 cen", "s1_centroid_hz", "{:.0f}"),
    ("S2 cen", "s2_centroid_hz", "{:.0f}"),
    ("S2/S1 cen", "s2s1_centroid_ratio", "{:.2f}x"),
]


def print_md_table(data: dict) -> None:
    """Print the REFERENCE_ANALYSIS annotated-measurements table rows for every group."""
    print("| group | " + " | ".join(c[0] for c in _TABLE_COLS) + " |")
    print("|---|" + "|".join("---" for _ in _TABLE_COLS) + "|")
    for rid, ref in data.items():
        if rid == SYSTOLE_LAW_KEY:  # a fit ACROSS the groups, not a recording
            continue
        for label, m in ref["groups"].items():
            cells = [fmt.format(m[key]) for _, key, fmt in _TABLE_COLS]
            print(f"| {rid} {label} | " + " | ".join(cells) + " |")


_TABLE_ROWS = [
    ("-- TIMING / ENVELOPE [C comparable] --", None, None),
    ("S1 rise 10-90 ms", "s1_attack_ms", "{:7.1f}"),
    ("S1 decay(pk->10%) ms", "s1_decay_ms", "{:7.1f}"),
    ("S2 rise 10-90 ms", "s2_attack_ms", "{:7.1f}"),
    ("S2 decay ms", "s2_decay_ms", "{:7.1f}"),
    ("S1 dur ms", "s1_dur_ms", "{:7.1f}"),
    ("S2 dur ms", "s2_dur_ms", "{:7.1f}"),
    ("systole ms", "systole_ms", "{:7.1f}"),
    ("  QS2 pred ms", "qs2_pred_ms", "{:7.1f}"),
    ("S1 crest (pk/rms)", "s1_crest", "{:7.2f}"),
    ("-- SNAP [C] --", None, None),
    ("S1 HF lead/lag [C, full-S1]", "s1_hf_skew", "{:+7.4f}"),
    ("S1 lobes (median)", "s1_lobes", "{:7.1f}"),
    ("  runner-up/peak height", "s1_lobe_runnerup", "{:7.2f}"),
    ("-- rise/body contrast [same-lobe-structure only] --", None, None),
    ("S1 contrast (rise/body)", "s1_contrast", "{:7.2f}"),
    ("  spread min", "s1_contrast_min", "{:7.2f}"),
    ("  spread max", "s1_contrast_max", "{:7.2f}"),
    ("S1 HF density rise [C]", "s1_hf_dens_rise", "{:7.2e}"),
    ("S1 HF density body [C]", "s1_hf_dens_body", "{:7.2e}"),
    ("-- PITCH (F0) [C comparable] --", None, None),
    ("S1 F0 Hz", "s1_f0_hz", "{:7.1f}"),
    ("S2 F0 Hz", "s2_f0_hz", "{:7.1f}"),
    ("-- WITHIN-RECORDING RATIOS [C comparable] --", None, None),
    ("S2/S1 loud (rms)", "s2s1_loud", "{:7.2f}"),
    ("S2/S1 peak dB", "s2s1_peak_db", "{:+7.1f}"),
    ("diastole/S1 rms dB", "dia_s1_db", "{:+7.1f}"),
    ("-- SPECTRAL SHAPE [~ shape ok, abs mic-dependent] --", None, None),
    ("S1 centroid Hz [X]", "s1_centroid_hz", "{:7.1f}"),
    ("S2 centroid Hz [X]", "s2_centroid_hz", "{:7.1f}"),
    ("S2/S1 centroid ratio", "s2s1_centroid_ratio", "{:7.2f}"),
    ("S1 rolloff85 Hz [X]", "s1_rolloff85_hz", "{:7.1f}"),
    ("S1 spread Hz [X]", "s1_spread_hz", "{:7.1f}"),
    ("S1 energy 0-20ms %", "s1_e0_20ms", "{:7.1%}"),
    ("S1 energy 20-40ms %", "s1_e20_40ms", "{:7.1%}"),
    ("S1 energy 40-80ms %", "s1_e40_80ms", "{:7.1%}"),
]


def print_table(columns: list) -> None:
    """Print (label, metrics) columns side by side."""
    heads = "".join(f"{lab[:16]:>17}" for lab, _ in columns)
    print(f"\n{'metric':<40}{heads}")
    for label, key, fmt in _TABLE_ROWS:
        if key is None:
            print(f"\n{label}")
            continue
        cells = "".join(f"{fmt.format(m.get(key, float('nan'))):>17}" for _, m in columns)
        print(f"  {label:<38}{cells}")
    print("\n  S1 band fractions (within-S1 shape) [~]")
    for lab in _BAND_LABELS:
        cells = "".join(f"{m['s1_bands'][lab]:>16.1%} " for _, m in columns)
        print(f"  {'S1 ' + lab + 'Hz':<38}{cells}")
    print("  S2 band fractions (within-S2 shape) [~]")
    for lab in _BAND_LABELS:
        cells = "".join(f"{m['s2_bands'][lab]:>16.1%} " for _, m in columns)
        print(f"  {'S2 ' + lab + 'Hz':<38}{cells}")


def _flat(d: dict, prefix: str = "") -> dict:
    """Flatten a group's metric dict (including nested band dicts) to {path: value}."""
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(_flat(v, f"{prefix}{k}/"))
        else:
            out[prefix + k] = v
    return out


def check(json_path: Path) -> int:
    """Recompute from annotations and diff against the committed JSON. Returns problem count.

    The fresh values are rounded to the JSON's stored precision before comparing, so this
    flags real drift (edited annotations, changed Constants.hpp) but not float-format noise.
    """
    if not json_path.exists():
        print(f"no measurements file at {json_path}; run --all first")
        return 1
    committed = json.loads(json_path.read_text(encoding="utf-8"))
    fresh = _round(measure_all())
    problems = 0
    for rid, ref in fresh.items():
        if rid not in committed:
            print(f"  {rid}: present in recompute, absent from JSON")
            problems += 1
            continue
        if rid == SYSTOLE_LAW_KEY:  # a fit ACROSS the groups, not a recording
            for key, val in _flat(ref).items():
                ov = _flat(committed[rid]).get(key)
                if not _close(val, ov):
                    print(f"  {rid}/{key}: JSON {ov} vs recompute {val}")
                    problems += 1
            continue
        for label, m in ref["groups"].items():
            old = committed[rid]["groups"].get(label)
            if old is None:
                print(f"  {rid}/{label}: new group not in JSON")
                problems += 1
                continue
            flat_old, flat_new = _flat(old), _flat(m)
            for key, val in flat_new.items():
                ov = flat_old.get(key)
                if not _close(val, ov):
                    print(f"  {rid}/{label}/{key}: JSON {ov} vs recompute {val}")
                    problems += 1
        # Breath leaves sit outside "groups" but are calibration inputs, so they require the same drift check.
        flat_old, flat_new = _flat(committed[rid].get("breath") or {}), _flat(ref.get("breath") or {})
        for key, val in flat_new.items():
            ov = flat_old.get(key)
            if not _close(val, ov):
                print(f"  {rid}/breath/{key}: JSON {ov} vs recompute {val}")
                problems += 1
    if problems == 0:
        print(f"measurements: {json_path.name} matches a fresh recompute")
    else:
        print(f"\n{problems} drift(s); regenerate with: python tools/ref_analyze.py --all")
    return problems


def _close(a, b) -> bool:
    """Equal at the JSON's stored precision (both already rounded); NaN == NaN."""
    if isinstance(a, float) and isinstance(b, float) and a != a and b != b:
        return True
    return a == b


def _round(obj, nd: int = 3):
    """Round for stable JSON diffs. Values below the decimal precision (the HF densities are ~1e-4)
    keep 4 significant figures instead, so they do not collapse to 0.0."""
    if isinstance(obj, float):
        if obj != obj or obj in (float("inf"), float("-inf")):
            return obj
        return round(obj, nd) if abs(obj) >= 10**-nd else float(f"{obj:.4g}")
    if isinstance(obj, dict):
        return {k: _round(v, nd) for k, v in obj.items()}
    return obj


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ref", nargs="?", help="reference recording wav")
    ap.add_argument("annot", nargs="?", help="S1/S2 landmark file (docs/references/timestamps/N.txt)")
    ap.add_argument("--group", help="restrict to one annotation group label (single-file mode)")
    ap.add_argument("--engine", help="engine render wav to compare (single-file mode)")
    ap.add_argument("--hr", type=float, help="engine render HR (with --engine)")
    ap.add_argument("-c", "--contractility", type=float, default=1.0, help="label only (with --engine)")
    ap.add_argument("--all", action="store_true", help="measure every annotation -> measurements JSON")
    ap.add_argument(
        "--json", type=Path, default=DEFAULT_JSON, help=f"measurements file (default {DEFAULT_JSON.name})"
    )
    ap.add_argument("--md-table", action="store_true", help="print the REFERENCE_ANALYSIS markdown rows")
    ap.add_argument("--check", action="store_true", help="diff a fresh recompute against the committed JSON")
    ap.add_argument(
        "--hf-skew-window-sweep",
        action="store_true",
        help="audit HF lead/lag over fractions of each annotated S1 duration",
    )
    a = ap.parse_args()

    if a.check:
        return check(a.json)

    if a.all:
        data = measure_all()
        if a.md_table:
            print_md_table(data)
        else:
            a.json.write_text(json.dumps(_round(data), indent=2) + "\n", encoding="utf-8")
            refs = {k: v for k, v in data.items() if k != SYSTOLE_LAW_KEY}
            n = sum(len(r["groups"]) for r in refs.values())
            law = data.get(SYSTOLE_LAW_KEY, {})
            print(f"wrote {a.json.relative_to(ROOT).as_posix()}  ({len(refs)} recordings, {n} groups)")
            if law:
                print(
                    f"  systole law fit: {law['fit_intercept_ms']:.1f} - "
                    f"{law['fit_slope_ms_per_bpm']:.3f}*HR ms  "
                    f"(SE {law['fit_intercept_se_ms']:.1f} / {law['fit_slope_se_ms_per_bpm']:.3f}, "
                    f"residual std {law['residual_std_ms']:.1f} ms, {law['n_groups']} groups)"
                )
        return 0

    if not (a.ref and a.annot):
        ap.error("provide REF and ANNOT, or use --all / --check")

    signal, sr = shrlib.load(a.ref)
    assert sr == SR, sr
    groups = shrlib.parse_annotations(a.annot)
    if a.group:
        groups = {a.group: groups[a.group]}
    if a.hf_skew_window_sweep:
        if a.engine:
            ap.error("--hf-skew-window-sweep does not accept --engine")
        print_hf_skew_window_sweep(signal, groups)
        return 0
    columns = [
        (f"{ref_id(a.annot)} {label}", measure_group(signal, beats)) for label, beats in groups.items()
    ]

    if a.engine:
        if a.hr is None:
            ap.error("--engine requires --hr")
        esig, esr = shrlib.load(a.engine)
        assert esr == SR, esr
        columns.append(
            (f"eng hr{a.hr:.0f} c{a.contractility:.2f}", measure_group(esig, engine_beats(esig, a.hr)))
        )

    if a.md_table:
        print_md_table(
            {
                ref_id(a.annot): {
                    "groups": {lab: m for lab, m in ((c[0].split(" ", 1)[-1], c[1]) for c in columns)}
                }
            }
        )
    else:
        print_table(columns)
    return 0


if __name__ == "__main__":
    sys.exit(main())
