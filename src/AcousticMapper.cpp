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

#include "Constants.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    namespace C = SHR::Constants;

    float PVCS2Fraction(float coupling)
    {
        return std::clamp(
            (coupling - C::PVCS2FailCoupling) / (C::PVCS2FullCoupling - C::PVCS2FailCoupling),
            0.0F,
            1.0F
        );
    }
}

SHR::RenderSpec SHR::CreateRenderSpec(
    const BeatEvent &event,
    const PhysiologySnapshot &physiology
)
{
    const bool isPVC = event.Kind == BeatKind::PVC;
    const float nominalIBI = 60.0F / physiology.HeartRate;
    const float frankStarling = std::clamp(
        event.FillingInterval / nominalIBI,
        C::FrankStarlingMin,
        C::FrankStarlingMax
    );

    const float contractilityGain = std::pow(
        10.0F,
        (C::ContractilityGainDb / 20.0F) * event.Vigor
    );
    const float sourceS1Amplitude = isPVC
        ? C::PVCS1Amplitude * frankStarling
        : frankStarling * contractilityGain;
    const float sourceS2Amplitude = isPVC
        ? C::PVCS2Amplitude * PVCS2Fraction(event.CouplingFraction)
        : 1.0F;

    const float nominalSystole = std::clamp(
        C::SystoleIntercept - physiology.HeartRate * C::SystoleSlope,
        C::SystoleMin,
        C::SystoleMax
    );
    const float sinusSystole = std::max(
        nominalSystole - C::SystolePEPShortening * physiology.ContractilityExcess,
        C::SystoleMin
    );
    const float systoleDuration = isPVC
        ? std::max(nominalSystole * C::PVCSystoleScale, C::PVCSystoleMin)
        : sinusSystole;

    const float lungInflation = std::sin(
        std::numbers::pi_v<float> * std::clamp(physiology.RespirationPhase, 0.0F, 1.0F)
    );
    const float breathDepthFactor = C::BreathDepthRestFraction +
        (1.0F - C::BreathDepthRestFraction) *
        std::clamp(physiology.RespirationDepth, 0.0F, 1.0F);
    const float breathAmplitude = 1.0F -
        C::BreathAmpDepth * breathDepthFactor * lungInflation;
    const float breathMuffle = std::clamp(
        breathDepthFactor * lungInflation,
        0.0F,
        1.0F
    );
    const float pitchDip = 1.0F - C::BreathPitchDipDepth * breathMuffle;

    const float normalizedFilling = std::clamp(
        (frankStarling - C::FrankStarlingMin) /
            (C::FrankStarlingMax - C::FrankStarlingMin),
        0.0F,
        1.0F
    );
    const float vigor = std::clamp(event.Vigor, 0.0F, 3.0F);
    const float onsetCompression = 1.0F +
        (C::AttackCompressMax - 1.0F) * vigor * normalizedFilling;

    return {
        .IBI              = event.IBI,
        .SystoleDuration  = systoleDuration,
        .S1Amplitude      = sourceS1Amplitude * breathAmplitude,
        .S2Amplitude      = sourceS2Amplitude * breathAmplitude,
        .S1ResampleRatio  = (isPVC ? C::ResamplePVCRatio : 1.0F) * pitchDip,
        .S2ResampleRatio  = pitchDip,
        .LowPassCutoffHz  = std::lerp(
            C::BreathLowPassOpenHz,
            C::BreathLowPassMinHz,
            breathMuffle
        ),
        .OnsetCompression = onsetCompression,
        .Kind             = event.Kind,
    };
}
