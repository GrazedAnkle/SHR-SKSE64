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
#include "PhysiologySnapshot.hpp"
#include "RenderSpec.hpp"

namespace SHR
{
    RenderSpec CreateRenderSpec(const BeatEvent &event, const PhysiologySnapshot &physiology);
    RenderSpec CreateRenderSpec(
        const BeatEvent                   &event,
        const PhysiologySnapshot          &physiology,
        const AcousticMappingCoefficients &coefficients
    );
}
