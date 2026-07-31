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
#include "core/AcousticMapper.hpp"
#include "core/BeatRenderer.hpp"
#include "core/Constants.hpp"
#include "core/HeartbeatSource.hpp"
#include "core/ModelCoefficients.hpp"
#include "core/RhythmEngine.hpp"
#include "core/Runtime.hpp"
#include "core/Simulation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    SHR::ModelCoefficients Replace(
        const SHR::ModelCoefficients     &base,
        SHR::SimulationModelCoefficients  simulation
    )
    {
        return {
            simulation,
            base.Rhythm,
            base.AcousticMapping,
            base.SourceConditioning,
            base.BeatRendering,
        };
    }

    SHR::ModelCoefficients Replace(
        const SHR::ModelCoefficients &base,
        SHR::RhythmModelCoefficients  rhythm
    )
    {
        return {
            base.Simulation,
            rhythm,
            base.AcousticMapping,
            base.SourceConditioning,
            base.BeatRendering,
        };
    }

    SHR::ModelCoefficients Replace(
        const SHR::ModelCoefficients     &base,
        SHR::AcousticMappingCoefficients  acoustic
    )
    {
        return {
            base.Simulation,
            base.Rhythm,
            acoustic,
            base.SourceConditioning,
            base.BeatRendering,
        };
    }

    SHR::ModelCoefficients Replace(
        const SHR::ModelCoefficients        &base,
        SHR::SourceConditioningCoefficients  source
    )
    {
        return {
            base.Simulation,
            base.Rhythm,
            base.AcousticMapping,
            source,
            base.BeatRendering,
        };
    }

    SHR::ModelCoefficients Replace(
        const SHR::ModelCoefficients   &base,
        SHR::BeatRenderingCoefficients  rendering
    )
    {
        return {
            base.Simulation,
            base.Rhythm,
            base.AcousticMapping,
            base.SourceConditioning,
            rendering,
        };
    }

    SHR::PhysiologySnapshot BaselinePhysiology()
    {
        return {
            .HeartRate = 60.0F,
            .FastHeartRate = 36.0F,
            .SlowHeartRate = 24.0F,
            .Exertion = 1.5F,
            .Adrenaline = 0.0F,
            .Contractility = 1.0F,
            .ContractilityExcess = 0.0F,
            .Fitness = 10.0F,
            .EffectiveFitness = 10.0F,
            .AcuteFatigue = 0.0F,
            .LongTermFatigue = 0.0F,
            .RespirationRate = 14.0F,
            .RespirationDepth = 0.0F,
            .RespirationPhase = 0.0F,
            .DeathSeconds = std::nullopt,
        };
    }
}

static_assert(std::is_const_v<decltype(SHR::ModelCoefficients::Simulation)>);
static_assert(!std::is_copy_assignable_v<SHR::ModelCoefficients>);

TEST_CASE("Production model coefficients preserve the Constants defaults", "[coefficients]")
{
    const SHR::ModelCoefficients &c = SHR::DefaultModelCoefficients();

    CHECK(c.Simulation.SlowRecoveryTau == SHR::Constants::SlowRecoveryTau);
    CHECK(c.Rhythm.PVCRunMaxLength == SHR::Constants::PVCRunMaxLength);
    CHECK(c.AcousticMapping.ContractilityGainDb == SHR::Constants::ContractilityGainDb);
    CHECK(c.SourceConditioning.SourceHighPassHz == SHR::Constants::SourceHighPassHz);
    CHECK(c.BeatRendering.BreathLowPassPoles == SHR::Constants::BreathLowPassPoles);
    CHECK(c.Simulation.FitnessAbsoluteMin() == SHR::Constants::FitnessAbsoluteMin);
}

TEST_CASE("Model coefficient validation protects formulas without narrow calibration bounds", "[coefficients]")
{
    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();

    SECTION("non-finite scalar")
    {
        auto simulation = base.Simulation;
        simulation.SlowRecoveryTau = std::numeric_limits<float>::infinity();
        CHECK_THROWS_AS(Replace(base, simulation), std::invalid_argument);
    }

    SECTION("coordinated knots validate only as a completed value")
    {
        auto simulation = base.Simulation;
        simulation.VentilationVT1Fraction = 0.50F;
        simulation.VentilationRCPFraction = 0.60F;
        CHECK_NOTHROW(Replace(base, simulation));
    }

    SECTION("derived fitness denominator")
    {
        auto simulation = base.Simulation;
        simulation.MaxRestingHR = 200.0F;
        CHECK_THROWS_AS(Replace(base, simulation), std::invalid_argument);
    }

    SECTION("resting heart-rate ceiling")
    {
        auto simulation = base.Simulation;
        simulation.MaxRestingHR = simulation.BaseRestingHR - 1.0F;
        CHECK_THROWS_AS(Replace(base, simulation), std::invalid_argument);
    }

    SECTION("ordered acoustic bounds")
    {
        auto acoustic = base.AcousticMapping;
        acoustic.SystoleMin = acoustic.SystoleMax + 0.1F;
        CHECK_THROWS_AS(Replace(base, acoustic), std::invalid_argument);
    }

    SECTION("integer structural control")
    {
        auto rendering = base.BeatRendering;
        rendering.BreathLowPassPoles = 0;
        CHECK_THROWS_AS(Replace(base, rendering), std::invalid_argument);
    }
}

