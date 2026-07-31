"""The immutable offline model-coefficient binding contract.

The acceptance gate for the coefficient surface itself: the stable named registry, immutable batch
replacement, actionable rejection paths, and one behavioral override through each scoped core owner. The
golden domains separately prove the *default* value leaves output unchanged. ARCHITECTURE.md (offline
execution) owns the coefficient surface.

Beside the goldens because it shares their built-binding prerequisite, not because it has a manifest.
"""

from __future__ import annotations

from pathlib import Path
from types import ModuleType

import numpy as np
import pytest
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"

# Every coefficient-taking surface must render its default as a stable expression rather than the
# address-bearing default object repr. The latter is not valid Python and prevents pybind11-stubgen from
# parsing these signatures.
COEFFICIENT_SURFACES = (
    "RhythmEngine.__init__",
    "Runtime.__init__",
    "create_render_spec",
    "prepare_source",
    "render_beat",
    "trace_beat_render",
    "trace_source_conditioning",
)

RENDER_ARGS = {
    "ibi": 0.5,
    "systole_duration": 0.2,
    "s1_amplitude": 4.0,
    "s2_amplitude": 0.0,
    "s1_resample_ratio": 1.0,
    "s2_resample_ratio": 1.0,
    "lowpass_cutoff_hz": 0.0,
    "onset_compression": 1.0,
}


def expect_error(call, *parts: str) -> None:
    """Assert the call fails and that its message carries every part a maintainer needs to act on."""
    with pytest.raises((AttributeError, TypeError, ValueError)) as caught:
        call()
    message = str(caught.value)
    missing = [part for part in parts if part not in message]
    assert not missing, f"error {message!r} omitted expected text: {', '.join(missing)}"


@pytest.fixture(scope="module")
def source_pcm() -> tuple[np.ndarray, int]:
    samples, rate = sf.read(SOURCE, dtype="int16", always_2d=True)
    return np.ascontiguousarray(samples), rate


def test_registry_exposes_named_defaults(shr_pybind: ModuleType) -> None:
    values = shr_pybind.default_model_coefficients.values
    assert values["SlowRecoveryTau"] == 180.0
    assert values["PVCRunMaxLength"] == 5


def test_coefficient_surfaces_document_a_stable_default(shr_pybind: ModuleType) -> None:
    for name in COEFFICIENT_SURFACES:
        surface = shr_pybind
        for part in name.split("."):
            surface = getattr(surface, part)
        assert "= default_model_coefficients" in (surface.__doc__ or ""), name


def test_reported_values_are_a_copy(shr_pybind: ModuleType) -> None:
    defaults = shr_pybind.default_model_coefficients
    reported = defaults.values
    reported["SlowRecoveryTau"] = -1.0
    assert defaults.values["SlowRecoveryTau"] == 180.0


def test_selected_value_exposes_no_writable_fields(shr_pybind: ModuleType) -> None:
    expect_error(
        lambda: setattr(shr_pybind.default_model_coefficients, "values", {}),
        "no setter",
    )


def test_overrides_replace_immutably(shr_pybind: ModuleType) -> None:
    defaults = shr_pybind.default_model_coefficients
    changed = defaults.with_overrides({"SlowRecoveryTau": 90.0, "PVCRunMaxLength": 3})
    assert changed.values["SlowRecoveryTau"] == 90.0
    assert changed.values["PVCRunMaxLength"] == 3
    assert defaults.values["SlowRecoveryTau"] == 180.0
    assert defaults.values["PVCRunMaxLength"] == 5


def test_the_final_batch_is_validated_once(shr_pybind: ModuleType) -> None:
    """A coordinated pair is valid even though either half alone conflicts with the old value."""
    defaults = shr_pybind.default_model_coefficients
    expect_error(
        lambda: defaults.with_overrides({"SystoleMin": 0.5}),
        "SystoleMax",
        "SystoleMin",
    )
    coordinated = defaults.with_overrides({"SystoleMin": 0.5, "SystoleMax": 0.6})
    assert coordinated.values["SystoleMin"] == 0.5
    assert coordinated.values["SystoleMax"] == np.float32(0.6)


@pytest.mark.parametrize(
    ("overrides", "expected_text"),
    [
        pytest.param(
            {"NoSuchCoefficient": 1.0},
            ("unknown model coefficient", "NoSuchCoefficient"),
            id="unknown-name",
        ),
        pytest.param({"FitnessAbsoluteMin": 2.0}, ("FitnessAbsoluteMin", "derived"), id="derived-value"),
        pytest.param({"S1OnsetFrames": 0}, ("S1OnsetFrames", "source asset"), id="source-asset-bound"),
        pytest.param(
            {"VoiceOutputGain": 1.0},
            ("VoiceOutputGain", "game-mix integration"),
            id="game-mix-integration",
        ),
        pytest.param({"SecondsPerHour": 1.0}, ("SecondsPerHour", "unit conversion"), id="unit-conversion"),
        pytest.param({"InspirationFraction": 0.5}, ("InspirationFraction", "dormant"), id="dormant"),
        pytest.param({"PVCRunMaxLength": 2.5}, ("PVCRunMaxLength", "integer"), id="non-integer"),
        pytest.param({"BreathLowPassPoles": True}, ("BreathLowPassPoles", "integer"), id="bool-for-integer"),
        pytest.param({"SoftClipKnee": float("nan")}, ("SoftClipKnee", "finite"), id="non-finite"),
    ],
)
def test_rejection_paths_are_actionable(
    shr_pybind: ModuleType, overrides: dict, expected_text: tuple[str, ...]
) -> None:
    expect_error(lambda: shr_pybind.default_model_coefficients.with_overrides(overrides), *expected_text)


