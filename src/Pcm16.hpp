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

#include "AudioBuffer.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace SHR
{
    AudioBuffer DecodePcm16(std::span<const std::int16_t> samples, AudioFormat format);

    // Preserves the shipping XAudio sink quantizer: 32767 scale, truncation toward zero, and saturation.
    // It is intentionally not an exact DecodePcm16 round trip.
    std::vector<std::int16_t> EncodePcm16(ConstAudioBufferView input);
}