TEST_CASE("Simulation consumes its selected coefficient group", "[coefficients][simulation]")
{
    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto simulationCoefficients = base.Simulation;
    simulationCoefficients.ExertionAccumulationRate = 1.0F;
    const SHR::ModelCoefficients selected = Replace(base, simulationCoefficients);

    SHR::HeartRateSimulation simulation(
        SHR::SimulationSettings{ },
        selected.Simulation
    );
    simulation.Init();
    simulation.Step(SHR::PlayerState{ .IsSprinting = true }, 1.0F);

    CHECK(
        simulation.GetSnapshot().Exertion ==
        Catch::Approx(selected.Simulation.IdleMets + 1.0F)
    );
}

TEST_CASE("Runtime validates coefficient relationships that require subject settings", "[coefficients][runtime]")
{
    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto rhythm = base.Rhythm;
    rhythm.ExtremeHeartRateRiskThreshold = 210.0F;
    const SHR::ModelCoefficients selected = Replace(base, rhythm);

    CHECK_THROWS_AS(
        SHR::Runtime(SHR::RuntimeSettings{ }, selected),
        std::invalid_argument
    );
}

TEST_CASE("Rhythm consumes its selected coefficient group", "[coefficients][rhythm]")
{
    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto rhythmCoefficients = base.Rhythm;
    rhythmCoefficients.VigorJitterScale = 0.0F;
    const SHR::ModelCoefficients selected = Replace(base, rhythmCoefficients);

    SHR::RhythmEngine rhythm(
        selected.Rhythm,
        {
            .Uniform = [](float, float) { return 1.0F; },
            .StandardNormal = []() { return 1.0F; },
        });
    rhythm.Init();
    const std::optional<SHR::BeatEvent> beat = rhythm.Advance({
        .DeltaSeconds = 1.0F,
        .HeartRate = 60.0F,
        .RespirationPhase = 0.0F,
        .ExertionFraction = 0.0F,
        .Contractility = 0.8F,
        .PVCChancePerSecond = 0.0F,
        .RiskFactor = 0.0F,
        .RunExtensionChance = 0.0F,
    });

    REQUIRE(beat);
    CHECK(beat->Vigor == Catch::Approx(0.8F));
}

TEST_CASE("Acoustic mapping consumes its selected coefficient group", "[coefficients][acoustic]")
{
    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto acoustic = base.AcousticMapping;
    acoustic.ContractilityGainDb = 0.0F;
    const SHR::ModelCoefficients selected = Replace(base, acoustic);
    const SHR::BeatEvent event{
        .IBI = 1.0F,
        .FillingInterval = 1.0F,
        .CouplingFraction = 0.0F,
        .Vigor = 1.0F,
        .Kind = SHR::BeatKind::Sinus,
    };

    const SHR::RenderSpec render = SHR::CreateRenderSpec(
        event,
        BaselinePhysiology(),
        selected.AcousticMapping
    );

    CHECK(render.S1Amplitude == Catch::Approx(1.0F));
}

TEST_CASE("Source conditioning consumes its selected coefficient group", "[coefficients][source]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 1 };
    SHR::AudioBuffer decoded(format, SHR::Constants::S2EndFrames);
    decoded.View()(SHR::Constants::S1OnsetFrames, 0) = 1.0F;
    decoded.View()(SHR::Constants::S2OnsetFrames, 0) = -0.5F;

    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto sourceCoefficients = base.SourceConditioning;
    sourceCoefficients.SourceHighPassHz = 0.0F;
    sourceCoefficients.SourceRestLevel = 0.1F;
    const SHR::ModelCoefficients selected = Replace(base, sourceCoefficients);
    const SHR::HeartbeatSource   source = SHR::PrepareHeartbeatSource(
        decoded.ConstView(),
        selected.SourceConditioning
    );

    const auto peak = std::ranges::max(
        source.S1.ConstView().Samples(),
        {},
        [](float sample) { return std::abs(sample); }
    );
    CHECK(std::abs(peak) == Catch::Approx(0.1F));
}

TEST_CASE("Beat rendering consumes its selected coefficient group", "[coefficients][rendering]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 1 };
    SHR::HeartbeatSource source{
        .S1 = SHR::AudioBuffer(format, 1),
        .S2 = SHR::AudioBuffer(format, 1),
        .S1BaselineAttack = std::nullopt,
    };
    source.S1.View()(0, 0) = 1.0F;
    source.S2.View()(0, 0) = 0.0F;
    const SHR::RenderSpec render{
        .IBI = 0.1F,
        .SystoleDuration = 0.05F,
        .S1Amplitude = 2.0F,
        .S2Amplitude = 0.0F,
        .S1ResampleRatio = 1.0F,
        .S2ResampleRatio = 1.0F,
        .LowPassCutoffHz = 0.0F,
        .OnsetCompression = 1.0F,
        .Kind = SHR::BeatKind::Sinus,
    };

    const SHR::ModelCoefficients &base = SHR::DefaultModelCoefficients();
    auto rendering = base.BeatRendering;
    rendering.SoftClipKnee = 0.25F;
    const SHR::ModelCoefficients selected = Replace(base, rendering);

    const SHR::AudioBuffer baseline = SHR::RenderBeat(source, render);
    const SHR::AudioBuffer overridden = SHR::RenderBeat(
        source,
        render,
        selected.BeatRendering
    );

    CHECK(overridden.ConstView()(0, 0) < baseline.ConstView()(0, 0));
}
