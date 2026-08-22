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

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    namespace C = SHR::Constants;
    constexpr SHR::SimulationSettings DefaultSimulationSettings;

    // Default step size approximates 30 FPS. gameHoursPerStep is used in fitness modeling.
    void RunFor(
        SHR::HeartRateSimulation &sim,
        SHR::PlayerState state,
        float seconds,
        float stepSize = 0.0333F,
        float gameHoursPerStep = 0.0F
    )
    {
        for (float elapsed = 0.0F; elapsed < seconds; elapsed += stepSize)
        {
            sim.Step(state, stepSize, gameHoursPerStep);
        }
    }

    // Helper function to derive fitness from resting HR.
    float StartingFitness()
    {
        return C::FitnessBaseMets +
            (C::BaseRestingHR - DefaultSimulationSettings.RestingHeartRate) / C::RestingHRSlope;
    }

    // Recreate the legacy co-save defaults used by the pre-SimulationState test setup. Production
    // compatibility translation belongs to the SKSE adapter; these tests use the helper only to keep
    // their existing physiological starting conditions legible.
    float AcuteFatigueMax(const SHR::HeartRateSimulation &sim)
    {
        return C::AcuteFatigueMaxFraction * sim.GetSnapshot().Fitness;
    }

    float LongTermFatigueMax(const SHR::HeartRateSimulation &sim)
    {
        return C::LongTermFatigueMaxFraction * sim.GetSnapshot().Fitness;
    }

    void RestoreLegacyState(
        SHR::HeartRateSimulation &sim,
        float heartRate,
        float exertion,
        float adrenaline = 0.0F,
        float fitness = 0.0F,
        float acuteFatigue = 0.0F,
        float longTermFatigue = 0.0F,
        float fastHeartRate = 0.0F,
        float respirationRate = 0.0F,
        float contractility = -1.0F,
        float respirationDepth = -1.0F
    )
    {
        SHR::SimulationState state = sim.GetState();
        state.FastHeartRate = fastHeartRate > 0.0F
            ? fastHeartRate
            : C::HRFastFraction * heartRate;
        state.SlowHeartRate = heartRate - state.FastHeartRate;
        state.Exertion = exertion;
        state.Adrenaline = adrenaline;
        state.Fitness = fitness > 0.0F ? fitness : sim.CreateInitialState().Fitness;
        state.AcuteFatigue = acuteFatigue;
        state.LongTermFatigue = longTermFatigue;
        state.RespirationRate = respirationRate > 0.0F
            ? respirationRate
            : C::RestingRespRate;
        state.RespirationDepth = respirationDepth >= 0.0F
            ? std::clamp(respirationDepth, 0.0F, 1.0F)
            : 0.0F;
        state.RespirationPhase = 0.0F;
        state.Contractility = contractility >= 0.0F
            ? contractility
            : sim.ComputeEquilibriumContractility(state);
        sim.Restore(state);
    }
}

struct SimFixture
{
    SimFixture() :
        sim(DefaultSimulationSettings)
    {
        sim.Init();
    }

    SHR::HeartRateSimulation sim;
};

TEST_CASE_METHOD(SimFixture, "Idle HR converges to resting", "[simulation]")
{
    const float resting = DefaultSimulationSettings.RestingHeartRate;

    RunFor(sim, SHR::PlayerState{ }, 300.0F);

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(resting, 1.0F));
}

TEST_CASE_METHOD(SimFixture, "Sprinting raises HR above resting", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);

    REQUIRE(sim.GetSnapshot().HeartRate > DefaultSimulationSettings.RestingHeartRate + 10.0F);
}

TEST_CASE_METHOD(SimFixture, "HR recovers toward resting after exercise", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);
    const float peakHR = sim.GetSnapshot().HeartRate;

    RunFor(sim, SHR::PlayerState{ }, 300.0F);

    REQUIRE(sim.GetSnapshot().HeartRate < peakHR - 10.0F);
}

TEST_CASE_METHOD(SimFixture, "Combat entry raises HR above idle baseline", "[simulation]")
{
    RunFor(sim, SHR::PlayerState{ }, 300.0F);
    const float idleHR = sim.GetSnapshot().HeartRate;

    sim.NotifyCombatEntry();
    RunFor(sim, SHR::PlayerState{ }, 60.0F);

    REQUIRE(sim.GetSnapshot().HeartRate > idleHR + 1.0F);
}

TEST_CASE_METHOD(SimFixture, "Jump impulse raises exertion immediately", "[simulation]")
{
    RunFor(sim, SHR::PlayerState{ }, 300.0F);
    const float beforeJump = sim.GetSnapshot().Exertion;

    sim.NotifyJump();
    sim.Step(SHR::PlayerState{ }, 0.1F);

    REQUIRE(sim.GetSnapshot().Exertion > beforeJump);
}

TEST_CASE_METHOD(SimFixture, "Death sets target HR to zero and tracks duration", "[simulation]")
{
    REQUIRE_FALSE(sim.GetSnapshot().DeathSeconds.has_value());

    SHR::PlayerState dead = { };
    dead.IsDead = true;

    sim.Step(dead, 1.0F);
    REQUIRE(sim.GetSnapshot().DeathSeconds.has_value());
    REQUIRE_THAT(*sim.GetSnapshot().DeathSeconds, Catch::Matchers::WithinAbs(1.0F, 0.001F));

    sim.Step(dead, 1.0F);
    REQUIRE_THAT(*sim.GetSnapshot().DeathSeconds, Catch::Matchers::WithinAbs(2.0F, 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Resurrection clears death seconds", "[simulation]")
{
    SHR::PlayerState dead = { };
    dead.IsDead = true;
    RunFor(sim, dead, 5.0F);
    REQUIRE(sim.GetSnapshot().DeathSeconds.has_value());

    sim.Step(SHR::PlayerState{ }, 1.0F);
    REQUIRE_FALSE(sim.GetSnapshot().DeathSeconds.has_value());
}

TEST_CASE_METHOD(SimFixture, "Sleep resets exertion and drops HR to sleep value", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 30.0F);
    REQUIRE(sim.GetSnapshot().Exertion > C::IdleMets + 1.0F);

    // By construction, ComputeTargetHeartRate(Idle) == config.HeartRate.Resting.
    const float expectedSleepHR = DefaultSimulationSettings.RestingHeartRate * C::SleepFraction;
    sim.NotifySleep(8.0F * 60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE_THAT(sim.GetSnapshot().Exertion, Catch::Matchers::WithinAbs(C::IdleMets, 0.01F));
    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(expectedSleepHR, 0.05F));
}

TEST_CASE_METHOD(SimFixture, "HR rises from sleep HR to resting after waking", "[simulation]")
{
    const float expectedSleepHR = DefaultSimulationSettings.RestingHeartRate * C::SleepFraction;
    sim.NotifySleep(8.0F * 60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.033F);
    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(expectedSleepHR, 0.05F));

    // After waking and idling, HR converges back to resting.
    RunFor(sim, SHR::PlayerState{ }, 500.0F);

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(DefaultSimulationSettings.RestingHeartRate, 1.0F));
}

