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
#include "HeartbeatSource.hpp"

#include "Constants.hpp"
#include "Pcm16.hpp"
#include "TestWav.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <numbers>
#include <vector>

namespace
{
    namespace C = SHR::Constants;

    SHR::Tests::Pcm16Wav LoadSourceWav()
    {
        const std::filesystem::path path =
            std::filesystem::path(SHR_TEST_SOURCE_DIR) /
            "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav";
        return SHR::Tests::LoadPcm16Wav(path);
    }

}

TEST_CASE("Heartbeat source slicing preserves interleaved frames", "[audio][source]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 2 };
    SHR::AudioBuffer decoded(format, C::S2EndFrames);
    for (std::size_t frame = 0; frame < decoded.FrameCount(); ++frame)
    {
        decoded.View()(frame, 0) = static_cast<float>(frame);
        decoded.View()(frame, 1) = -static_cast<float>(frame);
    }

    const SHR::HeartbeatSourceSlices sliced = SHR::SliceHeartbeatSource(decoded.ConstView());

    CHECK(sliced.S1.GetFormat() == format);
    CHECK(sliced.S1.FrameCount() == C::S1EndFrames - C::S1OnsetFrames);
    CHECK(sliced.S2.FrameCount() == C::S2EndFrames - C::S2OnsetFrames);
    CHECK(sliced.S1.ConstView()(0, 0) == static_cast<float>(C::S1OnsetFrames));
    CHECK(sliced.S1.ConstView()(sliced.S1.FrameCount() - 1, 1) ==
        -static_cast<float>(C::S1EndFrames - 1));
    CHECK(sliced.S2.ConstView()(0, 0) == static_cast<float>(C::S2OnsetFrames));
    CHECK(sliced.S2.ConstView()(sliced.S2.FrameCount() - 1, 1) ==
        -static_cast<float>(C::S2EndFrames - 1));
}

TEST_CASE("Heartbeat source high-pass keeps channel state independent", "[audio][source]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 2 };
    SHR::HeartbeatSourceSlices source{
        .S1 = SHR::AudioBuffer(format, 8),
        .S2 = SHR::AudioBuffer(format, 8),
    };
    source.S1.View()(0, 0) = 1.0F;
    source.S2.View()(0, 1) = -0.5F;

    SHR::ApplyHeartbeatSourceHighPass(source, 1000.0F);

    for (std::size_t frame = 0; frame < 8; ++frame)
    {
        CHECK(source.S1.ConstView()(frame, 1) == 0.0F);
        CHECK(source.S2.ConstView()(frame, 0) == 0.0F);
        CHECK(source.S2.ConstView()(frame, 1) ==
            Catch::Approx(-0.5F * source.S1.ConstView()(frame, 0)).margin(1.0e-6F));
    }
}

TEST_CASE("Disabled heartbeat source high-pass is an exact no-op", "[audio][source]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 1 };
    SHR::HeartbeatSourceSlices source{
        .S1 = SHR::AudioBuffer(format, 3, std::vector<float>{ 0.2F, -0.4F, 0.6F }),
        .S2 = SHR::AudioBuffer(format, 2, std::vector<float>{ -0.1F, 0.3F }),
    };
    const std::vector<float> expectedS1(
        source.S1.ConstView().Samples().begin(),
        source.S1.ConstView().Samples().end()
    );
    const std::vector<float> expectedS2(
        source.S2.ConstView().Samples().begin(),
        source.S2.ConstView().Samples().end()
    );

    SHR::ApplyHeartbeatSourceHighPass(source, 0.0F);

    CHECK(std::ranges::equal(source.S1.ConstView().Samples(), expectedS1));
    CHECK(std::ranges::equal(source.S2.ConstView().Samples(), expectedS2));
}

TEST_CASE("Heartbeat source normalization uses one joint float peak", "[audio][source]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 1 };
    SHR::HeartbeatSourceSlices source{
        .S1 = SHR::AudioBuffer(format, 3, std::vector<float>{ -0.5F, 0.25F, 0.0F }),
        .S2 = SHR::AudioBuffer(format, 2, std::vector<float>{ 1.0F, 0.125F }),
    };

    SHR::NormalizeHeartbeatSourceJoint(source, 0.2F);

    CHECK(source.S1.ConstView()(0, 0) == Catch::Approx(-0.1F));
    CHECK(source.S1.ConstView()(1, 0) == Catch::Approx(0.05F));
    CHECK(source.S2.ConstView()(0, 0) == Catch::Approx(0.2F));
    CHECK(source.S2.ConstView()(1, 0) == Catch::Approx(0.025F));
}

TEST_CASE("Baseline attack region has synthetic ground truth and scale invariance", "[audio][source]")
{
    constexpr std::size_t      frameCount = 1024;
    constexpr SHR::AudioFormat format{ .SampleRate = 48000, .ChannelCount = 2 };
    SHR::AudioBuffer signal(format, frameCount);
    SHR::AudioBuffer scaled(format, frameCount);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const double phase = 2.0 * std::numbers::pi *
            static_cast<double>(frame) / static_cast<double>(frameCount);
        const float envelope = static_cast<float>(0.5 - 0.5 * std::cos(phase));
        const float carrier = static_cast<float>(std::cos(100.0 * phase));
        signal.View()(frame, 0) = envelope * carrier;
        signal.View()(frame, 1) = 20.0F * static_cast<float>(std::sin(13.0 * phase));
        scaled.View()(frame, 0) = 0.25F * signal.ConstView()(frame, 0);
        scaled.View()(frame, 1) = -30.0F * signal.ConstView()(frame, 1);
    }

    // a[n] = 0.5 - 0.5 cos(2 pi n/N), so its 10% crossing is between frames 104 and
    // 105 and its peak is exactly frame 512. The carrier and envelope occupy disjoint DFT bins.
    const auto expected = SHR::BaselineAttackRegion{ .StartFrame = 105, .PeakFrame = 512 };
    CHECK(SHR::FindBaselineAttackRegion(signal.ConstView(), 0.1F) == expected);
    CHECK(SHR::FindBaselineAttackRegion(scaled.ConstView(), 0.1F) == expected);

    SHR::AudioBuffer silence(format, frameCount);
    CHECK_FALSE(SHR::FindBaselineAttackRegion(silence.ConstView(), 0.1F).has_value());
}

TEST_CASE("Production float source conditioning retains its source contract",
    "[audio][source][characterization]")
{
    const SHR::Tests::Pcm16Wav wav = LoadSourceWav();
    const SHR::AudioBuffer decoded = SHR::DecodePcm16(wav.Samples, wav.Format);
    const SHR::HeartbeatSource source = SHR::PrepareHeartbeatSource(decoded.ConstView());

    CHECK(source.S1.GetFormat() == wav.Format);
    CHECK(source.S2.GetFormat() == wav.Format);
    CHECK(source.S1.FrameCount() == C::S1EndFrames - C::S1OnsetFrames);
    CHECK(source.S2.FrameCount() == C::S2EndFrames - C::S2OnsetFrames);
    REQUIRE(source.S1BaselineAttack.has_value());
    CHECK(*source.S1BaselineAttack ==
        SHR::BaselineAttackRegion{ .StartFrame = 2314, .PeakFrame = 2779 });

    float peak = 0.0F;
    for (const float sample : source.S1.ConstView().Samples())
        peak = std::max(peak, std::abs(sample));
    for (const float sample : source.S2.ConstView().Samples())
        peak = std::max(peak, std::abs(sample));
    CHECK(peak == Catch::Approx(C::SourceRestLevel).margin(1.0e-7F));
}
