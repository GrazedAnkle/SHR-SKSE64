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
#include "core/RuntimeSettings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace SHR::Settings
{
    // The exposed settings surface. ARCHITECTURE.md owns why the scope split follows the type seam
    // rather than a per-setting judgement.

    enum class Kind
    {
        Float,
        Bool,
        Key,  // a DirectInput scan code carried as a float across the Papyrus boundary
    };

    // Per-character, persisted as co-save overrides. Count must stay last.
    enum class Subject : std::size_t
    {
        RestingHeartRate,
        MaximumHeartRate,
        ArrhythmiaSusceptibility,
        FitnessMaxMets,
        Count,
    };

    // Profile-wide, persisted in the configuration file. Count must stay last.
    enum class Profile : std::size_t
    {
        AudioVolume,
        ListenKey,
        NotificationsEnabled,
        Count,
    };

    inline constexpr std::size_t SubjectCount = static_cast<std::size_t>(Subject::Count);
    inline constexpr std::size_t ProfileCount = static_cast<std::size_t>(Profile::Count);

    // Id is the menu control's id and what its change event reports back, so the menu layout and this
    // table cannot disagree without the lookup failing loudly.
    struct SubjectSpec
    {
        std::string_view Id;
        std::uint32_t    Record;  // co-save 4CC, byte-swapped as written
        const char      *Name;    // the 4CC as text, for diagnostics
        Kind             Type;
        float            Min;
        float            Max;
        Subject          Field;
    };

    struct ProfileSpec
    {
        std::string_view Id;
        Kind             Type;
        float            Min;
        float            Max;
        Profile          Field;
    };

    std::span<const SubjectSpec> SubjectSettings() noexcept;
    std::span<const ProfileSpec> ProfileSettings() noexcept;

    const SubjectSpec *FindSubject(std::string_view id) noexcept;
    const SubjectSpec *FindSubject(Subject field) noexcept;
    const SubjectSpec *FindSubjectByRecord(std::uint32_t record) noexcept;
    const ProfileSpec *FindProfile(std::string_view id) noexcept;

    // Domain check only. Runtime owns the settings-versus-coefficients rules, which need the
    // coefficients this layer cannot see.
    bool InDomain(const SubjectSpec &spec, float value) noexcept;
    bool InDomain(const ProfileSpec &spec, float value) noexcept;

    // Write into a caller-owned copy, so a rejected value leaves the live settings untouched and a
    // complete update can be validated before any part of it is applied.
    bool Apply(RuntimeSettings &settings, const SubjectSpec &spec, float value) noexcept;
    bool Apply(Config &config, const ProfileSpec &spec, float value) noexcept;

    float Read(const RuntimeSettings &settings, Subject field) noexcept;
    float Read(const Config &config, Profile field) noexcept;

    // Per-character settings the player has actually moved. An absent entry and a save written
    // before the setting existed are the same state, which is what removes the need for migrations.
    class Overrides
    {
    public:
        bool Set(Subject field, float value) noexcept;
        void Clear(Subject field) noexcept;

        std::optional<float> Get(Subject field) const noexcept;
        bool Any() const noexcept;

        // The profile defaults with the engaged overrides layered on top.
        RuntimeSettings Resolve(const RuntimeSettings &defaults) const noexcept;

    private:
        std::array<std::optional<float>, SubjectCount> m_Values{ };
    };
}
