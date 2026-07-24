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
#include "Pcm16.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

SHR::AudioBuffer SHR::DecodePcm16(
    std::span<const std::int16_t> samples,
    AudioFormat                   format
)
{
    if (format.ChannelCount == 0)
    {
        throw std::invalid_argument("audio channel count must be positive");
    }
    if (samples.size() % format.ChannelCount != 0)
    {
        throw std::invalid_argument("PCM16 sample count does not contain complete frames");
    }

    const std::size_t frameCount = samples.size() / format.ChannelCount;
    std::vector<float> decoded(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        decoded[i] = static_cast<float>(samples[i]) / 32768.0F;
    }
    return AudioBuffer(format, frameCount, std::move(decoded));
}

std::vector<std::int16_t> SHR::EncodePcm16(ConstAudioBufferView input)
{
    std::vector<std::int16_t> encoded(input.SampleCount());
    const std::span<const float> samples = input.Samples();
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        if (!std::isfinite(samples[i]))
        {
            throw std::invalid_argument("cannot encode a non-finite audio sample");
        }
        encoded[i] = static_cast<std::int16_t>(
            std::clamp(samples[i] * 32767.0F, -32768.0F, 32767.0F)
        );
    }
    return encoded;
}
