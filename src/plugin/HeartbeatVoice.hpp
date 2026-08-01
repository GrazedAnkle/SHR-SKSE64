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

#include "core/HeartbeatSource.hpp"
#include "core/RenderSpec.hpp"
#include "plugin/SinkBuffer.hpp"
#include "plugin/ThreadTrace.hpp"

#include <RE/Skyrim.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace SHR
{
    class HeartbeatVoice
    {
    public:
        HeartbeatVoice() = default;
        ~HeartbeatVoice();

        // Init hands &m_Callback to XAudio2, so an initialized voice cannot be relocated.
        HeartbeatVoice(const HeartbeatVoice &)            = delete;
        HeartbeatVoice &operator=(const HeartbeatVoice &) = delete;
        HeartbeatVoice(HeartbeatVoice &&)                 = delete;
        HeartbeatVoice &operator=(HeartbeatVoice &&)      = delete;

        bool Init(float volume);
        void SetVolume(float volume);
        void Play(const RenderSpec &render);
        void Pause();
        void Resume();
        void FlushAndStop();

    private:
        // Encoded beat storage, owned by the sink between a successful submission and its callback.
        using BeatBuffer = std::vector<std::int16_t>;

        bool LoadWav();

        // Runs on the XAudio2 audio thread, and must touch only the context it is handed.
        class BeatBufferCallback final : public RE::IXAudio2VoiceCallback
        {
        public:
            void OnVoiceProcessingPassStart(std::uint32_t) override { }
            void OnVoiceProcessingPassEnd() override { }
            void OnStreamEnd() override { }
            void OnBufferStart(void *) override { }
            void OnLoopEnd(void *) override { }
            void OnVoiceError(void *, std::int32_t) override { }
            void OnBufferEnd(void *pContext) override
            {
                SHR_TRACE_THREAD("xaudio.OnBufferEnd");
                DeleteSinkBuffer<BeatBuffer>(pContext);
            }
        };

        BeatBufferCallback              m_Callback;
        RE::IXAudio2SourceVoice        *m_Voice         = nullptr;
        std::optional<HeartbeatSource>  m_Source;
        RE::WAVEFORMATEX                m_Format        = { };
        bool                            m_IsPaused      = false;
        bool                            m_SubmitFailing = false;
    };
}
