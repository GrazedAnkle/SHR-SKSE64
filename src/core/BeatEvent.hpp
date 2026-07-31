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
    struct BeatEvent
    {
        float    IBI;              // seconds; heartbeat buffer duration
        float    FillingInterval;  // seconds; raw rhythm-derived preload input
        float    CouplingFraction; // normal-IBI fraction; meaningful for PVCs
        float    Vigor;            // sampled per-beat contractility; may exceed 1
        BeatKind Kind;
    };
}
