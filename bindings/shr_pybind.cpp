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

#include <core/AcousticMapper.hpp>
#include <core/BeatEvent.hpp>
#include <core/BeatKind.hpp>
#include <core/BeatRenderer.hpp>
#include <core/HeartbeatSource.hpp>
#include <core/ModelCoefficients.hpp>
#include <core/Pcm16.hpp>
#include <core/PhysiologySnapshot.hpp>
#include <core/RenderSpec.hpp>
#include <core/RhythmEngine.hpp>
#include <core/RhythmInput.hpp>
#include <core/Runtime.hpp>
#include <core/RuntimeSettings.hpp>
#include <core/Simulation.hpp>
#include <core/StepInput.hpp>
#include <core/StepResult.hpp>

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
#include <type_traits>

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

    template <typename Scalar>
    Scalar CoefficientOverride(std::string_view name, py::handle value)
    {
        if constexpr (std::is_same_v<Scalar, float>)
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
        else
        {
            static_assert(std::is_same_v<Scalar, int>);
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
    }

    [[noreturn]] void UnsupportedOverride(std::string_view name)
    {
        struct UnsupportedDescriptor
        {
            std::string_view Name;
            std::string_view Reason;
        };
        static constexpr UnsupportedDescriptor descriptors[]{
#define SHR_UNSUPPORTED_DESCRIPTOR(classification, type, field, reason) { #field, reason },
            SHR_NONLIVE_MODEL_CONSTANTS(SHR_UNSUPPORTED_DESCRIPTOR)
#undef SHR_UNSUPPORTED_DESCRIPTOR
        };

        for (const auto &descriptor : descriptors)
        {
            if (descriptor.Name == name)
            {
                throw std::invalid_argument(
                    "model coefficient " + std::string(name) +
                    " cannot be overridden because it " + std::string(descriptor.Reason)
                );
            }
        }
        throw std::invalid_argument(
            "unknown model coefficient '" + std::string(name) + "'");
    }

    struct MutableModelCoefficients
    {
        SHR::SimulationModelCoefficients    Simulation;
        SHR::RhythmModelCoefficients        Rhythm;
        SHR::AcousticMappingCoefficients    AcousticMapping;
        SHR::SourceConditioningCoefficients SourceConditioning;
        SHR::BeatRenderingCoefficients      BeatRendering;
    };

    struct CoefficientDescriptor
    {
        std::string_view Name;
        void (*Set)(MutableModelCoefficients &, py::handle);
        py::object (*Get)(const SHR::ModelCoefficients &);
    };

    std::span<const CoefficientDescriptor> CoefficientDescriptors()
    {
        static const CoefficientDescriptor descriptors[]{
#define SHR_COEFFICIENT_DESCRIPTOR(group, type, field)                              \
    {                                                                               \
        #field,                                                                     \
        [](MutableModelCoefficients &coefficients, py::handle value) {              \
            coefficients.group.field = CoefficientOverride<type>(#field, value);    \
        },                                                                          \
        [](const SHR::ModelCoefficients &coefficients) {                            \
            return py::cast(coefficients.group.field);                              \
        },                                                                          \
    },
#define SHR_SIMULATION_DESCRIPTOR(type, field) \
    SHR_COEFFICIENT_DESCRIPTOR(Simulation, type, field)
            SHR_SIMULATION_MODEL_COEFFICIENTS(SHR_SIMULATION_DESCRIPTOR)
#undef SHR_SIMULATION_DESCRIPTOR
#define SHR_RHYTHM_DESCRIPTOR(type, field) \
    SHR_COEFFICIENT_DESCRIPTOR(Rhythm, type, field)
            SHR_RHYTHM_MODEL_COEFFICIENTS(SHR_RHYTHM_DESCRIPTOR)
#undef SHR_RHYTHM_DESCRIPTOR
#define SHR_ACOUSTIC_DESCRIPTOR(type, field) \
    SHR_COEFFICIENT_DESCRIPTOR(AcousticMapping, type, field)
            SHR_ACOUSTIC_MAPPING_COEFFICIENTS(SHR_ACOUSTIC_DESCRIPTOR)
#undef SHR_ACOUSTIC_DESCRIPTOR
#define SHR_SOURCE_DESCRIPTOR(type, field) \
    SHR_COEFFICIENT_DESCRIPTOR(SourceConditioning, type, field)
            SHR_SOURCE_CONDITIONING_COEFFICIENTS(SHR_SOURCE_DESCRIPTOR)
#undef SHR_SOURCE_DESCRIPTOR
#define SHR_RENDERING_DESCRIPTOR(type, field) \
    SHR_COEFFICIENT_DESCRIPTOR(BeatRendering, type, field)
            SHR_BEAT_RENDERING_COEFFICIENTS(SHR_RENDERING_DESCRIPTOR)
#undef SHR_RENDERING_DESCRIPTOR
#undef SHR_COEFFICIENT_DESCRIPTOR
        };
        static_assert(
            sizeof(descriptors) / sizeof(descriptors[0]) ==
            SHR::Detail::LiveModelCoefficientCount
        );
        return descriptors;
    }

    SHR::ModelCoefficients WithOverrides(
        const SHR::ModelCoefficients &base,
        const py::dict               &overrides
    )
    {
        MutableModelCoefficients coefficients{
            .Simulation = base.Simulation,
            .Rhythm = base.Rhythm,
            .AcousticMapping = base.AcousticMapping,
            .SourceConditioning = base.SourceConditioning,
            .BeatRendering = base.BeatRendering,
        };

        for (const auto &[key, value] : overrides)
        {
            if (!py::isinstance<py::str>(key))
            {
                throw std::invalid_argument("model coefficient override names must be strings");
            }
            const std::string name = py::cast<std::string>(key);

            bool found = false;
            for (const auto &descriptor : CoefficientDescriptors())
            {
                if (descriptor.Name == name)
                {
                    descriptor.Set(coefficients, value);
                    found = true;
                    break;
                }
            }
            if (found)
            {
                continue;
            }

            UnsupportedOverride(name);
        }

        // Validate only the final batch so coordinated bound/knot changes do not fail midway.
        return SHR::ModelCoefficients(
            coefficients.Simulation,
            coefficients.Rhythm,
            coefficients.AcousticMapping,
            coefficients.SourceConditioning,
            coefficients.BeatRendering
        );
    }

    py::dict CoefficientValues(const SHR::ModelCoefficients &coefficients)
    {
        py::dict out;
        for (const auto &descriptor : CoefficientDescriptors())
        {
            out[py::str(descriptor.Name)] = descriptor.Get(coefficients);
        }
        // Derived values are reportable but deliberately absent from the live setter registry.
        out["FitnessAbsoluteMin"] = coefficients.Simulation.FitnessAbsoluteMin();
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
