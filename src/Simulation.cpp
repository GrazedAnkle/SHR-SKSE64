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
#include "Simulation.hpp"

#include "Config.hpp"
#include "Constants.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    namespace C = SHR::Constants;

    float AdrenalineDecayFactor(float delta, float halfLife)
    {
        return std::exp(-std::numbers::ln2_v<float> * delta / halfLife);
    }

    float InterpolateVentilationTarget(
        float fraction,
        float rest,
        float atVT1,
        float atRCP,
        float maximum
    )
    {
        fraction = std::clamp(fraction, 0.0F, 1.0F);
        if (fraction <= C::VentilationVT1Fraction)
        {
            return std::lerp(rest, atVT1, fraction / C::VentilationVT1Fraction);
        }
        if (fraction <= C::VentilationRCPFraction)
        {
            const float local = (fraction - C::VentilationVT1Fraction) /
                (C::VentilationRCPFraction - C::VentilationVT1Fraction);
            return std::lerp(atVT1, atRCP, local);
        }
        const float local = (fraction - C::VentilationRCPFraction) /
            (1.0F - C::VentilationRCPFraction);
        return std::lerp(atRCP, maximum, local);
    }
}

void SHR::HeartRateSimulation::Init()
{
    const float restingHR = Config::Get().HeartRate.Resting;
    m_Fitness = C::FitnessBaseMets + (C::BaseRestingHR - restingHR) / C::RestingHRSlope;
    m_Exertion = C::IdleMets;
    m_AcuteFatigue = 0.0F;
    m_LongTermFatigue = 0.0F;
    m_TargetHeartRate = ComputeTargetHeartRate(C::IdleMets);
    m_FastHR = C::HRFastFraction * m_TargetHeartRate;
    m_SlowHR = (1.0F - C::HRFastFraction) * m_TargetHeartRate;
    m_Adrenaline = 0.0F;
    m_Contractility = 0.0F;
    m_RespRate = C::RestingRespRate;
    m_RespDepth = 0.0F;
    m_RespPhase = 0.0F;
    m_MaybeDeathSeconds = std::nullopt;
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
    m_Adrenaline += C::AdrenalineCombatEntry;
}

void SHR::HeartRateSimulation::NotifyHit()
{
    m_Adrenaline += C::AdrenalineTakeHit;
}

void SHR::HeartRateSimulation::Restore(
    float heartRate,
    float exertion,
    float adrenaline,
    float fitness,
    float acuteFatigue,
    float longTermFatigue,
    float fastHR,
    float respRate,
    float contractility,
    float respDepth
)
{
    m_FastHR = (fastHR > 0.0F) ? fastHR : C::HRFastFraction * heartRate;
    m_SlowHR = heartRate - m_FastHR;
    m_Exertion = exertion;
    m_Adrenaline = adrenaline;
    m_AcuteFatigue = acuteFatigue;
    m_LongTermFatigue = longTermFatigue;
    if (fitness > 0.0F)
    {
        m_Fitness = fitness;
    }
    else
    {
        const float restingHR = Config::Get().HeartRate.Resting;
        m_Fitness = C::FitnessBaseMets + (C::BaseRestingHR - restingHR) / C::RestingHRSlope;
    }
    m_RespRate = (respRate > 0.0F) ? respRate : C::RestingRespRate;
    m_RespDepth = (respDepth >= 0.0F) ? std::clamp(respDepth, 0.0F, 1.0F) : 0.0F;
    m_RespPhase = 0.0F;
    m_TargetHeartRate = ComputeTargetHeartRate(exertion);
    // Negative is the legacy-save sentinel - seed contractility at equilibrium.
    m_Contractility = (contractility >= 0.0F) ? contractility : ContractilityTarget();
}

float SHR::HeartRateSimulation::GetHeartRate() const
{
    return m_FastHR + m_SlowHR;
}

float SHR::HeartRateSimulation::GetFastHR() const
{
    return m_FastHR;
}

