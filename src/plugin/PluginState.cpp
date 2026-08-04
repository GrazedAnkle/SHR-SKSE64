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
#include "plugin/PluginState.hpp"

SHR::PluginState &SHR::PluginState::Get()
{
    // Leaked deliberately rather than held as a static object: a static would register ~PluginState
    // to run at process exit, which is the teardown the header explains this type must not do. A
    // static reference has no destructor of its own, so nothing runs.
    static PluginState &instance = *new PluginState();
    return instance;
}

void SHR::PluginState::Init(const Config &config)
{
    m_Defaults = {
        .Simulation = {
            .RestingHeartRate = config.HeartRate.Resting,
            .MaximumHeartRate = config.HeartRate.Max,
            .FitnessMaxMets   = config.HeartRate.FitnessCeiling / Constants::MetsToVO2,
        },
        .ArrhythmiaSusceptibility = config.Arrhythmia.Susceptibility,
    };
    m_Runtime.emplace(m_Defaults);
    m_Voice.Init(config.Audio.Volume);
    ResetForCharacter();
}

void SHR::PluginState::Revert()
{
    m_Voice.FlushAndStop();
    ResetForCharacter();
}

void SHR::PluginState::ResetForCharacter()
{
    // Before Init, so it reseeds from the profile rather than the last character's settings.
    ResetSubjectSettings();

    m_Runtime.value().Init();
    m_LevelTracker = { };
    m_GameClock.Reset();
    m_IsListening.store(0);
}

bool SHR::PluginState::ApplySubject(const Settings::SubjectSpec &spec, float value)
{
    const std::optional<float> previous = m_Overrides.Get(spec.Field);
    if (!m_Overrides.Set(spec.Field, value))
    {
        return false;
    }

    if (!m_Runtime.value().ApplySettings(m_Overrides.Resolve(m_Defaults)))
    {
        // Rejected as a whole, so the record of it goes back too.
        if (previous)
        {
            m_Overrides.Set(spec.Field, *previous);
        }
        else
        {
            m_Overrides.Clear(spec.Field);
        }
        return false;
    }
    return true;
}

bool SHR::PluginState::ApplyProfile(const Settings::ProfileSpec &spec, float value)
{
    Config updated = *Config::Get();
    if (!Settings::Apply(updated, spec, value))
    {
        return false;
    }
    Config::Set(updated);
    Config::Persist();

    // The only one with a live consumer to push into; the rest are read from the snapshot in place.
    if (spec.Field == Settings::Profile::AudioVolume)
    {
        m_Voice.SetVolume(value);
    }
    return true;
}

void SHR::PluginState::ResetSubjectSettings()
{
    m_Overrides = { };
    m_Runtime.value().ApplySettings(m_Defaults);
}

void SHR::PluginState::ResetProfileSettings()
{
    // Registry-driven, so a setting added later resets without being named here again.
    const Config defaults;
    Config updated = *Config::Get();
    for (const Settings::ProfileSpec &spec : Settings::ProfileSettings())
    {
        Settings::Apply(updated, spec, Settings::Read(defaults, spec.Field));
    }

    Config::Set(updated);
    Config::Persist();
    m_Voice.SetVolume(updated.Audio.Volume);
}

bool SHR::PluginState::AdoptOverrides(const Settings::Overrides &overrides)
{
    if (!m_Runtime.value().ApplySettings(overrides.Resolve(m_Defaults)))
    {
        return false;
    }
    m_Overrides = overrides;
    return true;
}

float SHR::PluginState::ReadSubject(Settings::Subject field) const
{
    return Settings::Read(m_Runtime.value().GetSettings(), field);
}

float SHR::PluginState::ReadProfile(Settings::Profile field)
{
    return Settings::Read(*Config::Get(), field);
}

float SHR::PluginState::ConsumeGameHoursDelta(float currentHours) noexcept
{
    return m_GameClock.Consume(currentHours);
}

bool SHR::PluginState::IsListening() const noexcept
{
    return m_IsListening.load() != 0;
}

void SHR::PluginState::ToggleListening() noexcept
{
    m_IsListening.fetch_xor(1);
}
