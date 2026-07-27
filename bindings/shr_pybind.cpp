/*
 * This file is part of SHR.
 *
 * SHR is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 3.
 *
 * SHR is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SHR. If not, see <https://www.gnu.org/licenses/>.
 */

// Offline binding: exposes the shr_core source conditioning, beat renderer, acoustic mapping, rhythm,
// and Runtime scenario surfaces to Python with no CommonLib/XAudio/Skyrim dependency (see
// docs/ARCHITECTURE.md, Offline execution). WAV container parsing stays in Python because the plugin
// owns it in-game, so this binding accepts already-decoded PCM16 samples.

#include <AcousticMapper.hpp>
#include <BeatEvent.hpp>
#include <BeatKind.hpp>
#include <BeatRenderer.hpp>
#include <HeartbeatSource.hpp>
#include <ModelCoefficients.hpp>
#include <Pcm16.hpp>
#include <PhysiologySnapshot.hpp>
#include <RenderSpec.hpp>
#include <RhythmEngine.hpp>
#include <RhythmInput.hpp>
#include <Runtime.hpp>
#include <RuntimeSettings.hpp>
#include <Simulation.hpp>
#include <StepInput.hpp>
#include <StepResult.hpp>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace py = pybind11;

namespace
{
    py::arg_v DefaultModelCoefficientsArg()
    {
        return py::arg_v(
            "coefficients",
            SHR::DefaultModelCoefficients(),
            "default_model_coefficients"
        );
    }

    float FloatOverride(std::string_view name, py::handle value)
    {
        if (
            py::isinstance<py::bool_>(value) ||
            (!py::isinstance<py::float_>(value) &&
                !py::isinstance<py::int_>(value))
        )
        {
            throw std::invalid_argument(
                "model coefficient " + std::string(name) + " requires a real number"
            );
        }
        return py::cast<float>(value);
    }

    int IntegerOverride(std::string_view name, py::handle value)
    {
        if (
            py::isinstance<py::bool_>(value) ||
            !py::isinstance<py::int_>(value)
        )
        {
            throw std::invalid_argument(
                "model coefficient " + std::string(name) + " requires an integer");
        }
        const long long converted = py::cast<long long>(value);
        if (
            converted < std::numeric_limits<int>::min() ||
            converted > std::numeric_limits<int>::max()
        )
        {
            throw std::invalid_argument(
                "model coefficient " + std::string(name) + " is outside the C++ int range"
            );
        }
        return static_cast<int>(converted);
    }

    [[noreturn]] void UnsupportedOverride(std::string_view name)
    {
        std::string_view reason;
        if (
            name == "S1OnsetFrames" ||
            name == "S1EndFrames" ||
            name == "S2OnsetFrames" ||
            name == "S2EndFrames"
        )
        {
            reason = "is fixed by the heartbeat source asset";
        }
        else if (name == "FitnessAbsoluteMin")
        {
            reason =
                "is derived from FitnessBaseMets, BaseRestingHR, MaxRestingHR, and RestingHRSlope";
        }
        else if (name == "SecondsPerHour")
        {
            reason = "is a unit conversion rather than a model coefficient";
        }
        else if (name == "VoiceOutputGain")
        {
            reason = "belongs to downstream game-mix integration";
        }
        else if (name == "InspirationFraction")
        {
            reason = "is dormant pending the state-dependent breath-curve work";
        }
        else
        {
            throw std::invalid_argument(
                "unknown model coefficient '" + std::string(name) + "'");
        }
        throw std::invalid_argument(
            "model coefficient " + std::string(name) + " cannot be overridden because it " +
            std::string(reason)
        );
    }

