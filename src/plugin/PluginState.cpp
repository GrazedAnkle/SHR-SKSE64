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

void SHR::PluginState::Init(const Config &config, float gameHours)
{
    m_Runtime.emplace(RuntimeSettings{
        .Simulation = {
            .RestingHeartRate = config.HeartRate.Resting,
            .MaximumHeartRate = config.HeartRate.Max,
        },
        .ArrhythmiaSusceptibility = config.Arrhythmia.Susceptibility,
    });
    m_Voice.Init(config.Audio.Volume);
    ResetForCharacter(gameHours);
}

void SHR::PluginState::Revert(float gameHours)
{
    m_Voice.FlushAndStop();
    ResetForCharacter(gameHours);
}

void SHR::PluginState::ResetForCharacter(float gameHours)
{
    m_Runtime.value().Init();
    m_LevelTracker = { };
    m_LastHoursPassed = gameHours;
    m_IsListening.store(0);
}

float SHR::PluginState::ConsumeGameHoursDelta(float currentHours) noexcept
{
    const float delta = currentHours - m_LastHoursPassed;
    m_LastHoursPassed = currentHours;
    return delta;
}

bool SHR::PluginState::IsListening() const noexcept
{
    return m_IsListening.load() != 0;
}

void SHR::PluginState::ToggleListening() noexcept
{
    m_IsListening.fetch_xor(1);
}
