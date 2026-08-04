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

#include <cstddef>
#include <cstdint>

// Structural catalog for the immutable model surface. Defaults, units, domains, semantics, and provenance
// remain in Constants.hpp. Each list accepts X(scalar_type, stable_name).
#define SHR_SIMULATION_MODEL_COEFFICIENTS(X) \
    X(float, BaseRestingHR)                   \
    X(float, SleepFraction)                   \
    X(float, HRFormulaCeiling)                \
    X(float, HRFastFraction)                  \
    X(float, FastOnsetTauSedentary)           \
    X(float, FastOnsetTauElite)               \
    X(float, SlowOnsetTau)                    \
    X(float, FastRecoveryTauSedentary)        \
    X(float, FastRecoveryTauElite)            \
    X(float, SlowRecoveryTau)                 \
    X(float, IdleMets)                        \
    X(float, WalkingMets)                     \
    X(float, RunningMets)                     \
    X(float, SprintingMets)                   \
    X(float, SwimmingMets)                    \
    X(float, JumpMets)                        \
    X(float, CrouchMovementMultiplier)        \
    X(float, MountedMultiplier)               \
    X(float, ExertionAccumulationRate)        \
    X(float, ExertionRecoveryRate)            \
    X(float, AdrenalineHalfLife)              \
    X(float, AdrenalineCombatEntry)           \
    X(float, AdrenalineTakeHit)               \
    X(float, ContractilityOnsetTau)           \
    X(float, ContractilityDecayTau)           \
    X(float, AdrenalineContractilityScale)    \
    X(float, FitnessGainTau)                  \
    X(float, FitnessDecayTau)                 \
    X(float, FitnessBaseMets)                 \
    X(float, FitnessEliteMets)                \
    X(float, RestingHRSlope)                  \
    X(float, MaxRestingHR)                    \
    X(float, RestingRespRate)                 \
    X(float, VentilationVT1Fraction)          \
    X(float, VentilationRCPFraction)          \
    X(float, RespRateAtVT1)                   \
    X(float, RespRateAtRCP)                   \
    X(float, RespDepthAtVT1)                  \
    X(float, RespDepthAtRCP)                  \
    X(float, MaxRespRate)                     \
    X(float, SleepRespRate)                   \
    X(float, RespOnsetTau)                    \
    X(float, RespRecoveryTau)                 \
    X(float, BreathDepthOnsetTau)             \
    X(float, BreathDepthRecoveryTau)          \
    X(float, AcuteFatigueMaxFraction)         \
    X(float, AcuteFatigueGainTau)             \
    X(float, AcuteFatigueDecayTau)            \
    X(float, LongTermFatigueMaxFraction)      \
    X(float, LongTermFatigueGainTau)          \
    X(float, LongTermFatigueDecayTau)         \
    X(float, SleepRecoveryRate)

#define SHR_RHYTHM_MODEL_COEFFICIENTS(X)  \
    X(float, RSAAmplitudeRest)            \
    X(float, PVCCouplingMax)              \
    X(float, PVCCouplingMin)              \
    X(float, PVCCouplingVariation)        \
    X(float, PVCPauseVariation)           \
    X(float, PVCChanceNormal)             \
    X(float, PVCChanceMax)                \
    X(float, PVCRunExtensionChance)        \
    X(int, PVCRunMaxLength)                \
    X(float, VigorJitterScale)             \
    X(float, VigorJitterMaxSigma)          \
    X(float, DeathRiskRampSeconds)         \
    X(float, ExtremeHeartRateRiskThreshold) \
    X(float, AdrenalineRunRiskScale)

#define SHR_ACOUSTIC_MAPPING_COEFFICIENTS(X) \
    X(float, AttackCompressMax)               \
    X(float, ResamplePVCRatio)                \
    X(float, BreathAmpDepth)                  \
    X(float, BreathDepthRestFraction)         \
    X(float, BreathPitchDipDepth)             \
    X(float, BreathLowPassOpenHz)             \
    X(float, BreathLowPassMinHz)               \
    X(float, SystoleIntercept)                 \
    X(float, SystoleSlope)                     \
    X(float, SystoleMin)                       \
    X(float, SystoleMax)                       \
    X(float, SystolePEPShortening)             \
    X(float, PVCSystoleScale)                  \
    X(float, PVCSystoleMin)                    \
    X(float, ContractilityGainDb)              \
    X(float, FrankStarlingMin)                 \
    X(float, FrankStarlingMax)                 \
    X(float, PVCS1Amplitude)                   \
    X(float, PVCS2Amplitude)                   \
    X(float, PVCS2FailCoupling)                \
    X(float, PVCS2FullCoupling)

