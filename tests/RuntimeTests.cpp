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
#include "core/Constants.hpp"
#include "core/Runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace
{
    namespace C = SHR::Constants;
    constexpr float Tolerance = 1.0e-5F;

    struct UniformDraw
    {
        float Min;
        float Max;
        float Value;
    };

    class ScriptedRandom
    {
    public:
        ScriptedRandom(std::vector<UniformDraw> uniform, std::vector<float> normal = { })
            : m_Uniform(std::move(uniform))
            , m_Normal(std::move(normal))
        {
        }

        SHR::RhythmRandom Callbacks()
        {
            return {
                .Uniform = [this](float min, float max) {
                    REQUIRE(m_UniformIndex < m_Uniform.size());
                    const UniformDraw draw = m_Uniform[m_UniformIndex++];
                    CHECK_THAT(min, Catch::Matchers::WithinAbs(draw.Min, Tolerance));
                    CHECK_THAT(max, Catch::Matchers::WithinAbs(draw.Max, Tolerance));
                    CHECK(draw.Value >= min);
                    CHECK(draw.Value <= max);
                    return draw.Value;
                },
                .StandardNormal = [this]() {
                    REQUIRE(m_NormalIndex < m_Normal.size());
                    return m_Normal[m_NormalIndex++];
                },
            };
        }

        void RequireConsumed() const
        {
            REQUIRE(m_UniformIndex == m_Uniform.size());
            REQUIRE(m_NormalIndex == m_Normal.size());
        }

    private:
        std::vector<UniformDraw> m_Uniform;
        std::vector<float>       m_Normal;
        std::size_t              m_UniformIndex = 0;
        std::size_t              m_NormalIndex  = 0;
    };

    SHR::RuntimeSettings Settings(float susceptibility = 0.0F)
    {
        return {
            .Simulation = {
                .RestingHeartRate = 60.0F,
                .MaximumHeartRate = 200.0F,
            },
            .ArrhythmiaSusceptibility = susceptibility,
        };
    }

    SHR::StepInput Input(float delta, bool outputEnabled, SHR::PlayerState player = { })
    {
        return {
            .Player         = player,
            .DeltaSeconds   = delta,
            .GameHoursDelta = 0.0F,
            .OutputEnabled  = outputEnabled,
        };
    }

    void CheckSameRender(const SHR::RenderSpec &actual, const SHR::RenderSpec &expected)
    {
        CHECK_THAT(actual.IBI, Catch::Matchers::WithinAbs(expected.IBI, Tolerance));
        CHECK_THAT(actual.SystoleDuration, Catch::Matchers::WithinAbs(expected.SystoleDuration, Tolerance));
        CHECK_THAT(actual.S1Amplitude, Catch::Matchers::WithinAbs(expected.S1Amplitude, Tolerance));
        CHECK_THAT(actual.S2Amplitude, Catch::Matchers::WithinAbs(expected.S2Amplitude, Tolerance));
        CHECK_THAT(actual.S1ResampleRatio, Catch::Matchers::WithinAbs(expected.S1ResampleRatio, Tolerance));
        CHECK_THAT(actual.S2ResampleRatio, Catch::Matchers::WithinAbs(expected.S2ResampleRatio, Tolerance));
        CHECK_THAT(actual.LowPassCutoffHz, Catch::Matchers::WithinAbs(expected.LowPassCutoffHz, Tolerance));
        CHECK_THAT(actual.OnsetCompression, Catch::Matchers::WithinAbs(expected.OnsetCompression, Tolerance));
        CHECK(actual.Kind == expected.Kind);
    }
}

TEST_CASE("Disabled output advances physiology but freezes rhythm", "[runtime][characterization]")
{
    ScriptedRandom random({
        { 0.0F, 1.0F, 0.5F },
    });
    SHR::Runtime runtime(Settings(), random.Callbacks());
    runtime.Init();

    SHR::PlayerState sprinting;
    sprinting.IsSprinting = true;
    const SHR::StepResult disabled = runtime.Step(Input(1.0F, false, sprinting));

    CHECK(disabled.Physiology.Exertion > C::IdleMets);
    CHECK_FALSE(disabled.Beat.has_value());

    const SHR::StepResult enabled = runtime.Step(Input(0.1F, true, sprinting));
    CHECK_FALSE(enabled.Beat.has_value());
    random.RequireConsumed();
}

