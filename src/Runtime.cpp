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
#include "Runtime.hpp"

#include "AcousticMapper.hpp"
#include "Constants.hpp"
#include "HeartRate.hpp"

#include <algorithm>
#include <utility>

namespace
{
    namespace C = SHR::Constants;

    // Preserve the adapter's two-times-two-minute death-risk ramp.
    constexpr float DeathRiskRampSeconds = 4.0F * 60.0F;
}

SHR::Runtime::Runtime(RuntimeSettings settings)
    : m_Settings(settings)
    , m_Simulation(settings.Simulation)
{
}

SHR::Runtime::Runtime(RuntimeSettings settings, RhythmRandom random)
    : m_Settings(settings)
    , m_Simulation(settings.Simulation)
    , m_Rhythm(std::move(random))
{
}

void SHR::Runtime::Init()
{
    m_Simulation.Init();
    m_Rhythm.Init();
}

SHR::StepResult SHR::Runtime::Step(const StepInput &input)
{
    m_Simulation.Step(input.Player, input.DeltaSeconds, input.GameHoursDelta);
    const PhysiologySnapshot physiology = m_Simulation.GetSnapshot();

    StepResult result{
        .Physiology = physiology,
        .Beat       = std::nullopt,
    };
    if (!input.OutputEnabled)
    {
        return result;
    }

    const float deathFactor = physiology.DeathSeconds
        .transform([](float seconds) {
            return std::min(seconds, DeathRiskRampSeconds) / DeathRiskRampSeconds;
        })
        .value_or(0.0F);
    const float heartRateRange =
        m_Settings.Simulation.MaximumHeartRate - VeryHighHeartRateThreshold;
    const float extremeHeartRateFactor = std::clamp(
        (physiology.HeartRate - VeryHighHeartRateThreshold) / heartRateRange,
        0.0F,
        1.0F
    );
    const float fatigueFactor = physiology.LongTermFatigue / C::LongTermFatigueMax;
    const float riskFactor = std::max({
        deathFactor,
        extremeHeartRateFactor,
        fatigueFactor,
    });

    const float exertionFraction = std::clamp(
        (physiology.Exertion - C::IdleMets) /
            (physiology.EffectiveFitness - C::IdleMets),
        0.0F,
        1.0F
    );
    const float pvcChance = m_Settings.ArrhythmiaSusceptibility *
        std::lerp(C::PVCChanceNormal, C::PVCChanceMax, riskFactor);

    const float adrenalineFactor = std::min(physiology.Adrenaline / 5.0F, 1.0F);
    const float acuteFatigueFactor = physiology.AcuteFatigue / C::AcuteFatigueMax;
    const float runExtensionChance = std::min(
        m_Settings.ArrhythmiaSusceptibility *
            C::PVCRunExtensionChance *
            (1.0F + adrenalineFactor + acuteFatigueFactor + extremeHeartRateFactor),
        1.0F
    );

    const std::optional<BeatEvent> event = m_Rhythm.Advance({
        .DeltaSeconds        = input.DeltaSeconds,
        .HeartRate           = physiology.HeartRate,
        .RespirationPhase    = physiology.RespirationPhase,
        .ExertionFraction    = exertionFraction,
        .Contractility       = physiology.Contractility,
        .PVCChancePerSecond  = pvcChance,
        .RiskFactor          = riskFactor,
        .RunExtensionChance  = runExtensionChance,
    });
    if (event)
    {
        result.Beat = RuntimeBeat{
            .Event  = *event,
            .Render = CreateRenderSpec(*event, physiology),
        };
    }
    return result;
}

void SHR::Runtime::NotifyJump()
{
    m_Simulation.NotifyJump();
}

void SHR::Runtime::NotifySleep(float duration)
{
    m_Simulation.NotifySleep(duration);
}

void SHR::Runtime::NotifyFastTravel(float duration)
{
    m_Simulation.NotifyFastTravel(duration);
}

void SHR::Runtime::NotifyCombatEntry()
{
    m_Simulation.NotifyCombatEntry();
}

void SHR::Runtime::NotifyHit()
{
    m_Simulation.NotifyHit();
}

SHR::PhysiologySnapshot SHR::Runtime::GetSnapshot() const
{
    return m_Simulation.GetSnapshot();
}

SHR::SimulationState SHR::Runtime::GetState() const
{
    return m_Simulation.GetState();
}

SHR::SimulationState SHR::Runtime::CreateInitialState() const
{
    return m_Simulation.CreateInitialState();
}

float SHR::Runtime::ComputeEquilibriumContractility(const SimulationState &state) const
{
    return m_Simulation.ComputeEquilibriumContractility(state);
}

void SHR::Runtime::Restore(const SimulationState &state)
{
    m_Simulation.Restore(state);
}
