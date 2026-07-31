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

#include "core/BeatKind.hpp"

namespace SHR
{
    struct RenderSpec
    {
        float    IBI;              // seconds; heartbeat buffer duration
        float    SystoleDuration;  // seconds; S1-to-S2 onset
        float    S1Amplitude;      // final source-copy gain
        float    S2Amplitude;      // final source-copy gain
        float    S1ResampleRatio;  // source frames per output frame
        float    S2ResampleRatio;  // source frames per output frame
        float    LowPassCutoffHz;
        float    OnsetCompression; // S1 final-ascent speed multiplier
        BeatKind Kind;
    };
}
