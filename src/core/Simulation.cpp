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
#include "core/Simulation.hpp"

#include "core/Constants.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace
{
    float AdrenalineDecayFactor(float delta, float halfLife)
    {
        return std::exp(-std::numbers::ln2_v<float> * delta / halfLife);
    }

    float InterpolateVentilationTarget(
        float fraction,
        float rest,
        float atVT1,
        float atRCP,
        float maximum,
        const SHR::SimulationModelCoefficients &coefficients
    )
    {
        fraction = std::clamp(fraction, 0.0F, 1.0F);
        if (fraction <= coefficients.VentilationVT1Fraction)
        {
            return std::lerp(rest, atVT1, fraction / coefficients.VentilationVT1Fraction);
        }
        if (fraction <= coefficients.VentilationRCPFraction)
        {
            const float local = (fraction - coefficients.VentilationVT1Fraction) /
                (coefficients.VentilationRCPFraction - coefficients.VentilationVT1Fraction);
            return std::lerp(atVT1, atRCP, local);
        }
        const float local = (fraction - coefficients.VentilationRCPFraction) /
            (1.0F - coefficients.VentilationRCPFraction);
        return std::lerp(atRCP, maximum, local);
    }
}

SHR::VentilationTargets SHR::ComputeVentilationTargets(float normalizedExertion)
{
    return ComputeVentilationTargets(
        normalizedExertion,
        DefaultModelCoefficients().Simulation
    );
}

SHR::VentilationTargets SHR::ComputeVentilationTargets(
    float normalizedExertion,
    const SimulationModelCoefficients &coefficients
)
{
    return {
        .Rate = InterpolateVentilationTarget(
            normalizedExertion,
            coefficients.RestingRespRate,
            coefficients.RespRateAtVT1,
            coefficients.RespRateAtRCP,
            coefficients.MaxRespRate,
            coefficients
        ),
        .Depth = InterpolateVentilationTarget(
            normalizedExertion,
            0.0F,
            coefficients.RespDepthAtVT1,
            coefficients.RespDepthAtRCP,
            1.0F,
            coefficients
        ),
    };
}

void SHR::HeartRateSimulation::Init()
{
    Restore(CreateInitialState());
}

void SHR::HeartRateSimulation::Step(PlayerState state, float realDelta, float gameHoursDelta)
{
    UpdateExertion(state, realDelta);
    if (!state.IsDead)
    {
        UpdateAcuteFatigue(m_Exertion, realDelta);
        UpdateLongTermFatigue(gameHoursDelta);
        UpdateFitness(m_Exertion, gameHoursDelta);
        m_TargetHeartRate = ComputeTargetHeartRate(m_Exertion);
        UpdateRespiration(realDelta);
        UpdateContractility(realDelta);
    }
    UpdateCurrentHeartRate(realDelta);
}

void SHR::HeartRateSimulation::NotifyJump()
{
    m_DidJump = true;
}

void SHR::HeartRateSimulation::NotifySleep(float duration)
{
    m_SleepDuration = duration;
}

void SHR::HeartRateSimulation::NotifyFastTravel(float duration)
{
    m_FastTravelDuration = duration;
}

void SHR::HeartRateSimulation::NotifyCombatEntry()
{
    m_Adrenaline += m_Coefficients.AdrenalineCombatEntry;
}

void SHR::HeartRateSimulation::NotifyHit()
{
    m_Adrenaline += m_Coefficients.AdrenalineTakeHit;
}

SHR::PhysiologySnapshot SHR::HeartRateSimulation::GetSnapshot() const
{
    return {
        .HeartRate           = CurrentHeartRate(),
        .FastHeartRate       = m_FastHR,
        .SlowHeartRate       = m_SlowHR,
        .Exertion            = m_Exertion,
        .Adrenaline          = m_Adrenaline,
        .Contractility       = m_Contractility,
        .ContractilityExcess = ContractilityExcess(),
        .Fitness             = m_Fitness,
        .EffectiveFitness    = EffectiveFitness(),
        .AcuteFatigue        = m_AcuteFatigue,
        .LongTermFatigue     = m_LongTermFatigue,
        .RespirationRate     = m_RespRate,
        .RespirationDepth    = m_RespDepth,
        .RespirationPhase    = m_RespPhase,
        .DeathSeconds        = m_MaybeDeathSeconds,
    };
}

float SHR::HeartRateSimulation::GetTargetHeartRate() const noexcept
{
    return m_TargetHeartRate;
}

