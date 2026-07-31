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

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace SHR
{
    struct AudioFormat
    {
        std::uint32_t SampleRate;
        std::uint32_t ChannelCount;

        friend constexpr bool operator==(const AudioFormat &, const AudioFormat &) = default;
    };

    namespace Detail
    {
        inline std::size_t CheckedSampleCount(AudioFormat format, std::size_t frameCount)
        {
            if (format.SampleRate == 0)
            {
                throw std::invalid_argument("audio sample rate must be positive");
            }
            if (format.ChannelCount == 0)
            {
                throw std::invalid_argument("audio channel count must be positive");
            }
            if (frameCount > std::numeric_limits<std::size_t>::max() / format.ChannelCount)
            {
                throw std::length_error("audio sample count overflows size_t");
            }
            return frameCount * format.ChannelCount;
        }
    }

    template <typename Sample>
        requires std::same_as<std::remove_const_t<Sample>, float>
    class BasicAudioBufferView
    {
    public:
        BasicAudioBufferView(
            std::span<Sample> samples,
            AudioFormat      format,
            std::size_t      frameCount
        )
            : m_Samples(samples)
            , m_Format(format)
            , m_FrameCount(frameCount)
        {
            if (m_Samples.size() != Detail::CheckedSampleCount(m_Format, m_FrameCount))
            {
                throw std::invalid_argument("audio sample count does not match frames and channels");
            }
        }

        [[nodiscard]] AudioFormat GetFormat() const noexcept { return m_Format; }
        [[nodiscard]] std::uint32_t SampleRate() const noexcept { return m_Format.SampleRate; }
        [[nodiscard]] std::uint32_t ChannelCount() const noexcept { return m_Format.ChannelCount; }
        [[nodiscard]] std::size_t FrameCount() const noexcept { return m_FrameCount; }
        [[nodiscard]] std::size_t SampleCount() const noexcept { return m_Samples.size(); }
        [[nodiscard]] bool Empty() const noexcept { return m_FrameCount == 0; }
        [[nodiscard]] std::span<Sample> Samples() const noexcept { return m_Samples; }

        [[nodiscard]] std::span<Sample> Frame(std::size_t frame) const
        {
            if (frame >= m_FrameCount)
            {
                throw std::out_of_range("audio frame index is out of range");
            }
            return m_Samples.subspan(frame * m_Format.ChannelCount, m_Format.ChannelCount);
        }

        [[nodiscard]] Sample &operator()(std::size_t frame, std::size_t channel) const
        {
            if (frame >= m_FrameCount || channel >= m_Format.ChannelCount)
            {
                throw std::out_of_range("audio sample index is out of range");
            }
            return m_Samples[frame * m_Format.ChannelCount + channel];
        }

        [[nodiscard]] BasicAudioBufferView Subview(
            std::size_t firstFrame,
            std::size_t frameCount
        ) const
        {
            if (firstFrame > m_FrameCount || frameCount > m_FrameCount - firstFrame)
            {
                throw std::out_of_range("audio frame subview is out of range");
            }
            const std::size_t firstSample = firstFrame * m_Format.ChannelCount;
            const std::size_t sampleCount = frameCount * m_Format.ChannelCount;
            return {
                m_Samples.subspan(firstSample, sampleCount),
                m_Format,
                frameCount,
            };
        }

    private:
        std::span<Sample> m_Samples;
        AudioFormat       m_Format;
        std::size_t       m_FrameCount;
    };

    using AudioBufferView = BasicAudioBufferView<float>;
    using ConstAudioBufferView = BasicAudioBufferView<const float>;

    class AudioBuffer
    {
    public:
        AudioBuffer(AudioFormat format, std::size_t frameCount, float initialValue = 0.0F)
            : m_Format(format)
            , m_FrameCount(frameCount)
            , m_Samples(Detail::CheckedSampleCount(format, frameCount), initialValue)
        {
        }

        AudioBuffer(AudioFormat format, std::size_t frameCount, std::vector<float> samples)
            : m_Format(format)
            , m_FrameCount(frameCount)
            , m_Samples(std::move(samples))
        {
            if (m_Samples.size() != Detail::CheckedSampleCount(m_Format, m_FrameCount))
            {
                throw std::invalid_argument("audio sample count does not match frames and channels");
            }
        }

        [[nodiscard]] AudioFormat GetFormat() const noexcept { return m_Format; }
        [[nodiscard]] std::uint32_t SampleRate() const noexcept { return m_Format.SampleRate; }
        [[nodiscard]] std::uint32_t ChannelCount() const noexcept { return m_Format.ChannelCount; }
        [[nodiscard]] std::size_t FrameCount() const noexcept { return m_FrameCount; }
        [[nodiscard]] std::size_t SampleCount() const noexcept { return m_Samples.size(); }
        [[nodiscard]] bool Empty() const noexcept { return m_FrameCount == 0; }

        [[nodiscard]] AudioBufferView View()
        {
            return { m_Samples, m_Format, m_FrameCount };
        }

        [[nodiscard]] ConstAudioBufferView View() const
        {
            return { m_Samples, m_Format, m_FrameCount };
        }

        [[nodiscard]] ConstAudioBufferView ConstView() const
        {
            return View();
        }

    private:
        AudioFormat        m_Format;
        std::size_t        m_FrameCount;
        std::vector<float> m_Samples;
    };
}
