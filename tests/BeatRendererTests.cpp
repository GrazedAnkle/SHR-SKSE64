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
#include "BeatRenderer.hpp"

#include "BeatRenderFixtures.hpp"
#include "Pcm16.hpp"
#include "TestWav.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace
{
    SHR::HeartbeatSource LoadSource()
    {
        const std::filesystem::path path =
            std::filesystem::path(SHR_TEST_SOURCE_DIR) /
            "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav";
        const SHR::Tests::Pcm16Wav wav = SHR::Tests::LoadPcm16Wav(path);
        const SHR::AudioBuffer decoded = SHR::DecodePcm16(wav.Samples, wav.Format);
        return SHR::PrepareHeartbeatSource(decoded.ConstView());
    }
}

TEST_CASE("Float beat trace exposes source transmission and transducer domains", "[audio][renderer]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 1000, .ChannelCount = 1 };
    const SHR::HeartbeatSource source{
        .S1 = SHR::AudioBuffer(format, 4, std::vector<float>(4, 0.2F)),
        .S2 = SHR::AudioBuffer(format, 4, std::vector<float>(4, 0.1F)),
        .S1BaselineAttack = std::nullopt,
    };
    const SHR::RenderSpec render{
        .IBI              = 0.020F,
        .SystoleDuration  = 0.010F,
        .S1Amplitude      = 4.0F,
        .S2Amplitude      = 1.0F,
        .S1ResampleRatio  = 1.0F,
        .S2ResampleRatio  = 1.0F,
        .LowPassCutoffHz  = 0.0F,
        .OnsetCompression = 1.0F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    const SHR::BeatRenderTrace trace = SHR::TraceBeatRender(source, render);
    const SHR::AudioBuffer output = SHR::RenderBeat(source, render);

    CHECK(std::ranges::equal(
        trace.SourceS1.ConstView().Samples(),
        source.S1.ConstView().Samples()
    ));
    CHECK(std::ranges::equal(
        trace.TransmittedS1.ConstView().Samples(),
        trace.SourceS1.ConstView().Samples()
    ));
    CHECK(trace.TransducerInput.ConstView()(0, 0) == Catch::Approx(0.8F));
    CHECK(trace.Output.ConstView()(0, 0) == Catch::Approx(
        0.5F + 0.5F * std::tanh(0.6F)
    ));
    CHECK(trace.TransducerInput.ConstView()(3, 0) == Catch::Approx(0.4F));
    CHECK(trace.Output.ConstView()(3, 0) == Catch::Approx(0.4F));
    CHECK(std::ranges::equal(
        trace.Output.ConstView().Samples(),
        output.ConstView().Samples()
    ));
}

TEST_CASE("Float onset compression consumes only the cached baseline region", "[audio][renderer]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 1000, .ChannelCount = 1 };
    const SHR::HeartbeatSource source{
        .S1 = SHR::AudioBuffer(
            format,
            8,
            std::vector<float>{ 0.0F, 1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F }
        ),
        .S2 = SHR::AudioBuffer(format, 2, std::vector<float>{ 0.0F, 0.0F }),
        .S1BaselineAttack = SHR::BaselineAttackRegion{ .StartFrame = 1, .PeakFrame = 5 },
    };
    const SHR::RenderSpec render{
        .IBI              = 0.020F,
        .SystoleDuration  = 0.015F,
        .S1Amplitude      = 0.0F,
        .S2Amplitude      = 0.0F,
        .S1ResampleRatio  = 1.0F,
        .S2ResampleRatio  = 1.0F,
        .LowPassCutoffHz  = 0.0F,
        .OnsetCompression = 2.0F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    const SHR::BeatRenderTrace sinus = SHR::TraceBeatRender(source, render);
    CHECK(sinus.SourceS1.FrameCount() == 6);
    CHECK(std::ranges::equal(
        sinus.SourceS1.ConstView().Samples(),
        std::array{ 0.0F, 1.0F, 3.0F, 5.0F, 6.0F, 7.0F }
    ));

    SHR::RenderSpec pvcRender = render;
    pvcRender.Kind = SHR::BeatKind::PVC;
    const SHR::BeatRenderTrace pvc = SHR::TraceBeatRender(source, pvcRender);
    CHECK(std::ranges::equal(
        pvc.SourceS1.ConstView().Samples(),
        source.S1.ConstView().Samples()
    ));
}

TEST_CASE("Float beat renderer bounds or rejects unsafe public inputs", "[audio][renderer]")
{
    constexpr SHR::AudioFormat format{ .SampleRate = 1000, .ChannelCount = 1 };
    SHR::HeartbeatSource source{
        .S1 = SHR::AudioBuffer(format, 2, std::vector<float>{ 0.1F, 0.0F }),
        .S2 = SHR::AudioBuffer(format, 2, std::vector<float>{ 0.1F, 0.0F }),
        .S1BaselineAttack = std::nullopt,
    };
    SHR::RenderSpec render{
        .IBI              = 0.020F,
        .SystoleDuration  = 0.010F,
        .S1Amplitude      = 1.0F,
        .S2Amplitude      = 1.0F,
        .S1ResampleRatio  = 1.0F,
        .S2ResampleRatio  = 1.0F,
        .LowPassCutoffHz  = 0.0F,
        .OnsetCompression = 1.0F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    SECTION("systole beyond the interval truncates S1 and omits S2")
    {
        render.SystoleDuration = 0.030F;
        const SHR::AudioBuffer output = SHR::RenderBeat(source, render);
        CHECK(output.FrameCount() == 20);
        CHECK(output.ConstView()(19, 0) == 0.0F);
    }

    SECTION("resampling ratios must be positive")
    {
        render.S1ResampleRatio = 0.0F;
        CHECK_THROWS_AS(SHR::RenderBeat(source, render), std::invalid_argument);
    }

    SECTION("cached attack must belong to baseline S1")
    {
        source.S1BaselineAttack = SHR::BaselineAttackRegion{
            .StartFrame = 1,
            .PeakFrame = 2,
        };
        CHECK_THROWS_AS(SHR::RenderBeat(source, render), std::invalid_argument);
    }
}

TEST_CASE("Float beat fixtures render consistently through RenderBeat and the trace",
    "[audio][renderer][characterization]")
{
    const SHR::HeartbeatSource source = LoadSource();

    // Guards that RenderBeat and TraceBeatRender agree over the real source; the committed beat-render
    // golden fingerprints the output samples, and Pcm16Tests covers EncodePcm16.
    for (const SHR::Tests::NamedBeatRenderFixture &fixture : SHR::Tests::BeatRenderFixtures)
    {
        DYNAMIC_SECTION(fixture.Name)
        {
            const SHR::BeatRenderTrace trace = SHR::TraceBeatRender(source, fixture.Render);
            const SHR::AudioBuffer output = SHR::RenderBeat(source, fixture.Render);

            CHECK(output.GetFormat() == source.S1.GetFormat());
            CHECK(std::ranges::equal(
                output.ConstView().Samples(),
                trace.Output.ConstView().Samples()
            ));
        }
    }
}
