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

#include <string_view>

namespace
{
    constexpr SHR::SimulationSettings DefaultSimulationSettings;

    struct Checkpoint
    {
        float HeartRate;
        float FastHeartRate;
        float Exertion;
        float Adrenaline;
        float Contractility;
        float Fitness;
        float AcuteFatigue;
        float LongTermFatigue;
        float RespRate;
        float RespDepth;
        float RespPhase;
    };

    void RunSteps(
        SHR::HeartRateSimulation &sim,
        SHR::PlayerState state,
        int steps,
        float realDelta = 0.1F,
        float gameHoursPerStep = 0.0F
    )
    {
        for (int step = 0; step < steps; ++step)
        {
            sim.Step(state, realDelta, gameHoursPerStep);
        }
    }

    void RequireCheckpoint(
        std::string_view label,
        const SHR::HeartRateSimulation &sim,
        const Checkpoint &expected
    )
    {
        constexpr float Tolerance = 1.0e-4F;

        const SHR::PhysiologySnapshot snapshot = sim.GetSnapshot();

        INFO(label);
        CHECK_THAT(snapshot.HeartRate,        Catch::Matchers::WithinAbs(expected.HeartRate,       Tolerance));
        CHECK_THAT(snapshot.FastHeartRate,    Catch::Matchers::WithinAbs(expected.FastHeartRate,   Tolerance));
        CHECK_THAT(snapshot.Exertion,         Catch::Matchers::WithinAbs(expected.Exertion,        Tolerance));
        CHECK_THAT(snapshot.Adrenaline,       Catch::Matchers::WithinAbs(expected.Adrenaline,      Tolerance));
        CHECK_THAT(snapshot.Contractility,    Catch::Matchers::WithinAbs(expected.Contractility,   Tolerance));
        CHECK_THAT(snapshot.Fitness,          Catch::Matchers::WithinAbs(expected.Fitness,         Tolerance));
        CHECK_THAT(snapshot.AcuteFatigue,     Catch::Matchers::WithinAbs(expected.AcuteFatigue,    Tolerance));
        CHECK_THAT(snapshot.LongTermFatigue,  Catch::Matchers::WithinAbs(expected.LongTermFatigue, Tolerance));
        CHECK_THAT(snapshot.RespirationRate,  Catch::Matchers::WithinAbs(expected.RespRate,        Tolerance));
        CHECK_THAT(snapshot.RespirationDepth, Catch::Matchers::WithinAbs(expected.RespDepth,       Tolerance));
        CHECK_THAT(snapshot.RespirationPhase, Catch::Matchers::WithinAbs(expected.RespPhase,       Tolerance));
    }
}

TEST_CASE("Simulation trajectory matches the extraction baseline", "[simulation][characterization]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    RunSteps(sim, SHR::PlayerState{ }, 300);
    RequireCheckpoint("rest", sim, {
        .HeartRate       = 55.0F,
        .FastHeartRate   = 33.0F,
        .Exertion        = 1.5F,
        .Adrenaline      = 0.0F,
        .Contractility   = 0.0F,
        .Fitness         = 15.0F,
        .AcuteFatigue    = 0.0F,
        .LongTermFatigue = 0.0F,
        .RespRate        = 14.0F,
        .RespDepth       = 0.0F,
        .RespPhase       = 0.999997497F,
    });

    sim.NotifyCombatEntry();
    SHR::PlayerState sprinting;
    sprinting.IsSprinting = true;
    RunSteps(sim, sprinting, 900);
    RequireCheckpoint("exercise", sim, {
        .HeartRate       = 186.549164F,
        .FastHeartRate   = 119.849915F,
        .Exertion        = 14.8729782F,
        .Adrenaline      = 1.78381789F,
        .Contractility   = 0.988891542F,
        .Fitness         = 15.0F,
        .AcuteFatigue    = 0.127159968F,
        .LongTermFatigue = 0.0F,
        .RespRate        = 49.8950386F,
        .RespDepth       = 0.997262478F,
        .RespPhase       = 0.552590549F,
    });

    RunSteps(sim, SHR::PlayerState{ }, 1200);
    RequireCheckpoint("recovery", sim, {
        .HeartRate       = 94.1870117F,
        .FastHeartRate   = 45.5919418F,
        .Exertion        = 2.39191484F,
        .Adrenaline      = 0.891914785F,
        .Contractility   = 0.687253177F,
        .Fitness         = 15.0F,
        .AcuteFatigue    = 0.138214484F,
        .LongTermFatigue = 0.0F,
        .RespRate        = 15.9723520F,
        .RespDepth       = 0.232665807F,
        .RespPhase       = 0.350518167F,
    });
}

TEST_CASE("Simulation time skips match the extraction baseline", "[simulation][characterization]")
{
    namespace C = SHR::Constants;

    SHR::PlayerState sprinting;
    sprinting.IsSprinting = true;

    SECTION("sleep")
    {
        SHR::HeartRateSimulation sim(DefaultSimulationSettings);
        sim.Init();
        sim.NotifyCombatEntry();
        RunSteps(sim, sprinting, 600);

        sim.NotifySleep(8.0F * C::SecondsPerHour);
        sim.Step(SHR::PlayerState{ }, 0.1F, 8.0F);
        RequireCheckpoint("sleep", sim, {
            .HeartRate       = 46.7923393F,
            .FastHeartRate   = 28.0866566F,
            .Exertion        = 1.5F,
            .Adrenaline      = 0.0F,
            .Contractility   = 0.0F,
            .Fitness         = 14.9053268F,
            .AcuteFatigue    = 0.0F,
            .LongTermFatigue = 0.0F,
            .RespRate        = 9.0332222F,
            .RespDepth       = 0.0F,
            .RespPhase       = 0.735814571F,
        });
    }

    SECTION("fast travel")
    {
        SHR::HeartRateSimulation sim(DefaultSimulationSettings);
        sim.Init();
        sim.NotifyCombatEntry();
        RunSteps(sim, sprinting, 600);

        sim.NotifyFastTravel(C::SecondsPerHour);
        sim.Step(SHR::PlayerState{ }, 0.1F, 1.0F);
        RequireCheckpoint("fast-travel", sim, {
            .HeartRate       = 176.571960F,
            .FastHeartRate   = 118.518059F,
            .Exertion        = 3.0F,
            .Adrenaline      = 1.97449901e-9F,
            .Contractility   = 0.111816563F,
            .Fitness         = 15.0005188F,
            .AcuteFatigue    = 0.0851814600F,
            .LongTermFatigue = 0.000489540000F,
            .RespRate        = 15.7890654F,
            .RespDepth       = 0.126725391F,
            .RespPhase       = 0.091134444F,
        });
    }
}
