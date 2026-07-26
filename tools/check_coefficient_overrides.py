"""Verify the immutable offline model-coefficient binding contract.

This is the focused acceptance gate for the coefficient surface. It checks the stable named registry,
immutable batch replacement, actionable rejection paths, and one behavioral override through each scoped
core owner.
The four compiled-core golden checkers separately prove that the default value leaves output unchanged.

Build the binding first with ``tools/build_pybind.py``.

    python tools/check_coefficient_overrides.py
    python tools/check_coefficient_overrides.py --module-dir build/dev-clang
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"


def _expect_error(call, *parts: str) -> None:
    try:
        call()
    except (AttributeError, TypeError, ValueError) as error:
        message = str(error)
        missing = [part for part in parts if part not in message]
        if missing:
            raise AssertionError(
                f"error {message!r} omitted expected text: {', '.join(missing)}"
            ) from error
    else:
        raise AssertionError(f"expected an error containing: {', '.join(parts)}")


def _snapshot(module, coefficients):
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


def verify(module) -> None:
    defaults = module.default_model_coefficients
    default_values = defaults.values
    assert default_values["SlowRecoveryTau"] == 180.0
    assert default_values["PVCRunMaxLength"] == 5

    # pybind11 must use a stable expression rather than the address-bearing default object repr. The
    # latter is not valid Python and prevents pybind11-stubgen from parsing these signatures.
    coefficient_surfaces = {
        "RhythmEngine.__init__": module.RhythmEngine.__init__,
        "Runtime.__init__": module.Runtime.__init__,
        "create_render_spec": module.create_render_spec,
        "prepare_source": module.prepare_source,
        "render_beat": module.render_beat,
        "trace_beat_render": module.trace_beat_render,
        "trace_source_conditioning": module.trace_source_conditioning,
    }
    for name, surface in coefficient_surfaces.items():
        assert "= default_model_coefficients" in (surface.__doc__ or ""), name

    # Reporting returns a copy, and the selected value itself exposes no writable fields.
    reported = defaults.values
    reported["SlowRecoveryTau"] = -1.0
    assert defaults.values["SlowRecoveryTau"] == 180.0
    _expect_error(
        lambda: setattr(defaults, "values", {}),
        "no setter",
    )

    changed = defaults.with_overrides(
        {
            "SlowRecoveryTau": 90.0,
            "PVCRunMaxLength": 3,
        }
    )
    assert changed.values["SlowRecoveryTau"] == 90.0
    assert changed.values["PVCRunMaxLength"] == 3
    assert defaults.values["SlowRecoveryTau"] == 180.0
    assert defaults.values["PVCRunMaxLength"] == 5

    # The final batch is validated once: this new pair is valid even though changing only the minimum
    # would conflict with the old maximum.
    _expect_error(
        lambda: defaults.with_overrides({"SystoleMin": 0.5}),
        "SystoleMax",
        "SystoleMin",
    )
    coordinated = defaults.with_overrides({"SystoleMin": 0.5, "SystoleMax": 0.6})
    assert coordinated.values["SystoleMin"] == 0.5
    assert coordinated.values["SystoleMax"] == np.float32(0.6)

    _expect_error(
        lambda: defaults.with_overrides({"NoSuchCoefficient": 1.0}),
        "unknown model coefficient",
        "NoSuchCoefficient",
    )
    _expect_error(
        lambda: defaults.with_overrides({"FitnessAbsoluteMin": 2.0}),
        "FitnessAbsoluteMin",
        "derived",
    )
    _expect_error(
        lambda: defaults.with_overrides({"S1OnsetFrames": 0}),
        "S1OnsetFrames",
        "source asset",
    )
    _expect_error(
        lambda: defaults.with_overrides({"VoiceOutputGain": 1.0}),
        "VoiceOutputGain",
        "game-mix integration",
    )
    _expect_error(
        lambda: defaults.with_overrides({"PVCRunMaxLength": 2.5}),
        "PVCRunMaxLength",
        "integer",
    )
    _expect_error(
        lambda: defaults.with_overrides({"BreathLowPassPoles": True}),
        "BreathLowPassPoles",
        "integer",
    )
    _expect_error(
        lambda: defaults.with_overrides({"SoftClipKnee": float("nan")}),
        "SoftClipKnee",
        "finite",
    )
    high_risk_knot = defaults.with_overrides({"ExtremeHeartRateRiskThreshold": 210.0})
    _expect_error(
        lambda: module.Runtime(maximum_heart_rate=200.0, coefficients=high_risk_knot),
        "maximum heart rate",
        "ExtremeHeartRateRiskThreshold",
    )

    # Simulation group: the one-second sprint ramp is limited by the selected upward slew.
    simulation = defaults.with_overrides({"ExertionAccumulationRate": 1.0})
    default_snapshot = _snapshot(module, defaults)
    changed_snapshot = _snapshot(module, simulation)
    assert changed_snapshot.exertion < default_snapshot.exertion
    assert changed_snapshot.exertion == simulation.values["IdleMets"] + 1.0

    # Rhythm group: disabling vigor jitter preserves the supplied contractility exactly.
    rhythm_coefficients = defaults.with_overrides({"VigorJitterScale": 0.0})
    rhythm = module.RhythmEngine(seed=0, coefficients=rhythm_coefficients)
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

    # Acoustic-mapping group: zero dB contractility gain leaves a fully filled sinus beat at unity.
    event = module.BeatEvent(
        ibi=1.0,
        filling_interval=1.0,
        coupling_fraction=0.0,
        vigor=1.0,
    )
    physiology = module.PhysiologySnapshot(
        heart_rate=60.0,
        effective_fitness=10.0,
        respiration_rate=14.0,
    )
    acoustic = defaults.with_overrides({"ContractilityGainDb": 0.0})
    mapped = module.create_render_spec(event, physiology, acoustic)
    assert mapped.s1_amplitude == 1.0

    samples, rate = sf.read(SOURCE, dtype="int16", always_2d=True)
    samples = np.ascontiguousarray(samples)

    # Source-conditioning group: the selected normalization peak reaches the traced stages.
    source_coefficients = defaults.with_overrides({"SourceRestLevel": 0.1})
    source_trace = module.trace_source_conditioning(samples, rate, source_coefficients)
    assert np.max(np.abs(source_trace["normalized_s1"])) == np.float32(0.1)

    source = module.prepare_source(samples, rate)
    render_args = {
        "ibi": 0.5,
        "systole_duration": 0.2,
        "s1_amplitude": 4.0,
        "s2_amplitude": 0.0,
        "s1_resample_ratio": 1.0,
        "s2_resample_ratio": 1.0,
        "lowpass_cutoff_hz": 0.0,
        "onset_compression": 1.0,
    }

    # Beat-rendering group: a lower knee changes only the final transducer stage.
    rendering = defaults.with_overrides({"SoftClipKnee": 0.25})
    baseline_render = module.trace_beat_render(source, **render_args)
    changed_render = module.trace_beat_render(
        source,
        **render_args,
        coefficients=rendering,
    )
    assert np.array_equal(
        baseline_render["transducer_input"],
        changed_render["transducer_input"],
    )
    assert not np.array_equal(baseline_render["output"], changed_render["output"])

    # Explicit production defaults and omitted arguments must be identical.
    explicit_default = module.trace_beat_render(
        source,
        **render_args,
        coefficients=defaults,
    )
    for stage in baseline_render:
        assert np.array_equal(baseline_render[stage], explicit_default[stage])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=ROOT / "build" / "pybind",
        help="Directory containing the built shr_pybind*.pyd (default: build/pybind).",
    )
    args = parser.parse_args()

    sys.path.insert(0, str(args.module_dir.resolve()))
    try:
        import shr_pybind
    except ImportError as error:
        sys.exit(
            f"cannot import shr_pybind from {args.module_dir} ({error}); "
            "build it with: python tools/build_pybind.py"
        )

    verify(shr_pybind)
    print("Immutable model-coefficient binding contract passed.")


if __name__ == "__main__":
    main()
