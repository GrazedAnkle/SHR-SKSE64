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
#pragma once

#include "Constants.hpp"

namespace SHR
{
    struct SimulationModelCoefficients
    {
        float BaseRestingHR = Constants::BaseRestingHR;
        float SleepFraction = Constants::SleepFraction;
        float HRFormulaCeiling = Constants::HRFormulaCeiling;
        float HRFastFraction = Constants::HRFastFraction;
        float FastOnsetTauSedentary = Constants::FastOnsetTauSedentary;
        float FastOnsetTauElite = Constants::FastOnsetTauElite;
        float SlowOnsetTau = Constants::SlowOnsetTau;
        float FastRecoveryTauSedentary = Constants::FastRecoveryTauSedentary;
        float FastRecoveryTauElite = Constants::FastRecoveryTauElite;
        float SlowRecoveryTau = Constants::SlowRecoveryTau;

        float IdleMets = Constants::IdleMets;
        float WalkingMets = Constants::WalkingMets;
        float RunningMets = Constants::RunningMets;
        float SprintingMets = Constants::SprintingMets;
        float SwimmingMets = Constants::SwimmingMets;
        float JumpMets = Constants::JumpMets;

        float CrouchMovementMultiplier = Constants::CrouchMovementMultiplier;
        float MountedMultiplier = Constants::MountedMultiplier;
        float ExertionAccumulationRate = Constants::ExertionAccumulationRate;
        float ExertionRecoveryRate = Constants::ExertionRecoveryRate;

        float AdrenalineHalfLife = Constants::AdrenalineHalfLife;
        float AdrenalineCombatEntry = Constants::AdrenalineCombatEntry;
        float AdrenalineTakeHit = Constants::AdrenalineTakeHit;

        float ContractilityOnsetTau = Constants::ContractilityOnsetTau;
        float ContractilityDecayTau = Constants::ContractilityDecayTau;
        float AdrenalineContractilityScale = Constants::AdrenalineContractilityScale;

        float FitnessGainTau = Constants::FitnessGainTau;
        float FitnessDecayTau = Constants::FitnessDecayTau;
        float FitnessBaseMets = Constants::FitnessBaseMets;
        float FitnessMaxMets = Constants::FitnessMaxMets;
        float RestingHRSlope = Constants::RestingHRSlope;
        float MaxRestingHR = Constants::MaxRestingHR;

        float RestingRespRate = Constants::RestingRespRate;
        float VentilationVT1Fraction = Constants::VentilationVT1Fraction;
        float VentilationRCPFraction = Constants::VentilationRCPFraction;
        float RespRateAtVT1 = Constants::RespRateAtVT1;
        float RespRateAtRCP = Constants::RespRateAtRCP;
        float RespDepthAtVT1 = Constants::RespDepthAtVT1;
        float RespDepthAtRCP = Constants::RespDepthAtRCP;
        float MaxRespRate = Constants::MaxRespRate;
        float SleepRespRate = Constants::SleepRespRate;
        float RespOnsetTau = Constants::RespOnsetTau;
        float RespRecoveryTau = Constants::RespRecoveryTau;
        float BreathDepthOnsetTau = Constants::BreathDepthOnsetTau;
        float BreathDepthRecoveryTau = Constants::BreathDepthRecoveryTau;

        float AcuteFatigueMax = Constants::AcuteFatigueMax;
        float AcuteFatigueGainTau = Constants::AcuteFatigueGainTau;
        float AcuteFatigueDecayTau = Constants::AcuteFatigueDecayTau;

        float LongTermFatigueMax = Constants::LongTermFatigueMax;
        float LongTermFatigueGainTau = Constants::LongTermFatigueGainTau;
        float LongTermFatigueDecayTau = Constants::LongTermFatigueDecayTau;
        float SleepRecoveryRate = Constants::SleepRecoveryRate;

        [[nodiscard]] constexpr float FitnessAbsoluteMin() const
        {
            return FitnessBaseMets + (BaseRestingHR - MaxRestingHR) / RestingHRSlope;
        }

        friend constexpr bool operator==(
            const SimulationModelCoefficients &,
            const SimulationModelCoefficients &
        ) = default;
    };

    struct RhythmModelCoefficients
    {
        float RSAAmplitudeRest = Constants::RSAAmplitudeRest;

