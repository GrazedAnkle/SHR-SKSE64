"""Compiled-core steady-state rhythm client for beat-to-beat analysis.

The tool supplies one fixed operating point to the bound ``RhythmEngine``, maps each fired event through
``create_render_spec``, and renders native-layout audio through ``shr_core``. Python owns only scenario
construction and the independent measurement rulers below.

Retired late-S1 tail/tamer controls remain explicit counterfactuals: they alter the compiled renderer's
post-onset source stage, then return it to C++ for transmission, mixing, and limiting.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

import core_offline
import legacy_s1
import shrlib
from shrlib import SR, analysis_channel


RHYTHM_FPS = 240.0


def ref8_peak_cv_target() -> tuple[float, float] | None:
    """Return ref8's peak-group S1 peak CV and jackknife SE."""
    path = Path(__file__).resolve().parents[1] / "docs" / "references" / "measurements.json"
    try:
        import json

        group = json.loads(path.read_text(encoding="utf-8"))["ref8"]["groups"]["peak (~180)"]
        return float(group["s1_peak_cv"]), float(group["s1_peak_cv_se"])
    except (OSError, KeyError, ValueError):
        return None


def _counterfactual_render(
    module,
    source,
    sample_rate: int,
    render,
    vigor: float,
    coefficients,
    *,
    tail_mode: str,
    tamer_enabled: bool,
) -> np.ndarray:
    trace = core_offline.trace_beat(module, source, render, coefficients)
    source_s1 = trace["source_s1"]
    if tamer_enabled:
        source_s1 = legacy_s1.tame_lobe(source_s1, min(vigor, 1.0))
    source_s1 = legacy_s1.apply_tail(
        source_s1,
        tail_mode,
        baseline_dry_frames=source.s1_frames,
    )
    return core_offline.render_from_source_stages(
        module,
        source_s1,
        trace["source_s2"],
        sample_rate,
        render,
        coefficients,
    )


