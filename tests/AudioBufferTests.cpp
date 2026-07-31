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
#include "core/AudioBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

TEST_CASE("AudioBuffer preserves explicit interleaved frame metadata", "[audio][buffer]")
{
    SHR::AudioBuffer buffer(
        { .SampleRate = 48000, .ChannelCount = 2 },
        3,
        std::vector<float>{ 1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F }
    );

    CHECK(buffer.SampleRate() == 48000);
    CHECK(buffer.ChannelCount() == 2);
    CHECK(buffer.FrameCount() == 3);
    CHECK(buffer.SampleCount() == 6);

    SHR::AudioBufferView view = buffer.View();
    CHECK(view(0, 0) == 1.0F);
    CHECK(view(0, 1) == 2.0F);
    CHECK(view(2, 0) == 5.0F);
    CHECK(view.Frame(1).data() == view.Samples().data() + 2);
    CHECK(view.Frame(1).size() == 2);

    SHR::AudioBufferView tail = view.Subview(1, 2);
    CHECK(tail.FrameCount() == 2);
    CHECK(tail(0, 0) == 3.0F);
    tail(1, 1) = 7.0F;
    CHECK(view(2, 1) == 7.0F);
}

TEST_CASE("AudioBuffer rejects inconsistent dimensions", "[audio][buffer]")
{
    CHECK_THROWS_AS(
        SHR::AudioBuffer({ .SampleRate = 0, .ChannelCount = 2 }, 1),
        std::invalid_argument
    );
    CHECK_THROWS_AS(
        SHR::AudioBuffer({ .SampleRate = 48000, .ChannelCount = 0 }, 1),
        std::invalid_argument
    );
    CHECK_THROWS_AS(
        SHR::AudioBuffer(
            { .SampleRate = 48000, .ChannelCount = 2 },
            2,
            std::vector<float>{ 1.0F, 2.0F, 3.0F }
        ),
        std::invalid_argument
    );
}

TEST_CASE("AudioBuffer views reject out-of-range frame access", "[audio][buffer]")
{
    SHR::AudioBuffer buffer({ .SampleRate = 48000, .ChannelCount = 2 }, 2);
    SHR::AudioBufferView view = buffer.View();

    CHECK_THROWS_AS(view.Frame(2), std::out_of_range);
    CHECK_THROWS_AS(view(0, 2), std::out_of_range);
    CHECK_THROWS_AS(view.Subview(1, 2), std::out_of_range);
}
