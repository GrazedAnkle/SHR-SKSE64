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
#include "ModelCoefficients.hpp"

#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    using NamedValue = std::pair<std::string_view, float>;

    [[noreturn]] void Invalid(std::string_view name, std::string_view requirement)
    {
        throw std::invalid_argument(
            "invalid model coefficient " + std::string(name) + ": " + std::string(requirement)
        );
    }

    void RequireFinite(std::initializer_list<NamedValue> values)
    {
        for (const auto &[name, value] : values)
        {
            if (!std::isfinite(value))
            {
                Invalid(name, "must be finite");
            }
        }
    }

    void RequirePositive(std::string_view name, float value)
    {
        if (value <= 0.0F)
        {
            Invalid(name, "must be positive");
        }
    }

    void RequireNonnegative(std::string_view name, float value)
    {
        if (value < 0.0F)
        {
            Invalid(name, "must be nonnegative");
        }
    }

    void RequireUnitRange(std::string_view name, float value)
    {
        if (value < 0.0F || value > 1.0F)
        {
            Invalid(name, "must be between zero and one");
        }
    }

    void Validate(const SHR::SimulationModelCoefficients &c)
    {
#define SHR_FINITE_MODEL_COEFFICIENT(type, name) { #name, static_cast<float>(c.name) },
        RequireFinite({
            SHR_SIMULATION_MODEL_COEFFICIENTS(SHR_FINITE_MODEL_COEFFICIENT)
        });
#undef SHR_FINITE_MODEL_COEFFICIENT

        RequirePositive("BaseRestingHR", c.BaseRestingHR);
        RequirePositive("HRFormulaCeiling", c.HRFormulaCeiling);
        RequireUnitRange("SleepFraction", c.SleepFraction);
        RequireUnitRange("HRFastFraction", c.HRFastFraction);

        RequirePositive("FastOnsetTauSedentary", c.FastOnsetTauSedentary);
        RequirePositive("FastOnsetTauElite", c.FastOnsetTauElite);
        RequirePositive("SlowOnsetTau", c.SlowOnsetTau);
        RequirePositive("FastRecoveryTauSedentary", c.FastRecoveryTauSedentary);
        RequirePositive("FastRecoveryTauElite", c.FastRecoveryTauElite);
        RequirePositive("SlowRecoveryTau", c.SlowRecoveryTau);

        RequirePositive("IdleMets", c.IdleMets);
        RequireNonnegative("WalkingMets", c.WalkingMets);
        RequireNonnegative("RunningMets", c.RunningMets);
        RequireNonnegative("SprintingMets", c.SprintingMets);
        RequireNonnegative("SwimmingMets", c.SwimmingMets);
        RequireNonnegative("JumpMets", c.JumpMets);
        RequireNonnegative("CrouchMovementMultiplier", c.CrouchMovementMultiplier);
        RequireNonnegative("MountedMultiplier", c.MountedMultiplier);
        RequireNonnegative("ExertionAccumulationRate", c.ExertionAccumulationRate);
        RequireNonnegative("ExertionRecoveryRate", c.ExertionRecoveryRate);

        RequirePositive("AdrenalineHalfLife", c.AdrenalineHalfLife);
        RequireNonnegative("AdrenalineCombatEntry", c.AdrenalineCombatEntry);
        RequireNonnegative("AdrenalineTakeHit", c.AdrenalineTakeHit);
        RequirePositive("ContractilityOnsetTau", c.ContractilityOnsetTau);
        RequirePositive("ContractilityDecayTau", c.ContractilityDecayTau);
        RequirePositive("AdrenalineContractilityScale", c.AdrenalineContractilityScale);

        RequirePositive("FitnessGainTau", c.FitnessGainTau);
        RequirePositive("FitnessDecayTau", c.FitnessDecayTau);
        RequirePositive("RestingHRSlope", c.RestingHRSlope);
        RequirePositive("MaxRestingHR", c.MaxRestingHR);
        if (c.MaxRestingHR < c.BaseRestingHR)
        {
            Invalid("MaxRestingHR", "must be at least BaseRestingHR");
        }
        if (c.FitnessMaxMets < c.FitnessBaseMets)
        {
            Invalid("FitnessMaxMets", "must be at least FitnessBaseMets");
        }
        if (c.FitnessBaseMets <= c.IdleMets)
        {
            Invalid("FitnessBaseMets", "must be greater than IdleMets");
        }
        if (c.FitnessAbsoluteMin() <= c.IdleMets)
        {
            Invalid(
                "FitnessAbsoluteMin",
                "derived value must be greater than IdleMets; adjust BaseRestingHR, MaxRestingHR, "
                "FitnessBaseMets, or RestingHRSlope");
        }

        RequirePositive("RestingRespRate", c.RestingRespRate);
        RequirePositive("SleepRespRate", c.SleepRespRate);
        RequirePositive("MaxRespRate", c.MaxRespRate);
        if (
            c.VentilationVT1Fraction <= 0.0F ||
            c.VentilationVT1Fraction >= c.VentilationRCPFraction ||
            c.VentilationRCPFraction >= 1.0F
        )
        {
            Invalid(
                "VentilationVT1Fraction/VentilationRCPFraction",
                "must satisfy 0 < VT1 < RCP < 1");
        }
        if (
            c.RespRateAtVT1 < c.RestingRespRate ||
            c.RespRateAtRCP < c.RespRateAtVT1 ||
            c.MaxRespRate < c.RespRateAtRCP
        )
        {
            Invalid(
                "RespRateAtVT1/RespRateAtRCP",
                "must be ordered from RestingRespRate through MaxRespRate");
        }
        RequireUnitRange("RespDepthAtVT1", c.RespDepthAtVT1);
        RequireUnitRange("RespDepthAtRCP", c.RespDepthAtRCP);
        if (c.RespDepthAtVT1 > c.RespDepthAtRCP)
        {
            Invalid("RespDepthAtVT1", "must not exceed RespDepthAtRCP");
        }
        RequirePositive("RespOnsetTau", c.RespOnsetTau);
        RequirePositive("RespRecoveryTau", c.RespRecoveryTau);
        RequirePositive("BreathDepthOnsetTau", c.BreathDepthOnsetTau);
        RequirePositive("BreathDepthRecoveryTau", c.BreathDepthRecoveryTau);

        RequirePositive("AcuteFatigueMax", c.AcuteFatigueMax);
        RequirePositive("AcuteFatigueGainTau", c.AcuteFatigueGainTau);
        RequirePositive("AcuteFatigueDecayTau", c.AcuteFatigueDecayTau);
        RequirePositive("LongTermFatigueMax", c.LongTermFatigueMax);
        RequirePositive("LongTermFatigueGainTau", c.LongTermFatigueGainTau);
        RequirePositive("LongTermFatigueDecayTau", c.LongTermFatigueDecayTau);
        RequireNonnegative("SleepRecoveryRate", c.SleepRecoveryRate);
    }

    void Validate(const SHR::RhythmModelCoefficients &c)
    {
#define SHR_FINITE_MODEL_COEFFICIENT(type, name) { #name, static_cast<float>(c.name) },
        RequireFinite({
            SHR_RHYTHM_MODEL_COEFFICIENTS(SHR_FINITE_MODEL_COEFFICIENT)
        });
#undef SHR_FINITE_MODEL_COEFFICIENT

        RequireUnitRange("RSAAmplitudeRest", c.RSAAmplitudeRest);
        RequireUnitRange("PVCCouplingMin", c.PVCCouplingMin);
        RequireUnitRange("PVCCouplingMax", c.PVCCouplingMax);
        RequirePositive("PVCCouplingMin", c.PVCCouplingMin);
        if (c.PVCCouplingMin > c.PVCCouplingMax)
        {
            Invalid("PVCCouplingMin", "must not exceed PVCCouplingMax");
        }
        RequireUnitRange("PVCCouplingVariation", c.PVCCouplingVariation);
        RequireUnitRange("PVCPauseVariation", c.PVCPauseVariation);
        RequireNonnegative("PVCChanceNormal", c.PVCChanceNormal);
        RequireNonnegative("PVCChanceMax", c.PVCChanceMax);
        if (c.PVCChanceNormal > c.PVCChanceMax)
        {
            Invalid("PVCChanceNormal", "must not exceed PVCChanceMax");
        }
        RequireUnitRange("PVCRunExtensionChance", c.PVCRunExtensionChance);
        if (c.PVCRunMaxLength < 1)
        {
            Invalid("PVCRunMaxLength", "must be at least one");
        }
        RequireNonnegative("VigorJitterScale", c.VigorJitterScale);
        RequireNonnegative("VigorJitterMaxSigma", c.VigorJitterMaxSigma);
        RequirePositive("DeathRiskRampSeconds", c.DeathRiskRampSeconds);
        RequirePositive("ExtremeHeartRateRiskThreshold", c.ExtremeHeartRateRiskThreshold);
        RequirePositive("AdrenalineRunRiskScale", c.AdrenalineRunRiskScale);
    }

    void Validate(const SHR::AcousticMappingCoefficients &c)
    {
#define SHR_FINITE_MODEL_COEFFICIENT(type, name) { #name, static_cast<float>(c.name) },
        RequireFinite({
            SHR_ACOUSTIC_MAPPING_COEFFICIENTS(SHR_FINITE_MODEL_COEFFICIENT)
        });
#undef SHR_FINITE_MODEL_COEFFICIENT

        if (c.AttackCompressMax < 1.0F)
        {
            Invalid("AttackCompressMax", "must be at least one");
        }
        if (c.ResamplePVCRatio <= 0.0F || c.ResamplePVCRatio > 1.0F)
        {
            Invalid("ResamplePVCRatio", "must be greater than zero and at most one");
        }
        RequireUnitRange("BreathAmpDepth", c.BreathAmpDepth);
        RequireUnitRange("BreathDepthRestFraction", c.BreathDepthRestFraction);
        if (c.BreathPitchDipDepth < 0.0F || c.BreathPitchDipDepth >= 1.0F)
        {
            Invalid("BreathPitchDipDepth", "must be at least zero and less than one");
        }
        RequireNonnegative("BreathLowPassMinHz", c.BreathLowPassMinHz);
        RequireNonnegative("BreathLowPassOpenHz", c.BreathLowPassOpenHz);
        if (c.BreathLowPassMinHz > c.BreathLowPassOpenHz)
        {
            Invalid("BreathLowPassMinHz", "must not exceed BreathLowPassOpenHz");
        }

        RequirePositive("SystoleIntercept", c.SystoleIntercept);
        RequireNonnegative("SystoleSlope", c.SystoleSlope);
        RequirePositive("SystoleMin", c.SystoleMin);
        if (c.SystoleMax < c.SystoleMin)
        {
            Invalid("SystoleMax", "must be at least SystoleMin");
        }
        RequireNonnegative("SystolePEPShortening", c.SystolePEPShortening);
        RequirePositive("PVCSystoleScale", c.PVCSystoleScale);
        RequirePositive("PVCSystoleMin", c.PVCSystoleMin);

        RequireUnitRange("FrankStarlingMin", c.FrankStarlingMin);
        if (c.FrankStarlingMax < 1.0F || c.FrankStarlingMax <= c.FrankStarlingMin)
        {
            Invalid("FrankStarlingMax", "must be at least one and greater than FrankStarlingMin");
        }
        RequireNonnegative("PVCS1Amplitude", c.PVCS1Amplitude);
        RequireNonnegative("PVCS2Amplitude", c.PVCS2Amplitude);
        RequireUnitRange("PVCS2FailCoupling", c.PVCS2FailCoupling);
        RequireUnitRange("PVCS2FullCoupling", c.PVCS2FullCoupling);
        if (c.PVCS2FailCoupling >= c.PVCS2FullCoupling)
        {
            Invalid("PVCS2FailCoupling", "must be less than PVCS2FullCoupling");
        }
    }

    void Validate(const SHR::SourceConditioningCoefficients &c)
    {
#define SHR_FINITE_MODEL_COEFFICIENT(type, name) { #name, static_cast<float>(c.name) },
        RequireFinite({
            SHR_SOURCE_CONDITIONING_COEFFICIENTS(SHR_FINITE_MODEL_COEFFICIENT)
        });
#undef SHR_FINITE_MODEL_COEFFICIENT
        RequireNonnegative("SourceHighPassHz", c.SourceHighPassHz);
        RequireUnitRange("SourceRestLevel", c.SourceRestLevel);
        RequireUnitRange("AttackBuildThreshold", c.AttackBuildThreshold);
    }

    void Validate(const SHR::BeatRenderingCoefficients &c)
    {
#define SHR_FINITE_MODEL_COEFFICIENT(type, name) { #name, static_cast<float>(c.name) },
        RequireFinite({
            SHR_BEAT_RENDERING_COEFFICIENTS(SHR_FINITE_MODEL_COEFFICIENT)
        });
#undef SHR_FINITE_MODEL_COEFFICIENT
        RequireNonnegative("CrossfadeMs", c.CrossfadeMs);
        RequireUnitRange("S1SystoleFraction", c.S1SystoleFraction);
        RequireUnitRange("S2WindowFraction", c.S2WindowFraction);
        if (c.BreathLowPassPoles < 1)
        {
            Invalid("BreathLowPassPoles", "must be at least one");
        }
        if (c.SoftClipKnee <= 0.0F || c.SoftClipKnee >= 1.0F)
        {
            Invalid("SoftClipKnee", "must be greater than zero and less than one");
        }
    }
}

SHR::ModelCoefficients::ModelCoefficients()
    : ModelCoefficients(
          SimulationModelCoefficients{ },
          RhythmModelCoefficients{ },
          AcousticMappingCoefficients{ },
          SourceConditioningCoefficients{ },
          BeatRenderingCoefficients{ }
      )
{
}

SHR::ModelCoefficients::ModelCoefficients(
    SimulationModelCoefficients    simulation,
    RhythmModelCoefficients        rhythm,
    AcousticMappingCoefficients    acousticMapping,
    SourceConditioningCoefficients sourceConditioning,
    BeatRenderingCoefficients      beatRendering
)
    : Simulation(std::move(simulation))
    , Rhythm(std::move(rhythm))
    , AcousticMapping(std::move(acousticMapping))
    , SourceConditioning(std::move(sourceConditioning))
    , BeatRendering(std::move(beatRendering))
{
    Validate(Simulation);
    Validate(Rhythm);
    Validate(AcousticMapping);
    Validate(SourceConditioning);
    Validate(BeatRendering);
}

const SHR::ModelCoefficients &SHR::DefaultModelCoefficients()
{
    static const ModelCoefficients value;
    return value;
}
