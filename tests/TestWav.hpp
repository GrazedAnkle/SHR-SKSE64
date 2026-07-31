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

#include "core/AudioBuffer.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace SHR::Tests
{
    struct Pcm16Wav
    {
        AudioFormat               Format;
        std::vector<std::int16_t> Samples;
    };

    inline Pcm16Wav LoadPcm16Wav(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            throw std::runtime_error("cannot open PCM16 fixture WAV");
        }
        const auto fileSize = static_cast<std::size_t>(file.tellg());
        file.seekg(0);

        std::vector<std::uint8_t> bytes(fileSize);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), fileSize))
        {
            throw std::runtime_error("cannot read PCM16 fixture WAV");
        }

        const auto readU16 = [&bytes](std::size_t offset)
        {
            if (offset + 2 > bytes.size()) throw std::runtime_error("truncated WAV u16");
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[offset]) |
                static_cast<std::uint16_t>(bytes[offset + 1]) << 8
            );
        };
        const auto readU32 = [&bytes](std::size_t offset)
        {
            if (offset + 4 > bytes.size()) throw std::runtime_error("truncated WAV u32");
            return static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(bytes[offset]) |
                static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
                static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
                static_cast<std::uint32_t>(bytes[offset + 3]) << 24
            );
        };
        const auto chunkId = [&bytes](std::size_t offset)
        {
            if (offset + 4 > bytes.size()) throw std::runtime_error("truncated WAV chunk ID");
            return std::string_view(
                reinterpret_cast<const char *>(bytes.data() + offset),
                4
            );
        };

        if (
            bytes.size() < 12 ||
            chunkId(0) != "RIFF" ||
            chunkId(8) != "WAVE"
        )
        {
            throw std::runtime_error("fixture is not a RIFF/WAVE file");
        }

        bool          formatFound = false;
        std::uint16_t formatTag = 0;
        std::uint16_t channelCount = 0;
        std::uint32_t sampleRate = 0;
        std::uint16_t bitsPerSample = 0;
        std::size_t   dataOffset = 0;
        std::size_t   dataSize = 0;

        for (std::size_t position = 12; position + 8 <= bytes.size(); )
        {
            const std::string_view id = chunkId(position);
            const std::uint32_t size = readU32(position + 4);
            const std::size_t payload = position + 8;
            if (payload > bytes.size() || size > bytes.size() - payload)
            {
                throw std::runtime_error("fixture WAV chunk exceeds the file");
            }

            if (id == "fmt " && size >= 16)
            {
                formatTag = readU16(payload);
                channelCount = readU16(payload + 2);
                sampleRate = readU32(payload + 4);
                bitsPerSample = readU16(payload + 14);
                formatFound = true;
            }
            else if (id == "data")
            {
                dataOffset = payload;
                dataSize = size;
            }

            position = payload + size + (size & 1U);
        }

        if (
            !formatFound ||
            dataSize == 0 ||
            formatTag != 1 ||
            bitsPerSample != 16 ||
            channelCount == 0 ||
            sampleRate == 0 ||
            dataSize % sizeof(std::int16_t) != 0
        )
        {
            throw std::runtime_error("fixture WAV is not complete interleaved PCM16");
        }

        std::vector<std::int16_t> samples(dataSize / sizeof(std::int16_t));
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const std::uint16_t bits = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[dataOffset + 2 * i]) |
                static_cast<std::uint16_t>(bytes[dataOffset + 2 * i + 1]) << 8
            );
            samples[i] = std::bit_cast<std::int16_t>(bits);
        }
        if (samples.size() % channelCount != 0)
        {
            throw std::runtime_error("fixture WAV ends with an incomplete frame");
        }

        return {
            .Format = {
                .SampleRate = sampleRate,
                .ChannelCount = channelCount,
            },
            .Samples = std::move(samples),
        };
    }
}
