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

#include "core/Simulation.hpp"

namespace SHR
{
    struct StepInput
    {
        PlayerState Player;
        float       DeltaSeconds;   // real frame time in seconds
        float       GameHoursDelta; // elapsed in-game hours; never negative
        bool        OutputEnabled;  // false advances physiology without advancing rhythm
    };
}