TEST_CASE_METHOD(SimFixture, "Fast travel from high exertion reduces exertion", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);
    const float preTravelExertion = sim.GetSnapshot().Exertion;

    sim.NotifyFastTravel(60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.1F);

    REQUIRE(sim.GetSnapshot().Exertion < preTravelExertion);
}

TEST_CASE_METHOD(SimFixture, "Sleep decays lingering contractility across the slept duration", "[simulation]")
{
    // Build up contractility with exertion, then sleep: contractility must decay toward its idle
    // target over the slept duration, not linger near its exercise peak (finding A in the sim audit).
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);
    REQUIRE(sim.GetSnapshot().Contractility > 0.8F);

    sim.NotifySleep(8.0F * 60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE(sim.GetSnapshot().Contractility < 0.05F);
}

TEST_CASE_METHOD(SimFixture, "Fast travel decays lingering contractility toward walking effort", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);
    REQUIRE(sim.GetSnapshot().Contractility > 0.8F);

    // An hour of (assumed-walking) travel is many decay time constants: contractility falls far
    // from its sprint peak toward the modest walking-effort target, rather than advancing only one
    // frame.
    sim.NotifyFastTravel(60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.1F);

    REQUIRE(sim.GetSnapshot().Contractility < 0.3F);
}

TEST_CASE_METHOD(SimFixture, "A nap decays acute fatigue rather than clearing it", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, C::AcuteFatigueGainTau, 0.5F);
    const float earned = sim.GetSnapshot().AcuteFatigue;
    REQUIRE(earned > 0.5F);

    // One hour is one AcuteFatigueDecayTau, so a third of the fatigue survives the shortest sleep
    // the game offers. A full night is many time constants and does clear it.
    sim.NotifySleep(C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.1F);
    CHECK_THAT(
        sim.GetSnapshot().AcuteFatigue,
        Catch::Matchers::WithinRel(earned * std::exp(-1.0F), 0.02F)
    );

    sim.NotifySleep(8.0F * C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.1F);
    CHECK(sim.GetSnapshot().AcuteFatigue < 0.01F);
}

TEST_CASE_METHOD(SimFixture, "Waiting settles the acute state toward a stationary baseline", "[simulation]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    sim.NotifyCombatEntry();
    RunFor(sim, sprinting, 120.0F);
    REQUIRE(sim.GetSnapshot().Contractility > 0.8F);
    REQUIRE(sim.GetSnapshot().Adrenaline > 0.5F);

    // Four hours the character lived through, not the single frame the paused menu cost.
    sim.NotifyWait(4.0F * C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.1F);

    const SHR::PhysiologySnapshot settled = sim.GetSnapshot();
    CHECK_THAT(settled.Exertion, Catch::Matchers::WithinAbs(C::IdleMets, 0.01F));
    CHECK(settled.Adrenaline < 0.001F);
    CHECK(settled.Contractility < 0.01F);
    CHECK(settled.AcuteFatigue < 0.01F);
    CHECK_THAT(
        settled.HeartRate,
        Catch::Matchers::WithinAbs(DefaultSimulationSettings.RestingHeartRate, 1.0F)
    );
}

TEST_CASE_METHOD(SimFixture, "A time skip leaves the chronic states to the calendar", "[simulation]")
{
    RunFor(sim, SHR::PlayerState{ }, 1.0F);
    const SHR::PhysiologySnapshot before = sim.GetSnapshot();

    // Passing no game hours is what makes a double-credit visible: the chronic pair may move only
    // on the calendar delta, never on the skip.
    sim.NotifyWait(8.0F * C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.1F, 0.0F);

    const SHR::PhysiologySnapshot after = sim.GetSnapshot();
    CHECK_THAT(after.Fitness, Catch::Matchers::WithinAbs(before.Fitness, 1.0e-6F));
    CHECK_THAT(after.LongTermFatigue, Catch::Matchers::WithinAbs(before.LongTermFatigue, 1.0e-6F));
}

TEST_CASE_METHOD(SimFixture, "Restore sets HR and exertion for co-save loading", "[simulation]")
{
    RestoreLegacyState(sim, 72.0F, 3.5F);

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(72.0F, 0.001F));
    REQUIRE_THAT(sim.GetSnapshot().Exertion, Catch::Matchers::WithinAbs(3.5F, 0.001F));
    // Without explicit fastHR, the components are split proportionally.
    REQUIRE_THAT(sim.GetSnapshot().FastHeartRate, Catch::Matchers::WithinAbs(C::HRFastFraction * 72.0F, 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Restore with explicit fast component preserves bi-exponential state", "[simulation]")
{
    const float totalHR = 90.0F;
    const float fastHR = 60.0F;
    RestoreLegacyState(sim, totalHR, C::IdleMets, 0.0F, 0.0F, 0.0F, 0.0F, fastHR);

    REQUIRE_THAT(sim.GetSnapshot().FastHeartRate, Catch::Matchers::WithinAbs(fastHR,  0.001F));
    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(totalHR, 0.001F));
}

// --- Physiological magnitude tests ---
// These verify that the default config produces HR curves consistent with the
// underlying physiology model, not just directionally correct behavior.

TEST_CASE_METHOD(SimFixture, "Sustained sprint converges to Max HR", "[simulation][physiology]")
{
    const float maxHR = DefaultSimulationSettings.MaximumHeartRate;

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    // 6 time constants of the slow component (the bottleneck) gives >99.7%
    // convergence. The extra tau accounts for exertion ramp-up.
    RunFor(sim, sprinting, 6.0F * C::SlowOnsetTau);

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(maxHR, 1.0F));
}