def test_construction_rejects_a_coefficient_conflicting_with_its_arguments(
    shr_pybind: ModuleType,
) -> None:
    high_risk_knot = shr_pybind.default_model_coefficients.with_overrides(
        {"ExtremeHeartRateRiskThreshold": 210.0}
    )
    expect_error(
        lambda: shr_pybind.Runtime(maximum_heart_rate=200.0, coefficients=high_risk_knot),
        "maximum heart rate",
        "ExtremeHeartRateRiskThreshold",
    )


def _sprint_snapshot(module: ModuleType, coefficients):
    runtime = module.Runtime(
        resting_heart_rate=55.0,
        maximum_heart_rate=200.0,
        arrhythmia_susceptibility=0.0,
        seed=0,
        coefficients=coefficients,
    )
    runtime.init()
    return runtime.step(
        module.PlayerState(is_sprinting=True),
        delta_seconds=1.0,
        output_enabled=False,
    ).physiology


def test_simulation_group_reaches_the_simulation(shr_pybind: ModuleType) -> None:
    """The one-second sprint ramp is limited by the selected upward slew."""
    defaults = shr_pybind.default_model_coefficients
    simulation = defaults.with_overrides({"ExertionAccumulationRate": 1.0})
    default_exertion = _sprint_snapshot(shr_pybind, defaults).exertion
    slewed_exertion = _sprint_snapshot(shr_pybind, simulation).exertion
    assert slewed_exertion < default_exertion
    assert slewed_exertion == simulation.values["IdleMets"] + 1.0


def test_rhythm_group_reaches_the_rhythm_engine(shr_pybind: ModuleType) -> None:
    """Disabling vigor jitter preserves the supplied contractility exactly."""
    coefficients = shr_pybind.default_model_coefficients.with_overrides({"VigorJitterScale": 0.0})
    rhythm = shr_pybind.RhythmEngine(seed=0, coefficients=coefficients)
    rhythm.init()
    beat = rhythm.advance(
        delta_seconds=1.0,
        heart_rate=60.0,
        respiration_phase=0.0,
        exertion_fraction=0.0,
        contractility=0.8,
        pvc_chance_per_second=0.0,
        risk_factor=0.0,
        run_extension_chance=0.0,
    )
    assert beat is not None
    assert beat.vigor == np.float32(0.8)


def test_acoustic_mapping_group_reaches_the_mapper(shr_pybind: ModuleType) -> None:
    """Zero dB contractility gain leaves a fully filled sinus beat at unity."""
    event = shr_pybind.BeatEvent(
        ibi=1.0,
        filling_interval=1.0,
        coupling_fraction=0.0,
        vigor=1.0,
    )
    physiology = shr_pybind.PhysiologySnapshot(
        heart_rate=60.0,
        effective_fitness=10.0,
        respiration_rate=14.0,
    )
    acoustic = shr_pybind.default_model_coefficients.with_overrides({"ContractilityGainDb": 0.0})
    assert shr_pybind.create_render_spec(event, physiology, acoustic).s1_amplitude == 1.0


def test_source_conditioning_group_reaches_the_traced_stages(
    shr_pybind: ModuleType, source_pcm: tuple[np.ndarray, int]
) -> None:
    """The selected normalization peak reaches the traced stages."""
    samples, rate = source_pcm
    coefficients = shr_pybind.default_model_coefficients.with_overrides({"SourceRestLevel": 0.1})
    trace = shr_pybind.trace_source_conditioning(samples, rate, coefficients)
    assert np.max(np.abs(trace["normalized_s1"])) == np.float32(0.1)


def test_beat_rendering_group_changes_only_the_final_stage(
    shr_pybind: ModuleType, source_pcm: tuple[np.ndarray, int]
) -> None:
    """A lower knee changes the transducer output while leaving its input untouched."""
    samples, rate = source_pcm
    source = shr_pybind.prepare_source(samples, rate)
    rendering = shr_pybind.default_model_coefficients.with_overrides({"SoftClipKnee": 0.25})
    baseline = shr_pybind.trace_beat_render(source, **RENDER_ARGS)
    changed = shr_pybind.trace_beat_render(source, **RENDER_ARGS, coefficients=rendering)
    assert np.array_equal(baseline["transducer_input"], changed["transducer_input"])
    assert not np.array_equal(baseline["output"], changed["output"])


def test_explicit_defaults_match_omitted_arguments(
    shr_pybind: ModuleType, source_pcm: tuple[np.ndarray, int]
) -> None:
    samples, rate = source_pcm
    source = shr_pybind.prepare_source(samples, rate)
    baseline = shr_pybind.trace_beat_render(source, **RENDER_ARGS)
    explicit = shr_pybind.trace_beat_render(
        source, **RENDER_ARGS, coefficients=shr_pybind.default_model_coefficients
    )
    for stage in baseline:
        assert np.array_equal(baseline[stage], explicit[stage]), stage