#define SHR_SOURCE_CONDITIONING_COEFFICIENTS(X) \
    X(float, SourceHighPassHz)                   \
    X(float, SourceRestLevel)                    \
    X(float, AttackBuildThreshold)

#define SHR_BEAT_RENDERING_COEFFICIENTS(X) \
    X(float, CrossfadeMs)                   \
    X(float, S1SystoleFraction)             \
    X(float, S2WindowFraction)              \
    X(int, BreathLowPassPoles)               \
    X(float, SoftClipKnee)

// X(public_group, group_type, field_list)
#define SHR_MODEL_COEFFICIENT_GROUPS(X)                                           \
    X(Simulation, SimulationModelCoefficients, SHR_SIMULATION_MODEL_COEFFICIENTS) \
    X(Rhythm, RhythmModelCoefficients, SHR_RHYTHM_MODEL_COEFFICIENTS)              \
    X(AcousticMapping, AcousticMappingCoefficients, SHR_ACOUSTIC_MAPPING_COEFFICIENTS) \
    X(SourceConditioning, SourceConditioningCoefficients, SHR_SOURCE_CONDITIONING_COEFFICIENTS) \
    X(BeatRendering, BeatRenderingCoefficients, SHR_BEAT_RENDERING_COEFFICIENTS)

// Constants that deliberately do not belong to the live override surface.
// X(classification, scalar_type, stable_name, rejection_reason)
#define SHR_NONLIVE_MODEL_CONSTANTS(X)                                                        \
    X(AssetFixed, std::uint32_t, S1OnsetFrames, "is fixed by the heartbeat source asset")     \
    X(AssetFixed, std::uint32_t, S1EndFrames, "is fixed by the heartbeat source asset")       \
    X(AssetFixed, std::uint32_t, S2OnsetFrames, "is fixed by the heartbeat source asset")     \
    X(AssetFixed, std::uint32_t, S2EndFrames, "is fixed by the heartbeat source asset")       \
    X(Derived, float, FitnessAbsoluteMin, "is derived from FitnessBaseMets, BaseRestingHR, MaxRestingHR, and RestingHRSlope") \
    X(Utility, float, SecondsPerHour, "is a unit conversion rather than a model coefficient") \
    X(Utility, float, MetsToVO2, "is a unit conversion rather than a model coefficient")     \
    X(GameIntegration, float, VoiceOutputGain, "belongs to downstream game-mix integration")  \
    X(Dormant, float, InspirationFraction, "is dormant pending the state-dependent breath-curve work")

namespace SHR::Detail
{
    inline constexpr std::size_t LiveModelCoefficientCount =
        0
#define SHR_COUNT_LIVE_MODEL_COEFFICIENT(type, name) + 1
        SHR_SIMULATION_MODEL_COEFFICIENTS(SHR_COUNT_LIVE_MODEL_COEFFICIENT)
        SHR_RHYTHM_MODEL_COEFFICIENTS(SHR_COUNT_LIVE_MODEL_COEFFICIENT)
        SHR_ACOUSTIC_MAPPING_COEFFICIENTS(SHR_COUNT_LIVE_MODEL_COEFFICIENT)
        SHR_SOURCE_CONDITIONING_COEFFICIENTS(SHR_COUNT_LIVE_MODEL_COEFFICIENT)
        SHR_BEAT_RENDERING_COEFFICIENTS(SHR_COUNT_LIVE_MODEL_COEFFICIENT)
#undef SHR_COUNT_LIVE_MODEL_COEFFICIENT
        ;

    // Duplicate names in either catalog are a compile error even if a binding lookup would otherwise hide them.
    enum class RegisteredModelConstantName
    {
#define SHR_REGISTER_LIVE_NAME(type, name) name,
        SHR_SIMULATION_MODEL_COEFFICIENTS(SHR_REGISTER_LIVE_NAME)
        SHR_RHYTHM_MODEL_COEFFICIENTS(SHR_REGISTER_LIVE_NAME)
        SHR_ACOUSTIC_MAPPING_COEFFICIENTS(SHR_REGISTER_LIVE_NAME)
        SHR_SOURCE_CONDITIONING_COEFFICIENTS(SHR_REGISTER_LIVE_NAME)
        SHR_BEAT_RENDERING_COEFFICIENTS(SHR_REGISTER_LIVE_NAME)
#undef SHR_REGISTER_LIVE_NAME
#define SHR_REGISTER_NONLIVE_NAME(classification, type, name, reason) name,
        SHR_NONLIVE_MODEL_CONSTANTS(SHR_REGISTER_NONLIVE_NAME)
#undef SHR_REGISTER_NONLIVE_NAME
    };
}