def beat_sequence(
    path: str | Path,
    hr: float,
    contractility: float,
    exertion: float,
    n_beats: int = 24,
    vigor_sigma: float | None = None,
    seed: int = 0,
    src_highpass: float | None = None,
    resp_rate: float | None = None,
    breath_depth: float | None = None,
    synth_options: dict | None = None,
    *,
    module=None,
    coefficients=None,
    module_dir: str | Path = core_offline.DEFAULT_MODULE_DIR,
    render_resp_phase: float | None = None,
) -> dict:
    """Render a seeded steady-state sinus run through the compiled core."""
    if hr <= 0.0:
        raise ValueError("hr must be positive")
    if n_beats <= 0:
        raise ValueError("n_beats must be positive")
    if not 0.0 <= exertion <= 1.0:
        raise ValueError("exertion must be in [0, 1]")

    module = module or core_offline.load_binding(module_dir)
    coefficients = coefficients or module.default_model_coefficients
    direct_overrides: dict[str, float] = {}
    if vigor_sigma is not None:
        direct_overrides["VigorJitterScale"] = vigor_sigma
    if src_highpass is not None:
        direct_overrides["SourceHighPassHz"] = src_highpass
    if direct_overrides:
        coefficients = coefficients.with_overrides(direct_overrides)

    values = dict(coefficients.values)
    vigor_sigma = float(values["VigorJitterScale"])
    target_rate, target_depth = module.ventilation_targets(exertion, coefficients)
    resp_rate = float(target_rate if resp_rate is None else resp_rate)
    breath_depth = float(target_depth if breath_depth is None else breath_depth)
    if resp_rate <= 0.0:
        raise ValueError("resp_rate must be positive")

    options = dict(synth_options or {})
    unknown_options = set(options) - {"tail_mode", "tamer_enabled"}
    if unknown_options:
        raise ValueError(f"unknown counterfactual options: {sorted(unknown_options)}")
    tail_mode = options.get("tail_mode", "off")
    tamer_enabled = bool(options.get("tamer_enabled", False))
    if tail_mode not in legacy_s1.LEGACY_TAIL_MODES:
        raise ValueError(f"unknown tail mode {tail_mode!r}; expected one of {legacy_s1.LEGACY_TAIL_MODES}")

    source, sample_rate = core_offline.prepare_source(module, path, coefficients)
    if sample_rate != SR:
        raise ValueError(f"analysis client requires {SR} Hz source audio, got {sample_rate}")

    engine = module.RhythmEngine(seed=seed, coefficients=coefficients)
    engine.init()
    delta = 1.0 / RHYTHM_FPS
    elapsed = 0.0
    beats: list[dict] = []
    max_steps = int(np.ceil(n_beats * max(60.0 / hr, 0.1) / delta * 4.0)) + 1
    for _ in range(max_steps):
        rhythm_phase = (elapsed * resp_rate / 60.0) % 1.0
        event = engine.advance(
            delta_seconds=delta,
            heart_rate=hr,
            respiration_phase=rhythm_phase,
            exertion_fraction=exertion,
            contractility=contractility,
            pvc_chance_per_second=0.0,
            risk_factor=0.0,
            run_extension_chance=0.0,
        )
        elapsed += delta
        if event is None:
            continue

        phase = rhythm_phase if render_resp_phase is None else render_resp_phase
        physiology = module.PhysiologySnapshot(
            heart_rate=hr,
            exertion=exertion,
            contractility=contractility,
            contractility_excess=0.0,
            respiration_rate=resp_rate,
            respiration_depth=breath_depth,
            respiration_phase=phase,
        )
        render = module.create_render_spec(
            event=event,
            physiology=physiology,
            coefficients=coefficients,
        )
        if tail_mode == "off" and not tamer_enabled:
            audio = core_offline.render_beat(module, source, render, coefficients)
        else:
            audio = _counterfactual_render(
                module,
                source,
                sample_rate,
                render,
                event.vigor,
                coefficients,
                tail_mode=tail_mode,
                tamer_enabled=tamer_enabled,
            )
        beats.append(
            {
                "audio": audio,
                "event": event,
                "render": render,
                "ibi": float(event.ibi),
                "filling_interval": float(event.filling_interval),
                "contractility": float(event.vigor),
                "resp_phase": float(phase),
                "lung_inflation": float(module.lung_inflation(phase)),
                "systole_duration": float(render.systole_duration),
            }
        )
        if len(beats) == n_beats:
            break
    if len(beats) != n_beats:
        raise RuntimeError(f"compiled RhythmEngine fired only {len(beats)} of {n_beats} requested beats")

    audio = np.concatenate([beat["audio"] for beat in beats], axis=0)
    return {
        "audio": audio,
        "beats": beats,
        "hr": hr,
        "exertion": exertion,
        "contractility": contractility,
        "resp_rate": resp_rate,
        "breath_depth": breath_depth,
        "vigor_sigma": vigor_sigma,
        "sample_rate": sample_rate,
        "coefficients": coefficients,
    }


def _s1_window(beat_audio: np.ndarray, systole_duration: float, dur_ms: float) -> np.ndarray:
    """Return an onset-aligned channel-zero S1 slice matching reference windows."""
    mono = analysis_channel(beat_audio)
    region = mono[: int(systole_duration * SR)]
    env = shrlib.env_analytic(region, SR)
    peak = int(np.argmax(env))
    onset = 0
    if peak > 0 and env[peak] > 0:
        climb = np.maximum.accumulate(env[: peak + 1])
        above = np.flatnonzero(climb >= 0.10 * env[peak])
        onset = int(above[0]) if len(above) else 0
    return region[onset : onset + int(dur_ms * 1.0e-3 * SR)]


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
    """Fixed channel-zero S1-start window matching the ref20 breath calibration."""
    return analysis_channel(beat_audio)[: int(BREATH_WIN_MS * 1.0e-3 * SR)]


