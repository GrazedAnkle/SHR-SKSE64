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
#include "adapter/Settings.hpp"
#include "core/Runtime.hpp"
#include "plugin/HeartbeatVoice.hpp"

#include <atomic>
#include <cstddef>
#include <limits>
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

        // Update thread. A rejected value is recorded nowhere, so the store cannot claim a setting
        // is in force when it is not.
        bool ApplySubject(const Settings::SubjectSpec &spec, float value);
        bool ApplyProfile(const Settings::ProfileSpec &spec, float value);

        void ResetSubjectSettings();
        void ResetProfileSettings();

        float ReadSubject(Settings::Subject field) const;
        static float ReadProfile(Settings::Profile field);

        const Settings::Overrides &GetOverrides() const noexcept { return m_Overrides; }

        // Must run BEFORE Runtime::Restore; reversing the two double-counts the seed transform on
        // every load. Returns false having applied nothing.
        bool AdoptOverrides(const Settings::Overrides &overrides);

        // Update thread. Returns in-game hours since the previous call and rebases.
        float ConsumeGameHoursDelta(float currentHours) noexcept;

        // Update thread. The pool index for the next arrhythmia message, avoiding the one it last
        // returned while the pool offers an alternative.
        std::size_t NextArrhythmiaDraw(std::size_t poolSize) noexcept;

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

        // Out of range on purpose, which the policy reads as "nothing shown yet". Not character
        // state, so it deliberately survives Revert.
        std::size_t m_LastArrhythmiaDraw = std::numeric_limits<std::size_t>::max();

        // The profile's values, kept apart from the runtime's live settings: resolving an override
        // against those would not be idempotent.
        RuntimeSettings     m_Defaults;
        Settings::Overrides m_Overrides;
    };
}
