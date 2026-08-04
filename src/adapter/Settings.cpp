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
#include "adapter/Settings.hpp"

#include "core/Constants.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace
{
    namespace C = SHR::Constants;
    using namespace SHR::Settings;

    // A 4CC literal is an int, so the swap cannot sit in a braced initializer without narrowing.
    consteval std::uint32_t RecordType(std::uint32_t fourCC) noexcept
    {
        return std::byteswap(fourCC);
    }

    // Ranges are the native validation authority; the menu's own limits are presentation over them.
    // MaximumHeartRate stops well above ExtremeHeartRateRiskThreshold so a legal slider position
    // cannot produce an update Runtime then rejects.
    constexpr std::array<SubjectSpec, SubjectCount> SubjectTable{ {
        { "fRestingHeartRate:Subject",  RecordType('SRHR'), "SRHR", Kind::Float,
          40.0F, C::MaxRestingHR,   Subject::RestingHeartRate },
        { "fMaximumHeartRate:Subject",  RecordType('SMHR'), "SMHR", Kind::Float,
          180.0F, C::HRFormulaCeiling, Subject::MaximumHeartRate },
        { "fArrhythmia:Subject",        RecordType('SARR'), "SARR", Kind::Float,
          0.0F, 5.0F,              Subject::ArrhythmiaSusceptibility },
        // In mL/kg/min, the unit VO2max is quoted in; converted at this boundary because the model
        // works in METs. Bracket per LITERATURE_ANALYSIS.md.
        { "fFitnessCeiling:Subject",    RecordType('SFMX'), "SFMX", Kind::Float,
          28.0F, 70.0F,            Subject::FitnessMaxMets },
    } };

    constexpr std::array<ProfileSpec, ProfileCount> ProfileTable{ {
        { "fVolume:Profile",        Kind::Float, 0.0F, 1.0F,      Profile::AudioVolume },
        { "iListenKey:Profile",     Kind::Key,   0.0F, 255.0F,    Profile::ListenKey },
        { "bNotifications:Profile", Kind::Bool,  0.0F, 1.0F,      Profile::NotificationsEnabled },
    } };
}

std::span<const SHR::Settings::SubjectSpec> SHR::Settings::SubjectSettings() noexcept
{
    return SubjectTable;
}

std::span<const SHR::Settings::ProfileSpec> SHR::Settings::ProfileSettings() noexcept
{
    return ProfileTable;
}

const SHR::Settings::SubjectSpec *SHR::Settings::FindSubject(std::string_view id) noexcept
{
    const auto match = std::ranges::find(SubjectTable, id, &SubjectSpec::Id);
    return match != SubjectTable.end() ? &*match : nullptr;
}

const SHR::Settings::SubjectSpec *SHR::Settings::FindSubject(Subject field) noexcept
{
    const auto match = std::ranges::find(SubjectTable, field, &SubjectSpec::Field);
    return match != SubjectTable.end() ? &*match : nullptr;
}

const SHR::Settings::SubjectSpec *SHR::Settings::FindSubjectByRecord(std::uint32_t record) noexcept
{
    const auto match = std::ranges::find(SubjectTable, record, &SubjectSpec::Record);
    return match != SubjectTable.end() ? &*match : nullptr;
}

const SHR::Settings::ProfileSpec *SHR::Settings::FindProfile(std::string_view id) noexcept
{
    const auto match = std::ranges::find(ProfileTable, id, &ProfileSpec::Id);
    return match != ProfileTable.end() ? &*match : nullptr;
}

bool SHR::Settings::InDomain(const SubjectSpec &spec, float value) noexcept
{
    return std::isfinite(value) && value >= spec.Min && value <= spec.Max;
}

bool SHR::Settings::InDomain(const ProfileSpec &spec, float value) noexcept
{
    return std::isfinite(value) && value >= spec.Min && value <= spec.Max;
}

bool SHR::Settings::Apply(RuntimeSettings &settings, const SubjectSpec &spec, float value) noexcept
{
    if (!InDomain(spec, value))
    {
        return false;
    }

    switch (spec.Field)
    {
    case Subject::RestingHeartRate:
        settings.Simulation.RestingHeartRate = value;
        break;
    case Subject::MaximumHeartRate:
        settings.Simulation.MaximumHeartRate = value;
        break;
    case Subject::ArrhythmiaSusceptibility:
        settings.ArrhythmiaSusceptibility = value;
        break;
    case Subject::FitnessMaxMets:
        settings.Simulation.FitnessMaxMets = value / C::MetsToVO2;
        break;
    case Subject::Count:
        return false;
    }
    return true;
}

bool SHR::Settings::Apply(Config &config, const ProfileSpec &spec, float value) noexcept
{
    if (!InDomain(spec, value))
    {
        return false;
    }

    switch (spec.Field)
    {
    case Profile::AudioVolume:
        config.Audio.Volume = value;
        break;
    case Profile::ListenKey:
        config.Input.Listen = static_cast<std::uint32_t>(value);
        break;
    case Profile::NotificationsEnabled:
        config.Notification.Enabled = value != 0.0F;
        break;
    case Profile::Count:
        return false;
    }
    return true;
}

float SHR::Settings::Read(const RuntimeSettings &settings, Subject field) noexcept
{
    switch (field)
    {
    case Subject::RestingHeartRate:
        return settings.Simulation.RestingHeartRate;
    case Subject::MaximumHeartRate:
        return settings.Simulation.MaximumHeartRate;
    case Subject::ArrhythmiaSusceptibility:
        return settings.ArrhythmiaSusceptibility;
    case Subject::FitnessMaxMets:
        return settings.Simulation.FitnessMaxMets * C::MetsToVO2;
    case Subject::Count:
        break;
    }
    return 0.0F;
}

float SHR::Settings::Read(const Config &config, Profile field) noexcept
{
    switch (field)
    {
    case Profile::AudioVolume:
        return config.Audio.Volume;
    case Profile::ListenKey:
        return static_cast<float>(config.Input.Listen);
    case Profile::NotificationsEnabled:
        return config.Notification.Enabled ? 1.0F : 0.0F;
    case Profile::Count:
        break;
    }
    return 0.0F;
}

bool SHR::Settings::Overrides::Set(Subject field, float value) noexcept
{
    const SubjectSpec *spec = FindSubject(field);
    if (spec == nullptr || !InDomain(*spec, value))
    {
        return false;
    }

    m_Values[static_cast<std::size_t>(field)] = value;
    return true;
}

void SHR::Settings::Overrides::Clear(Subject field) noexcept
{
    const auto index = static_cast<std::size_t>(field);
    if (index < SubjectCount)
    {
        m_Values[index].reset();
    }
}

std::optional<float> SHR::Settings::Overrides::Get(Subject field) const noexcept
{
    const auto index = static_cast<std::size_t>(field);
    return index < SubjectCount ? m_Values[index] : std::nullopt;
}

bool SHR::Settings::Overrides::Any() const noexcept
{
    return std::ranges::any_of(m_Values, [](const auto &value) { return value.has_value(); });
}

SHR::RuntimeSettings SHR::Settings::Overrides::Resolve(const RuntimeSettings &defaults) const noexcept
{
    RuntimeSettings resolved = defaults;
    for (const SubjectSpec &spec : SubjectTable)
    {
        if (const std::optional<float> value = Get(spec.Field))
        {
            Apply(resolved, spec, *value);
        }
    }
    return resolved;
}
