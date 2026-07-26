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

#include "BeatEvent.hpp"
#include "ModelCoefficients.hpp"
#include "RhythmInput.hpp"
#include "RhythmRandom.hpp"

#include <optional>

namespace SHR
{
    class RhythmEngine
    {
    public:
        RhythmEngine();
        explicit RhythmEngine(RhythmRandom random);
        explicit RhythmEngine(RhythmModelCoefficients coefficients);
        RhythmEngine(RhythmModelCoefficients coefficients, RhythmRandom random);

        void Init();

        // Advances rhythm state and returns no event until a beat is due.
        std::optional<BeatEvent> Advance(const RhythmInput &input);

    private:
        const RhythmModelCoefficients m_Coefficients;
        RhythmRandom m_Random;

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
