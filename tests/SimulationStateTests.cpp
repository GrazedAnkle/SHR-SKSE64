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
#include "core/Constants.hpp"
#include "core/Simulation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

namespace
{
    namespace C = SHR::Constants;
    constexpr float Tolerance = 1.0e-5F;

    void RequireSameState(const SHR::SimulationState &actual, const SHR::SimulationState &expected)
    {
        CHECK_THAT(actual.FastHeartRate,    Catch::Matchers::WithinAbs(expected.FastHeartRate,    Tolerance));
        CHECK_THAT(actual.SlowHeartRate,    Catch::Matchers::WithinAbs(expected.SlowHeartRate,    Tolerance));
        CHECK_THAT(actual.Exertion,         Catch::Matchers::WithinAbs(expected.Exertion,         Tolerance));
        CHECK_THAT(actual.Adrenaline,       Catch::Matchers::WithinAbs(expected.Adrenaline,       Tolerance));
        CHECK_THAT(actual.Contractility,    Catch::Matchers::WithinAbs(expected.Contractility,    Tolerance));
        CHECK_THAT(actual.Fitness,          Catch::Matchers::WithinAbs(expected.Fitness,          Tolerance));
        CHECK_THAT(actual.AcuteFatigue,     Catch::Matchers::WithinAbs(expected.AcuteFatigue,     Tolerance));
        CHECK_THAT(actual.LongTermFatigue,  Catch::Matchers::WithinAbs(expected.LongTermFatigue,  Tolerance));
        CHECK_THAT(actual.RespirationRate,  Catch::Matchers::WithinAbs(expected.RespirationRate,  Tolerance));
        CHECK_THAT(actual.RespirationDepth, Catch::Matchers::WithinAbs(expected.RespirationDepth, Tolerance));
        CHECK_THAT(actual.RespirationPhase, Catch::Matchers::WithinAbs(expected.RespirationPhase, Tolerance));
        CHECK(actual.DeathSeconds == expected.DeathSeconds);
    }
}

TEST_CASE("SimulationState round-trips every resumable core field", "[simulation][state]")
{
    SHR::HeartRateSimulation simulation(SHR::SimulationSettings{});
    const SHR::SimulationState expected = {
        .FastHeartRate    = 74.0F,
        .SlowHeartRate    = 38.0F,
        .Exertion         = 7.25F,
        .Adrenaline       = 1.5F,
        .Contractility    = 0.61F,
        .Fitness          = 13.0F,
        .AcuteFatigue     = 0.35F,
        .LongTermFatigue  = 0.45F,
        .RespirationRate  = 28.0F,
        .RespirationDepth = 0.72F,
        .RespirationPhase = 0.37F,
        .DeathSeconds     = 4.5F,
    };

    simulation.Restore(expected);

    RequireSameState(simulation.GetState(), expected);

    const SHR::PhysiologySnapshot snapshot = simulation.GetSnapshot();
    CHECK_THAT(snapshot.HeartRate, Catch::Matchers::WithinAbs(112.0F, Tolerance));
    CHECK_THAT(snapshot.FastHeartRate, Catch::Matchers::WithinAbs(expected.FastHeartRate, Tolerance));
    CHECK_THAT(snapshot.SlowHeartRate, Catch::Matchers::WithinAbs(expected.SlowHeartRate, Tolerance));
    CHECK_THAT(snapshot.EffectiveFitness, Catch::Matchers::WithinAbs(12.2F, Tolerance));
    CHECK(snapshot.DeathSeconds == expected.DeathSeconds);
}

TEST_CASE("Restoring SimulationState reconstructs derived targets", "[simulation][state]")
{
    SHR::HeartRateSimulation original(SHR::SimulationSettings{ });
    original.Init();

    SHR::PlayerState sprinting;
    sprinting.IsSprinting = true;
    for (int step = 0; step < 200; ++step)
    {
        original.Step(sprinting, 0.1F);
    }

    SHR::HeartRateSimulation restored(SHR::SimulationSettings{ });
    restored.Init();
    restored.Restore(original.GetState());

    for (int step = 0; step < 100; ++step)
    {
        original.Step(sprinting, 0.1F);
        restored.Step(sprinting, 0.1F);
    }

    RequireSameState(restored.GetState(), original.GetState());
}

TEST_CASE("Legacy contractility equilibrium is a pure state translation", "[simulation][state]")
{
    SHR::HeartRateSimulation simulation(SHR::SimulationSettings{ });
    SHR::SimulationState state = simulation.CreateInitialState();
    state.Fitness = 10.0F;
    state.AcuteFatigue = 1.0F;
    state.LongTermFatigue = 1.0F;
    state.Exertion = std::lerp(C::IdleMets, 8.0F, 0.5F);
    state.Adrenaline = C::AdrenalineContractilityScale * 0.25F;

    CHECK_THAT(simulation.ComputeEquilibriumContractility(state), Catch::Matchers::WithinAbs(0.75F, Tolerance));
}
