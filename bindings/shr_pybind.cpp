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
        py::array_t<std::int16_t, py::array::c_style | py::array::forcecast> samples,
        std::uint32_t                                                        sampleRate
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
        return SHR::PrepareHeartbeatSource(decoded.ConstView());
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
        "Condition interleaved PCM16 (frames, channels) into a renderable HeartbeatSource."
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
            const std::string &kind
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
                    )
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
            const std::string &kind
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
                )
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
        "Render one beat and return every stage as a dict of (frames, channels) float32 arrays."
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

    py::class_<SHR::RhythmEngine>(m, "RhythmEngine")
        .def(
            py::init([](std::uint64_t seed) { return SHR::RhythmEngine(SeededRandom(seed)); }),
            py::arg("seed"),
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
        [](const SHR::BeatEvent &event, const SHR::PhysiologySnapshot &physiology) {
            return SHR::CreateRenderSpec(event, physiology);
        },
        py::arg("event"),
        py::arg("physiology"),
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
                std::uint64_t seed
            ) {
                const SHR::RuntimeSettings settings{
                    .Simulation = {
                        .RestingHeartRate = resting_heart_rate,
                        .MaximumHeartRate = maximum_heart_rate,
                    },
                    .ArrhythmiaSusceptibility = arrhythmia_susceptibility,
                };
                return std::make_unique<SHR::Runtime>(settings, SeededRandom(seed));
            }),
            py::arg("resting_heart_rate")        = 55.0F,
            py::arg("maximum_heart_rate")        = 200.0F,
            py::arg("arrhythmia_susceptibility") = 1.0F,
            py::arg("seed")                      = 0,
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
        .def("notify_hit", &SHR::Runtime::NotifyHit);
}
