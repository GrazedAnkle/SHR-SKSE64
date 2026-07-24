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

#include "HeartbeatSource.hpp"
#include "RenderSpec.hpp"

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
        bool Init();
        void Shutdown();
        void Play(const RenderSpec &render);
        void Pause();
        void Resume();
        void FlushAndStop();

    private:
        bool LoadWav();

        // Deletes the beat buffer when XAudio2 returns it, including during explicit flushes.
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
                delete static_cast<std::vector<std::int16_t> *>(pContext);
            }
        };

        BeatBufferCallback              m_Callback;
        RE::IXAudio2SourceVoice        *m_Voice    = nullptr;
        std::optional<HeartbeatSource>  m_Source;
        RE::WAVEFORMATEX                m_Format   = { };
        bool                            m_IsPaused = false;
    };
}
