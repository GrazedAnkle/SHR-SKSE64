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
#include "plugin/HeartbeatVoice.hpp"

#include "core/BeatRenderer.hpp"
#include "adapter/Config.hpp"
#include "core/Constants.hpp"
#include "core/Pcm16.hpp"

namespace
{
    namespace C = SHR::Constants;
    constexpr const char WavPath[] = "Data\\Sound\\fx\\SHR_HeartBeat\\HeartBeat_Shortened.wav";
}

bool SHR::HeartbeatVoice::LoadWav()
{
    std::ifstream file(WavPath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        SKSE::log::error("[HeartbeatVoice] Cannot open {}", WavPath);
        return false;
    }

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    std::vector<std::byte> raw(fileSize);
    if (!file.read(reinterpret_cast<char *>(raw.data()), fileSize))
    {
        SKSE::log::error("[HeartbeatVoice] Read failed for {}", WavPath);
        return false;
    }

    if (fileSize < 12 ||
        std::memcmp(raw.data(),     "RIFF", 4) != 0 ||
        std::memcmp(raw.data() + 8, "WAVE", 4) != 0)
    {
        SKSE::log::error("[HeartbeatVoice] Not a valid RIFF/WAVE file");
        return false;
    }

    bool fmtFound = false, dataFound = false;
    std::vector<std::byte> pcmData;

    for (std::size_t pos = 12; pos + 8 <= fileSize; )
    {
        const auto *id = reinterpret_cast<const char *>(raw.data() + pos);
        std::uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, raw.data() + pos + 4, sizeof(chunkSize));

        if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            // Standard PCM fmt chunks are 16 bytes (no cbSize field); WAVEFORMATEX
            // is 18. Zero-init first so cbSize = 0 when the field is absent.
            m_Format = { };
            std::memcpy(
                &m_Format,
                raw.data() + pos + 8,
                std::min(static_cast<std::size_t>(chunkSize), sizeof(RE::WAVEFORMATEX))
            );
            fmtFound = true;
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            const std::size_t dataOffset = pos + 8;
            const std::size_t dataBytes  = std::min(static_cast<std::size_t>(chunkSize), fileSize - dataOffset);
            pcmData.assign(raw.data() + dataOffset, raw.data() + dataOffset + dataBytes);
            dataFound = true;
        }

        pos += 8 + chunkSize + (chunkSize & 1); // skip chunk + word-align pad
    }

    if (!fmtFound || !dataFound)
    {
        SKSE::log::error("[HeartbeatVoice] WAV missing fmt or data chunk");
        return false;
    }

    if (m_Format.wFormatTag != 1 || m_Format.wBitsPerSample != 16)
    {
        SKSE::log::error(
            "[HeartbeatVoice] Only 16-bit PCM WAV is supported (got tag={} bits={})",
            m_Format.wFormatTag,
            m_Format.wBitsPerSample
        );
        return false;
    }

    const std::size_t pcmSampleCount = pcmData.size() / sizeof(std::int16_t);
    std::vector<std::int16_t> pcmSamples(pcmSampleCount);
    std::memcpy(
        pcmSamples.data(),
        pcmData.data(),
        pcmSampleCount * sizeof(std::int16_t)
    );
    try
    {
        const AudioFormat sourceFormat{
            .SampleRate = m_Format.nSamplesPerSec,
            .ChannelCount = m_Format.nChannels,
        };
        const AudioBuffer decoded = DecodePcm16(pcmSamples, sourceFormat);
        m_Source = PrepareHeartbeatSource(decoded.ConstView());
    }
    catch (const std::exception &error)
    {
        SKSE::log::error("[HeartbeatVoice] Cannot prepare WAV source: {}", error.what());
        return false;
    }

    SKSE::log::info(
        "[HeartbeatVoice] WAV loaded: {}Hz {}ch {}bit - float S1 {} frames S2 {} frames",
        m_Format.nSamplesPerSec,
        m_Format.nChannels,
        m_Format.wBitsPerSample,
        m_Source->S1.FrameCount(),
        m_Source->S2.FrameCount()
    );
    return true;
}

bool SHR::HeartbeatVoice::Init()
{
    if (!LoadWav()) return false;

    auto *platform = RE::BSAudioManager::QPlatformInstance();
    if (!platform)
    {
        SKSE::log::error("[HeartbeatVoice] QPlatformInstance() returned null");
        return false;
    }

    auto *xa = static_cast<RE::BSXAudio2Audio *>(platform);
    if (!xa->XAudio)
    {
        SKSE::log::error("[HeartbeatVoice] `IXAudio2 *` is null");
        return false;
    }

    // cbSize must be 0 for WAVE_FORMAT_PCM.
    RE::WAVEFORMATEX fmt = m_Format;
    fmt.cbSize = 0;

    const auto hr = xa->XAudio->CreateSourceVoice(
        &m_Voice,
        &fmt,
        0,
        RE::XAUDIO2_DEFAULT_FREQ_RATIO,
        &m_Callback
    );
    if (hr != 0 || !m_Voice)
    {
        SKSE::log::error(
            "[HeartbeatVoice] CreateSourceVoice failed: {:08X}",
            static_cast<std::uint32_t>(hr)
        );
        return false;
    }

    // Voice volume persists across Stop()/Start().
    const float voiceVolume = Config::Get().Audio.Volume * C::VoiceOutputGain;
    m_Voice->SetVolume(voiceVolume);

    m_Voice->Start();
    SKSE::log::info(
        "[HeartbeatVoice] Source voice ready (float renderer -> PCM16 sink, volume {} -> {})",
        Config::Get().Audio.Volume,
        voiceVolume
    );
    return true;
}

void SHR::HeartbeatVoice::Play(const RenderSpec &render)
{
    if (!m_Voice || !m_Source) return;

    const AudioBuffer rendered = RenderBeat(*m_Source, render);

    // The sink quantizer runs once, immediately before the existing callback-owned XAudio buffer
    // handoff. WI-022 owns failed-submission and shutdown lifetime.
    auto *heapBuf = new std::vector<std::int16_t>(EncodePcm16(rendered.ConstView()));

    RE::XAUDIO2_BUFFER buf = { };
    buf.AudioBytes = static_cast<std::uint32_t>(heapBuf->size() * sizeof(std::int16_t));
    buf.pAudioData = reinterpret_cast<const std::byte *>(heapBuf->data());
    buf.pContext = heapBuf;
    m_Voice->SubmitSourceBuffer(&buf);
}

void SHR::HeartbeatVoice::Pause()
{
    if (!m_Voice || m_IsPaused) return;
    m_Voice->Stop();
    m_IsPaused = true;
}

void SHR::HeartbeatVoice::Resume()
{
    if (!m_Voice || !m_IsPaused) return;
    m_Voice->Start();
    m_IsPaused = false;
}

void SHR::HeartbeatVoice::FlushAndStop()
{
    if (!m_Voice) return;
    m_Voice->Stop();
    m_Voice->FlushSourceBuffers();
    m_Voice->Start();
}

void SHR::HeartbeatVoice::Shutdown()
{
    if (!m_Voice) return;
    m_Voice->Stop();
    m_Voice->FlushSourceBuffers();
    m_Voice->DestroyVoice();
    m_Voice = nullptr;
    m_Source.reset();
}