        float PVCCouplingMax = Constants::PVCCouplingMax;
        float PVCCouplingMin = Constants::PVCCouplingMin;
        float PVCCouplingVariation = Constants::PVCCouplingVariation;
        float PVCPauseVariation = Constants::PVCPauseVariation;
        float PVCChanceNormal = Constants::PVCChanceNormal;
        float PVCChanceMax = Constants::PVCChanceMax;
        float PVCRunExtensionChance = Constants::PVCRunExtensionChance;
        int   PVCRunMaxLength = Constants::PVCRunMaxLength;

        float VigorJitterScale = Constants::VigorJitterScale;
        float VigorJitterMaxSigma = Constants::VigorJitterMaxSigma;

        // Runtime risk calibration lives here rather than borrowing HUD thresholds or literals.
        float DeathRiskRampSeconds = Constants::DeathRiskRampSeconds;
        float ExtremeHeartRateRiskThreshold = Constants::ExtremeHeartRateRiskThreshold;
        float AdrenalineRunRiskScale = Constants::AdrenalineRunRiskScale;

        friend constexpr bool operator==(
            const RhythmModelCoefficients &,
            const RhythmModelCoefficients &
        ) = default;
    };

    struct AcousticMappingCoefficients
    {
        float AttackCompressMax = Constants::AttackCompressMax;
        float ResamplePVCRatio = Constants::ResamplePVCRatio;

        float BreathAmpDepth = Constants::BreathAmpDepth;
        float BreathDepthRestFraction = Constants::BreathDepthRestFraction;
        float BreathPitchDipDepth = Constants::BreathPitchDipDepth;
        float BreathLowPassOpenHz = Constants::BreathLowPassOpenHz;
        float BreathLowPassMinHz = Constants::BreathLowPassMinHz;

        float SystoleIntercept = Constants::SystoleIntercept;
        float SystoleSlope = Constants::SystoleSlope;
        float SystoleMin = Constants::SystoleMin;
        float SystoleMax = Constants::SystoleMax;
        float SystolePEPShortening = Constants::SystolePEPShortening;
        float PVCSystoleScale = Constants::PVCSystoleScale;
        float PVCSystoleMin = Constants::PVCSystoleMin;

        float ContractilityGainDb = Constants::ContractilityGainDb;
        float FrankStarlingMin = Constants::FrankStarlingMin;
        float FrankStarlingMax = Constants::FrankStarlingMax;

        float PVCS1Amplitude = Constants::PVCS1Amplitude;
        float PVCS2Amplitude = Constants::PVCS2Amplitude;
        float PVCS2FailCoupling = Constants::PVCS2FailCoupling;
        float PVCS2FullCoupling = Constants::PVCS2FullCoupling;

        friend constexpr bool operator==(
            const AcousticMappingCoefficients &,
            const AcousticMappingCoefficients &
        ) = default;
    };

    struct SourceConditioningCoefficients
    {
        float SourceHighPassHz = Constants::SourceHighPassHz;
        float SourceRestLevel = Constants::SourceRestLevel;
        float AttackBuildThreshold = Constants::AttackBuildThreshold;

        friend constexpr bool operator==(
            const SourceConditioningCoefficients &,
            const SourceConditioningCoefficients &
        ) = default;
    };

    struct BeatRenderingCoefficients
    {
        float CrossfadeMs = Constants::CrossfadeMs;
        float S1SystoleFraction = Constants::S1SystoleFraction;
        float S2WindowFraction = Constants::S2WindowFraction;
        int   BreathLowPassPoles = Constants::BreathLowPassPoles;
        float SoftClipKnee = Constants::SoftClipKnee;

        friend constexpr bool operator==(
            const BeatRenderingCoefficients &,
            const BeatRenderingCoefficients &
        ) = default;
    };

    class ModelCoefficients
    {
    public:
        ModelCoefficients();
        ModelCoefficients(
            SimulationModelCoefficients    simulation,
            RhythmModelCoefficients        rhythm,
            AcousticMappingCoefficients    acousticMapping,
            SourceConditioningCoefficients sourceConditioning,
            BeatRenderingCoefficients      beatRendering
        );

        const SimulationModelCoefficients    Simulation;
        const RhythmModelCoefficients        Rhythm;
        const AcousticMappingCoefficients    AcousticMapping;
        const SourceConditioningCoefficients SourceConditioning;
        const BeatRenderingCoefficients      BeatRendering;

        friend constexpr bool operator==(
            const ModelCoefficients &,
            const ModelCoefficients &
        ) = default;
    };

    // The immutable production default. Offline callers construct replacements; live owners never mutate it.
    const ModelCoefficients &DefaultModelCoefficients();
}