float SHR::HeartRateSimulation::GetExertion() const
{
    return m_Exertion;
}

float SHR::HeartRateSimulation::GetAdrenaline() const
{
    return m_Adrenaline;
}

float SHR::HeartRateSimulation::GetContractility() const
{
    return m_Contractility;
}

float SHR::HeartRateSimulation::GetContractilityExcess() const
{
    // Compare against the steady-state contractility implied by current HR.
    const float effResting = EffectiveRestingHR();
    const float maxHR      = Config::Get().HeartRate.Max;
    const float hrImplied  = std::clamp((GetHeartRate() - effResting) / (maxHR - effResting), 0.0F, 1.0F);
    return std::max(0.0F, m_Contractility - hrImplied);
}

float SHR::HeartRateSimulation::GetFitness() const
{
    return m_Fitness;
}

float SHR::HeartRateSimulation::GetEffectiveFitness() const
{
    return std::max(m_Fitness - m_AcuteFatigue - m_LongTermFatigue, C::FitnessAbsoluteMin);
}

float SHR::HeartRateSimulation::GetAcuteFatigue() const
{
    return m_AcuteFatigue;
}

float SHR::HeartRateSimulation::GetLongTermFatigue() const
{
    return m_LongTermFatigue;
}

float SHR::HeartRateSimulation::GetRespRate() const
{
    return m_RespRate;
}

float SHR::HeartRateSimulation::GetRespDepth() const
{
    return m_RespDepth;
}

float SHR::HeartRateSimulation::GetRespPhase() const
{
    return m_RespPhase;
}

std::optional<float> SHR::HeartRateSimulation::GetDeathSeconds() const
{
    return m_MaybeDeathSeconds;
}

float SHR::HeartRateSimulation::EffectiveRestingHR() const
{
    return C::BaseRestingHR - (GetEffectiveFitness() - C::FitnessBaseMets) * C::RestingHRSlope;
}

float SHR::HeartRateSimulation::NormalizedExertion(float exertion) const
{
    // Effective-capacity fraction, floored at zero but intentionally unbounded above.
    return std::max((exertion - C::IdleMets) / (GetEffectiveFitness() - C::IdleMets), 0.0F);
}

void SHR::HeartRateSimulation::UpdateAcuteFatigue(float exertion, float delta)
{
    // Normalize by raw fitness. Effective fitness would feed fatigue back into its own target.
    const float normalizedExertion = std::clamp(
        (exertion - C::IdleMets) / (m_Fitness - C::IdleMets),
        0.0F,
        1.0F
    );
    const float target = C::AcuteFatigueMax * normalizedExertion;
    const float difference = target - m_AcuteFatigue;
    const float tau = difference > 0.0F ? C::AcuteFatigueGainTau : C::AcuteFatigueDecayTau;
    m_AcuteFatigue += (1.0F - std::exp(-delta / tau)) * difference;
}

void SHR::HeartRateSimulation::UpdateLongTermFatigue(float gameHoursDelta)
{
    const float normalizedAcute = m_AcuteFatigue / C::AcuteFatigueMax;
    const float target = C::LongTermFatigueMax * normalizedAcute;
    const float difference = target - m_LongTermFatigue;
    const float tau = difference > 0.0F ? C::LongTermFatigueGainTau : C::LongTermFatigueDecayTau;
    m_LongTermFatigue += (1.0F - std::exp(-gameHoursDelta / tau)) * difference;
}

void SHR::HeartRateSimulation::UpdateFitness(float exertion, float gameHoursDelta)
{
    const float normalizedFatigue =
        (m_AcuteFatigue / C::AcuteFatigueMax + m_LongTermFatigue / C::LongTermFatigueMax) * 0.5F;
    const float trainingEfficacy = std::max(0.0F, 1.0F - normalizedFatigue);

    const bool isTraining = exertion > C::IdleMets;
    const float target = isTraining ? C::FitnessMaxMets : C::FitnessBaseMets;
    const float tau = isTraining ? C::FitnessGainTau : C::FitnessDecayTau;
    const float step = (1.0F - std::exp(-gameHoursDelta / tau)) * (target - m_Fitness);
    m_Fitness += isTraining ? step * trainingEfficacy : step;
}

