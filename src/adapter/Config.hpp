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

#include "core/Constants.hpp"
#include "core/SimulationSettings.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <memory>
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
        static constexpr const char RestingKey[]        = "resting";
        static constexpr const char MaxKey[]            = "max";
        static constexpr const char FitnessCeilingKey[] = "fitness_ceiling";

        // Initial resting heart rate; also initializes fitness.
        float Resting = SimulationSettings{ }.RestingHeartRate;
        float Max     = SimulationSettings{ }.MaximumHeartRate;
        // mL/kg/min, the unit VO2max is published in; the model works in METs.
        float FitnessCeiling =
            SimulationSettings{ }.FitnessMaxMets * Constants::MetsToVO2;
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

    // Interchangeable wordings for one notification; a firing draws one of them. Configuration may
    // write a bare string wherever a pool is expected.
    using MessagePool = std::vector<std::string>;

    struct Notification
    {
        static constexpr const char EnabledKey[]    = "enabled";
        static constexpr const char PulseKey[]      = "pulse";
        static constexpr const char DyingKey[]      = "dying";
        static constexpr const char DeadKey[]       = "dead";
        static constexpr const char ArrhythmiaKey[] = "arrhythmia";

        bool                     Enabled = false;
        // One pool per heart-rate band, in order of increasing rate.
        std::vector<MessagePool> Pulse;
        MessagePool              Dying;
        MessagePool              Dead;
        MessagePool              Arrhythmia;
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

        // Writes the menu-owned profile settings back to the file Init read, so a change made in
        // game survives a restart. Only those keys are touched, and comments and key order are kept,
        // because the file is the player's to hand-edit. Returns false if it could not be written.
        static bool Persist();

        // Immutable snapshot; read related fields from one, as two calls can straddle a Set.
        static std::shared_ptr<const Config> Get();
    };
}