float SHR::HeartRateSimulation::GetTargetRespirationRate() const
{
    return ComputeTargetRespRate(
        std::clamp(NormalizedExertion(m_Exertion), 0.0F, 1.0F)
    );
}

float SHR::HeartRateSimulation::GetTargetRespirationDepth() const
{
    return ComputeTargetRespDepth(
        std::clamp(NormalizedExertion(m_Exertion), 0.0F, 1.0F)
    );
}

SHR::SimulationState SHR::HeartRateSimulation::GetState() const
{
    return {
        .FastHeartRate    = m_FastHR,
        .SlowHeartRate    = m_SlowHR,
        .Exertion         = m_Exertion,
        .Adrenaline       = m_Adrenaline,
        .Contractility    = m_Contractility,
        .Fitness          = m_Fitness,
        .AcuteFatigue     = m_AcuteFatigue,
        .LongTermFatigue  = m_LongTermFatigue,
        .RespirationRate  = m_RespRate,
        .RespirationDepth = m_RespDepth,
        .RespirationPhase = m_RespPhase,
        .DeathSeconds     = m_MaybeDeathSeconds,
    };
}

SHR::SimulationState SHR::HeartRateSimulation::CreateInitialState() const
{
    const float fitness = m_Coefficients.FitnessBaseMets +
        (m_Coefficients.BaseRestingHR - m_Settings.RestingHeartRate) /
            m_Coefficients.RestingHRSlope;
    return {
        .FastHeartRate    = m_Coefficients.HRFastFraction * m_Settings.RestingHeartRate,
        .SlowHeartRate    = (1.0F - m_Coefficients.HRFastFraction) * m_Settings.RestingHeartRate,
        .Exertion         = m_Coefficients.IdleMets,
        .Adrenaline       = 0.0F,
        .Contractility    = 0.0F,
        .Fitness          = fitness,
        .AcuteFatigue     = 0.0F,
        .LongTermFatigue  = 0.0F,
        .RespirationRate  = m_Coefficients.RestingRespRate,
        .RespirationDepth = 0.0F,
        .RespirationPhase = 0.0F,
        .DeathSeconds     = std::nullopt,
    };
}

void SHR::HeartRateSimulation::ApplySettings(SimulationSettings settings)
{
    // Mirrors the linear seeding in CreateInitialState, which is the only consumer of
    // RestingHeartRate; a plain assignment would leave an existing character untouched.
    m_Fitness += (m_Settings.RestingHeartRate - settings.RestingHeartRate) /
        m_Coefficients.RestingHRSlope;

    // Lower bound only: NormalizedExertion divides by (fitness - IdleMets), while a lowered ceiling
    // is left to detrain over FitnessDecayTau rather than deleting progression on a slider drag.
    m_Fitness = std::max(m_Fitness, m_Coefficients.FitnessAbsoluteMin());

    m_Settings = settings;
}

float SHR::HeartRateSimulation::FatigueScale() const
{
    // Floored like EffectiveFitness: these divide, and a restored state can carry a fitness small
    // enough to make the ratios explode.
    return std::max(m_Fitness, m_Coefficients.FitnessAbsoluteMin());
}

float SHR::HeartRateSimulation::AcuteFatigueMax() const
{
    return m_Coefficients.AcuteFatigueMaxFraction * FatigueScale();
}

float SHR::HeartRateSimulation::LongTermFatigueMax() const
{
    return m_Coefficients.LongTermFatigueMaxFraction * FatigueScale();
}

float SHR::HeartRateSimulation::ComputeEquilibriumContractility(const SimulationState &state) const
{
    const float effectiveFitness = std::max(
        state.Fitness - state.AcuteFatigue - state.LongTermFatigue,
        m_Coefficients.FitnessAbsoluteMin()
    );
    const float normalizedExertion = std::clamp(
        (state.Exertion - m_Coefficients.IdleMets) / (effectiveFitness - m_Coefficients.IdleMets),
        0.0F,
        1.0F
    );
    const float adrenergic = std::clamp(
        state.Adrenaline / m_Coefficients.AdrenalineContractilityScale,
        0.0F,
        1.0F
    );
    return std::min(normalizedExertion + adrenergic, 1.0F);
}

void SHR::HeartRateSimulation::Restore(const SimulationState &state)
{
    m_FastHR = state.FastHeartRate;
    m_SlowHR = state.SlowHeartRate;
    m_Exertion = state.Exertion;
    m_Adrenaline = state.Adrenaline;
    m_Contractility = state.Contractility;
    m_Fitness = state.Fitness;
    m_AcuteFatigue = state.AcuteFatigue;
    m_LongTermFatigue = state.LongTermFatigue;
    m_RespRate = state.RespirationRate;
    m_RespDepth = state.RespirationDepth;
    m_RespPhase = state.RespirationPhase;
    m_MaybeDeathSeconds = state.DeathSeconds;
    m_TargetHeartRate = ComputeTargetHeartRate(m_Exertion);
}