TEST_CASE("Runtime returns raw beat metadata with its resolved render specification", "[runtime][characterization]")
{
    ScriptedRandom random(
        {
            { 0.0F, 1.0F, 0.5F },
        },
        { 0.25F }
    );
    SHR::Runtime runtime(Settings(), random.Callbacks());
    runtime.Init();

    const SHR::StepResult result = runtime.Step(Input(1.0F, true));

    REQUIRE(result.Beat.has_value());
    CHECK(result.Beat->Event.Kind == SHR::BeatKind::Sinus);
    CHECK_THAT(
        result.Physiology.HeartRate,
        Catch::Matchers::WithinAbs(runtime.GetSnapshot().HeartRate, Tolerance)
    );
    CheckSameRender(
        result.Beat->Render,
        SHR::CreateRenderSpec(result.Beat->Event, result.Physiology)
    );
    random.RequireConsumed();
}

TEST_CASE("Runtime applies the legacy maximum-risk PVC policy before rhythm advance", "[runtime][characterization]")
{
    ScriptedRandom random({
        { 0.0F, 1.0F, 0.0F },
        { 1.0F - C::PVCCouplingVariation, 1.0F + C::PVCCouplingVariation, 1.0F },
        { 0.0F, 1.0F, 0.5F },
        { 1.0F - C::PVCPauseVariation, 1.0F + C::PVCPauseVariation, 1.0F },
    });
    SHR::Runtime runtime(Settings(1.0F), random.Callbacks());
    runtime.Init();

    SHR::SimulationState state = runtime.GetState();
    state.Contractility = 0.5F;
    state.DeathSeconds = 240.0F;
    runtime.Restore(state);

    SHR::PlayerState dead;
    dead.IsDead = true;
    const SHR::StepResult result = runtime.Step(Input(1.0F, true, dead));

    REQUIRE(result.Beat.has_value());
    CHECK(result.Beat->Event.Kind == SHR::BeatKind::PVC);
    CHECK_THAT(
        result.Beat->Event.CouplingFraction,
        Catch::Matchers::WithinAbs(C::PVCCouplingMin, Tolerance)
    );
    CHECK(result.Beat->Render.Kind == SHR::BeatKind::PVC);
    random.RequireConsumed();
}

TEST_CASE("Runtime initialization resets simulation and rhythm together", "[runtime][characterization]")
{
    ScriptedRandom random({
        { 0.0F, 1.0F, 0.5F },
        { 0.0F, 1.0F, 0.5F },
    });
    SHR::Runtime runtime(Settings(), random.Callbacks());
    runtime.Init();

    // Before the step: notifications are applied by the step's drain, not at the call site.
    runtime.NotifyCombatEntry();
    REQUIRE_FALSE(runtime.Step(Input(0.6F, true)).Beat.has_value());
    REQUIRE(runtime.GetState().Adrenaline > 0.0F);

    runtime.Init();

    CHECK_THAT(runtime.GetState().Adrenaline, Catch::Matchers::WithinAbs(0.0F, Tolerance));
    CHECK_FALSE(runtime.Step(Input(0.5F, true)).Beat.has_value());
    random.RequireConsumed();
}

TEST_CASE("Applying settings shifts fitness so resting heart rate is not inert", "[runtime][settings]")
{
    SHR::Runtime runtime(Settings());
    runtime.Init();

    const float seededFitness = runtime.GetState().Fitness;

    SHR::RuntimeSettings lowered = Settings();
    lowered.Simulation.RestingHeartRate = 51.0F;
    REQUIRE(runtime.ApplySettings(lowered));

    const float expected = seededFitness + (60.0F - 51.0F) / C::RestingHRSlope;
    CHECK_THAT(runtime.GetState().Fitness, Catch::Matchers::WithinAbs(expected, Tolerance));
}

