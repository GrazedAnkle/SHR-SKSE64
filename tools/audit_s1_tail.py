"""Audit late-S1 morphology and build a paired blind audition package.

The reference report measures fixed, S1-onset-relative 20-100 Hz energy windows.
Every value is a within-beat ratio to the 40-80 ms S1 body window; it describes
decay geometry, not a transferable absolute spectrum or a resonator-frequency
target.

The audition package keeps the same compiled rhythm events, mapping, transmission,
mix, and limiter path across variants. The only changes are explicit retired
source-stage transforms:

  legacy_current       tamer on, retired fixed tail
  tail_off             tamer on, no tail
  dry_end_tail         tamer on, tail splice relative to the shortened dry end
  no_tamer_fixed_tail  tamer off, retired fixed tail
  shipping             tamer off, no tail

``shipping`` is the unmodified compiled render. Every counterfactual starts from
the compiled post-onset source stage and returns to C++ for the active downstream
stages.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

import core_offline
import legacy_s1
import rhythm_offline as ro
import shrlib
from shrlib import SR, analysis_channel


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = ROOT / "contrib" / "Distribution" / "Sound" / "fx" / "SHR_HeartBeat" / "HeartBeat_Shortened.wav"
REFERENCE_DIR = ROOT / "docs" / "references"
LF_BAND_HZ = (20.0, 100.0)
BODY_WINDOW_S = (0.040, 0.080)
LATE_WINDOWS_S = ((0.080, 0.100), (0.100, 0.120))


@dataclass(frozen=True)
class Variant:
    name: str
    tail_mode: str
    tamer_enabled: bool


@dataclass(frozen=True)
class Scenario:
    name: str
    hr: float
    contractility: float
    exertion: float
    breath_depth: float
    beats: int
    seed_offset: int
    fixed_resp_phase: float | None = None


VARIANTS = (
    Variant("legacy_current", "fixed", True),
    Variant("tail_off", "off", True),
    Variant("dry_end_tail", "dry-end", True),
    Variant("no_tamer_fixed_tail", "fixed", False),
    Variant("shipping", "off", False),
)

SCENARIOS = (
    Scenario("rest", 68.0, 0.0, 0.0, 0.0, 6, 0),
    Scenario("high_drive_expiration", 181.0, 1.0, 1.0, 1.0, 14, 1, 0.0),
    Scenario("high_drive_natural_breath", 181.0, 1.0, 1.0, 1.0, 18, 2),
    Scenario("recovery", 141.0, 1.0, 0.25, 1.0, 14, 3),
)

REFERENCE_SCOPES = (
    (8, "peak (~180)"),
    (8, "clean, long segment (~175)"),
    (11, "resting, elevated supine"),
    (11, "post-exercise, elevated supine"),
    (15, "~80bpm"),
    (15, "~150bpm"),
    (20, "peak (~178)"),
)


def _rms(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.asarray(x, dtype=float) ** 2))) if len(x) else float("nan")


def _db_ratio(numerator: float, denominator: float) -> float:
    if not np.isfinite(numerator) or not np.isfinite(denominator) or denominator <= 0.0:
        return float("nan")
    return float(20.0 * np.log10(max(numerator / denominator, 1.0e-12)))


def _lf_filter(x: np.ndarray) -> np.ndarray:
    sos = butter(4, LF_BAND_HZ, btype="band", fs=SR, output="sos")
    return sosfiltfilt(sos, np.asarray(x, dtype=float))


def _summary(values: list[float]) -> dict:
    a = np.asarray(values, dtype=float)
    return {
        "median": float(np.nanmedian(a)),
        "p10": float(np.nanpercentile(a, 10)),
        "p90": float(np.nanpercentile(a, 90)),
        "n": int(np.count_nonzero(np.isfinite(a))),
    }


def measure_reference_group(ref: int, group_name: str) -> dict:
    """Measure one annotated reference group with whole-local-run filtering."""
    wav = REFERENCE_DIR / "original" / f"{ref}.wav"
    annotations = REFERENCE_DIR / "timestamps" / f"{ref}.txt"
    beats = shrlib.parse_annotations(annotations)[group_name]

    read_start = max(0.0, min(b[0] for b in beats) - 1.0)
    read_end = max(b[3] for b in beats) + 1.0
    with sf.SoundFile(wav) as handle:
        if handle.samplerate != SR:
            raise ValueError(f"{wav}: expected {SR} Hz, got {handle.samplerate}")
        handle.seek(int(round(read_start * SR)))
        signal = handle.read(
            int(round((read_end - read_start) * SR)),
            dtype="float64",
            always_2d=True,
        )[:, 0]
    lf = _lf_filter(signal)

    def window_rms(start: float, end: float) -> float:
        i0 = max(0, int(round((start - read_start) * SR)))
        i1 = min(len(lf), int(round((end - read_start) * SR)))
        return _rms(lf[i0:i1])

    late_80_100: list[float] = []
    late_100_120: list[float] = []
    gap: list[float] = []
    pre_gap: list[float] = []
    s1_duration: list[float] = []
    gap_duration: list[float] = []
    for s1a, s1b, s2a, _ in beats:
        body = window_rms(s1a + BODY_WINDOW_S[0], s1a + BODY_WINDOW_S[1])
        late_80_100.append(_db_ratio(window_rms(s1a + 0.080, s1a + 0.100), body))
        late_100_120.append(_db_ratio(window_rms(s1a + 0.100, s1a + 0.120), body))

        gap_s = max(0.0, s2a - s1b)
        gap.append(_db_ratio(window_rms(s1b, s2a), body))
        pre_gap.append(_db_ratio(window_rms(s1a - gap_s, s1a), body))
        s1_duration.append((s1b - s1a) * 1.0e3)
        gap_duration.append(gap_s * 1.0e3)

    return {
        "reference": f"ref{ref}",
        "group": group_name,
        "n_beats": len(beats),
        "ruler": {
            "band_hz": LF_BAND_HZ,
            "body_window_ms": tuple(v * 1.0e3 for v in BODY_WINDOW_S),
            "late_windows_ms": [tuple(v * 1.0e3 for v in w) for w in LATE_WINDOWS_S],
            "anchor": "hand S1 onset",
            "normalization": "per-beat LF RMS in the 40-80 ms body window",
        },
        "late_80_100_db": _summary(late_80_100),
        "late_100_120_db": _summary(late_100_120),
        "s1_to_s2_gap_db": _summary(gap),
        "equal_pre_s1_background_db": _summary(pre_gap),
        "s1_duration_ms": _summary(s1_duration),
        "s1_to_s2_gap_ms": _summary(gap_duration),
    }


def reference_report() -> list[dict]:
    out = []
    for ref, group in REFERENCE_SCOPES:
        wav = REFERENCE_DIR / "original" / f"{ref}.wav"
        if not wav.exists():
            print(f"warning: {wav} is unavailable; skipping ref{ref} {group}", file=sys.stderr)
            continue
        out.append(measure_reference_group(ref, group))
    return out


def _paired_states(
    module,
    coefficients,
    source: Path,
    scenario: Scenario,
    seed: int,
) -> list[dict]:
    seq = ro.beat_sequence(
        source,
        scenario.hr,
        scenario.contractility,
        scenario.exertion,
        n_beats=scenario.beats,
        seed=seed + scenario.seed_offset,
        breath_depth=scenario.breath_depth,
        render_resp_phase=scenario.fixed_resp_phase,
        module=module,
        coefficients=coefficients,
    )
    return seq["beats"]


def render_variant_scenario(
    module,
    source,
    coefficients,
    states: list[dict],
    variant: Variant,
) -> list[np.ndarray]:
    beats: list[np.ndarray] = []
    for state in states:
        if variant.name == "shipping":
            beats.append(state["audio"])
            continue
        trace = core_offline.trace_beat(
            module,
            source,
            state["render"],
            coefficients,
        )
        source_s1 = trace["source_s1"]
        if variant.tamer_enabled:
            source_s1 = legacy_s1.tame_lobe(
                source_s1,
                min(state["contractility"], 1.0),
            )
        source_s1 = legacy_s1.apply_tail(
            source_s1,
            variant.tail_mode,
            baseline_dry_frames=source.s1_frames,
        )
        beats.append(
            core_offline.render_from_source_stages(
                module,
                source_s1,
                trace["source_s2"],
                SR,
                state["render"],
                coefficients,
            )
        )
    return beats


def measure_engine_beats(beats: list[np.ndarray], states: list[dict]) -> dict:
    """Measure fixed late-S1 windows on an engine sequence."""
    native_run = np.concatenate(beats, axis=0)
    run = analysis_channel(native_run)
    pad = SR
    lf = _lf_filter(np.pad(run, (pad, pad)))[pad:pad + len(run)]
    starts = np.concatenate([[0], np.cumsum([len(b) for b in beats[:-1]])])

    late_80_100: list[float] = []
    late_100_120: list[float] = []
    post_120_gap: list[float] = []
    peaks: list[float] = []
    crests: list[float] = []
    for beat, state, start in zip(beats, states, starts):
        systole_s = state["systole_duration"]

        def wrms(lo: float, hi: float) -> float:
            return _rms(lf[start + int(lo * SR):start + int(hi * SR)])

        body = wrms(*BODY_WINDOW_S)
        late_80_100.append(_db_ratio(wrms(0.080, 0.100), body))
        late_100_120.append(_db_ratio(wrms(0.100, 0.120), body))
        post_120_gap.append(_db_ratio(wrms(0.120, systole_s), body))
        s1 = analysis_channel(beat)[:int(round(systole_s * SR))]
        peaks.append(shrlib.peak(s1))
        crests.append(20.0 * np.log10(shrlib.crest(s1)))

    return {
        "n_beats": len(beats),
        "late_80_100_db": _summary(late_80_100),
        "late_100_120_db": _summary(late_100_120),
        "post_120_to_s2_db": _summary(post_120_gap),
        "peak": _summary(peaks),
        "crest_db": _summary(crests),
        "max_abs": float(np.max(np.abs(native_run))),
    }


def splice_geometry(module, source, coefficients) -> list[dict]:
    out = []
    nominal_ibi = 60.0 / 180.0
    frank_starling_max = float(coefficients.values["FrankStarlingMax"])
    for contractility in (0.0, 0.5, 1.0, 1.4):
        event = module.BeatEvent(
            ibi=nominal_ibi,
            filling_interval=nominal_ibi * frank_starling_max,
            coupling_fraction=0.0,
            vigor=contractility,
            kind="sinus",
        )
        physiology = module.PhysiologySnapshot(heart_rate=180.0)
        render = module.create_render_spec(
            event=event,
            physiology=physiology,
            coefficients=coefficients,
        )
        trace = core_offline.trace_beat(module, source, render, coefficients)
        dry = trace["source_s1"]
        env = shrlib.env_analytic(analysis_channel(dry), SR, ms=0.0)
        peak = int(np.argmax(env))
        fixed = legacy_s1.tail_splice_frame(len(dry), source.s1_frames, "fixed")
        relative = legacy_s1.tail_splice_frame(len(dry), source.s1_frames, "dry-end")
        ramp = int(legacy_s1.LEGACY_TAIL_RAMP_MS * 1.0e-3 * SR)
        out.append({
            "contractility": contractility,
            "compression_k": float(render.onset_compression),
            "dry_ms": len(dry) / SR * 1.0e3,
            "analytic_peak_ms": peak / SR * 1.0e3,
            "fixed_splice_ms": fixed / SR * 1.0e3,
            "fixed_ramp_end_minus_dry_end_ms": (fixed + ramp - len(dry)) / SR * 1.0e3,
            "dry_end_splice_ms": relative / SR * 1.0e3,
            "dry_end_ramp_end_minus_dry_end_ms": (relative + ramp - len(dry)) / SR * 1.0e3,
            "fixed_peak_to_splice_ms": (fixed - peak) / SR * 1.0e3,
            "dry_end_peak_to_splice_ms": (relative - peak) / SR * 1.0e3,
        })
    return out


def _a_weighted_rms(signal: np.ndarray) -> float:
    return _rms(shrlib.a_weight(analysis_channel(signal), SR))


def _write_markdown_readme(path: Path, mapping: dict[str, str], sections: list[dict]) -> None:
    del mapping  # The blind README deliberately does not expose the key.
    lines = [
        "# Late-S1 blind audition",
        "",
        "The same letter identifies the same variant in both folders.",
        "",
        "- `production/`: production gain and limiter output.",
        "- `level_matched/`: post-render A-weighted file-level matching; only attenuation is used.",
        "- Keep `UNBLIND_KEY.json` closed until preferences are recorded.",
        "",
        "Each file contains these sections in order:",
        "",
    ]
    for section in sections:
        lines.append(
            f"- {section['name']}: starts {section['start_s']:.2f} s, "
            f"duration {section['duration_s']:.2f} s"
        )
    lines += [
        "",
        "Suggested pass: choose a preferred letter for the controlled high-drive expiration section first,",
        "then reject it if the natural-breath, recovery, or rest sections expose a regression.",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def build_audition(module, coefficients, source_path: Path, out_dir: Path, seed: int) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    production_dir = out_dir / "production"
    matched_dir = out_dir / "level_matched"
    production_dir.mkdir(exist_ok=True)
    matched_dir.mkdir(exist_ok=True)

    source, sample_rate = core_offline.prepare_source(module, source_path, coefficients)
    if sample_rate != SR:
        raise ValueError(f"late-S1 audit requires {SR} Hz source audio, got {sample_rate}")
    states = {
        scenario.name: _paired_states(module, coefficients, source_path, scenario, seed)
        for scenario in SCENARIOS
    }
    channels = states[SCENARIOS[0].name][0]["audio"].shape[1]
    silence = np.zeros((int(0.75 * SR), channels), dtype=np.float32)

    variant_files: dict[str, np.ndarray] = {}
    metrics: dict[str, dict] = {}
    sections: list[dict] = []
    cursor = 0
    for scenario in SCENARIOS:
        duration = sum(len(beat["audio"]) for beat in states[scenario.name]) / SR
        sections.append({"name": scenario.name, "start_s": cursor / SR, "duration_s": duration})
        cursor += int(round(duration * SR))
        if scenario is not SCENARIOS[-1]:
            cursor += len(silence)

    for variant in VARIANTS:
        chunks = []
        metrics[variant.name] = {}
        for index, scenario in enumerate(SCENARIOS):
            scenario_states = states[scenario.name]
            beats = render_variant_scenario(
                module,
                source,
                coefficients,
                scenario_states,
                variant,
            )
            chunks.append(np.concatenate(beats, axis=0))
            metrics[variant.name][scenario.name] = measure_engine_beats(
                beats,
                scenario_states,
            )
            if index != len(SCENARIOS) - 1:
                chunks.append(silence)
        variant_files[variant.name] = np.concatenate(chunks, axis=0)

    codes = [chr(ord("A") + i) for i in range(len(VARIANTS))]
    random.Random(seed).shuffle(codes)
    mapping = {variant.name: code for variant, code in zip(VARIANTS, codes)}

    weighted_levels = {name: _a_weighted_rms(audio) for name, audio in variant_files.items()}
    match_target = min(weighted_levels.values())
    level_match_gain = {name: match_target / level for name, level in weighted_levels.items()}

    for name, audio in variant_files.items():
        code = mapping[name]
        sf.write(production_dir / f"{code}.wav", np.clip(audio, -1.0, 1.0), SR, subtype="PCM_16")
        matched = audio * level_match_gain[name]
        sf.write(matched_dir / f"{code}.wav", np.clip(matched, -1.0, 1.0), SR, subtype="PCM_16")

    key = {
        "seed": seed,
        "letter_to_variant": {code: name for name, code in mapping.items()},
        "variants": {v.name: asdict(v) for v in VARIANTS},
    }
    (out_dir / "UNBLIND_KEY.json").write_text(json.dumps(key, indent=2) + "\n", encoding="utf-8")
    _write_markdown_readme(out_dir / "README.md", mapping, sections)

    report = {
        "source": str(source_path),
        "sample_rate": SR,
        "seed": seed,
        "sections": sections,
        "level_match": {
            "ruler": "whole-file A-weighted RMS after production rendering",
            "target": match_target,
            "gain": level_match_gain,
        },
        "splice_geometry": splice_geometry(module, source, coefficients),
        "engine_metrics": metrics,
    }
    (out_dir / "UNBLIND_ENGINE_REPORT.json").write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=core_offline.DEFAULT_MODULE_DIR,
        help="directory containing shr_pybind (default: build/pybind)",
    )
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260723)
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="apply a named immutable model-coefficient override; repeatable",
    )
    parser.add_argument("--skip-references", action="store_true")
    args = parser.parse_args()

    module = core_offline.load_binding(args.module_dir)
    coefficients, overrides = core_offline.coefficients_from_args(module, args.set)
    core_offline.print_overrides(overrides)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    if not args.skip_references:
        refs = reference_report()
        (args.out_dir / "REFERENCE_LATE_S1_REPORT.json").write_text(
            json.dumps(refs, indent=2) + "\n",
            encoding="utf-8",
        )
        for row in refs:
            print(
                f"{row['reference']} {row['group']}: "
                f"80-100 {row['late_80_100_db']['median']:+.1f} dB, "
                f"100-120 {row['late_100_120_db']['median']:+.1f} dB, "
                f"gap {row['s1_to_s2_gap_db']['median']:+.1f} dB"
            )

    report = build_audition(
        module,
        coefficients,
        args.source,
        args.out_dir,
        args.seed,
    )
    print(f"wrote blind audition package to {args.out_dir}")
    for row in report["splice_geometry"]:
        print(
            f"c={row['contractility']:.1f}: dry {row['dry_ms']:.2f} ms, "
            f"fixed ramp end-dry {row['fixed_ramp_end_minus_dry_end_ms']:+.2f} ms, "
            f"dry-end ramp end-dry {row['dry_end_ramp_end_minus_dry_end_ms']:+.2f} ms"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