float SHR::HeartRateSimulation::EffectiveRestingHR() const
{
    return m_Coefficients.BaseRestingHR -
        (EffectiveFitness() - m_Coefficients.FitnessBaseMets) * m_Coefficients.RestingHRSlope;
}

float SHR::HeartRateSimulation::CurrentHeartRate() const
{
    return m_FastHR + m_SlowHR;
}

float SHR::HeartRateSimulation::EffectiveFitness() const
{
    return std::max(
        m_Fitness - m_AcuteFatigue - m_LongTermFatigue,
        m_Coefficients.FitnessAbsoluteMin()
    );
}

float SHR::HeartRateSimulation::ContractilityExcess() const
{
    const float effResting = EffectiveRestingHR();
    const float maxHR      = m_Settings.MaximumHeartRate;
    const float hrImplied  = std::clamp((CurrentHeartRate() - effResting) / (maxHR - effResting), 0.0F, 1.0F);
    return std::max(0.0F, m_Contractility - hrImplied);
}

float SHR::HeartRateSimulation::NormalizedExertion(float exertion) const
{
    // Effective-capacity fraction, floored at zero but intentionally unbounded above.
    return std::max(
        (exertion - m_Coefficients.IdleMets) / (EffectiveFitness() - m_Coefficients.IdleMets),
        0.0F
    );
}

void SHR::HeartRateSimulation::UpdateAcuteFatigue(float exertion, float delta)
{
    // Normalize by raw fitness. Effective fitness would feed fatigue back into its own target.
    const float normalizedExertion = std::clamp(
        (exertion - m_Coefficients.IdleMets) / (m_Fitness - m_Coefficients.IdleMets),
        0.0F,
        1.0F
    );
    const float target = AcuteFatigueMax() * normalizedExertion;
    const float difference = target - m_AcuteFatigue;
    const float tau = difference > 0.0F ?
        m_Coefficients.AcuteFatigueGainTau :
        m_Coefficients.AcuteFatigueDecayTau;
    m_AcuteFatigue += (1.0F - std::exp(-delta / tau)) * difference;
}

void SHR::HeartRateSimulation::UpdateLongTermFatigue(float gameHoursDelta)
{
    const float normalizedAcute = m_AcuteFatigue / AcuteFatigueMax();
    const float target = LongTermFatigueMax() * normalizedAcute;
    const float difference = target - m_LongTermFatigue;
    const float tau = difference > 0.0F ?
        m_Coefficients.LongTermFatigueGainTau :
        m_Coefficients.LongTermFatigueDecayTau;
    m_LongTermFatigue += (1.0F - std::exp(-gameHoursDelta / tau)) * difference;
}

void SHR::HeartRateSimulation::UpdateFitness(float exertion, float gameHoursDelta)
{
    const float normalizedFatigue =
        (m_AcuteFatigue / AcuteFatigueMax() +
            m_LongTermFatigue / LongTermFatigueMax()) *
        0.5F;
    const float trainingEfficacy = std::max(0.0F, 1.0F - normalizedFatigue);

    const bool isTraining = exertion > m_Coefficients.IdleMets;
    const float target = isTraining
        ? m_Settings.FitnessMaxMets
        : m_Coefficients.FitnessBaseMets;
    const float tau = isTraining ? m_Coefficients.FitnessGainTau : m_Coefficients.FitnessDecayTau;
    const float step = (1.0F - std::exp(-gameHoursDelta / tau)) * (target - m_Fitness);
    m_Fitness += isTraining ? step * trainingEfficacy : step;
}

float SHR::HeartRateSimulation::ComputeTargetHeartRate(float exertion) const
{
    const float effectiveResting = EffectiveRestingHR();
    const float fraction = NormalizedExertion(exertion);
    const float target = effectiveResting + fraction * (m_Settings.MaximumHeartRate - effectiveResting);
    return std::min(target, m_Coefficients.HRFormulaCeiling);
}

