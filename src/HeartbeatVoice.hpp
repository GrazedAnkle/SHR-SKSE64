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

#include "RhythmEngine.hpp"

#include <RE/Skyrim.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SHR
{
    class HeartbeatVoice
    {
    public:
        bool Init();
        void Shutdown();
        void Play(const RhythmEngine::Beat &beat);
        void Pause();
        void Resume();
        void FlushAndStop();

    private:
        bool LoadWav();
        static void CopySamples(
            std::byte       *dst,
            std::uint32_t    dstFrameOffset,
            const std::byte *src,
            std::uint32_t    srcFrames,
            std::uint32_t    outFrames,
            float            resampleRatio,  // source frames per output frame (>1 = higher/shorter)
            std::uint32_t    bytesPerFrame,
            float            amplitude,
            std::uint32_t    crossfadeFrames,
            bool             fadeIn,
            bool             fadeOut
        );

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
                delete static_cast<std::vector<std::byte>*>(pContext);
            }
        };

        BeatBufferCallback       m_Callback;
        RE::IXAudio2SourceVoice *m_Voice    = nullptr;
        std::vector<std::byte>   m_S1Data;
        std::vector<std::byte>   m_S2Data;
        RE::WAVEFORMATEX         m_Format   = { };
        bool                     m_IsPaused = false;
    };
}