TEST_CASE("The fitness transform commutes with drift", "[runtime][settings]")
{
    SHR::Runtime driftedThenApplied(Settings());
    driftedThenApplied.Init();

    const float seeded = driftedThenApplied.GetState().Fitness;

    // Fitness integrates over GAME hours, so real delta alone leaves it where it started.
    SHR::PlayerState exerting;
    exerting.IsSprinting = true;
    for (int i = 0; i < 200; ++i)
    {
        driftedThenApplied.Step({
            .Player         = exerting,
            .DeltaSeconds   = 0.1F,
            .GameHoursDelta = 0.5F,
            .OutputEnabled  = false,
        });
    }

    const float drifted = driftedThenApplied.GetState().Fitness;
    REQUIRE(drifted != seeded);

    SHR::RuntimeSettings lowered = Settings();
    lowered.Simulation.RestingHeartRate = 51.0F;
    REQUIRE(driftedThenApplied.ApplySettings(lowered));

    CHECK_THAT(
        driftedThenApplied.GetState().Fitness,
        Catch::Matchers::WithinAbs(drifted + (60.0F - 51.0F) / C::RestingHRSlope, Tolerance)
    );
}

TEST_CASE("An invalid settings update is rejected without applying any part of it", "[runtime][settings]")
{
    SHR::Runtime runtime(Settings());
    runtime.Init();

    const SHR::SimulationState before = runtime.GetState();

    SHR::RuntimeSettings invalid;
    invalid.Simulation.RestingHeartRate = 51.0F;  // valid on its own
    invalid.Simulation.MaximumHeartRate = C::ExtremeHeartRateRiskThreshold;
    invalid.ArrhythmiaSusceptibility = 0.0F;

    CHECK_FALSE(runtime.ApplySettings(invalid));

    CHECK_THAT(runtime.GetState().Fitness, Catch::Matchers::WithinAbs(before.Fitness, Tolerance));
}

// The menu displays what GetSettings reports, so a divergence between it and what was applied would
// show the player a control that disagrees with the simulation behind it.
TEST_CASE("Live settings report what was applied", "[runtime][settings]")
{
    SHR::Runtime runtime(Settings());
    runtime.Init();

    SHR::RuntimeSettings updated = Settings();
    updated.Simulation.RestingHeartRate = 51.0F;
    updated.ArrhythmiaSusceptibility = 2.5F;
    REQUIRE(runtime.ApplySettings(updated));

    CHECK_THAT(
        runtime.GetSettings().Simulation.RestingHeartRate,
        Catch::Matchers::WithinAbs(51.0F, Tolerance)
    );
    CHECK_THAT(
        runtime.GetSettings().ArrhythmiaSusceptibility,
        Catch::Matchers::WithinAbs(2.5F, Tolerance)
    );

    SHR::RuntimeSettings invalid = updated;
    invalid.Simulation.MaximumHeartRate = C::ExtremeHeartRateRiskThreshold;
    REQUIRE_FALSE(runtime.ApplySettings(invalid));

    CHECK_THAT(
        runtime.GetSettings().Simulation.MaximumHeartRate,
        Catch::Matchers::WithinAbs(updated.Simulation.MaximumHeartRate, Tolerance)
    );
}

// The co-save load path applies a character's setting overrides to an already-constructed runtime,
// so the shift ApplySettings performs has to land a fresh character exactly where constructing with
// those settings would have. This equality is what lets overrides be adopted after construction.
TEST_CASE("Applying settings to a fresh runtime seeds as construction would", "[runtime][settings]")
{
    SHR::RuntimeSettings overridden = Settings();
    overridden.Simulation.RestingHeartRate = 48.0F;

    SHR::Runtime constructed(overridden);
    constructed.Init();

    SHR::Runtime applied(Settings());
    applied.Init();
    REQUIRE(applied.ApplySettings(overridden));

    CHECK_THAT(
        applied.GetState().Fitness,
        Catch::Matchers::WithinAbs(constructed.GetState().Fitness, Tolerance)
    );
}

// The other half of that ordering: for a character who already has saved progression, the shift must
// be discarded rather than compounded. Restore running last is what guarantees it, so a reordering
// that reintroduced the double-count would fail here.
TEST_CASE("Restoring after a settings change discards the seed shift", "[runtime][settings]")
{
    SHR::Runtime runtime(Settings());
    runtime.Init();

    SHR::SimulationState saved = runtime.GetState();
    saved.Fitness = 17.5F;

    SHR::RuntimeSettings overridden = Settings();
    overridden.Simulation.RestingHeartRate = 48.0F;
    REQUIRE(runtime.ApplySettings(overridden));
    runtime.Restore(saved);

    CHECK_THAT(runtime.GetState().Fitness, Catch::Matchers::WithinAbs(17.5F, Tolerance));
}