void SHR::HeartRateSimulation::UpdateExertion(PlayerState state, float delta)
{
    if (state.IsDead)
    {
        if (!m_MaybeDeathSeconds.has_value())
        {
            m_MaybeDeathSeconds = 0.0F;
        }

        *m_MaybeDeathSeconds += delta;
        m_TargetHeartRate = 0.0F;

        return;
    }

    m_MaybeDeathSeconds = std::nullopt;

    m_Adrenaline *= AdrenalineDecayFactor(delta, m_Coefficients.AdrenalineHalfLife);

    // Select the highest-priority active land movement.
    float movementMets;
    if (state.IsSprinting)
    {
        movementMets = m_Coefficients.SprintingMets;
    }
    else if (state.IsRunning)
    {
        movementMets = m_Coefficients.RunningMets;
    }
    else if (state.IsWalking)
    {
        movementMets = m_Coefficients.WalkingMets;
    }
    else
    {
        movementMets = m_Coefficients.IdleMets;
    }

    const bool isMoving = movementMets > m_Coefficients.IdleMets;

    // Swimming replaces land-based movement METs.
    if (isMoving && state.IsSwimming)
    {
        movementMets = m_Coefficients.SwimmingMets;
    }

    // Multipliers apply to the movement contribution only.
    if (state.IsSneaking)
    {
        const float movementContribution = movementMets - m_Coefficients.IdleMets;
        movementMets =
            m_Coefficients.IdleMets +
            movementContribution * m_Coefficients.CrouchMovementMultiplier;
    }
    if (state.IsOnMount)
    {
        const float movementContribution = movementMets - m_Coefficients.IdleMets;
        movementMets =
            m_Coefficients.IdleMets +
            movementContribution * m_Coefficients.MountedMultiplier;
    }

    // Known v1 routing: adrenaline enters exertion/HR here and contractility again in
    // ContractilityTarget. Contractility v2 addresses the separation. See CONTRACTILITY_SPEC.md.
    float targetMets = std::min(movementMets + m_Adrenaline, EffectiveFitness());

    const float difference = targetMets - m_Exertion;
    const float rate = difference > 0.0F ?
        m_Coefficients.ExertionAccumulationRate :
        m_Coefficients.ExertionRecoveryRate;
    m_Exertion += std::copysign(std::min(std::abs(difference), rate * delta), difference);

    // A jump may exceed effective fitness, capped at the exertion that produces AbsoluteMaxHR.
    if (std::exchange(m_DidJump, false))
    {
        const float effective = EffectiveFitness();
        const float effectiveResting = EffectiveRestingHR();
        const float exertionCap =
            m_Coefficients.IdleMets +
            (effective - m_Coefficients.IdleMets) *
                (m_Coefficients.HRFormulaCeiling - effectiveResting) /
                (m_Settings.MaximumHeartRate - effectiveResting);
        m_Exertion = std::min(m_Exertion + m_Coefficients.JumpMets, exertionCap);
    }

    // Sleep lasts at least one hour, so advance affected states analytically.
    if (const float durationSeconds = std::exchange(m_SleepDuration, Sentinel); durationSeconds != Sentinel)
    {
        const float durationHours = durationSeconds / SHR::Constants::SecondsPerHour;
        m_Adrenaline = 0.0F;
        m_AcuteFatigue = 0.0F;
        m_LongTermFatigue *= std::exp(-durationHours * m_Coefficients.SleepRecoveryRate);
        m_Exertion = m_Coefficients.IdleMets;
        const float sleepHR =
            ComputeTargetHeartRate(m_Coefficients.IdleMets) * m_Coefficients.SleepFraction;
        m_FastHR = m_Coefficients.HRFastFraction * sleepHR;
        m_SlowHR = (1.0F - m_Coefficients.HRFastFraction) * sleepHR;
        m_RespRate = m_Coefficients.SleepRespRate;
        UpdateRespDepth(durationSeconds, 0.0F);
        // Advance contractility over the full sleep duration.
        UpdateContractility(durationSeconds);
    }

    if (const float duration = std::exchange(m_FastTravelDuration, Sentinel); duration != Sentinel)
    {
        m_Adrenaline *= AdrenalineDecayFactor(duration, m_Coefficients.AdrenalineHalfLife);

        const float difference = m_Coefficients.WalkingMets - m_Exertion;
        const float rate = difference > 0.0F ?
            m_Coefficients.ExertionAccumulationRate :
            m_Coefficients.ExertionRecoveryRate;
        m_Exertion += std::copysign(std::min(std::abs(difference), rate * duration), difference);

        const float walkFraction = std::max(
            (m_Coefficients.WalkingMets - m_Coefficients.IdleMets) /
                (EffectiveFitness() - m_Coefficients.IdleMets),
            0.0F
        );
        const float walkTargetRR  = ComputeTargetRespRate(walkFraction);
        const float rrDifference  = walkTargetRR - m_RespRate;
        const float rrTau         = rrDifference > 0.0F ?
            m_Coefficients.RespOnsetTau :
            m_Coefficients.RespRecoveryTau;
        m_RespRate  += (1.0F - std::exp(-duration / rrTau)) * rrDifference;
        UpdateRespDepth(duration, ComputeTargetRespDepth(walkFraction));
        m_RespPhase  = std::fmod(m_RespPhase + m_RespRate / 60.0F * duration, 1.0F);
        // Advance contractility over the full travel duration.
        UpdateContractility(duration);
    }
}