ESTIMATOR_METADATA = {
    "peak": {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "crest": {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "centroid": {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "peak_cv": {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "peak_cv_slow": {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "peak_cv_fast": {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "centroid_cv": {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "peak_mean": {"window": "onset+dur_ms", "width": "invariant", "converges": None},
    "crest_mean_db": {"window": "onset+dur_ms", "width": "sensitive", "converges": None},
    "aw_swing_db": {"window": "beat+fixed", "width": "sensitive", "converges": "breath-cycles"},
    "centroid_ratio": {"window": "beat+fixed", "width": "sensitive", "converges": "breath-cycles"},
    "inflation_exp_median": {"window": "sequence", "width": "invariant", "converges": "breath-cycles"},
    "inflation_insp_median": {"window": "sequence", "width": "invariant", "converges": "breath-cycles"},
    "breath_cycles": {"window": "sequence", "width": "invariant", "converges": None},
    "breath_problems": {"window": "sequence", "width": "invariant", "converges": None},
}


def measure_sequence(seq: dict, dur_ms: float = 128.0, warmup: int = 4) -> dict:
    """Return the preserved per-beat S1 metrics over compiled native-layout renders."""
    native_run = np.concatenate([beat["audio"] for beat in seq["beats"]], axis=0)
    weighted_run = shrlib.a_weight(analysis_channel(native_run), SR)
    starts = np.concatenate([[0], np.cumsum([len(beat["audio"]) for beat in seq["beats"][:-1]])])
    breath_win = int(BREATH_WIN_MS * 1.0e-3 * SR)

    peaks, crests, cents = [], [], []
    breath_cents, aw_rms, inflations = [], [], []
    for index, (beat, start) in enumerate(zip(seq["beats"], starts)):
        s1 = _s1_window(beat["audio"], beat["systole_duration"], dur_ms)
        breath_cents.append(shrlib.centroid(_fixed_s1_window(beat["audio"]), SR))
        aw_rms.append(shrlib.rms(weighted_run[start : start + breath_win]))
        inflations.append(beat["lung_inflation"])
        if index >= warmup:
            peaks.append(shrlib.peak(s1))
            crests.append(shrlib.crest(s1))
            cents.append(shrlib.centroid(s1, SR))
    peaks, crests, cents = np.array(peaks), np.array(crests), np.array(cents)
    breath_cents = np.array(breath_cents)
    aw_rms = np.array(aw_rms)
    inflations = np.array(inflations)

    def cv(values: np.ndarray) -> float:
        return float(np.std(values) / np.mean(values)) if np.mean(values) else 0.0

    slow, fast = _slow_fast(peaks)
    expiration, inspiration = breath_groups(inflations)
    problems = breath_swing_problems(inflations, seq["hr"], seq["resp_rate"], shrlib.SINE)
    return {
        "peak": peaks,
        "crest": crests,
        "centroid": cents,
        "peak_cv": cv(peaks),
        "peak_cv_slow": slow,
        "peak_cv_fast": fast,
        "centroid_cv": cv(cents),
        "peak_mean": float(peaks.mean()),
        "crest_mean_db": float(20 * np.log10(crests.mean())),
        "aw_swing_db": breath_swing_db(aw_rms, inflations),
        "centroid_ratio": breath_group_ratio(breath_cents, inflations),
        "inflation_exp_median": float(np.median(inflations[expiration])),
        "inflation_insp_median": float(np.median(inflations[inspiration])),
        "breath_cycles": breath_cycles(len(inflations), seq["hr"], seq["resp_rate"]),
        "breath_problems": problems,
    }


def breath_period_beats(values: np.ndarray) -> int:
    """Return the dominant 2..len/3-beat period by autocorrelation."""
    values = np.asarray(values, float)
    centered = values - values.mean()
    autocorrelation = np.correlate(centered, centered, "full")[len(centered) - 1 :]
    high = max(3, len(values) // 3)
    return 2 + int(np.argmax(autocorrelation[2:high])) if high > 2 else 3


def _slow_fast(
    values: np.ndarray,
    period: int | None = None,
    n_harm: int = 2,
) -> tuple[float, float]:
    """Split per-beat CV into respiratory and residual components."""
    values = np.asarray(values, float)
    count = len(values)
    if count < 6 or values.mean() == 0:
        return 0.0, 0.0
    period = period or breath_period_beats(values)
    time = np.arange(count)
    columns = [np.ones(count), time]
    for harmonic in range(1, n_harm + 1):
        columns += [
            np.cos(2 * np.pi * harmonic * time / period),
            np.sin(2 * np.pi * harmonic * time / period),
        ]
    design = np.column_stack(columns)
    coefficients, *_ = np.linalg.lstsq(design, values, rcond=None)
    respiratory = design[:, 2:] @ coefficients[2:]
    residual = values - design @ coefficients
    mean = values.mean()
    return float(respiratory.std() / mean), float(residual.std() / mean)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=core_offline.DEFAULT_MODULE_DIR,
        help=core_offline.MODULE_DIR_HELP,
    )
    parser.add_argument("--hr", type=float, default=180)
    parser.add_argument("--contractility", type=float, default=1.0)
    parser.add_argument("--exertion", type=float, default=1.0)
    parser.add_argument("--resp-rate", type=float, default=None)
    parser.add_argument("--breath-depth", type=float, default=None)
    parser.add_argument("--beats", type=int, default=24)
    parser.add_argument(
        "--vigor-sigma",
        type=float,
        default=None,
        help="override VigorJitterScale for this run; 0 disables it",
    )
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--dur-ms", type=float, default=128.0)
    parser.add_argument(
        "--set",
        action="append",
        metavar="NAME=VALUE",
        default=[],
        help="apply a named immutable model-coefficient override; repeatable",
    )
    parser.add_argument(
        "--legacy-tail-mode",
        choices=legacy_s1.LEGACY_TAIL_MODES,
        default="off",
        help="late-S1 counterfactual; fixed/dry-end re-enable retired tail variants",
    )
    parser.add_argument(
        "--legacy-tamer",
        action="store_true",
        help="late-S1 counterfactual: re-enable the retired secondary-lobe tamer",
    )
    parser.add_argument("--out", help="optional native-layout WAV of the beat run")
    args = parser.parse_args()

    module = core_offline.load_binding(args.module_dir)
    coefficients, overrides = core_offline.coefficients_from_args(module, args.set)
    core_offline.print_overrides(overrides)
    seq = beat_sequence(
        args.source,
        args.hr,
        args.contractility,
        args.exertion,
        args.beats,
        args.vigor_sigma,
        args.seed,
        resp_rate=args.resp_rate,
        breath_depth=args.breath_depth,
        synth_options={
            "tail_mode": args.legacy_tail_mode,
            "tamer_enabled": args.legacy_tamer,
        },
        module=module,
        coefficients=coefficients,
    )
    measured = measure_sequence(seq, args.dur_ms)
    beats_per_breath = args.hr / seq["resp_rate"]
    print(
        f"HR{args.hr:.0f} c{args.contractility:.2f} ex{args.exertion:.2f} "
        f"vigor_sigma{seq['vigor_sigma']:.2f} respRate{seq['resp_rate']:.1f} "
        f"depth{seq['breath_depth']:.2f} beats/breath{beats_per_breath:.2f} "
        f"tail={args.legacy_tail_mode} tamer={'on' if args.legacy_tamer else 'off'} "
        f"({args.beats} beats, {len(measured['peak'])} measured)"
    )
    target = ref8_peak_cv_target()
    note = (
        f"   (ref8 peak {target[0] * 100:.1f}% +/-{target[1] * 100:.1f}% SE over 8 beats; "
        "NOT a calibration target yet - see data_citations.toml)"
        if target
        else ""
    )
    print(f"  S1 peak-amp: mean {measured['peak_mean']:.3f}  CV {measured['peak_cv'] * 100:.1f}%{note}")
    print(
        f"  S1 brightness (centroid): CV {measured['centroid_cv'] * 100:.1f}%   "
        "(within-signal only - width-sensitive, no valid cross-recording target)"
    )
    print(f"  S1 crest: {measured['crest_mean_db']:.1f} dB mean")
    targets = breath_group_median_targets(shrlib.SINE)
    status = "UNSOUND - do not calibrate on this" if measured["breath_problems"] else "sound"
    print(
        f"  breath top/bottom 30%: A-weighted swing {measured['aw_swing_db']:.2f} dB  "
        f"centroid ratio {measured['centroid_ratio']:.3f}  [{status}]"
    )
    print(
        f"    over {measured['breath_cycles']:.1f} respiratory cycles; inflation-group medians "
        f"{measured['inflation_exp_median']:.3f}/{measured['inflation_insp_median']:.3f} "
        f"vs {targets[0]:.3f}/{targets[1]:.3f} under uniform phase coverage"
    )
    for problem in measured["breath_problems"]:
        print(f"  WARNING: {problem}", file=sys.stderr)
    print("  per-beat peak-amp: " + " ".join(f"{peak:.3f}" for peak in measured["peak"]))
    if args.out:
        import soundfile as sf

        sf.write(args.out, np.clip(seq["audio"], -1, 1), SR, subtype="PCM_16")
        print(
            f"  wrote native-layout {args.out}  {len(seq['audio']) / SR:.2f}s  "
            f"{seq['audio'].shape[1]} channels"
        )


if __name__ == "__main__":
    main()