TEST_CASE_METHOD(SimFixture, "Exertion reaches effective fitness at the configured accumulation rate", "[simulation][physiology]")
{
    const float startFitness = StartingFitness();

    // From idle to startFitness at AccumulationRate METs/s, using 1s steps.
    const auto expectedSteps = static_cast<int>(
        std::ceil((startFitness - C::IdleMets) / C::ExertionAccumulationRate)
    );

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    for (int i = 0; i < expectedSteps; ++i)
    {
        sim.Step(sprinting, 1.0F);
    }

    // Exertion is capped at effective fitness, which may be slightly below raw fitness due to acute
    // fatigue accumulated over the steps. Check against the actual ceiling.
    REQUIRE_THAT(sim.GetSnapshot().Exertion, Catch::Matchers::WithinAbs(sim.GetSnapshot().EffectiveFitness, 0.01F));
}

TEST_CASE("Fast HR component reaches 63% convergence after one FastOnsetTau", "[simulation][physiology]")
{
    auto settings = DefaultSimulationSettings;
    settings.RestingHeartRate = C::BaseRestingHR;
    SHR::HeartRateSimulation sim(settings);
    sim.Init();

    const float maxHR = settings.MaximumHeartRate;

    // Sedentary fitness (FitnessBaseMets), which means normFitness = 0, and
    // thus FastOnsetTau should be FastOnsetTauSedentary.
    // Exertion pinned to the fitness ceiling so target is MaxHR from t=0.
    RestoreLegacyState(sim, C::BaseRestingHR, C::FitnessBaseMets, 0.0F, C::FitnessBaseMets);

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, C::FastOnsetTauSedentary);

    const float fastTarget = C::HRFastFraction * maxHR;
    const float fastInitial = C::HRFastFraction * C::BaseRestingHR;
    const float expected = fastInitial + (fastTarget - fastInitial) * (1.0F - std::exp(-1.0F));

    REQUIRE_THAT(sim.GetSnapshot().FastHeartRate, Catch::Matchers::WithinAbs(expected, 1.0F));
}

TEST_CASE_METHOD(SimFixture, "Slow HR component reaches 63% convergence after one SlowOnsetTau", "[simulation][physiology]")
{
    const float resting = DefaultSimulationSettings.RestingHeartRate;
    const float maxHR   = DefaultSimulationSettings.MaximumHeartRate;

    // Seed at resting with exertion already at the fitness ceiling so target is
    // Max from t=0.
    RestoreLegacyState(sim, resting, StartingFitness());

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, C::SlowOnsetTau);

    const float slowTarget  = (1.0F - C::HRFastFraction) * maxHR;
    const float slowInitial = (1.0F - C::HRFastFraction) * resting;
    const float slowHR = sim.GetSnapshot().HeartRate - sim.GetSnapshot().FastHeartRate;
    const float expected = slowInitial + (slowTarget - slowInitial) * (1.0F - std::exp(-1.0F));

    REQUIRE_THAT(slowHR, Catch::Matchers::WithinAbs(expected, 1.0F));
}

TEST_CASE("Fast component decays 63% toward resting in one FastRecoveryTau", "[simulation][physiology]")
{
    auto settings = DefaultSimulationSettings;
    settings.RestingHeartRate = C::BaseRestingHR;
    SHR::HeartRateSimulation sim(settings);
    sim.Init();

    const float maxHR = settings.MaximumHeartRate;
    const float fastPeak = C::HRFastFraction * maxHR;

    // Sedentary fitness impies normFitness = 0, which implies
    // FastRecoveryTauSedentary.
    RestoreLegacyState(sim, maxHR, C::IdleMets, 0.0F, C::FitnessBaseMets, 0.0F, 0.0F, fastPeak);
    RunFor(sim, SHR::PlayerState{ }, C::FastRecoveryTauSedentary);

    const float fastTarget = C::HRFastFraction * C::BaseRestingHR;
    const float expected = fastTarget + (fastPeak - fastTarget) * std::exp(-1.0F);

    REQUIRE_THAT(sim.GetSnapshot().FastHeartRate, Catch::Matchers::WithinAbs(expected, 1.0F));
}

TEST_CASE("Slow component retains most of its value after one FastRecoveryTau", "[simulation][physiology]")
{
    auto settings = DefaultSimulationSettings;
    settings.RestingHeartRate = C::BaseRestingHR;
    SHR::HeartRateSimulation sim(settings);
    sim.Init();

    const float maxHR = settings.MaximumHeartRate;
    const float fastPeak = C::HRFastFraction * maxHR;
    const float slowPeak = (1.0F - C::HRFastFraction) * maxHR;

    RestoreLegacyState(sim, maxHR, C::IdleMets, 0.0F, C::FitnessBaseMets, 0.0F, 0.0F, fastPeak);
    RunFor(sim, SHR::PlayerState{ }, C::FastRecoveryTauSedentary);

    const float slowTarget = (1.0F - C::HRFastFraction) * C::BaseRestingHR;
    const float slowHR = sim.GetSnapshot().HeartRate - sim.GetSnapshot().FastHeartRate;
    const float expected = slowTarget + (slowPeak - slowTarget) * std::exp(-C::FastRecoveryTauSedentary / C::SlowRecoveryTau);

    REQUIRE_THAT(slowHR, Catch::Matchers::WithinAbs(expected, 1.0F));
}

