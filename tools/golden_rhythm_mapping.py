"""The golden rhythm and acoustic-mapping domain: what to freeze and how to compare it.

Drives shr_pybind's RhythmEngine (over scripted, seeded scenarios) and create_render_spec (over a table
of operating points) and describes their output as a manifest. This oracle locks the compiled core's
rhythm scheduling and acoustic mapping so future core changes are caught. Independent C++ behavioral
coverage lives in RhythmEngineTests.cpp and AcousticMapperTests.cpp; this golden anchors full sequences
end to end.

Outputs are small structured values, stored rounded and compared at a loose tolerance.

One golden domain; tools/golden_registry.py describes the surface it exposes.
"""
from __future__ import annotations

from pathlib import Path

from rhythm_mapping_fixtures import (
    BEAT_FIELDS,
    MAPPING_CASES,
    RENDER_SPEC_FIELDS,
    RHYTHM_SCENARIOS,
)

ROOT = Path(__file__).resolve().parent.parent

ID = "rhythm-mapping"
MANIFEST = ROOT / "tests" / "golden" / "rhythm_mapping.json"

ATOL = 1.0e-6


def _round(value: float) -> float:
    return round(float(value), 7)


def _run_scenario(module, scenario) -> list[dict]:
    engine = module.RhythmEngine(seed=scenario.seed)
    engine.init()
    delta = 1.0 / scenario.fps
    frames = int(round(scenario.seconds * scenario.fps))
    breaths_per_second = scenario.breaths_per_min / 60.0
    beats: list[dict] = []
    for frame in range(frames):
        phase = (frame * delta * breaths_per_second) % 1.0
        event = engine.advance(
            delta_seconds=delta,
            heart_rate=scenario.heart_rate,
            respiration_phase=phase,
            exertion_fraction=scenario.exertion_fraction,
            contractility=scenario.contractility,
            pvc_chance_per_second=scenario.pvc_chance_per_second,
            risk_factor=scenario.risk_factor,
            run_extension_chance=scenario.run_extension_chance,
        )
        if event is not None:
            beat = {"frame": frame}
            for name in BEAT_FIELDS:
                value = getattr(event, name)
                beat[name] = value if name == "kind" else _round(value)
            beats.append(beat)
    return beats


def _run_mapping(module, case) -> dict:
    event = module.BeatEvent(
        ibi=case.ibi,
        filling_interval=case.filling_interval,
        coupling_fraction=case.coupling_fraction,
        vigor=case.vigor,
        kind=case.kind,
    )
    snapshot = module.PhysiologySnapshot(
        heart_rate=case.heart_rate,
        contractility_excess=case.contractility_excess,
        respiration_phase=case.respiration_phase,
        respiration_depth=case.respiration_depth,
    )
    render = module.create_render_spec(event=event, physiology=snapshot)
    result: dict = {}
    for name in RENDER_SPEC_FIELDS:
        value = getattr(render, name)
        result[name] = value if name == "kind" else _round(value)
    return result


def build(module) -> dict:
    return {
        "_comment": (
            "Golden rhythm and acoustic-mapping output from the compiled shr_core via shr_pybind "
            "(Release-Clang, deterministic). Regenerate: python tools/capture_goldens.py rhythm-mapping"
        ),
        "rhythm": {name: _run_scenario(module, sc) for name, sc in RHYTHM_SCENARIOS.items()},
        "mapping": {name: _run_mapping(module, case) for name, case in MAPPING_CASES.items()},
    }


def _diff_value(label: str, expected, actual, problems: list[str]) -> None:
    if isinstance(expected, str) or isinstance(actual, str):
        if expected != actual:
            problems.append(f"{label}: golden {expected!r} != core {actual!r}")
    elif abs(float(expected) - float(actual)) > ATOL:
        problems.append(f"{label}: golden {expected} != core {actual}")


def diff(expected: dict, actual: dict) -> list[str]:
    problems: list[str] = []

    exp_rhythm = expected.get("rhythm", {})
    act_rhythm = actual.get("rhythm", {})
    for name in RHYTHM_SCENARIOS:
        exp_beats = exp_rhythm.get(name, [])
        act_beats = act_rhythm.get(name, [])
        if len(exp_beats) != len(act_beats):
            problems.append(
                f"rhythm/{name}: beat count golden {len(exp_beats)} != core {len(act_beats)}"
            )
            continue
        for i, (exp_beat, act_beat) in enumerate(zip(exp_beats, act_beats)):
            if exp_beat.get("frame") != act_beat.get("frame"):
                problems.append(
                    f"rhythm/{name}[{i}]: frame golden {exp_beat.get('frame')} != core {act_beat.get('frame')}"
                )
            for field_name in BEAT_FIELDS:
                _diff_value(
                    f"rhythm/{name}[{i}].{field_name}",
                    exp_beat.get(field_name),
                    act_beat.get(field_name),
                    problems,
                )

    exp_map = expected.get("mapping", {})
    act_map = actual.get("mapping", {})
    for name in MAPPING_CASES:
        exp_spec = exp_map.get(name, {})
        act_spec = act_map.get(name, {})
        for field_name in RENDER_SPEC_FIELDS:
            _diff_value(
                f"mapping/{name}.{field_name}",
                exp_spec.get(field_name),
                act_spec.get(field_name),
                problems,
            )
    return problems


def summary(manifest: dict) -> list[str]:
    total = sum(len(beats) for beats in manifest["rhythm"].values())
    lines = [
        f"{len(RHYTHM_SCENARIOS)} scenarios / {total} beats, {len(MAPPING_CASES)} mapping cases"
    ]
    for name, beats in manifest["rhythm"].items():
        kinds = ", ".join(sorted({beat["kind"] for beat in beats})) or "none"
        lines.append(f"rhythm/{name}: {len(beats)} beats ({kinds})")
    for name in MAPPING_CASES:
        lines.append(f"mapping/{name}: OK")
    return lines
