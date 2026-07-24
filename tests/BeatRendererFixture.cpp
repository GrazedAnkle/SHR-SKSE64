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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    void WriteSamples(
        const std::filesystem::path &directory,
        std::string_view             name,
        SHR::ConstAudioBufferView    input
    )
    {
        std::ofstream output(directory / name, std::ios::binary);
        const std::span<const float> samples = input.Samples();
        if (
            !output.write(
                reinterpret_cast<const char *>(samples.data()),
                static_cast<std::streamsize>(samples.size_bytes())
            )
        )
        {
            throw std::runtime_error("cannot write beat-renderer fixture");
        }
    }

    void WriteTrace(
        const std::filesystem::path &directory,
        const SHR::Tests::NamedBeatRenderFixture &fixture,
        const SHR::BeatRenderTrace &trace,
        std::ofstream &metadata
    )
    {
        const std::string prefix(fixture.Name);
        WriteSamples(directory, prefix + "_source_s1.f32", trace.SourceS1.ConstView());
        WriteSamples(directory, prefix + "_source_s2.f32", trace.SourceS2.ConstView());
        WriteSamples(
            directory,
            prefix + "_transmitted_s1.f32",
            trace.TransmittedS1.ConstView()
        );
        WriteSamples(
            directory,
            prefix + "_transmitted_s2.f32",
            trace.TransmittedS2.ConstView()
        );
        WriteSamples(
            directory,
            prefix + "_transducer_input.f32",
            trace.TransducerInput.ConstView()
        );
        WriteSamples(directory, prefix + "_output.f32", trace.Output.ConstView());

        metadata
            << prefix << "_source_s1_frames " << trace.SourceS1.FrameCount() << '\n'
            << prefix << "_source_s2_frames " << trace.SourceS2.FrameCount() << '\n'
            << prefix << "_transmitted_s1_frames " << trace.TransmittedS1.FrameCount() << '\n'
            << prefix << "_transmitted_s2_frames " << trace.TransmittedS2.FrameCount() << '\n'
            << prefix << "_transducer_input_frames " << trace.TransducerInput.FrameCount() << '\n'
            << prefix << "_output_frames " << trace.Output.FrameCount() << '\n';
    }
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "usage: SHRBeatRendererFixture <source.wav> <output-directory>\n";
            return 2;
        }

        const std::filesystem::path outputDirectory = argv[2];
        std::filesystem::create_directories(outputDirectory);

        const SHR::Tests::Pcm16Wav wav = SHR::Tests::LoadPcm16Wav(argv[1]);
        const SHR::AudioBuffer decoded = SHR::DecodePcm16(wav.Samples, wav.Format);
        const SHR::HeartbeatSource source = SHR::PrepareHeartbeatSource(decoded.ConstView());

        std::ofstream metadata(outputDirectory / "metadata.txt");
        metadata << "sample_rate " << wav.Format.SampleRate << '\n'
                 << "channel_count " << wav.Format.ChannelCount << '\n';
        for (const SHR::Tests::NamedBeatRenderFixture &fixture : SHR::Tests::BeatRenderFixtures)
        {
            WriteTrace(
                outputDirectory,
                fixture,
                SHR::TraceBeatRender(source, fixture.Render),
                metadata
            );
        }
        if (!metadata)
        {
            throw std::runtime_error("cannot write beat-renderer metadata");
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