TEST_CASE("Fit character's fast HR recovers more than sedentary from same peak HR", "[simulation][physiology]")
{
    const float maxHR = DefaultSimulationSettings.MaximumHeartRate;
    const float fastPeak = C::HRFastFraction * maxHR;

    // Both start at the same fast peak; athlete has shorter tau AND lower resting target.
    SHR::HeartRateSimulation sedentary(DefaultSimulationSettings);
    RestoreLegacyState(sedentary, maxHR, C::IdleMets, 0.0F, C::FitnessBaseMets, 0.0F, 0.0F, fastPeak);

    SHR::HeartRateSimulation athlete(DefaultSimulationSettings);
    RestoreLegacyState(athlete, maxHR, C::IdleMets, 0.0F, DefaultSimulationSettings.FitnessMaxMets, 0.0F, 0.0F, fastPeak);

    RunFor(sedentary, SHR::PlayerState{ }, C::FastRecoveryTauSedentary);
    RunFor(athlete,   SHR::PlayerState{ }, C::FastRecoveryTauSedentary);

    REQUIRE(athlete.GetSnapshot().FastHeartRate < sedentary.GetSnapshot().FastHeartRate);
}

// --- Adrenaline tests ---

TEST_CASE_METHOD(SimFixture, "Combat entry spikes adrenaline immediately", "[simulation][adrenaline]")
{
    REQUIRE_THAT(sim.GetSnapshot().Adrenaline, Catch::Matchers::WithinAbs(0.0F, 0.001F));

    sim.NotifyCombatEntry();

    REQUIRE(sim.GetSnapshot().Adrenaline > 0.0F);
}

TEST_CASE_METHOD(SimFixture, "Hit stacks adrenaline on top of combat entry", "[simulation][adrenaline]")
{
    sim.NotifyCombatEntry();
    const float afterCombat = sim.GetSnapshot().Adrenaline;

    sim.NotifyHit();

    REQUIRE(sim.GetSnapshot().Adrenaline > afterCombat);
}

TEST_CASE_METHOD(SimFixture, "Adrenaline decays by half after one half-life", "[simulation][adrenaline]")
{
    sim.NotifyCombatEntry();
    const float initial = sim.GetSnapshot().Adrenaline;

    RunFor(sim, SHR::PlayerState{ }, C::AdrenalineHalfLife);

    REQUIRE_THAT(sim.GetSnapshot().Adrenaline, Catch::Matchers::WithinAbs(initial * 0.5F, 0.05F));
}

TEST_CASE_METHOD(SimFixture, "Stacked adrenaline cannot drive exertion above fitness ceiling", "[simulation][adrenaline]")
{
    // Stack enough hits to push idle + adrenaline well above fitness ceiling.
    for (int i = 0; i < 20; ++i)
    {
        sim.NotifyHit();
    }
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE(sim.GetSnapshot().Exertion <= sim.GetSnapshot().Fitness + 0.01F);
}

