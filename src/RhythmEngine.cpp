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
#include "RhythmEngine.hpp"

#include "Constants.hpp"
#include "Random.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
    namespace C = SHR::Constants;

    // Clamp coupling to a linear [0, 1] ramp between the failed- and full-perfusion thresholds.
    float PVCS2Fraction(float coupling)
    {
        return std::clamp(
            (coupling - C::PVCS2FailCoupling) / (C::PVCS2FullCoupling - C::PVCS2FailCoupling),
            0.0F,
            1.0F
        );
    }

    float BaseIBI(float heartRate, float respPhase, float exertionFraction)
    {
        const float nominalIBI = 60.0F / heartRate;
        // Apply RSA around the nominal interval, fading its amplitude with exertion.
        const float rsaAmplitude = C::RSAAmplitudeRest * (1.0F - exertionFraction);
        const float jitter = rsaAmplitude * std::sin(2.0F * std::numbers::pi_v<float> * respPhase);
        return std::max(0.1F, nominalIBI * (1.0F - jitter));
    }
}

void SHR::RhythmEngine::Init()
{
    m_ElapsedSinceBeat = 0.0F;
    m_NextIBI          = 0.0F;
    m_PauseDuration    = 0.0F;
    m_PrecedingRR      = 0.0F;
    m_PVCCoupling      = 0.0F;
    m_PendingPVCIBI    = 0.0F;
    m_RunRemaining     = 0;
    m_PVCPending       = false;
    m_DidJustPVC       = false;
    m_InPause          = false;
}