    SHR::ModelCoefficients WithOverrides(
        const SHR::ModelCoefficients &base,
        const py::dict               &overrides
    )
    {
        SHR::SimulationModelCoefficients    simulation = base.Simulation;
        SHR::RhythmModelCoefficients        rhythm = base.Rhythm;
        SHR::AcousticMappingCoefficients    acoustic = base.AcousticMapping;
        SHR::SourceConditioningCoefficients source = base.SourceConditioning;
        SHR::BeatRenderingCoefficients      rendering = base.BeatRendering;

        for (const auto &[key, value] : overrides)
        {
            if (!py::isinstance<py::str>(key))
            {
                throw std::invalid_argument("model coefficient override names must be strings");
            }
            const std::string name = py::cast<std::string>(key);

#define SHR_FLOAT_OVERRIDE(group, field)          \
    if (name == #field)                           \
    {                                             \
        group.field = FloatOverride(name, value); \
        continue;                                 \
    }
#define SHR_INT_OVERRIDE(group, field)              \
    if (name == #field)                             \
    {                                               \
        group.field = IntegerOverride(name, value); \
        continue;                                   \
    }

            SHR_FLOAT_OVERRIDE(simulation, BaseRestingHR)
            SHR_FLOAT_OVERRIDE(simulation, SleepFraction)
            SHR_FLOAT_OVERRIDE(simulation, HRFormulaCeiling)
            SHR_FLOAT_OVERRIDE(simulation, HRFastFraction)
            SHR_FLOAT_OVERRIDE(simulation, FastOnsetTauSedentary)
            SHR_FLOAT_OVERRIDE(simulation, FastOnsetTauElite)
            SHR_FLOAT_OVERRIDE(simulation, SlowOnsetTau)
            SHR_FLOAT_OVERRIDE(simulation, FastRecoveryTauSedentary)
            SHR_FLOAT_OVERRIDE(simulation, FastRecoveryTauElite)
            SHR_FLOAT_OVERRIDE(simulation, SlowRecoveryTau)
            SHR_FLOAT_OVERRIDE(simulation, IdleMets)
            SHR_FLOAT_OVERRIDE(simulation, WalkingMets)
            SHR_FLOAT_OVERRIDE(simulation, RunningMets)
            SHR_FLOAT_OVERRIDE(simulation, SprintingMets)
            SHR_FLOAT_OVERRIDE(simulation, SwimmingMets)
            SHR_FLOAT_OVERRIDE(simulation, JumpMets)
            SHR_FLOAT_OVERRIDE(simulation, CrouchMovementMultiplier)
            SHR_FLOAT_OVERRIDE(simulation, MountedMultiplier)
            SHR_FLOAT_OVERRIDE(simulation, ExertionAccumulationRate)
            SHR_FLOAT_OVERRIDE(simulation, ExertionRecoveryRate)
            SHR_FLOAT_OVERRIDE(simulation, AdrenalineHalfLife)
            SHR_FLOAT_OVERRIDE(simulation, AdrenalineCombatEntry)
            SHR_FLOAT_OVERRIDE(simulation, AdrenalineTakeHit)
            SHR_FLOAT_OVERRIDE(simulation, ContractilityOnsetTau)
            SHR_FLOAT_OVERRIDE(simulation, ContractilityDecayTau)
            SHR_FLOAT_OVERRIDE(simulation, AdrenalineContractilityScale)
            SHR_FLOAT_OVERRIDE(simulation, FitnessGainTau)
            SHR_FLOAT_OVERRIDE(simulation, FitnessDecayTau)
            SHR_FLOAT_OVERRIDE(simulation, FitnessBaseMets)
            SHR_FLOAT_OVERRIDE(simulation, FitnessMaxMets)
            SHR_FLOAT_OVERRIDE(simulation, RestingHRSlope)
            SHR_FLOAT_OVERRIDE(simulation, MaxRestingHR)
            SHR_FLOAT_OVERRIDE(simulation, RestingRespRate)
            SHR_FLOAT_OVERRIDE(simulation, VentilationVT1Fraction)
            SHR_FLOAT_OVERRIDE(simulation, VentilationRCPFraction)
            SHR_FLOAT_OVERRIDE(simulation, RespRateAtVT1)
            SHR_FLOAT_OVERRIDE(simulation, RespRateAtRCP)
            SHR_FLOAT_OVERRIDE(simulation, RespDepthAtVT1)
            SHR_FLOAT_OVERRIDE(simulation, RespDepthAtRCP)
            SHR_FLOAT_OVERRIDE(simulation, MaxRespRate)
            SHR_FLOAT_OVERRIDE(simulation, SleepRespRate)
            SHR_FLOAT_OVERRIDE(simulation, RespOnsetTau)
            SHR_FLOAT_OVERRIDE(simulation, RespRecoveryTau)
            SHR_FLOAT_OVERRIDE(simulation, BreathDepthOnsetTau)
            SHR_FLOAT_OVERRIDE(simulation, BreathDepthRecoveryTau)
            SHR_FLOAT_OVERRIDE(simulation, AcuteFatigueMax)
            SHR_FLOAT_OVERRIDE(simulation, AcuteFatigueGainTau)
            SHR_FLOAT_OVERRIDE(simulation, AcuteFatigueDecayTau)
            SHR_FLOAT_OVERRIDE(simulation, LongTermFatigueMax)
            SHR_FLOAT_OVERRIDE(simulation, LongTermFatigueGainTau)
            SHR_FLOAT_OVERRIDE(simulation, LongTermFatigueDecayTau)
            SHR_FLOAT_OVERRIDE(simulation, SleepRecoveryRate)

            SHR_FLOAT_OVERRIDE(rhythm, RSAAmplitudeRest)
            SHR_FLOAT_OVERRIDE(rhythm, PVCCouplingMax)
            SHR_FLOAT_OVERRIDE(rhythm, PVCCouplingMin)
            SHR_FLOAT_OVERRIDE(rhythm, PVCCouplingVariation)
            SHR_FLOAT_OVERRIDE(rhythm, PVCPauseVariation)
            SHR_FLOAT_OVERRIDE(rhythm, PVCChanceNormal)
            SHR_FLOAT_OVERRIDE(rhythm, PVCChanceMax)
            SHR_FLOAT_OVERRIDE(rhythm, PVCRunExtensionChance)
            SHR_INT_OVERRIDE(rhythm, PVCRunMaxLength)
            SHR_FLOAT_OVERRIDE(rhythm, VigorJitterScale)
            SHR_FLOAT_OVERRIDE(rhythm, VigorJitterMaxSigma)
            SHR_FLOAT_OVERRIDE(rhythm, DeathRiskRampSeconds)
            SHR_FLOAT_OVERRIDE(rhythm, ExtremeHeartRateRiskThreshold)
            SHR_FLOAT_OVERRIDE(rhythm, AdrenalineRunRiskScale)

            SHR_FLOAT_OVERRIDE(acoustic, AttackCompressMax)
            SHR_FLOAT_OVERRIDE(acoustic, ResamplePVCRatio)
            SHR_FLOAT_OVERRIDE(acoustic, BreathAmpDepth)
            SHR_FLOAT_OVERRIDE(acoustic, BreathDepthRestFraction)
            SHR_FLOAT_OVERRIDE(acoustic, BreathPitchDipDepth)
            SHR_FLOAT_OVERRIDE(acoustic, BreathLowPassOpenHz)
            SHR_FLOAT_OVERRIDE(acoustic, BreathLowPassMinHz)
            SHR_FLOAT_OVERRIDE(acoustic, SystoleIntercept)
            SHR_FLOAT_OVERRIDE(acoustic, SystoleSlope)
            SHR_FLOAT_OVERRIDE(acoustic, SystoleMin)
            SHR_FLOAT_OVERRIDE(acoustic, SystoleMax)
            SHR_FLOAT_OVERRIDE(acoustic, SystolePEPShortening)
            SHR_FLOAT_OVERRIDE(acoustic, PVCSystoleScale)
            SHR_FLOAT_OVERRIDE(acoustic, PVCSystoleMin)
            SHR_FLOAT_OVERRIDE(acoustic, ContractilityGainDb)
            SHR_FLOAT_OVERRIDE(acoustic, FrankStarlingMin)
            SHR_FLOAT_OVERRIDE(acoustic, FrankStarlingMax)
            SHR_FLOAT_OVERRIDE(acoustic, PVCS1Amplitude)
            SHR_FLOAT_OVERRIDE(acoustic, PVCS2Amplitude)
            SHR_FLOAT_OVERRIDE(acoustic, PVCS2FailCoupling)
            SHR_FLOAT_OVERRIDE(acoustic, PVCS2FullCoupling)

            SHR_FLOAT_OVERRIDE(source, SourceHighPassHz)
            SHR_FLOAT_OVERRIDE(source, SourceRestLevel)
            SHR_FLOAT_OVERRIDE(source, AttackBuildThreshold)

            SHR_FLOAT_OVERRIDE(rendering, CrossfadeMs)
            SHR_FLOAT_OVERRIDE(rendering, S1SystoleFraction)
            SHR_FLOAT_OVERRIDE(rendering, S2WindowFraction)
            SHR_INT_OVERRIDE(rendering, BreathLowPassPoles)
            SHR_FLOAT_OVERRIDE(rendering, SoftClipKnee)

#undef SHR_INT_OVERRIDE
#undef SHR_FLOAT_OVERRIDE

            UnsupportedOverride(name);
        }

        // Validate only the final batch so coordinated bound/knot changes do not fail midway.
        return SHR::ModelCoefficients(
            simulation,
            rhythm,
            acoustic,
            source,
            rendering
        );
    }

    py::dict CoefficientValues(const SHR::ModelCoefficients &coefficients)
    {
        py::dict out;
#define SHR_REPORT(group, field) out[#field] = coefficients.group.field

        SHR_REPORT(Simulation, BaseRestingHR);
        SHR_REPORT(Simulation, SleepFraction);
        SHR_REPORT(Simulation, HRFormulaCeiling);
        SHR_REPORT(Simulation, HRFastFraction);
        SHR_REPORT(Simulation, FastOnsetTauSedentary);
        SHR_REPORT(Simulation, FastOnsetTauElite);
        SHR_REPORT(Simulation, SlowOnsetTau);
        SHR_REPORT(Simulation, FastRecoveryTauSedentary);
        SHR_REPORT(Simulation, FastRecoveryTauElite);
        SHR_REPORT(Simulation, SlowRecoveryTau);
        SHR_REPORT(Simulation, IdleMets);
        SHR_REPORT(Simulation, WalkingMets);
        SHR_REPORT(Simulation, RunningMets);
        SHR_REPORT(Simulation, SprintingMets);
        SHR_REPORT(Simulation, SwimmingMets);
        SHR_REPORT(Simulation, JumpMets);
        SHR_REPORT(Simulation, CrouchMovementMultiplier);
        SHR_REPORT(Simulation, MountedMultiplier);
        SHR_REPORT(Simulation, ExertionAccumulationRate);
        SHR_REPORT(Simulation, ExertionRecoveryRate);
        SHR_REPORT(Simulation, AdrenalineHalfLife);
        SHR_REPORT(Simulation, AdrenalineCombatEntry);
        SHR_REPORT(Simulation, AdrenalineTakeHit);
        SHR_REPORT(Simulation, ContractilityOnsetTau);
        SHR_REPORT(Simulation, ContractilityDecayTau);
        SHR_REPORT(Simulation, AdrenalineContractilityScale);
        SHR_REPORT(Simulation, FitnessGainTau);
        SHR_REPORT(Simulation, FitnessDecayTau);
        SHR_REPORT(Simulation, FitnessBaseMets);
        SHR_REPORT(Simulation, FitnessMaxMets);
        SHR_REPORT(Simulation, RestingHRSlope);
        SHR_REPORT(Simulation, MaxRestingHR);
        out["FitnessAbsoluteMin"] = coefficients.Simulation.FitnessAbsoluteMin();
        SHR_REPORT(Simulation, RestingRespRate);
        SHR_REPORT(Simulation, VentilationVT1Fraction);
        SHR_REPORT(Simulation, VentilationRCPFraction);
        SHR_REPORT(Simulation, RespRateAtVT1);
        SHR_REPORT(Simulation, RespRateAtRCP);
        SHR_REPORT(Simulation, RespDepthAtVT1);
        SHR_REPORT(Simulation, RespDepthAtRCP);
        SHR_REPORT(Simulation, MaxRespRate);
        SHR_REPORT(Simulation, SleepRespRate);
        SHR_REPORT(Simulation, RespOnsetTau);
        SHR_REPORT(Simulation, RespRecoveryTau);
        SHR_REPORT(Simulation, BreathDepthOnsetTau);
        SHR_REPORT(Simulation, BreathDepthRecoveryTau);
        SHR_REPORT(Simulation, AcuteFatigueMax);
        SHR_REPORT(Simulation, AcuteFatigueGainTau);
        SHR_REPORT(Simulation, AcuteFatigueDecayTau);
        SHR_REPORT(Simulation, LongTermFatigueMax);
        SHR_REPORT(Simulation, LongTermFatigueGainTau);
        SHR_REPORT(Simulation, LongTermFatigueDecayTau);
        SHR_REPORT(Simulation, SleepRecoveryRate);

        SHR_REPORT(Rhythm, RSAAmplitudeRest);
        SHR_REPORT(Rhythm, PVCCouplingMax);
        SHR_REPORT(Rhythm, PVCCouplingMin);
        SHR_REPORT(Rhythm, PVCCouplingVariation);
        SHR_REPORT(Rhythm, PVCPauseVariation);
        SHR_REPORT(Rhythm, PVCChanceNormal);
        SHR_REPORT(Rhythm, PVCChanceMax);
        SHR_REPORT(Rhythm, PVCRunExtensionChance);
        SHR_REPORT(Rhythm, PVCRunMaxLength);
        SHR_REPORT(Rhythm, VigorJitterScale);
        SHR_REPORT(Rhythm, VigorJitterMaxSigma);
        SHR_REPORT(Rhythm, DeathRiskRampSeconds);
        SHR_REPORT(Rhythm, ExtremeHeartRateRiskThreshold);
        SHR_REPORT(Rhythm, AdrenalineRunRiskScale);

        SHR_REPORT(AcousticMapping, AttackCompressMax);
        SHR_REPORT(AcousticMapping, ResamplePVCRatio);
        SHR_REPORT(AcousticMapping, BreathAmpDepth);
        SHR_REPORT(AcousticMapping, BreathDepthRestFraction);
        SHR_REPORT(AcousticMapping, BreathPitchDipDepth);
        SHR_REPORT(AcousticMapping, BreathLowPassOpenHz);
        SHR_REPORT(AcousticMapping, BreathLowPassMinHz);
        SHR_REPORT(AcousticMapping, SystoleIntercept);
        SHR_REPORT(AcousticMapping, SystoleSlope);
        SHR_REPORT(AcousticMapping, SystoleMin);
        SHR_REPORT(AcousticMapping, SystoleMax);
        SHR_REPORT(AcousticMapping, SystolePEPShortening);
        SHR_REPORT(AcousticMapping, PVCSystoleScale);
        SHR_REPORT(AcousticMapping, PVCSystoleMin);
        SHR_REPORT(AcousticMapping, ContractilityGainDb);
        SHR_REPORT(AcousticMapping, FrankStarlingMin);
        SHR_REPORT(AcousticMapping, FrankStarlingMax);
        SHR_REPORT(AcousticMapping, PVCS1Amplitude);
        SHR_REPORT(AcousticMapping, PVCS2Amplitude);
        SHR_REPORT(AcousticMapping, PVCS2FailCoupling);
        SHR_REPORT(AcousticMapping, PVCS2FullCoupling);

        SHR_REPORT(SourceConditioning, SourceHighPassHz);
        SHR_REPORT(SourceConditioning, SourceRestLevel);
        SHR_REPORT(SourceConditioning, AttackBuildThreshold);

        SHR_REPORT(BeatRendering, CrossfadeMs);
        SHR_REPORT(BeatRendering, S1SystoleFraction);
        SHR_REPORT(BeatRendering, S2WindowFraction);
        SHR_REPORT(BeatRendering, BreathLowPassPoles);
        SHR_REPORT(BeatRendering, SoftClipKnee);

#undef SHR_REPORT
        return out;
    }

    SHR::BeatKind ParseKind(std::string_view kind)
    {
        if (kind == "sinus") return SHR::BeatKind::Sinus;
        if (kind == "pvc") return SHR::BeatKind::PVC;
        throw std::invalid_argument("beat kind must be 'sinus' or 'pvc'");
    }

    std::string KindName(SHR::BeatKind kind)
    {
        return kind == SHR::BeatKind::PVC ? "pvc" : "sinus";
    }

    // A deterministic RhythmRandom: a seeded mt19937 with the same per-call distributions production
    // uses (Random.hpp constructs a fresh distribution each draw), so a seed reproduces production's
    // draw math exactly while remaining reproducible for goldens.
    SHR::RhythmRandom SeededRandom(std::uint64_t seed)
    {
        const auto engine = std::make_shared<std::mt19937>(
            static_cast<std::mt19937::result_type>(seed)
        );
        return SHR::RhythmRandom{
            .Uniform = [engine](float min, float max) {
                std::uniform_real_distribution<float> distribution(min, max);
                return distribution(*engine);
            },
            .StandardNormal = [engine]() {
                std::normal_distribution<float> distribution(0.0F, 1.0F);
                return distribution(*engine);
            },
        };
    }

    // Interleaved PCM16 frames (frames, channels) -> conditioned source, mirroring the plugin's
    // DecodePcm16 -> PrepareHeartbeatSource path exactly.
    SHR::HeartbeatSource PrepareSource(
        py::array_t<std::int16_t, py::array::c_style | py::array::forcecast>  samples,
        std::uint32_t                                                         sampleRate,
        const SHR::ModelCoefficients                                         &coefficients
    )
    {
        if (samples.ndim() != 2)
        {
            throw std::invalid_argument("samples must be a 2-D (frames, channels) int16 array");
        }
        const auto channels = static_cast<std::uint32_t>(samples.shape(1));
        const SHR::AudioFormat format{ .SampleRate = sampleRate, .ChannelCount = channels };
        const std::span<const std::int16_t> flat(samples.data(), static_cast<std::size_t>(samples.size()));
        const SHR::AudioBuffer decoded = SHR::DecodePcm16(flat, format);
        return SHR::PrepareHeartbeatSource(
            decoded.ConstView(),
            coefficients.SourceConditioning
        );
    }

    py::array_t<float> ToArray(SHR::ConstAudioBufferView view)
    {
        py::array_t<float> out({
            static_cast<py::ssize_t>(view.FrameCount()),
            static_cast<py::ssize_t>(view.ChannelCount()),
        });
        const std::span<const float> samples = view.Samples();
        std::copy(samples.begin(), samples.end(), out.mutable_data());
        return out;
    }

    SHR::AudioBuffer FromArray(
        py::array_t<float, py::array::c_style | py::array::forcecast> samples,
        std::uint32_t                                                 sampleRate
    )
    {
        if (samples.ndim() != 2)
        {
            throw std::invalid_argument("source stage must be a 2-D (frames, channels) float array");
        }
        const auto channels = static_cast<std::uint32_t>(samples.shape(1));
        const SHR::AudioFormat format{
            .SampleRate = sampleRate,
            .ChannelCount = channels,
        };
        return SHR::AudioBuffer(
            format,
            static_cast<std::size_t>(samples.shape(0)),
            std::vector<float>(samples.data(), samples.data() + samples.size())
        );
    }

    SHR::RenderSpec MakeSpec(
        float              ibi,
        float              systoleDuration,
        float              s1Amplitude,
        float              s2Amplitude,
        float              s1ResampleRatio,
        float              s2ResampleRatio,
        float              lowPassCutoffHz,
        float              onsetCompression,
        const std::string &kind
    )
    {
        return {
            .IBI              = ibi,
            .SystoleDuration  = systoleDuration,
            .S1Amplitude      = s1Amplitude,
            .S2Amplitude      = s2Amplitude,
            .S1ResampleRatio  = s1ResampleRatio,
            .S2ResampleRatio  = s2ResampleRatio,
            .LowPassCutoffHz  = lowPassCutoffHz,
            .OnsetCompression = onsetCompression,
            .Kind             = ParseKind(kind),
        };
    }
}

PYBIND11_MODULE(shr_pybind, m)
{
    m.doc() = "Offline binding to the shr_core physiology, rhythm, mapping, and rendering surfaces.";

    py::class_<SHR::ModelCoefficients>(m, "ModelCoefficients")
        .def(py::init<>(), "Construct the immutable production-default coefficient value.")
        .def_property_readonly(
            "values",
            &CoefficientValues,
            "Return a new dict containing every selected value plus derived FitnessAbsoluteMin."
        )
        .def(
            "with_overrides",
            &WithOverrides,
            py::arg("overrides"),
            "Apply a validated batch of named overrides and return a new immutable value."
        );
    m.attr("default_model_coefficients") = SHR::DefaultModelCoefficients();

    py::class_<SHR::HeartbeatSource>(m, "HeartbeatSource")
        .def_property_readonly(
            "s1_frames",
            [](const SHR::HeartbeatSource &s) { return s.S1.FrameCount(); }
        )
        .def_property_readonly(
            "s2_frames",
            [](const SHR::HeartbeatSource &s) { return s.S2.FrameCount(); }
        )
        .def_property_readonly(
            "has_baseline_attack",
            [](const SHR::HeartbeatSource &s) { return s.S1BaselineAttack.has_value(); }
        );

    m.def(
        "prepare_source",
        &PrepareSource,
        py::arg("samples"),
        py::arg("sample_rate"),
        DefaultModelCoefficientsArg(),
        "Condition interleaved PCM16 (frames, channels) into a renderable HeartbeatSource."
    );

    m.def(
        "trace_source_conditioning",
        [](
            py::array_t<std::int16_t, py::array::c_style | py::array::forcecast>  samples,
            std::uint32_t                                                         sampleRate,
            const SHR::ModelCoefficients                                         &coefficients
        )
        {
            if (samples.ndim() != 2)
            {
                throw std::invalid_argument("samples must be a 2-D (frames, channels) int16 array");
            }
            const auto channels = static_cast<std::uint32_t>(samples.shape(1));
            const SHR::AudioFormat format{ .SampleRate = sampleRate, .ChannelCount = channels };
            const std::span<const std::int16_t> flat(
                samples.data(),
                static_cast<std::size_t>(samples.size())
            );
            const SHR::AudioBuffer decoded = SHR::DecodePcm16(flat, format);

            // The conditioning steps mutate `stages` in place, so copy each stage out before the next runs.
            SHR::HeartbeatSourceSlices stages = SHR::SliceHeartbeatSource(decoded.ConstView());
            py::dict out;
            out["sliced_s1"] = ToArray(stages.S1.ConstView());
            out["sliced_s2"] = ToArray(stages.S2.ConstView());
            SHR::ApplyHeartbeatSourceHighPass(
                stages,
                coefficients.SourceConditioning.SourceHighPassHz
            );
            out["highpass_s1"] = ToArray(stages.S1.ConstView());
            out["highpass_s2"] = ToArray(stages.S2.ConstView());
            SHR::NormalizeHeartbeatSourceJoint(
                stages,
                coefficients.SourceConditioning.SourceRestLevel
            );
            out["normalized_s1"] = ToArray(stages.S1.ConstView());
            out["normalized_s2"] = ToArray(stages.S2.ConstView());

            const auto attack = SHR::FindBaselineAttackRegion(
                stages.S1.ConstView(),
                coefficients.SourceConditioning.AttackBuildThreshold
            );
            if (!attack)
            {
                throw std::runtime_error("conditioned source has no baseline S1 attack region");
            }
            out["sample_rate"]   = sampleRate;
            out["channel_count"] = channels;
            out["attack_start"]  = attack->StartFrame;
            out["attack_peak"]   = attack->PeakFrame;
            return out;
        },
        py::arg("samples"),
        py::arg("sample_rate"),
        DefaultModelCoefficientsArg(),
        "Condition PCM16 (frames, channels) and return each conditioning stage as a (frames, channels) "
        "float32 array plus the baseline S1 attack landmarks."
    );

    m.def(
        "render_beat",
        [](
            const SHR::HeartbeatSource &source,
            float ibi,
            float systoleDuration,
            float s1Amplitude,
            float s2Amplitude,
            float s1ResampleRatio,
            float s2ResampleRatio,
            float lowPassCutoffHz,
            float onsetCompression,
            const std::string &kind,
            const SHR::ModelCoefficients &coefficients
        )
        {
            return ToArray(
                SHR::RenderBeat(
                    source,
                    MakeSpec(
                        ibi,
                        systoleDuration,
                        s1Amplitude,
                        s2Amplitude,
                        s1ResampleRatio,
                        s2ResampleRatio,
                        lowPassCutoffHz,
                        onsetCompression,
                        kind
                    ),
                    coefficients.BeatRendering
                ).ConstView()
            );
        },
        py::arg("source"),
        py::arg("ibi"),
        py::arg("systole_duration"),
        py::arg("s1_amplitude"),
        py::arg("s2_amplitude"),
        py::arg("s1_resample_ratio"),
        py::arg("s2_resample_ratio"),
        py::arg("lowpass_cutoff_hz"),
        py::arg("onset_compression"),
        py::arg("kind") = "sinus",
        DefaultModelCoefficientsArg(),
        "Render one beat to a (frames, channels) float32 array."
    );

    m.def(
        "trace_beat_render",
        [](
            const SHR::HeartbeatSource &source,
            float ibi,
            float systoleDuration,
            float s1Amplitude,
            float s2Amplitude,
            float s1ResampleRatio,
            float s2ResampleRatio,
            float lowPassCutoffHz,
            float onsetCompression,
            const std::string &kind,
            const SHR::ModelCoefficients &coefficients
        )
        {
            const SHR::BeatRenderTrace trace = SHR::TraceBeatRender(
                source,
                MakeSpec(
                    ibi,
                    systoleDuration,
                    s1Amplitude,
                    s2Amplitude,
                    s1ResampleRatio,
                    s2ResampleRatio,
                    lowPassCutoffHz,
                    onsetCompression,
                    kind
                ),
                coefficients.BeatRendering
            );
            py::dict stages;
            stages["source_s1"] = ToArray(trace.SourceS1.ConstView());
            stages["source_s2"] = ToArray(trace.SourceS2.ConstView());
            stages["transmitted_s1"] = ToArray(trace.TransmittedS1.ConstView());
            stages["transmitted_s2"] = ToArray(trace.TransmittedS2.ConstView());
            stages["transducer_input"] = ToArray(trace.TransducerInput.ConstView());
            stages["output"] = ToArray(trace.Output.ConstView());
            return stages;
        },
        py::arg("source"),
        py::arg("ibi"),
        py::arg("systole_duration"),
        py::arg("s1_amplitude"),
        py::arg("s2_amplitude"),
        py::arg("s1_resample_ratio"),
        py::arg("s2_resample_ratio"),
        py::arg("lowpass_cutoff_hz"),
        py::arg("onset_compression"),
        py::arg("kind") = "sinus",
        DefaultModelCoefficientsArg(),
        "Render one beat and return every stage as a dict of (frames, channels) float32 arrays."
    );

    m.def(
        "render_beat_from_source_stages",
        [](
            py::array_t<float, py::array::c_style | py::array::forcecast> sourceS1,
            py::array_t<float, py::array::c_style | py::array::forcecast> sourceS2,
            std::uint32_t                                                 sampleRate,
            float                                                         ibi,
            float                                                         systoleDuration,
            float                                                         s1Amplitude,
            float                                                         s2Amplitude,
            float                                                         s1ResampleRatio,
            float                                                         s2ResampleRatio,
            float                                                         lowPassCutoffHz,
            float                                                         onsetCompression,
            const std::string                                             &kind,
            const SHR::ModelCoefficients                                  &coefficients
        )
        {
            const SHR::AudioBuffer s1 = FromArray(sourceS1, sampleRate);
            const SHR::AudioBuffer s2 = FromArray(sourceS2, sampleRate);
            return ToArray(
                SHR::RenderBeatFromSourceStages(
                    s1.ConstView(),
                    s2.ConstView(),
                    MakeSpec(
                        ibi,
                        systoleDuration,
                        s1Amplitude,
                        s2Amplitude,
                        s1ResampleRatio,
                        s2ResampleRatio,
                        lowPassCutoffHz,
                        onsetCompression,
                        kind
                    ),
                    coefficients.BeatRendering
                ).ConstView()
            );
        },
        py::arg("source_s1"),
        py::arg("source_s2"),
        py::arg("sample_rate"),
        py::arg("ibi"),
        py::arg("systole_duration"),
        py::arg("s1_amplitude"),
        py::arg("s2_amplitude"),
        py::arg("s1_resample_ratio"),
        py::arg("s2_resample_ratio"),
        py::arg("lowpass_cutoff_hz"),
        py::arg("onset_compression"),
        py::arg("kind") = "sinus",
        DefaultModelCoefficientsArg(),
        "Continue a beat from post-source-stage float arrays through compiled transmission, mixing, "
        "and limiting; intended for explicit offline counterfactual transforms."
    );

    py::class_<SHR::BeatEvent>(m, "BeatEvent")
        .def(
            py::init([](
                float ibi,
                float filling_interval,
                float coupling_fraction,
                float vigor,
                const std::string &kind
            ) {
                return SHR::BeatEvent{
                    .IBI              = ibi,
                    .FillingInterval  = filling_interval,
                    .CouplingFraction = coupling_fraction,
                    .Vigor            = vigor,
                    .Kind             = ParseKind(kind),
                };
            }),
            py::arg("ibi"),
            py::arg("filling_interval"),
            py::arg("coupling_fraction"),
            py::arg("vigor"),
            py::arg("kind") = "sinus"
        )
        .def_readwrite("ibi", &SHR::BeatEvent::IBI)
        .def_readwrite("filling_interval", &SHR::BeatEvent::FillingInterval)
        .def_readwrite("coupling_fraction", &SHR::BeatEvent::CouplingFraction)
        .def_readwrite("vigor", &SHR::BeatEvent::Vigor)
        .def_property(
            "kind",
            [](const SHR::BeatEvent &event) { return KindName(event.Kind); },
            [](SHR::BeatEvent &event, const std::string &kind) { event.Kind = ParseKind(kind); }
        );

    py::class_<SHR::PhysiologySnapshot>(m, "PhysiologySnapshot")
        .def(
            py::init([](
                float heart_rate,
                float fast_heart_rate,
                float slow_heart_rate,
                float exertion,
                float adrenaline,
                float contractility,
                float contractility_excess,
                float fitness,
                float effective_fitness,
                float acute_fatigue,
                float long_term_fatigue,
                float respiration_rate,
                float respiration_depth,
                float respiration_phase,
                std::optional<float> death_seconds
            ) {
                return SHR::PhysiologySnapshot{
                    .HeartRate           = heart_rate,
                    .FastHeartRate       = fast_heart_rate,
                    .SlowHeartRate       = slow_heart_rate,
                    .Exertion            = exertion,
                    .Adrenaline          = adrenaline,
                    .Contractility       = contractility,
                    .ContractilityExcess = contractility_excess,
                    .Fitness             = fitness,
                    .EffectiveFitness    = effective_fitness,
                    .AcuteFatigue        = acute_fatigue,
                    .LongTermFatigue     = long_term_fatigue,
                    .RespirationRate     = respiration_rate,
                    .RespirationDepth    = respiration_depth,
                    .RespirationPhase    = respiration_phase,
                    .DeathSeconds        = death_seconds,
                };
            }),
            py::arg("heart_rate")           = 0.0F,
            py::arg("fast_heart_rate")      = 0.0F,
            py::arg("slow_heart_rate")      = 0.0F,
            py::arg("exertion")             = 0.0F,
            py::arg("adrenaline")           = 0.0F,
            py::arg("contractility")        = 0.0F,
            py::arg("contractility_excess") = 0.0F,
            py::arg("fitness")              = 0.0F,
            py::arg("effective_fitness")    = 0.0F,
            py::arg("acute_fatigue")        = 0.0F,
            py::arg("long_term_fatigue")    = 0.0F,
            py::arg("respiration_rate")     = 0.0F,
            py::arg("respiration_depth")    = 0.0F,
            py::arg("respiration_phase")    = 0.0F,
            py::arg("death_seconds")        = std::optional<float>()
        )
        .def_readwrite("heart_rate", &SHR::PhysiologySnapshot::HeartRate)
        .def_readwrite("fast_heart_rate", &SHR::PhysiologySnapshot::FastHeartRate)
        .def_readwrite("slow_heart_rate", &SHR::PhysiologySnapshot::SlowHeartRate)
        .def_readwrite("exertion", &SHR::PhysiologySnapshot::Exertion)
        .def_readwrite("adrenaline", &SHR::PhysiologySnapshot::Adrenaline)
        .def_readwrite("contractility", &SHR::PhysiologySnapshot::Contractility)
        .def_readwrite("contractility_excess", &SHR::PhysiologySnapshot::ContractilityExcess)
        .def_readwrite("fitness", &SHR::PhysiologySnapshot::Fitness)
        .def_readwrite("effective_fitness", &SHR::PhysiologySnapshot::EffectiveFitness)
        .def_readwrite("acute_fatigue", &SHR::PhysiologySnapshot::AcuteFatigue)
        .def_readwrite("long_term_fatigue", &SHR::PhysiologySnapshot::LongTermFatigue)
        .def_readwrite("respiration_rate", &SHR::PhysiologySnapshot::RespirationRate)
        .def_readwrite("respiration_depth", &SHR::PhysiologySnapshot::RespirationDepth)
        .def_readwrite("respiration_phase", &SHR::PhysiologySnapshot::RespirationPhase)
        .def_readwrite("death_seconds", &SHR::PhysiologySnapshot::DeathSeconds);

    py::class_<SHR::RenderSpec>(m, "RenderSpec")
        .def_readonly("ibi", &SHR::RenderSpec::IBI)
        .def_readonly("systole_duration", &SHR::RenderSpec::SystoleDuration)
        .def_readonly("s1_amplitude", &SHR::RenderSpec::S1Amplitude)
        .def_readonly("s2_amplitude", &SHR::RenderSpec::S2Amplitude)
        .def_readonly("s1_resample_ratio", &SHR::RenderSpec::S1ResampleRatio)
        .def_readonly("s2_resample_ratio", &SHR::RenderSpec::S2ResampleRatio)
        .def_readonly("lowpass_cutoff_hz", &SHR::RenderSpec::LowPassCutoffHz)
        .def_readonly("onset_compression", &SHR::RenderSpec::OnsetCompression)
        .def_property_readonly(
            "kind",
            [](const SHR::RenderSpec &render) { return KindName(render.Kind); }
        );

    m.def(
        "lung_inflation",
        &SHR::ComputeLungInflation,
        py::arg("respiration_phase"),
        "Return the compiled acoustic mapping's normalized lung-inflation curve."
    );

    m.def(
        "ventilation_targets",
        [](
            float normalizedExertion,
            const SHR::ModelCoefficients &coefficients
        ) {
            const SHR::VentilationTargets targets = SHR::ComputeVentilationTargets(
                normalizedExertion,
                coefficients.Simulation
            );
            return py::make_tuple(targets.Rate, targets.Depth);
        },
        py::arg("normalized_exertion"),
        DefaultModelCoefficientsArg(),
        "Return the compiled simulation's steady-state respiration-rate and depth targets."
    );

    py::class_<SHR::RhythmEngine>(m, "RhythmEngine")
        .def(
            py::init([](
                std::uint64_t                 seed,
                const SHR::ModelCoefficients &coefficients
            ) {
                return SHR::RhythmEngine(coefficients.Rhythm, SeededRandom(seed));
            }),
            py::arg("seed"),
            DefaultModelCoefficientsArg(),
            "Construct a rhythm engine driven by a seeded deterministic RNG."
        )
        .def("init", &SHR::RhythmEngine::Init)
        .def(
            "advance",
            [](
                SHR::RhythmEngine &self,
                float delta_seconds,
                float heart_rate,
                float respiration_phase,
                float exertion_fraction,
                float contractility,
                float pvc_chance_per_second,
                float risk_factor,
                float run_extension_chance
            ) -> std::optional<SHR::BeatEvent> {
                return self.Advance(SHR::RhythmInput{
                    .DeltaSeconds       = delta_seconds,
                    .HeartRate          = heart_rate,
                    .RespirationPhase   = respiration_phase,
                    .ExertionFraction   = exertion_fraction,
                    .Contractility      = contractility,
                    .PVCChancePerSecond = pvc_chance_per_second,
                    .RiskFactor         = risk_factor,
                    .RunExtensionChance = run_extension_chance,
                });
            },
            py::arg("delta_seconds"),
            py::arg("heart_rate"),
            py::arg("respiration_phase"),
            py::arg("exertion_fraction"),
            py::arg("contractility"),
            py::arg("pvc_chance_per_second"),
            py::arg("risk_factor"),
            py::arg("run_extension_chance"),
            "Advance rhythm one step; returns a BeatEvent when a beat is due, else None."
        );

    m.def(
        "create_render_spec",
        [](
            const SHR::BeatEvent          &event,
            const SHR::PhysiologySnapshot &physiology,
            const SHR::ModelCoefficients  &coefficients
        ) {
            return SHR::CreateRenderSpec(
                event,
                physiology,
                coefficients.AcousticMapping
            );
        },
        py::arg("event"),
        py::arg("physiology"),
        DefaultModelCoefficientsArg(),
        "Map a beat event and physiology snapshot to a RenderSpec (acoustic mapping)."
    );

    py::class_<SHR::PlayerState>(m, "PlayerState")
        .def(
            py::init([](
                bool is_dead,
                bool is_sprinting,
                bool is_running,
                bool is_walking,
                bool is_swimming,
                bool is_sneaking,
                bool is_on_mount
            ) {
                return SHR::PlayerState{
                    .IsDead      = is_dead,
                    .IsSprinting = is_sprinting,
                    .IsRunning   = is_running,
                    .IsWalking   = is_walking,
                    .IsSwimming  = is_swimming,
                    .IsSneaking  = is_sneaking,
                    .IsOnMount   = is_on_mount,
                };
            }),
            py::arg("is_dead")      = false,
            py::arg("is_sprinting") = false,
            py::arg("is_running")   = false,
            py::arg("is_walking")   = false,
            py::arg("is_swimming")  = false,
            py::arg("is_sneaking")  = false,
            py::arg("is_on_mount")  = false
        );

    py::class_<SHR::RuntimeBeat>(m, "RuntimeBeat")
        .def_readonly("event", &SHR::RuntimeBeat::Event)
        .def_readonly("render", &SHR::RuntimeBeat::Render);

    py::class_<SHR::StepResult>(m, "StepResult")
        .def_readonly("physiology", &SHR::StepResult::Physiology)
        .def_readonly("beat", &SHR::StepResult::Beat);

    // Runtime holds std::atomic members (via HeartRateSimulation), so it is neither copyable nor
    // movable: construct it in place through the default unique_ptr holder.
    py::class_<SHR::Runtime>(m, "Runtime")
        .def(
            py::init([](
                float resting_heart_rate,
                float maximum_heart_rate,
                float arrhythmia_susceptibility,
                std::uint64_t seed,
                const SHR::ModelCoefficients &coefficients
            ) {
                const SHR::RuntimeSettings settings{
                    .Simulation = {
                        .RestingHeartRate = resting_heart_rate,
                        .MaximumHeartRate = maximum_heart_rate,
                    },
                    .ArrhythmiaSusceptibility = arrhythmia_susceptibility,
                };
                return std::make_unique<SHR::Runtime>(
                    settings,
                    SeededRandom(seed),
                    coefficients
                );
            }),
            py::arg("resting_heart_rate")        = 55.0F,
            py::arg("maximum_heart_rate")        = 200.0F,
            py::arg("arrhythmia_susceptibility") = 1.0F,
            py::arg("seed")                      = 0,
            DefaultModelCoefficientsArg(),
            "Construct a runtime from overridable settings and a seeded deterministic rhythm RNG."
        )
        .def("init", &SHR::Runtime::Init, "Reset simulation and rhythm to their initial state.")
        .def(
            "step",
            [](
                SHR::Runtime &self,
                const SHR::PlayerState &player,
                float delta_seconds,
                float game_hours_delta,
                bool output_enabled
            ) {
                return self.Step(SHR::StepInput{
                    .Player         = player,
                    .DeltaSeconds   = delta_seconds,
                    .GameHoursDelta = game_hours_delta,
                    .OutputEnabled  = output_enabled,
                });
            },
            py::arg("player"),
            py::arg("delta_seconds"),
            py::arg("game_hours_delta") = 0.0F,
            py::arg("output_enabled")   = true,
            "Advance one step; returns a StepResult (snapshot plus an optional fired beat)."
        )
        .def("notify_jump", &SHR::Runtime::NotifyJump)
        .def("notify_sleep", &SHR::Runtime::NotifySleep, py::arg("duration"))
        .def("notify_fast_travel", &SHR::Runtime::NotifyFastTravel, py::arg("duration"))
        .def("notify_combat_entry", &SHR::Runtime::NotifyCombatEntry)
        .def("notify_hit", &SHR::Runtime::NotifyHit)
        .def_property_readonly("target_heart_rate", &SHR::Runtime::GetTargetHeartRate)
        .def_property_readonly(
            "target_respiration_rate",
            &SHR::Runtime::GetTargetRespirationRate
        )
        .def_property_readonly(
            "target_respiration_depth",
            &SHR::Runtime::GetTargetRespirationDepth
        );
}