float SHR::HeartRateSimulation::ContractilityTarget() const
{
    const float normalizedExertion = std::min(NormalizedExertion(m_Exertion), 1.0F);
    const float adrenergic = std::clamp(
        m_Adrenaline / m_Coefficients.AdrenalineContractilityScale,
        0.0F,
        1.0F
    );
    return std::min(normalizedExertion + adrenergic, 1.0F);
}

void SHR::HeartRateSimulation::UpdateContractility(float delta)
{
    // Relax exponentially toward the target with separate onset and decay time constants.
    const float target = ContractilityTarget();
    const float tau = target > m_Contractility ?
        m_Coefficients.ContractilityOnsetTau :
        m_Coefficients.ContractilityDecayTau;
    m_Contractility += (1.0F - std::exp(-delta / tau)) * (target - m_Contractility);
}

void SHR::HeartRateSimulation::UpdateRespiration(float delta)
{
    m_RespPhase = std::fmod(m_RespPhase + m_RespRate / 60.0F * delta, 1.0F);

    const float fraction = std::clamp(NormalizedExertion(m_Exertion), 0.0F, 1.0F);
    const float targetRespRate = ComputeTargetRespRate(fraction);

    const float difference = targetRespRate - m_RespRate;
    const float tau = difference > 0.0F ? m_Coefficients.RespOnsetTau : m_Coefficients.RespRecoveryTau;
    m_RespRate += (1.0F - std::exp(-delta / tau)) * difference;
    UpdateRespDepth(delta, ComputeTargetRespDepth(fraction));
}

float SHR::HeartRateSimulation::ComputeTargetRespRate(float normalizedExertion) const
{
    return ComputeVentilationTargets(normalizedExertion, m_Coefficients).Rate;
}

float SHR::HeartRateSimulation::ComputeTargetRespDepth(float normalizedExertion) const
{
    return ComputeVentilationTargets(normalizedExertion, m_Coefficients).Depth;
}

void SHR::HeartRateSimulation::UpdateRespDepth(float delta, float target)
{
    target = std::clamp(target, 0.0F, 1.0F);
    const float difference = target - m_RespDepth;
    const float tau = difference > 0.0F ?
        m_Coefficients.BreathDepthOnsetTau :
        m_Coefficients.BreathDepthRecoveryTau;
    m_RespDepth += (1.0F - std::exp(-delta / tau)) * difference;
}

void SHR::HeartRateSimulation::UpdateCurrentHeartRate(float delta)
{
    // Comparison against 0 is safe here since 0 is set explicitly.
    if (m_TargetHeartRate == 0.0F && m_FastHR == 0.0F && m_SlowHR == 0.0F)
    {
        return;
    }

    const auto expStep = [delta](float current, float target, float onsetTau, float recoveryTau)
    {
        const float difference = target - current;
        const float tau = difference > 0.0F ? onsetTau : recoveryTau;
        return (1.0F - std::exp(-delta / tau)) * difference;
    };

    const float normFitness = std::clamp(
        (EffectiveFitness() - m_Coefficients.FitnessBaseMets) /
            (m_Coefficients.FitnessEliteMets - m_Coefficients.FitnessBaseMets),
        0.0F,
        1.0F
    );
    const float fastOnsetTau = std::lerp(
        m_Coefficients.FastOnsetTauSedentary,
        m_Coefficients.FastOnsetTauElite,
        normFitness
    );
    const float fastRecoveryTau = std::lerp(
        m_Coefficients.FastRecoveryTauSedentary,
        m_Coefficients.FastRecoveryTauElite,
        normFitness
    );

    m_FastHR += expStep(
        m_FastHR,
        m_Coefficients.HRFastFraction * m_TargetHeartRate,
        fastOnsetTau,
        fastRecoveryTau
    );
    m_SlowHR += expStep(
        m_SlowHR,
        (1.0F - m_Coefficients.HRFastFraction) * m_TargetHeartRate,
        m_Coefficients.SlowOnsetTau,
        m_Coefficients.SlowRecoveryTau
    );
}