float SHR::HeartRateSimulation::ComputeTargetHeartRate(float exertion) const
{
    const float effectiveResting = EffectiveRestingHR();
    const float fraction = NormalizedExertion(exertion);
    const float target = effectiveResting + fraction * (Config::Get().HeartRate.Max - effectiveResting);
    return std::min(target, C::HRFormulaCeiling);
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

    const auto &config = Config::Get();

    m_Adrenaline *= AdrenalineDecayFactor(delta, C::AdrenalineHalfLife);

    // Select the highest-priority active land movement.
    float movementMets;
    if (state.IsSprinting)
    {
        movementMets = C::SprintingMets;
    }
    else if (state.IsRunning)
    {
        movementMets = C::RunningMets;
    }
    else if (state.IsWalking)
    {
        movementMets = C::WalkingMets;
    }
    else
    {
        movementMets = C::IdleMets;
    }

    const bool isMoving = movementMets > C::IdleMets;

    // Swimming replaces land-based movement METs.
    if (isMoving && state.IsSwimming)
    {
        movementMets = C::SwimmingMets;
    }

    // Multipliers apply to the movement contribution only.
    if (state.IsSneaking)
    {
        const float movementContribution = movementMets - C::IdleMets;
        movementMets = C::IdleMets + movementContribution * C::CrouchMovementMultiplier;
    }
    if (state.IsOnMount)
    {
        const float movementContribution = movementMets - C::IdleMets;
        movementMets = C::IdleMets + movementContribution * C::MountedMultiplier;
    }

    // Known v1 routing: adrenaline enters exertion/HR here and contractility again in
    // ContractilityTarget. Contractility v2 addresses the separation. See CONTRACTILITY_SPEC.md.
    float targetMets = std::min(movementMets + m_Adrenaline, GetEffectiveFitness());

    const float difference = targetMets - m_Exertion;
    const float rate = difference > 0.0F ? C::ExertionAccumulationRate : C::ExertionRecoveryRate;
    m_Exertion += std::copysign(std::min(std::abs(difference), rate * delta), difference);

    // A jump may exceed effective fitness, capped at the exertion that produces AbsoluteMaxHR.
    if (m_DidJump.exchange(false))
    {
        const float effective = GetEffectiveFitness();
        const float effectiveResting = EffectiveRestingHR();
        const float exertionCap =
            C::IdleMets +
            (effective - C::IdleMets) *
                (C::HRFormulaCeiling - effectiveResting) /
                (config.HeartRate.Max - effectiveResting);
        m_Exertion = std::min(m_Exertion + C::JumpMets, exertionCap);
    }

    // Sleep lasts at least one hour, so advance affected states analytically.
    if (const float durationSeconds = m_SleepDuration.exchange(Sentinel); durationSeconds != Sentinel)
    {
        const float durationHours = durationSeconds / C::SecondsPerHour;
        m_Adrenaline = 0.0F;
        m_AcuteFatigue = 0.0F;
        m_LongTermFatigue *= std::exp(-durationHours * C::SleepRecoveryRate);
        m_Exertion = C::IdleMets;
        const float sleepHR = ComputeTargetHeartRate(C::IdleMets) * C::SleepFraction;
        m_FastHR = C::HRFastFraction * sleepHR;
        m_SlowHR = (1.0F - C::HRFastFraction) * sleepHR;
        m_RespRate = C::SleepRespRate;
        UpdateRespDepth(durationSeconds, 0.0F);
        // Advance contractility over the full sleep duration.
        UpdateContractility(durationSeconds);
    }

    if (const float duration = m_FastTravelDuration.exchange(Sentinel); duration != Sentinel)
    {
        m_Adrenaline *= AdrenalineDecayFactor(duration, C::AdrenalineHalfLife);

        const float difference = C::WalkingMets - m_Exertion;
        const float rate = difference > 0.0F ? C::ExertionAccumulationRate : C::ExertionRecoveryRate;
        m_Exertion += std::copysign(std::min(std::abs(difference), rate * duration), difference);

        const float walkFraction = std::max(
            (C::WalkingMets - C::IdleMets) / (GetEffectiveFitness() - C::IdleMets),
            0.0F
        );
        const float walkTargetRR  = ComputeTargetRespRate(walkFraction);
        const float rrDifference  = walkTargetRR - m_RespRate;
        const float rrTau         = rrDifference > 0.0F ? C::RespOnsetTau : C::RespRecoveryTau;
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
    const float adrenergic = std::clamp(m_Adrenaline / C::AdrenalineContractilityScale, 0.0F, 1.0F);
    return std::min(normalizedExertion + adrenergic, 1.0F);
}

void SHR::HeartRateSimulation::UpdateContractility(float delta)
{
    // Relax exponentially toward the target with separate onset and decay time constants.
    const float target = ContractilityTarget();
    const float tau = (target > m_Contractility) ? C::ContractilityOnsetTau : C::ContractilityDecayTau;
    m_Contractility += (1.0F - std::exp(-delta / tau)) * (target - m_Contractility);
}

void SHR::HeartRateSimulation::UpdateRespiration(float delta)
{
    m_RespPhase = std::fmod(m_RespPhase + m_RespRate / 60.0F * delta, 1.0F);

    const float fraction = std::clamp(NormalizedExertion(m_Exertion), 0.0F, 1.0F);
    const float targetRespRate = ComputeTargetRespRate(fraction);

    const float difference = targetRespRate - m_RespRate;
    const float tau = difference > 0.0F ? C::RespOnsetTau : C::RespRecoveryTau;
    m_RespRate += (1.0F - std::exp(-delta / tau)) * difference;
    UpdateRespDepth(delta, ComputeTargetRespDepth(fraction));
}

float SHR::HeartRateSimulation::ComputeTargetRespRate(float normalizedExertion)
{
    return InterpolateVentilationTarget(
        normalizedExertion,
        C::RestingRespRate,
        C::RespRateAtVT1,
        C::RespRateAtRCP,
        C::MaxRespRate
    );
}

float SHR::HeartRateSimulation::ComputeTargetRespDepth(float normalizedExertion)
{
    return InterpolateVentilationTarget(
        normalizedExertion,
        0.0F,
        C::RespDepthAtVT1,
        C::RespDepthAtRCP,
        1.0F
    );
}

void SHR::HeartRateSimulation::UpdateRespDepth(float delta, float target)
{
    target = std::clamp(target, 0.0F, 1.0F);
    const float difference = target - m_RespDepth;
    const float tau = difference > 0.0F ? C::BreathDepthOnsetTau : C::BreathDepthRecoveryTau;
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
        (GetEffectiveFitness() - C::FitnessBaseMets) / (C::FitnessMaxMets - C::FitnessBaseMets),
        0.0F,
        1.0F
    );
    const float fastOnsetTau    = std::lerp(C::FastOnsetTauSedentary,    C::FastOnsetTauElite,    normFitness);
    const float fastRecoveryTau = std::lerp(C::FastRecoveryTauSedentary, C::FastRecoveryTauElite, normFitness);

    m_FastHR += expStep(m_FastHR, C::HRFastFraction * m_TargetHeartRate, fastOnsetTau, fastRecoveryTau);
    m_SlowHR += expStep(m_SlowHR, (1.0F - C::HRFastFraction) * m_TargetHeartRate, C::SlowOnsetTau, C::SlowRecoveryTau);
}
