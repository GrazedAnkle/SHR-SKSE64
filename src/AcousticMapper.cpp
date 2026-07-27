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
#include "AcousticMapper.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    float PVCS2Fraction(
        float                                    coupling,
        const SHR::AcousticMappingCoefficients  &coefficients
    )
    {
        return std::clamp(
            (coupling - coefficients.PVCS2FailCoupling) /
                (coefficients.PVCS2FullCoupling - coefficients.PVCS2FailCoupling),
            0.0F,
            1.0F
        );
    }
}

float SHR::ComputeLungInflation(float respirationPhase)
{
    return std::sin(
        std::numbers::pi_v<float> * std::clamp(respirationPhase, 0.0F, 1.0F)
    );
}

SHR::RenderSpec SHR::CreateRenderSpec(
    const BeatEvent &event,
    const PhysiologySnapshot &physiology
)
{
    return CreateRenderSpec(event, physiology, DefaultModelCoefficients().AcousticMapping);
}

SHR::RenderSpec SHR::CreateRenderSpec(
    const BeatEvent                   &event,
    const PhysiologySnapshot          &physiology,
    const AcousticMappingCoefficients &coefficients
)
{
    const bool isPVC = event.Kind == BeatKind::PVC;
    const float nominalIBI = 60.0F / physiology.HeartRate;
    const float frankStarling = std::clamp(
        event.FillingInterval / nominalIBI,
        coefficients.FrankStarlingMin,
        coefficients.FrankStarlingMax
    );

    const float contractilityGain = std::pow(
        10.0F,
        (coefficients.ContractilityGainDb / 20.0F) * event.Vigor
    );
    const float sourceS1Amplitude = isPVC
        ? coefficients.PVCS1Amplitude * frankStarling
        : frankStarling * contractilityGain;
    const float sourceS2Amplitude = isPVC
        ? coefficients.PVCS2Amplitude * PVCS2Fraction(event.CouplingFraction, coefficients)
        : 1.0F;

    const float nominalSystole = std::clamp(
        coefficients.SystoleIntercept - physiology.HeartRate * coefficients.SystoleSlope,
        coefficients.SystoleMin,
        coefficients.SystoleMax
    );
    const float sinusSystole = std::max(
        nominalSystole - coefficients.SystolePEPShortening * physiology.ContractilityExcess,
        coefficients.SystoleMin
    );
    const float systoleDuration = isPVC
        ? std::max(
            nominalSystole * coefficients.PVCSystoleScale,
            coefficients.PVCSystoleMin
        )
        : sinusSystole;

    const float lungInflation = ComputeLungInflation(physiology.RespirationPhase);
    const float breathDepthFactor = coefficients.BreathDepthRestFraction +
        (1.0F - coefficients.BreathDepthRestFraction) *
        std::clamp(physiology.RespirationDepth, 0.0F, 1.0F);
    const float breathAmplitude = 1.0F -
        coefficients.BreathAmpDepth * breathDepthFactor * lungInflation;
    const float breathMuffle = std::clamp(
        breathDepthFactor * lungInflation,
        0.0F,
        1.0F
    );
    const float pitchDip = 1.0F - coefficients.BreathPitchDipDepth * breathMuffle;

    const float normalizedFilling = std::clamp(
        (frankStarling - coefficients.FrankStarlingMin) /
            (coefficients.FrankStarlingMax - coefficients.FrankStarlingMin),
        0.0F,
        1.0F
    );
    const float vigor = std::clamp(event.Vigor, 0.0F, 3.0F);
    const float onsetCompression = 1.0F +
        (coefficients.AttackCompressMax - 1.0F) * vigor * normalizedFilling;

    return {
        .IBI              = event.IBI,
        .SystoleDuration  = systoleDuration,
        .S1Amplitude      = sourceS1Amplitude * breathAmplitude,
        .S2Amplitude      = sourceS2Amplitude * breathAmplitude,
        .S1ResampleRatio  = (isPVC ? coefficients.ResamplePVCRatio : 1.0F) * pitchDip,
        .S2ResampleRatio  = pitchDip,
        .LowPassCutoffHz  = std::lerp(
            coefficients.BreathLowPassOpenHz,
            coefficients.BreathLowPassMinHz,
            breathMuffle
        ),
        .OnsetCompression = onsetCompression,
        .Kind             = event.Kind,
    };
}
