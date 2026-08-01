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
#include "core/Runtime.hpp"

#include "core/AcousticMapper.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{
    void ValidateRuntimeCoefficientContext(
        const SHR::RuntimeSettings   &settings,
        const SHR::ModelCoefficients &coefficients
    )
    {
        if (
            settings.Simulation.MaximumHeartRate <=
            coefficients.Rhythm.ExtremeHeartRateRiskThreshold
        )
        {
            throw std::invalid_argument(
                "runtime maximum heart rate must be greater than "
                "ExtremeHeartRateRiskThreshold"
            );
        }
    }
}

SHR::Runtime::Runtime(RuntimeSettings settings)
    : Runtime(settings, DefaultModelCoefficients())
{
}

SHR::Runtime::Runtime(RuntimeSettings settings, RhythmRandom random)
    : Runtime(settings, std::move(random), DefaultModelCoefficients())
{
}

SHR::Runtime::Runtime(
    RuntimeSettings   settings,
    ModelCoefficients coefficients
)
    : m_Settings(settings)
    , m_Coefficients(std::move(coefficients))
    , m_Simulation(settings.Simulation, m_Coefficients.Simulation)
    , m_Rhythm(m_Coefficients.Rhythm)
{
    ValidateRuntimeCoefficientContext(m_Settings, m_Coefficients);
}

SHR::Runtime::Runtime(
    RuntimeSettings   settings,
    RhythmRandom      random,
    ModelCoefficients coefficients
)
    : m_Settings(settings)
    , m_Coefficients(std::move(coefficients))
    , m_Simulation(settings.Simulation, m_Coefficients.Simulation)
    , m_Rhythm(m_Coefficients.Rhythm, std::move(random))
{
    ValidateRuntimeCoefficientContext(m_Settings, m_Coefficients);
}

void SHR::Runtime::Init()
{
    // Init is the adapter's revert path, so pending events must not survive into the new character.
    m_Events.Clear();
    m_Simulation.Init();
    m_Rhythm.Init();
    m_PublishedHeartRate.store(m_Simulation.GetSnapshot().HeartRate, std::memory_order_relaxed);
}

SHR::StepResult SHR::Runtime::Step(const StepInput &input)
{
    // Before the simulation advances, so a notification lands on the step it would have under the
    // previous direct-mutation path.
    DrainEvents();

    m_Simulation.Step(input.Player, input.DeltaSeconds, input.GameHoursDelta);
    const PhysiologySnapshot physiology = m_Simulation.GetSnapshot();
    m_PublishedHeartRate.store(physiology.HeartRate, std::memory_order_relaxed);

    StepResult result{
        .Physiology = physiology,
        .Beat       = std::nullopt,
    };
    if (!input.OutputEnabled)
    {
        return result;
    }

    const float deathFactor = physiology.DeathSeconds
        .transform([riskRamp = m_Coefficients.Rhythm.DeathRiskRampSeconds](float seconds) {
            return std::min(seconds, riskRamp) / riskRamp;
        })
        .value_or(0.0F);
    const float extremeRiskThreshold =
        m_Coefficients.Rhythm.ExtremeHeartRateRiskThreshold;
    const float heartRateRange =
        m_Settings.Simulation.MaximumHeartRate - extremeRiskThreshold;
    const float extremeHeartRateFactor = std::clamp(
        (physiology.HeartRate - extremeRiskThreshold) / heartRateRange,
        0.0F,
        1.0F
    );
    const float fatigueFactor =
        physiology.LongTermFatigue / m_Coefficients.Simulation.LongTermFatigueMax;
    const float riskFactor = std::max({
        deathFactor,
        extremeHeartRateFactor,
        fatigueFactor,
    });

    const float exertionFraction = std::clamp(
        (physiology.Exertion - m_Coefficients.Simulation.IdleMets) /
            (physiology.EffectiveFitness - m_Coefficients.Simulation.IdleMets),
        0.0F,
        1.0F
    );
    const float pvcChance = m_Settings.ArrhythmiaSusceptibility *
        std::lerp(
            m_Coefficients.Rhythm.PVCChanceNormal,
            m_Coefficients.Rhythm.PVCChanceMax,
            riskFactor
        );

    const float adrenalineFactor = std::min(
        physiology.Adrenaline / m_Coefficients.Rhythm.AdrenalineRunRiskScale,
        1.0F
    );
    const float acuteFatigueFactor =
        physiology.AcuteFatigue / m_Coefficients.Simulation.AcuteFatigueMax;
    const float runExtensionChance = std::min(
        m_Settings.ArrhythmiaSusceptibility *
            m_Coefficients.Rhythm.PVCRunExtensionChance *
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
            .Render = CreateRenderSpec(
                *event,
                physiology,
                m_Coefficients.AcousticMapping
            ),
        };
    }
    return result;
}

void SHR::Runtime::NotifyJump()
{
    m_Events.Post({ .Kind = RuntimeEventKind::Jump });
}

void SHR::Runtime::NotifySleep(float duration)
{
    m_Events.Post({ .Kind = RuntimeEventKind::Sleep, .Duration = duration });
}

void SHR::Runtime::NotifyFastTravel(float duration)
{
    m_Events.Post({ .Kind = RuntimeEventKind::FastTravel, .Duration = duration });
}

void SHR::Runtime::NotifyCombatEntry()
{
    m_Events.Post({ .Kind = RuntimeEventKind::CombatEntry });
}

void SHR::Runtime::NotifyHit()
{
    m_Events.Post({ .Kind = RuntimeEventKind::Hit });
}

std::uint64_t SHR::Runtime::GetDroppedEventCount() const
{
    return m_Events.DroppedCount();
}

void SHR::Runtime::DrainEvents()
{
    // Post order preserves each kind's combining rule: jumps collapse into one flag, sleep and
    // fast-travel durations are last-wins, combat and hit accumulate.
    const std::size_t count = m_Events.Drain(m_DrainBuffer);
    for (std::size_t i = 0; i < count; ++i)
    {
        const RuntimeEvent &event = m_DrainBuffer[i];
        switch (event.Kind)
        {
        case RuntimeEventKind::Jump:
            m_Simulation.NotifyJump();
            break;
        case RuntimeEventKind::Sleep:
            m_Simulation.NotifySleep(event.Duration);
            break;
        case RuntimeEventKind::FastTravel:
            m_Simulation.NotifyFastTravel(event.Duration);
            break;
        case RuntimeEventKind::CombatEntry:
            m_Simulation.NotifyCombatEntry();
            break;
        case RuntimeEventKind::Hit:
            m_Simulation.NotifyHit();
            break;
        }
    }
}

SHR::PhysiologySnapshot SHR::Runtime::GetSnapshot() const
{
    return m_Simulation.GetSnapshot();
}

float SHR::Runtime::GetPublishedHeartRate() const noexcept
{
    return m_PublishedHeartRate.load(std::memory_order_relaxed);
}

float SHR::Runtime::GetTargetHeartRate() const noexcept
{
    return m_Simulation.GetTargetHeartRate();
}

float SHR::Runtime::GetTargetRespirationRate() const
{
    return m_Simulation.GetTargetRespirationRate();
}

float SHR::Runtime::GetTargetRespirationDepth() const
{
    return m_Simulation.GetTargetRespirationDepth();
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
    m_PublishedHeartRate.store(m_Simulation.GetSnapshot().HeartRate, std::memory_order_relaxed);
}
