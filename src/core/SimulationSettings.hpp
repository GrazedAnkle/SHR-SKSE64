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
    struct SimulationSettings
    {
        float RestingHeartRate = 55.0F;
        float MaximumHeartRate = 200.0F;
        // Per-character adaptation ceiling: what this subject's training converges toward. Must stay
        // above the fitness that RestingHeartRate seeds, or training would reduce fitness.
        float FitnessMaxMets   = 55.0F / 3.5F;
    };
}
