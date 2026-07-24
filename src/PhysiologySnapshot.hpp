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

#include <optional>

namespace SHR
{
    struct PhysiologySnapshot
    {
        float HeartRate;           // bpm
        float FastHeartRate;       // bpm
        float SlowHeartRate;       // bpm
        float Exertion;            // METs
        float Adrenaline;
        float Contractility;       // [0, 1] mean state
        float ContractilityExcess; // nonnegative excess over the current HR-implied state
        float Fitness;             // METs
        float EffectiveFitness;    // METs after acute and long-term fatigue
        float AcuteFatigue;        // METs
        float LongTermFatigue;     // METs
        float RespirationRate;     // breaths/min
        float RespirationDepth;    // normalized tidal-volume state [0, 1]
        float RespirationPhase;    // [0, 1)
        std::optional<float> DeathSeconds;
    };
}
