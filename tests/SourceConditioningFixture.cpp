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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
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
            throw std::runtime_error("cannot write source-conditioning fixture");
        }
    }
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "usage: SHRSourceConditioningFixture <source.wav> <output-directory>\n";
            return 2;
        }

        const std::filesystem::path outputDirectory = argv[2];
        std::filesystem::create_directories(outputDirectory);

        const SHR::Tests::Pcm16Wav wav = SHR::Tests::LoadPcm16Wav(argv[1]);
        const SHR::AudioBuffer decoded = SHR::DecodePcm16(wav.Samples, wav.Format);
        SHR::HeartbeatSourceSlices stages = SHR::SliceHeartbeatSource(decoded.ConstView());
        WriteSamples(outputDirectory, "sliced_s1.f32", stages.S1.ConstView());
        WriteSamples(outputDirectory, "sliced_s2.f32", stages.S2.ConstView());

        SHR::ApplyHeartbeatSourceHighPass(stages, SHR::Constants::SourceHighPassHz);
        WriteSamples(outputDirectory, "highpass_s1.f32", stages.S1.ConstView());
        WriteSamples(outputDirectory, "highpass_s2.f32", stages.S2.ConstView());

        SHR::NormalizeHeartbeatSourceJoint(stages, SHR::Constants::SourceRestLevel);
        WriteSamples(outputDirectory, "normalized_s1.f32", stages.S1.ConstView());
        WriteSamples(outputDirectory, "normalized_s2.f32", stages.S2.ConstView());

        const auto attack = SHR::FindBaselineAttackRegion(
            stages.S1.ConstView(),
            SHR::Constants::AttackBuildThreshold
        );
        if (!attack)
        {
            throw std::runtime_error("conditioned source has no baseline S1 attack region");
        }

        std::ofstream metadata(outputDirectory / "metadata.txt");
        metadata
            << "sample_rate " << wav.Format.SampleRate << '\n'
            << "channel_count " << wav.Format.ChannelCount << '\n'
            << "s1_frames " << stages.S1.FrameCount() << '\n'
            << "s2_frames " << stages.S2.FrameCount() << '\n'
            << "attack_start " << attack->StartFrame << '\n'
            << "attack_peak " << attack->PeakFrame << '\n';
        if (!metadata)
        {
            throw std::runtime_error("cannot write source-conditioning metadata");
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
