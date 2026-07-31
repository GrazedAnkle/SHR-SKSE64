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

#include "core/SimulationSettings.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SHR
{
    struct Debug
    {
        static constexpr const char LogKey[]   = "log";
        static constexpr const char FlushKey[] = "flush";

        spdlog::level::level_enum Log   = spdlog::level::level_enum::info;
        spdlog::level::level_enum Flush = spdlog::level::level_enum::trace;
    };

    struct HeartRate
    {
        static constexpr const char RestingKey[] = "resting";
        static constexpr const char MaxKey[]     = "max";

        // Initial resting heart rate; also initializes fitness.
        float Resting = SimulationSettings{ }.RestingHeartRate;
        float Max     = SimulationSettings{ }.MaximumHeartRate;
    };

    struct Arrhythmia
    {
        static constexpr const char SusceptibilityKey[] = "susceptibility";

        // Scalar multiplier on PVC frequency and run-extension.
        float Susceptibility = 1.0F;
    };

    struct Input
    {
        static constexpr const char ListenKey[] = "listen";

        std::uint32_t Listen = 0x23;  // H key.
    };

    struct Audio
    {
        static constexpr const char VolumeKey[] = "volume";

        float Volume = 1.0F;
    };

    struct Notification
    {
        static constexpr const char EnabledKey[]    = "enabled";
        static constexpr const char PulseKey[]      = "pulse";
        static constexpr const char DyingKey[]      = "dying";
        static constexpr const char DeadKey[]       = "dead";
        static constexpr const char ArrhythmiaKey[] = "arrhythmia";

        bool                     Enabled = false;
        std::vector<std::string> Pulse;
        std::string              Dying;
        std::string              Dead;
        std::string              Arrhythmia;
    };

    struct Config
    {
        static constexpr const char DebugKey[]        = "debug";
        static constexpr const char HeartRateKey[]    = "heart_rate";
        static constexpr const char ArrhythmiaKey[]   = "arrhythmia";
        static constexpr const char InputKey[]        = "input";
        static constexpr const char AudioKey[]        = "audio";
        static constexpr const char NotificationKey[] = "notification";

        Debug        Debug;
        HeartRate    HeartRate;
        Arrhythmia   Arrhythmia;
        Input        Input;
        Audio        Audio;
        Notification Notification;

        static void Init(std::string_view configPath);
        static void Set(Config config);

        static const Config &Get();
    };
}
