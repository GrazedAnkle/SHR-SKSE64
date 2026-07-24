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
#include <utility>

namespace
{
    namespace C = SHR::Constants;

    float BaseIBI(float heartRate, float respPhase, float exertionFraction)
    {
        const float nominalIBI = 60.0F / heartRate;
        // Apply RSA around the nominal interval, fading its amplitude with exertion.
        const float rsaAmplitude = C::RSAAmplitudeRest * (1.0F - exertionFraction);
        const float jitter = rsaAmplitude * std::sin(2.0F * std::numbers::pi_v<float> * respPhase);
        return std::max(0.1F, nominalIBI * (1.0F - jitter));
    }
}

SHR::RhythmEngine::RhythmEngine()
    : RhythmEngine({
        .Uniform = [](float min, float max) {
            return Random(min, max);
        },
        .StandardNormal = []() {
            return RandomNormal();
        },
    })
{
}

SHR::RhythmEngine::RhythmEngine(RhythmRandom random)
    : m_Random(std::move(random))
{
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

std::optional<SHR::BeatEvent> SHR::RhythmEngine::Advance(const RhythmInput &input)
{
    const float delta               = input.DeltaSeconds;
    const float heartRate           = input.HeartRate;
    const float respPhase           = input.RespirationPhase;
    const float exertionFraction    = input.ExertionFraction;
    const float contractility       = input.Contractility;
    const float pvcChancePerSecond  = input.PVCChancePerSecond;
    const float riskFactor          = input.RiskFactor;
    const float runExtensionChance  = input.RunExtensionChance;

    m_ElapsedSinceBeat += delta;

    if (m_InPause)
    {
        if (m_ElapsedSinceBeat < m_PauseDuration)
        {
            return std::nullopt;
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
            return std::nullopt;
        }

        m_ElapsedSinceBeat -= m_PendingPVCIBI;
        --m_RunRemaining;
        m_DidJustPVC = true;
        // Pause only after the last run PVC; its duration was fixed when PVC1 started the run.
        if (m_RunRemaining == 0) m_InPause = true;

        const float nominalIBI    = 60.0F / heartRate;
        const float couplingInRun = m_PendingPVCIBI / nominalIBI;

        return BeatEvent{
            .IBI              = m_PendingPVCIBI,
            .FillingInterval  = m_PendingPVCIBI,
            .CouplingFraction = couplingInRun,
            .Vigor            = contractility,
            .Kind             = BeatKind::PVC,
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
        if (m_Random.Uniform(0.0F, 1.0F) < pvcChancePerSecond * delta)
        {
            m_PVCPending  = true;
            // Sample coupling once so the firing target and later pause use the same value.
            m_PVCCoupling = std::clamp(
                std::lerp(C::PVCCouplingMax, C::PVCCouplingMin, riskFactor)
                    * m_Random.Uniform(
                        1.0F - C::PVCCouplingVariation,
                        1.0F + C::PVCCouplingVariation
                    ),
                C::PVCCouplingMin,
                C::PVCCouplingMax
            );
        }
    }

    const float coupling = m_PVCPending ? m_PVCCoupling : 0.0F;
    const float effectiveIBI = m_PVCPending ? coupling * m_NextIBI : m_NextIBI;

    if (m_ElapsedSinceBeat < effectiveIBI)
    {
        return std::nullopt;
    }

    const bool isPVC = m_PVCPending;
    m_PVCPending = false;
    m_DidJustPVC = isPVC;

    // Carry overshoot into the next interval so timing self-corrects.
    m_ElapsedSinceBeat -= effectiveIBI;

    // Use the preceding sinus interval, but the shortened coupling interval for a PVC.
    // This is a bounded cycle-length proxy, not literal diastolic time.
    const float nominalIBI      = 60.0F / heartRate;
    const float precedingRR     = (m_PrecedingRR > 0.0F) ? m_PrecedingRR : nominalIBI;
    const float fillingInterval = isPVC ? effectiveIBI : precedingRR;

    // Apply bounded Gaussian jitter to sinus beats; PVCs retain mean contractility.
    const float vigor = isPVC
        ? contractility
        : std::max(0.0F, contractility * (1.0F + C::VigorJitterScale
            * std::min(C::VigorJitterMaxSigma, m_Random.StandardNormal())));

    if (isPVC)
    {
        // Target the next sinus impulse at two nominal cycles after the preceding sinus beat.
        const float fullPause  = (2.0F - coupling) * m_NextIBI;
        const float pendingIBI = C::PVCCouplingMin * m_NextIBI;

        // Hand-rolled geometric sampling supports runExtensionChance == 1.
        int runExtensions = 0;
        while (runExtensions < C::PVCRunMaxLength - 1 &&
               m_Random.Uniform(0.0F, 1.0F) < runExtensionChance)
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
                m_Random.Uniform(
                    1.0F - C::PVCPauseVariation,
                    1.0F + C::PVCPauseVariation
                );
        }
        else
        {
            m_InPause = true;
            m_PauseDuration = fullPause *
                m_Random.Uniform(
                    1.0F - C::PVCPauseVariation,
                    1.0F + C::PVCPauseVariation
                );
        }

        m_PrecedingRR = m_PauseDuration;
    }
    else
    {
        m_DidJustPVC  = false;
        m_NextIBI     = BaseIBI(heartRate, respPhase, exertionFraction);
        m_PrecedingRR = effectiveIBI;
    }

    return BeatEvent{
        .IBI              = effectiveIBI,
        .FillingInterval  = fillingInterval,
        .CouplingFraction = isPVC ? coupling : 0.0F,
        .Vigor            = vigor,
        .Kind             = isPVC ? BeatKind::PVC : BeatKind::Sinus,
    };
}
