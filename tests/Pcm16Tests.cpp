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
#include "core/Pcm16.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr float Tolerance = 1.0e-7F;
}

TEST_CASE("PCM16 decode produces normalized interleaved float frames", "[audio][pcm]")
{
    const std::vector<std::int16_t> pcm{
        -32768, -16384,
        0, 16384,
        32767, 1,
    };

    const SHR::AudioBuffer decoded = SHR::DecodePcm16(
        pcm,
        { .SampleRate = 48000, .ChannelCount = 2 }
    );
    const SHR::ConstAudioBufferView view = decoded.View();

    CHECK(view.SampleRate() == 48000);
    CHECK(view.ChannelCount() == 2);
    CHECK(view.FrameCount() == 3);
    CHECK_THAT(view(0, 0), Catch::Matchers::WithinAbs(-1.0F, Tolerance));
    CHECK_THAT(view(0, 1), Catch::Matchers::WithinAbs(-0.5F, Tolerance));
    CHECK_THAT(view(1, 0), Catch::Matchers::WithinAbs(0.0F, Tolerance));
    CHECK_THAT(view(1, 1), Catch::Matchers::WithinAbs(0.5F, Tolerance));
    CHECK_THAT(
        view(2, 0),
        Catch::Matchers::WithinAbs(32767.0F / 32768.0F, Tolerance)
    );
    CHECK_THAT(view(2, 1), Catch::Matchers::WithinAbs(1.0F / 32768.0F, Tolerance));
}

TEST_CASE("PCM16 decode rejects an incomplete interleaved frame", "[audio][pcm]")
{
    const std::vector<std::int16_t> pcm{ 1, 2, 3 };
    CHECK_THROWS_AS(
        SHR::DecodePcm16(pcm, { .SampleRate = 48000, .ChannelCount = 2 }),
        std::invalid_argument
    );
}

TEST_CASE("PCM16 encode preserves the shipping sink quantizer", "[audio][pcm][characterization]")
{
    const SHR::AudioBuffer input(
        { .SampleRate = 48000, .ChannelCount = 1 },
        7,
        std::vector<float>{ -2.0F, -1.0F, -0.5F, 0.0F, 0.5F, 1.0F, 2.0F }
    );

    const std::vector<std::int16_t> encoded = SHR::EncodePcm16(input.View());
    CHECK(
        encoded ==
        std::vector<std::int16_t>{ -32768, -32767, -16383, 0, 16383, 32767, 32767 }
    );
}

TEST_CASE("PCM16 encode rejects non-finite samples", "[audio][pcm]")
{
    const SHR::AudioBuffer input(
        { .SampleRate = 48000, .ChannelCount = 1 },
        1,
        std::vector<float>{ std::numeric_limits<float>::quiet_NaN() }
    );
    CHECK_THROWS_AS(SHR::EncodePcm16(input.View()), std::invalid_argument);
}