TEST_CASE_METHOD(SimFixture, "Sleep clears adrenaline", "[simulation][adrenaline]")
{
    sim.NotifyCombatEntry();
    REQUIRE(sim.GetSnapshot().Adrenaline > 0.0F);

    sim.NotifySleep(8.0F * 60.0F * 60.0F);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE_THAT(sim.GetSnapshot().Adrenaline, Catch::Matchers::WithinAbs(0.0F, 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Fast travel clears adrenaline over elapsed duration", "[simulation][adrenaline]")
{
    sim.NotifyCombatEntry();
    const float initial = sim.GetSnapshot().Adrenaline;

    // 1 hour (3600s) is much longer than two 2-minute half-lives so we should expect adrenaline to be nearly gone.
    sim.NotifyFastTravel(3600.0F);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE(sim.GetSnapshot().Adrenaline < initial * 0.01F);
}

TEST_CASE_METHOD(SimFixture, "Restore preserves adrenaline from co-save", "[simulation][adrenaline]")
{
    const float savedAdrenaline = 1.5F;
    RestoreLegacyState(sim, 72.0F, 3.5F, savedAdrenaline);

    REQUIRE_THAT(sim.GetSnapshot().Adrenaline, Catch::Matchers::WithinAbs(savedAdrenaline, 0.001F));
}

// --- Long-term stamina (fitness) tests ---

TEST_CASE("High resting HR setting initializes at that HR without clamping to BaseRestingHR", "[simulation][fitness]")
{
    auto settings = DefaultSimulationSettings;
    settings.RestingHeartRate = C::MaxRestingHR;

    SHR::HeartRateSimulation sim(settings);
    sim.Init();

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(C::MaxRestingHR, 0.001F));
}

TEST_CASE("Simulation settings are instance-local", "[simulation][settings]")
{
    auto customSettings = DefaultSimulationSettings;
    customSettings.RestingHeartRate = 65.0F;
    customSettings.MaximumHeartRate = 175.0F;

    SHR::HeartRateSimulation defaultSim(DefaultSimulationSettings);
    SHR::HeartRateSimulation customSim(customSettings);
    defaultSim.Init();
    customSim.Init();

    REQUIRE_THAT(
        defaultSim.GetSnapshot().HeartRate,
        Catch::Matchers::WithinAbs(DefaultSimulationSettings.RestingHeartRate, 0.001F)
    );
    REQUIRE_THAT(
        customSim.GetSnapshot().HeartRate,
        Catch::Matchers::WithinAbs(customSettings.RestingHeartRate, 0.001F)
    );

    SHR::PlayerState sprinting;
    sprinting.IsSprinting = true;
    RunFor(defaultSim, sprinting, 600.0F);
    RunFor(customSim, sprinting, 600.0F);

    REQUIRE_THAT(
        defaultSim.GetSnapshot().HeartRate,
        Catch::Matchers::WithinAbs(DefaultSimulationSettings.MaximumHeartRate, 1.0F)
    );
    REQUIRE_THAT(
        customSim.GetSnapshot().HeartRate,
        Catch::Matchers::WithinAbs(customSettings.MaximumHeartRate, 1.0F)
    );
}

TEST_CASE_METHOD(SimFixture, "Fitness initializes from resting heart rate", "[simulation][fitness]")
{
    REQUIRE_THAT(sim.GetSnapshot().Fitness, Catch::Matchers::WithinAbs(StartingFitness(), 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Fitness rises ~63% toward max after one gain tau of training", "[simulation][fitness]")
{
    const float initial = sim.GetSnapshot().Fitness;

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    sim.Step(sprinting, 0.033F, C::FitnessGainTau);

    const float expected = initial + (1.0F - std::exp(-1.0F)) * (DefaultSimulationSettings.FitnessMaxMets - initial);
    REQUIRE_THAT(sim.GetSnapshot().Fitness, Catch::Matchers::WithinAbs(expected, 0.01F));
}

TEST_CASE_METHOD(SimFixture, "Fitness decays ~63% toward base after one decay tau of inactivity", "[simulation][fitness]")
{
    RestoreLegacyState(sim, DefaultSimulationSettings.RestingHeartRate, C::IdleMets, 0.0F, DefaultSimulationSettings.FitnessMaxMets);

    const float initial = sim.GetSnapshot().Fitness;

    sim.Step(SHR::PlayerState{ }, 0.033F, C::FitnessDecayTau);

    const float expected = initial + (1.0F - std::exp(-1.0F)) * (C::FitnessBaseMets - initial);
    REQUIRE_THAT(sim.GetSnapshot().Fitness, Catch::Matchers::WithinAbs(expected, 0.01F));
}

TEST_CASE_METHOD(SimFixture, "Higher fitness lowers steady-state resting HR", "[simulation][fitness]")
{
    RunFor(sim, SHR::PlayerState{ }, 300.0F);
    const float baseRestingHR = sim.GetSnapshot().HeartRate;

    SHR::HeartRateSimulation simFit(DefaultSimulationSettings);
    simFit.Init();
    RestoreLegacyState(simFit, DefaultSimulationSettings.RestingHeartRate, C::IdleMets, 0.0F, DefaultSimulationSettings.FitnessMaxMets);
    RunFor(simFit, SHR::PlayerState{ }, 300.0F);
    const float fitRestingHR = simFit.GetSnapshot().HeartRate;

    REQUIRE(fitRestingHR < baseRestingHR - 1.0F);
}

TEST_CASE_METHOD(SimFixture, "Resting HR at max fitness matches slope formula", "[simulation][fitness][physiology]")
{
    // Seed at max fitness; let HR converge to its new resting point at idle.
    RestoreLegacyState(sim, DefaultSimulationSettings.RestingHeartRate, C::IdleMets, 0.0F, DefaultSimulationSettings.FitnessMaxMets);
    RunFor(sim, SHR::PlayerState{ }, 800.0F);

    const float expected = C::BaseRestingHR - (DefaultSimulationSettings.FitnessMaxMets - C::FitnessBaseMets) * C::RestingHRSlope;

    REQUIRE_THAT(sim.GetSnapshot().HeartRate, Catch::Matchers::WithinAbs(expected, 1.0F));
}

TEST_CASE_METHOD(SimFixture, "Restore preserves fitness from co-save", "[simulation][fitness]")
{
    const float savedFitness = 17.0F;
    RestoreLegacyState(sim, 55.0F, 1.5F, 0.0F, savedFitness);

    REQUIRE_THAT(sim.GetSnapshot().Fitness, Catch::Matchers::WithinAbs(savedFitness, 0.001F));
}

// --- Acute fatigue tests ---

TEST_CASE_METHOD(SimFixture, "Acute fatigue accumulates during sustained exertion", "[simulation][fatigue]")
{
    REQUIRE_THAT(sim.GetSnapshot().AcuteFatigue, Catch::Matchers::WithinAbs(0.0F, 0.001F));

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 3.0F * C::AcuteFatigueGainTau);

    // The system is self-limiting: as fatigue rises it reduces effective-fitness snapshot, which
    // lowers the exertion cap and thus the fatigue target. The equilibrium is roughly
    // (fitness - idle) / (fitness - idle + AcuteFatigueMax) * max, ~73% for default config. After
    // 3 tau we should be well above 50% and close to that ceiling.
    REQUIRE(sim.GetSnapshot().AcuteFatigue > AcuteFatigueMax(sim) * 0.5F);
    REQUIRE(sim.GetSnapshot().AcuteFatigue < AcuteFatigueMax(sim));
}

TEST_CASE_METHOD(SimFixture, "Acute fatigue decays toward zero at rest", "[simulation][fatigue]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 3.0F * C::AcuteFatigueGainTau);
    REQUIRE(sim.GetSnapshot().AcuteFatigue > AcuteFatigueMax(sim) * 0.5F);

    RunFor(sim, SHR::PlayerState{ }, 3.0F * C::AcuteFatigueDecayTau);

    REQUIRE(sim.GetSnapshot().AcuteFatigue < AcuteFatigueMax(sim) * 0.1F);
}

TEST_CASE_METHOD(SimFixture, "Sleep clears acute fatigue", "[simulation][fatigue]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 600.0F);
    REQUIRE(sim.GetSnapshot().AcuteFatigue > 0.0F);

    sim.NotifySleep(8.0F * C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    REQUIRE_THAT(sim.GetSnapshot().AcuteFatigue, Catch::Matchers::WithinAbs(0.0F, 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Acute fatigue reduces effective fitness below raw fitness", "[simulation][fatigue]")
{
    const float rawFitness = sim.GetSnapshot().Fitness;

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 3.0F * C::AcuteFatigueGainTau);

    REQUIRE(sim.GetSnapshot().EffectiveFitness < rawFitness - 0.5F);
}

TEST_CASE_METHOD(SimFixture, "Fatigued exertion cap is lower than rested exertion cap", "[simulation][fatigue]")
{
    // Run to exhaustion from rest.
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 3.0F * C::AcuteFatigueGainTau);
    const float fatiguedExertionCap = sim.GetSnapshot().EffectiveFitness;

    // Compare against a fresh sim at the same fitness level.
    SHR::HeartRateSimulation fresh(DefaultSimulationSettings);
    fresh.Init();
    REQUIRE(fatiguedExertionCap < fresh.GetSnapshot().EffectiveFitness - 0.5F);
}

// --- Long-term fatigue tests ---

TEST_CASE_METHOD(SimFixture, "Long-term fatigue builds from sustained acute fatigue", "[simulation][fatigue]")
{
    REQUIRE_THAT(sim.GetSnapshot().LongTermFatigue, Catch::Matchers::WithinAbs(0.0F, 0.001F));

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;

    // Run at max exertion for several game-hour steps to drive long-term fatigue gain. Using large
    // gameHoursDelta to compress time.
    for (int i = 0; i < 20; ++i)
    {
        sim.Step(sprinting, 1.0F, C::LongTermFatigueGainTau / 10.0F);
    }

    REQUIRE(sim.GetSnapshot().LongTermFatigue > 0.0F);
}

TEST_CASE_METHOD(SimFixture, "Sleep partially clears long-term fatigue proportional to duration", "[simulation][fatigue]")
{
    // Seed with max long-term fatigue.
    RestoreLegacyState(sim, 55.0F, C::IdleMets, 0.0F, 0.0F, 0.0F, LongTermFatigueMax(sim));
    const float initial = sim.GetSnapshot().LongTermFatigue;

    // 8 game-hours of sleep should clear ~55% (rate = 0.099/h, 1 - exp(-8*0.099)
    // ~= 0.55).
    sim.NotifySleep(8.0F * C::SecondsPerHour);
    sim.Step(SHR::PlayerState{ }, 0.033F);

    const float remaining = sim.GetSnapshot().LongTermFatigue;
    REQUIRE(remaining < initial * 0.5F);   // more than half cleared
    REQUIRE(remaining > 0.0F);             // not fully cleared
}

TEST_CASE_METHOD(SimFixture, "Long-term fatigue reduces effective fitness", "[simulation][fatigue]")
{
    const float rawFitness = sim.GetSnapshot().Fitness;
    RestoreLegacyState(
        sim, 55.0F, C::IdleMets, 0.0F, rawFitness, 0.0F,
        C::LongTermFatigueMaxFraction * rawFitness
    );

    REQUIRE(sim.GetSnapshot().EffectiveFitness < rawFitness - 0.5F);
}

// --- Overtraining tests ---

TEST_CASE_METHOD(SimFixture, "Training with maximum fatigue produces no fitness gain", "[simulation][fatigue]")
{
    // Seed at base fitness with both fatigue scalars maxed out.
    const float baseFitness = C::FitnessBaseMets;
    RestoreLegacyState(
        sim,
        DefaultSimulationSettings.RestingHeartRate,
        C::IdleMets,
        0.0F,
        baseFitness,
        C::AcuteFatigueMaxFraction * baseFitness,
        C::LongTermFatigueMaxFraction * baseFitness
    );

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    // One large training step: this would normally produce substantial fitness gains.
    sim.Step(sprinting, 1.0F, C::FitnessGainTau);

    // With full fatigue (trainingEfficacy = 0), fitness should be unchanged.
    REQUIRE_THAT(sim.GetSnapshot().Fitness, Catch::Matchers::WithinAbs(baseFitness, 0.01F));
}

TEST_CASE_METHOD(SimFixture, "Training with partial fatigue produces reduced fitness gain", "[simulation][fatigue]")
{
    const float startFitness = sim.GetSnapshot().Fitness;

    // Half acute fatigue only - efficacy = 0.75 (average of 0.5 acute + 0 long-term).
    RestoreLegacyState(
        sim,
        DefaultSimulationSettings.RestingHeartRate,
        C::IdleMets,
        0.0F,
        startFitness,
        C::AcuteFatigueMaxFraction * startFitness * 0.5F,
        0.0F
    );

    SHR::HeartRateSimulation fresh(DefaultSimulationSettings);
    fresh.Init();
    const float freshStartFitness = fresh.GetSnapshot().Fitness;

    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    const float dt = C::FitnessGainTau / 10.0F;
    sim.Step(sprinting, 1.0F, dt);
    fresh.Step(sprinting, 1.0F, dt);

    const float fatiguedGain = sim.GetSnapshot().Fitness - startFitness;
    const float freshGain = fresh.GetSnapshot().Fitness - freshStartFitness;
    REQUIRE(fatiguedGain < freshGain);
}

// --- Fatigue co-save tests ---

TEST_CASE_METHOD(SimFixture, "Restore preserves acute and long-term fatigue from co-save", "[simulation][fatigue]")
{
    const float savedAcute    = 2.5F;
    const float savedLongTerm = 1.2F;
    RestoreLegacyState(sim, 55.0F, C::IdleMets, 0.0F, 0.0F, savedAcute, savedLongTerm);

    REQUIRE_THAT(sim.GetSnapshot().AcuteFatigue,    Catch::Matchers::WithinAbs(savedAcute,    0.001F));
    REQUIRE_THAT(sim.GetSnapshot().LongTermFatigue, Catch::Matchers::WithinAbs(savedLongTerm, 0.001F));
}

// --- Respiratory rate tests ---

TEST_CASE("Ventilation knots produce convex RR and plateauing depth targets", "[simulation][respiration]")
{
    REQUIRE(C::VentilationVT1Fraction > 0.0F);
    REQUIRE(C::VentilationRCPFraction > C::VentilationVT1Fraction);
    REQUIRE(C::VentilationRCPFraction < 1.0F);
    REQUIRE(C::RestingRespRate < C::RespRateAtVT1);
    REQUIRE(C::RespRateAtVT1 < C::RespRateAtRCP);
    REQUIRE(C::RespRateAtRCP < C::MaxRespRate);
    REQUIRE(0.0F < C::RespDepthAtVT1);
    REQUIRE(C::RespDepthAtVT1 < C::RespDepthAtRCP);
    REQUIRE(C::RespDepthAtRCP < 1.0F);

    const float rrSlope1 = (C::RespRateAtVT1 - C::RestingRespRate) / C::VentilationVT1Fraction;
    const float rrSlope2 = (C::RespRateAtRCP - C::RespRateAtVT1) / (C::VentilationRCPFraction - C::VentilationVT1Fraction);
    const float rrSlope3 = (C::MaxRespRate - C::RespRateAtRCP) / (1.0F - C::VentilationRCPFraction);
    REQUIRE(rrSlope1 < rrSlope2);
    REQUIRE(rrSlope2 < rrSlope3);

    const float depthSlope1 = C::RespDepthAtVT1 / C::VentilationVT1Fraction;
    const float depthSlope2 = (C::RespDepthAtRCP - C::RespDepthAtVT1) / (C::VentilationRCPFraction - C::VentilationVT1Fraction);
    const float depthSlope3 = (1.0F - C::RespDepthAtRCP) / (1.0F - C::VentilationRCPFraction);
    REQUIRE(depthSlope1 > depthSlope2);
    REQUIRE(depthSlope2 > depthSlope3);

    const auto observeTarget = [](float fraction)
    {
        SHR::HeartRateSimulation sim(DefaultSimulationSettings);
        const float fitness = C::IdleMets + (C::SprintingMets - C::IdleMets) / fraction;
        RestoreLegacyState(
            sim,
            100.0F,
            C::SprintingMets,
            0.0F,
            fitness,
            0.0F,
            0.0F,
            60.0F,
            C::RestingRespRate,
            0.0F,
            0.0F
        );
        constexpr float delta = 0.001F;
        sim.Step(SHR::PlayerState{ .IsSprinting = true }, delta);
        const float rrGain = 1.0F - std::exp(-delta / C::RespOnsetTau);
        const float depthGain = 1.0F - std::exp(-delta / C::BreathDepthOnsetTau);
        const float inferredRR = C::RestingRespRate +
            (sim.GetSnapshot().RespirationRate - C::RestingRespRate) / rrGain;
        return std::pair{ inferredRR, sim.GetSnapshot().RespirationDepth / depthGain };
    };

    const auto [rrVT1, depthVT1] = observeTarget(C::VentilationVT1Fraction);
    REQUIRE_THAT(rrVT1, Catch::Matchers::WithinAbs(C::RespRateAtVT1, 0.02F));
    REQUIRE_THAT(depthVT1, Catch::Matchers::WithinAbs(C::RespDepthAtVT1, 0.002F));

    const auto [rrRCP, depthRCP] = observeTarget(C::VentilationRCPFraction);
    REQUIRE_THAT(rrRCP, Catch::Matchers::WithinAbs(C::RespRateAtRCP, 0.02F));
    REQUIRE_THAT(depthRCP, Catch::Matchers::WithinAbs(C::RespDepthAtRCP, 0.002F));
}

TEST_CASE("RespPhase advances at the correct rate during idle breathing", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    // At idle, target == RestingRespRate == current, so m_RespRate is stable.
    // Phase should advance by exactly RespRate/60 * delta per step.
    const float delta = 60.0F / C::RestingRespRate * 0.5F;  // half a breath cycle
    sim.Step(SHR::PlayerState{ }, delta);

    REQUIRE_THAT(sim.GetSnapshot().RespirationPhase, Catch::Matchers::WithinAbs(0.5F, 0.001F));
}

TEST_CASE("RespPhase wraps around after one full breath cycle", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    const float fullCycle = 60.0F / C::RestingRespRate;
    sim.Step(SHR::PlayerState{ }, fullCycle);

    REQUIRE_THAT(sim.GetSnapshot().RespirationPhase, Catch::Matchers::WithinAbs(0.0F, 0.001F));
}

TEST_CASE("RR rises above resting after sustained maximal exertion", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    SHR::PlayerState sprinting{ };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 5.0F * C::RespOnsetTau);

    REQUIRE(sim.GetSnapshot().RespirationRate > C::RestingRespRate * 1.5F);
}

TEST_CASE("RR converges 63% toward target in one onset tau from resting", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    const float maxHR = DefaultSimulationSettings.MaximumHeartRate;
    const float fastHR = C::HRFastFraction * maxHR;
    // Fitness == SprintingMets makes sprinting the aerobic ceiling. Acute fatigue can make the raw
    // fraction exceed one during the step, but the ventilation target remains clamped at maximum.
    RestoreLegacyState(sim, maxHR, C::SprintingMets, 0.0F, C::SprintingMets, 0.0F, 0.0F, fastHR, C::RestingRespRate);

    SHR::PlayerState sprinting{ };
    sprinting.IsSprinting = true;
    sim.Step(sprinting, C::RespOnsetTau);

    const float expected = C::RestingRespRate +
        (1.0F - std::exp(-1.0F)) * (C::MaxRespRate - C::RestingRespRate);
    REQUIRE_THAT(sim.GetSnapshot().RespirationRate, Catch::Matchers::WithinAbs(expected, 0.5F));
}

TEST_CASE("RR recovers 63% toward resting in one recovery tau after exertion", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    const float maxHR = DefaultSimulationSettings.MaximumHeartRate;
    const float fastHR = C::HRFastFraction * maxHR;
    // Start at MaxRespRate with idle exertion, target = RestingRespRate, RR should decay.
    RestoreLegacyState(sim, maxHR, C::IdleMets, 0.0F, C::FitnessBaseMets, 0.0F, 0.0F, fastHR, C::MaxRespRate);

    sim.Step(SHR::PlayerState{ }, C::RespRecoveryTau);

    const float expected = C::MaxRespRate + (1.0F - std::exp(-1.0F)) * (C::RestingRespRate - C::MaxRespRate);
    REQUIRE_THAT(sim.GetSnapshot().RespirationRate, Catch::Matchers::WithinAbs(expected, 0.5F));
}

TEST_CASE("Respiratory depth rises gradually on the onset time constant", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    RestoreLegacyState(
        sim,
        DefaultSimulationSettings.MaximumHeartRate,
        C::SprintingMets,
        0.0F,
        C::SprintingMets,
        0.0F,
        0.0F,
        C::HRFastFraction * DefaultSimulationSettings.MaximumHeartRate,
        C::RestingRespRate,
        0.0F,
        0.0F
    );
    sim.Step(SHR::PlayerState{ .IsSprinting = true }, C::BreathDepthOnsetTau);

    const float expected = 1.0F - std::exp(-1.0F);
    REQUIRE_THAT(sim.GetSnapshot().RespirationDepth, Catch::Matchers::WithinAbs(expected, 0.01F));
    REQUIRE(sim.GetSnapshot().RespirationDepth < 1.0F);
}

TEST_CASE_METHOD(SimFixture, "Respiratory depth lingers after exertion stops", "[simulation][respiration]")
{
    SHR::PlayerState sprinting{ };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);

    const float peakDepth = sim.GetSnapshot().RespirationDepth;
    REQUIRE(peakDepth > 0.8F);

    RunFor(sim, SHR::PlayerState{ }, 30.0F);

    REQUIRE_THAT(sim.GetSnapshot().Exertion, Catch::Matchers::WithinAbs(C::IdleMets, 0.5F));
    REQUIRE(sim.GetSnapshot().RespirationDepth > 0.5F);
    REQUIRE(sim.GetSnapshot().RespirationDepth < peakDepth);

    const float rrRecoveryFraction =
        (sim.GetSnapshot().RespirationRate - C::RestingRespRate) / (C::MaxRespRate - C::RestingRespRate);
    REQUIRE(sim.GetSnapshot().RespirationDepth > rrRecoveryFraction);
}

TEST_CASE("Restore preserves respiratory rate from co-save", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    const float savedRR = 28.5F;
    RestoreLegacyState(sim, 72.0F, C::IdleMets, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, savedRR);

    REQUIRE_THAT(sim.GetSnapshot().RespirationRate,  Catch::Matchers::WithinAbs(savedRR, 0.001F));
    REQUIRE_THAT(sim.GetSnapshot().RespirationPhase, Catch::Matchers::WithinAbs(0.0F,    0.001F));
}

TEST_CASE("Restore preserves respiratory depth and defaults old saves to rest", "[simulation][respiration]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    RestoreLegacyState(sim, 72.0F, C::IdleMets, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, C::RestingRespRate, -1.0F, 0.62F);
    REQUIRE_THAT(sim.GetSnapshot().RespirationDepth, Catch::Matchers::WithinAbs(0.62F, 0.001F));

    RestoreLegacyState(sim, 72.0F, C::IdleMets);
    REQUIRE_THAT(sim.GetSnapshot().RespirationDepth, Catch::Matchers::WithinAbs(0.0F, 0.001F));
}

TEST_CASE_METHOD(SimFixture, "Contractility rises during sustained exertion", "[simulation][contractility]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);

    // Exertion is capped at fitness, so normalized exertion -> 1; after several
    // onset taus contractility approaches its target.
    REQUIRE(sim.GetSnapshot().Contractility > 0.8F);
}

TEST_CASE_METHOD(SimFixture, "Contractility lingers after exertion stops (recovery hysteresis)", "[simulation][contractility]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 120.0F);
    const float peak = sim.GetSnapshot().Contractility;

    // Idle long enough for exertion to fall back to idle (fast) but well within
    // the contractility decay tau (~120s): the signal must still be elevated.
    RunFor(sim, SHR::PlayerState{ }, 30.0F);

    REQUIRE_THAT(sim.GetSnapshot().Exertion, Catch::Matchers::WithinAbs(C::IdleMets, 0.5F));
    REQUIRE(sim.GetSnapshot().Contractility > 0.4F);   // lingers despite exertion gone
    REQUIRE(sim.GetSnapshot().Contractility < peak);   // but has begun to decay
}

TEST_CASE_METHOD(SimFixture, "Idle contractility settles near zero", "[simulation][contractility]")
{
    RunFor(sim, SHR::PlayerState{ }, 300.0F);

    REQUIRE(sim.GetSnapshot().Contractility < 0.1F);
}

TEST_CASE("Restore preserves contractility from co-save", "[simulation][contractility]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    const float saved = 0.62F;
    RestoreLegacyState(sim, 160.0F, 8.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, saved);
    REQUIRE_THAT(sim.GetSnapshot().Contractility, Catch::Matchers::WithinAbs(saved, 0.001F));
}

TEST_CASE("Restore without contractility record re-seeds equilibrium", "[simulation][contractility]")
{
    SHR::HeartRateSimulation sim(DefaultSimulationSettings);
    sim.Init();

    // Idle drive -> equilibrium ~0. Omitting the arg uses the sentinel default.
    RestoreLegacyState(sim, 72.0F, C::IdleMets);
    REQUIRE(sim.GetSnapshot().Contractility < 0.05F);
}

TEST_CASE_METHOD(SimFixture, "Contractility excess is ~zero in steady state", "[simulation][contractility]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 180.0F);   // steady sprint: contractility and HR both maxed

    REQUIRE(sim.GetSnapshot().ContractilityExcess < 0.05F);
}

TEST_CASE_METHOD(SimFixture, "Contractility excess appears during recovery", "[simulation][contractility]")
{
    SHR::PlayerState sprinting = { };
    sprinting.IsSprinting = true;
    RunFor(sim, sprinting, 180.0F);
    RunFor(sim, SHR::PlayerState{ }, 25.0F);   // HR drops faster than contractility decays

    REQUIRE(sim.GetSnapshot().ContractilityExcess > 0.1F);
}
