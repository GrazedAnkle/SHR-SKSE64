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

#include "core/BeatEvent.hpp"
#include "core/ModelCoefficients.hpp"
#include "core/PhysiologySnapshot.hpp"
#include "core/RenderSpec.hpp"

namespace SHR
{
    float ComputeLungInflation(float respirationPhase);

    RenderSpec CreateRenderSpec(const BeatEvent &event, const PhysiologySnapshot &physiology);
    RenderSpec CreateRenderSpec(
        const BeatEvent                   &event,
        const PhysiologySnapshot          &physiology,
        const AcousticMappingCoefficients &coefficients
    );
}
