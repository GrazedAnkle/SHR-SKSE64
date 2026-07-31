"""Shared layer-2 fixtures: rhythm scenarios and acoustic-mapping cases.

These drive the compiled core through shr_pybind (RhythmEngine + create_render_spec) to produce the
layer-2 golden. Scenarios are held at one operating point with a swept respiration phase; each carries a
fixed seed so its stochastic output (vigor jitter, PVC sampling) is reproducible. They exercise the RNG
deliberately: contractility > 0 activates vigor jitter, and pvc_chance_per_second > 0 activates the PVC
path. The values are test inputs, not physiological claims.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class RhythmScenario:
    seed: int
    seconds: float
    fps: float
    heart_rate: float
    exertion_fraction: float
    contractility: float
    breaths_per_min: float
    pvc_chance_per_second: float = 0.0
    risk_factor: float = 0.0
    run_extension_chance: float = 0.0


# One operating point each, phase swept at breaths_per_min. contractility > 0 keeps vigor jitter live.
RHYTHM_SCENARIOS: dict[str, RhythmScenario] = {
    "rest": RhythmScenario(
        seed=1,
        seconds=12.0,
        fps=30.0,
        heart_rate=64.0,
        exertion_fraction=0.0,
        contractility=0.20,
        breaths_per_min=12.0,
    ),
    "exercise": RhythmScenario(
        seed=2,
        seconds=8.0,
        fps=30.0,
        heart_rate=168.0,
        exertion_fraction=0.95,
        contractility=0.85,
        breaths_per_min=40.0,
    ),
    "recovery": RhythmScenario(
        seed=3,
        seconds=10.0,
        fps=30.0,
        heart_rate=120.0,
        exertion_fraction=0.40,
        contractility=0.55,
        breaths_per_min=24.0,
    ),
    # PVC-prone: elevated ectopy chance + run extension + risk so coupling, pauses, and runs all fire.
    "pvc": RhythmScenario(
        seed=4,
        seconds=20.0,
        fps=30.0,
        heart_rate=88.0,
        exertion_fraction=0.15,
        contractility=0.35,
        breaths_per_min=16.0,
        pvc_chance_per_second=0.60,
        risk_factor=0.30,
        run_extension_chance=0.35,
    ),
}


@dataclass(frozen=True)
class MappingCase:
    # BeatEvent fields.
    ibi: float
    filling_interval: float
    coupling_fraction: float
    vigor: float
    kind: str
    # PhysiologySnapshot fields the mapper reads (others default to zero in the binding).
    heart_rate: float
    contractility_excess: float
    respiration_phase: float
    respiration_depth: float = field(default=0.0)


# Representative operating points for CreateRenderSpec, mirroring the render fixtures' physiology.
MAPPING_CASES: dict[str, MappingCase] = {
    "rest": MappingCase(
        ibi=60.0 / 64.0,
        filling_interval=60.0 / 64.0,
        coupling_fraction=0.0,
        vigor=0.0,
        kind="sinus",
        heart_rate=64.0,
        contractility_excess=0.0,
        respiration_phase=0.0,
    ),
    "peak": MappingCase(
        ibi=60.0 / 177.0,
        filling_interval=60.0 / 177.0,
        coupling_fraction=0.0,
        vigor=1.0,
        kind="sinus",
        heart_rate=177.0,
        contractility_excess=0.8,
        respiration_phase=0.0,
    ),
    "recovery": MappingCase(
        ibi=0.6,
        filling_interval=0.6,
        coupling_fraction=0.0,
        vigor=0.5,
        kind="sinus",
        heart_rate=110.0,
        contractility_excess=0.3,
        respiration_phase=0.0,
    ),
    "inspiration": MappingCase(
        ibi=60.0 / 79.0,
        filling_interval=60.0 / 79.0,
        coupling_fraction=0.0,
        vigor=0.2,
        kind="sinus",
        heart_rate=79.0,
        contractility_excess=0.0,
        respiration_phase=0.5,
        respiration_depth=1.0,
    ),
    "pvc": MappingCase(
        ibi=0.42,
        filling_interval=0.42,
        coupling_fraction=0.55,
        vigor=0.35,
        kind="pvc",
        heart_rate=88.0,
        contractility_excess=0.1,
        respiration_phase=0.25,
    ),
}

# BeatEvent / RenderSpec fields recorded in the golden.
BEAT_FIELDS = ("ibi", "filling_interval", "coupling_fraction", "vigor", "kind")
RENDER_SPEC_FIELDS = (
    "ibi",
    "systole_duration",
    "s1_amplitude",
    "s2_amplitude",
    "s1_resample_ratio",
    "s2_resample_ratio",
    "lowpass_cutoff_hz",
    "onset_compression",
    "kind",
)
