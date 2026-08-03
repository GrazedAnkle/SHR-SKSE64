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

#include "adapter/Config.hpp"
#include "adapter/GameClock.hpp"
#include "adapter/HeartRateLevelTracker.hpp"
#include "core/Runtime.hpp"
#include "plugin/HeartbeatVoice.hpp"

#include <atomic>
#include <optional>

namespace SHR
{
    // Single owner of the plugin's long-lived mutable state, so its lifetime and writers are stated
    // once rather than per variable. ARCHITECTURE.md (thread contract) owns which threads reach what.
    //
    // Config stays a process global: it is profile-wide and parsed before this exists, while
    // everything here is per-character and resets with the save.
    class PluginState
    {
    public:
        // Deliberately never destroyed: DestroyVoice waits on the XAudio2 audio thread, and at
        // process exit the BSXAudio2Audio engine owning the voice may already be gone.
        static PluginState &Get();

        // HeartbeatVoice::Init hands out &m_Callback, so an initialized owner cannot be relocated.
        PluginState(const PluginState &)            = delete;
        PluginState &operator=(const PluginState &) = delete;
        PluginState(PluginState &&)                 = delete;
        PluginState &operator=(PluginState &&)      = delete;

        void Init(const Config &config);
        // The co-save revert path: everything a new character must not inherit resets here.
        void Revert();

        Runtime &GetRuntime() { return m_Runtime.value(); }
        HeartbeatVoice &GetVoice() noexcept { return m_Voice; }
        HeartRateLevelTracker &GetLevelTracker() noexcept { return m_LevelTracker; }

        // Update thread. Returns in-game hours since the previous call and rebases.
        float ConsumeGameHoursDelta(float currentHours) noexcept;

        // Toggled from the engine's worker pool, read on the update thread.
        bool IsListening() const noexcept;
        void ToggleListening() noexcept;

    private:
        PluginState() = default;

        // Everything that must not survive into a different character. Init and Revert share it so
        // the two cannot disagree about what a fresh character starts with.
        void ResetForCharacter();

        std::optional<Runtime> m_Runtime;
        HeartbeatVoice         m_Voice;
        HeartRateLevelTracker  m_LevelTracker;
        GameClock              m_GameClock;
        std::atomic_int        m_IsListening = 0;
    };
}