SHR::RhythmEngine::Beat SHR::RhythmEngine::Advance(
    float delta,
    float heartRate,
    float respPhase,
    float exertionFraction,
    float breathDepth,
    float contractility,
    float contractilityExcess,
    float pvcChancePerSecond,
    float riskFactor,
    float runExtensionChance
)
{
    auto noFire = [&]() -> Beat {
        return {
            .ShouldFire       = false,
            .IBI              = 0.0F,
            .SystoleDuration  = 0.0F,
            .S1Amplitude      = 0.0F,
            .S2Amplitude      = 0.0F,
            .RespPhase        = respPhase,
            .ExertionFraction = exertionFraction,
            .BreathDepth      = breathDepth,
            .Contractility    = contractility,
            .FrankStarling    = 1.0F,
            .IsPVC            = false,
        };
    };

    m_ElapsedSinceBeat += delta;

    if (m_InPause)
    {
        if (m_ElapsedSinceBeat < m_PauseDuration)
        {
            return noFire();
        }

        m_InPause = false;
        m_ElapsedSinceBeat -= m_PauseDuration;
        // WI-019: seeding a full IBI here delays the next sinus beat beyond the completed pause.
        m_NextIBI = BaseIBI(heartRate, respPhase, exertionFraction);
    }

    // Fire the next in-run PVC at the inter-ectopic interval.
    if (m_RunRemaining > 0)
    {
        if (m_ElapsedSinceBeat < m_PendingPVCIBI)
        {
            return noFire();
        }

        m_ElapsedSinceBeat -= m_PendingPVCIBI;
        --m_RunRemaining;
        m_DidJustPVC = true;
        // Pause only after the last run PVC; its duration was fixed when PVC1 started the run.
        if (m_RunRemaining == 0) m_InPause = true;

        const float nominalIBI2    = 60.0F / heartRate;
        const float frankStarling2 = std::clamp(
            m_PendingPVCIBI / nominalIBI2,
            C::FrankStarlingMin,
            C::FrankStarlingMax
        );
        const float nominalSystole2 = std::clamp(
            C::SystoleIntercept - heartRate * C::SystoleSlope,
            C::SystoleMin,
            C::SystoleMax
        );
        const float systoleDuration = std::max(nominalSystole2 * C::PVCSystoleScale, C::PVCSystoleMin);
        const float couplingInRun = m_PendingPVCIBI / nominalIBI2;

        return {
            .ShouldFire       = true,
            .IBI              = m_PendingPVCIBI,
            .SystoleDuration  = systoleDuration,
            .S1Amplitude      = C::PVCS1Amplitude * frankStarling2,
            .S2Amplitude      = C::PVCS2Amplitude * PVCS2Fraction(couplingInRun),
            .RespPhase        = respPhase,
            .ExertionFraction = exertionFraction,
            .BreathDepth      = breathDepth,
            .Contractility    = contractility,
            .FrankStarling    = frankStarling2,
            .IsPVC            = true,
        };
    }

    // First call: seed the interval now that we know the expected rhythm.
    if (m_NextIBI == 0.0F)
    {
        m_NextIBI = BaseIBI(heartRate, respPhase, exertionFraction);
    }

    // Sample until a PVC is pending; after a PVC, require one sinus beat before sampling again.
    if (!m_PVCPending && !m_DidJustPVC)
    {
        if (Random(0.0F, 1.0F) < pvcChancePerSecond * delta)
        {
            m_PVCPending  = true;
            // Sample coupling once so the firing target and later pause use the same value.
            m_PVCCoupling = std::clamp(
                std::lerp(C::PVCCouplingMax, C::PVCCouplingMin, riskFactor)
                    * Random(1.0F - C::PVCCouplingVariation, 1.0F + C::PVCCouplingVariation),
                C::PVCCouplingMin,
                C::PVCCouplingMax
            );
        }
    }

    const float coupling = m_PVCPending ? m_PVCCoupling : 0.0F;
    const float effectiveIBI = m_PVCPending ? coupling * m_NextIBI : m_NextIBI;

    if (m_ElapsedSinceBeat < effectiveIBI)
    {
        return noFire();
    }

    const bool isPVC = m_PVCPending;
    m_PVCPending = false;
    m_DidJustPVC = isPVC;

    // Carry overshoot into the next interval so timing self-corrects.
    m_ElapsedSinceBeat -= effectiveIBI;

    // Use the preceding sinus interval, but the shortened coupling interval for a PVC.
    // This is a bounded cycle-length proxy, not literal diastolic time.
    const float nominalIBI    = 60.0F / heartRate;
    const float precedingRR   = (m_PrecedingRR > 0.0F) ? m_PrecedingRR : nominalIBI;
    const float frankStarlingRR = isPVC ? effectiveIBI : precedingRR;
    const float frankStarling = std::clamp(
        frankStarlingRR / nominalIBI,
        C::FrankStarlingMin,
        C::FrankStarlingMax
    );

    // Apply bounded Gaussian jitter to sinus beats; PVCs retain mean contractility.
    const float vigor = isPVC
        ? contractility
        : std::max(0.0F, contractility * (1.0F + C::VigorJitterScale
            * std::min(C::VigorJitterMaxSigma, RandomNormal())));

    const float contractilityGain = std::pow(10.0F, (C::ContractilityGainDb / 20.0F) * vigor);
    const float s1Amplitude = isPVC
        ? C::PVCS1Amplitude * frankStarling
        : frankStarling * contractilityGain;
    const float s2Amplitude = isPVC
        ? C::PVCS2Amplitude * PVCS2Fraction(coupling)
        : 1.0F;

    // Use smoothed HR rather than per-beat IBI so irregular intervals do not move systole independently.
    const float nominalSystole = std::clamp(
        C::SystoleIntercept - heartRate * C::SystoleSlope,
        C::SystoleMin,
        C::SystoleMax
    );
    // Only contractility above the HR-implied steady state shortens sinus systole.
    const float sinusSystole = std::max(
        nominalSystole - C::SystolePEPShortening * contractilityExcess,
        C::SystoleMin
    );
    // PVC systole uses the unshortened nominal value, independent of sympathetic PEP shortening.
    const float systoleDuration = isPVC
        ? std::max(nominalSystole * C::PVCSystoleScale, C::PVCSystoleMin)
        : sinusSystole;

    if (isPVC)
    {
        // Target the next sinus impulse at two nominal cycles after the preceding sinus beat.
        const float fullPause  = (2.0F - coupling) * m_NextIBI;
        const float pendingIBI = C::PVCCouplingMin * m_NextIBI;

        // Hand-rolled geometric sampling supports runExtensionChance == 1.
        int runExtensions = 0;
        while (runExtensions < C::PVCRunMaxLength - 1 &&
               Random(0.0F, 1.0F) < runExtensionChance)
        {
            ++runExtensions;
        }

        if (runExtensions > 0)
        {
            m_RunRemaining  = runExtensions;
            m_PendingPVCIBI = pendingIBI;
            // Compensatory window minus all in-run IEIs, floored at 30% IBI.
            const float remaining = std::max(
                fullPause - static_cast<float>(runExtensions) * pendingIBI,
                0.30F * m_NextIBI
            );
            m_PauseDuration = remaining *
                Random(1.0F - C::PVCPauseVariation, 1.0F + C::PVCPauseVariation);
        }
        else
        {
            m_InPause = true;
            m_PauseDuration = fullPause *
                Random(1.0F - C::PVCPauseVariation, 1.0F + C::PVCPauseVariation);
        }

        m_PrecedingRR = m_PauseDuration;
    }
    else
    {
        m_DidJustPVC  = false;
        m_NextIBI     = BaseIBI(heartRate, respPhase, exertionFraction);
        m_PrecedingRR = effectiveIBI;
    }

    return {
        .ShouldFire       = true,
        .IBI              = effectiveIBI,
        .SystoleDuration  = systoleDuration,
        .S1Amplitude      = s1Amplitude,
        .S2Amplitude      = s2Amplitude,
        .RespPhase        = respPhase,
        .ExertionFraction = exertionFraction,
        .BreathDepth      = breathDepth,
        .Contractility    = vigor,
        .FrankStarling    = frankStarling,
        .IsPVC            = isPVC,
    };
}
