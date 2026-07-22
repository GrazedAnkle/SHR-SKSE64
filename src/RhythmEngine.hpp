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

namespace SHR
{
    class RhythmEngine
    {
    public:
        struct Beat
        {
            bool  ShouldFire;
            float IBI;              // seconds; heartbeat buffer duration
            float SystoleDuration;  // seconds; S1-to-S2 onset
            float S1Amplitude;      // source-relative gain
            float S2Amplitude;      // source-relative gain; may be zero for a PVC
            float RespPhase;        // [0, 1]
            float ExertionFraction; // [0, 1] RSA attenuation driver
            float BreathDepth;      // [0, 1]
            float Contractility;    // per-beat value; jitter may push it above 1
            float FrankStarling;    // bounded preload multiplier
            bool  IsPVC;
        };

        void Init();

        // Advances rhythm state by delta seconds and returns ShouldFire=false until a beat is due.
        Beat Advance(
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
        );

    private:
        float m_ElapsedSinceBeat = 0.0F;
        float m_NextIBI          = 0.0F;
        float m_PauseDuration    = 0.0F;
        float m_PrecedingRR      = 0.0F;
        float m_PVCCoupling      = 0.0F; // sampled coupling fraction for the pending PVC
        float m_PendingPVCIBI    = 0.0F; // inter-ectopic interval within a run
        int   m_RunRemaining     = 0;    // additional PVCs left in the current run
        bool  m_PVCPending       = false;
        bool  m_DidJustPVC       = false;
        bool  m_InPause          = false;
    };
}
