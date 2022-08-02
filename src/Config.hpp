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

namespace SHR
{
    struct Debug
    {
        static constexpr const char LogKey[]   = "log";
        static constexpr const char FlushKey[] = "flush";

        spdlog::level::level_enum Log   = spdlog::level::level_enum::info;
        spdlog::level::level_enum Flush = spdlog::level::level_enum::trace;
    };

    struct Limit
    {
        static constexpr const char RestingKey[]   = "resting";
        static constexpr const char IdleKey[]      = "idle";
        static constexpr const char WalkingKey[]   = "walking";
        static constexpr const char RunningKey[]   = "running";
        static constexpr const char SprintingKey[] = "sprinting";
        static constexpr const char CombatKey[]    = "combat";

        float Resting   =  55.0F;
        float Idle      =  80.0F;
        float Walking   = 110.0F;
        float Running   = 160.0F;
        float Sprinting = 190.0F;
        float Combat    = 200.0F;
    };

    struct Multiplier
    {
        static constexpr const char ModKey[]         = "general";
        static constexpr const char IncDecRatioKey[] = "increase_decrease_ratio";
        static constexpr const char SkipChanceKey[]  = "skip_chance";

        float Mod         = 1.0F;
        float IncDecRatio = 3.0F;
        float SkipChance  = 1.0F;
    };

    struct Input
    {
        static constexpr const char ListenKey[] = "listen";

        std::uint32_t Listen = 0x23; // H key.
    };

    struct Notification
    {
        static constexpr const char EnabledKey[]      = "enabled";
        static constexpr const char PulseKey[]        = "pulse";
        static constexpr const char DyingKey[]        = "dying";
        static constexpr const char FibrillatingKey[] = "fibrillating";
        static constexpr const char DeadKey[]         = "dead";
        static constexpr const char SkippedKey[]      = "skipped";

        bool Enabled = false;
        std::vector<std::string> Pulse;
        std::string Dying;
        std::string Fibrillating;
        std::string Dead;
        std::string Skipped;
    };

    struct Config
    {
        static constexpr const char DebugKey[]        = "debug";
        static constexpr const char LimitKey[]        = "limit";
        static constexpr const char MultiplierKey[]   = "multiplier";
        static constexpr const char InputKey[]        = "input";
        static constexpr const char NotificationKey[] = "notification";

        Debug        Debug;
        Limit        Limit;
        Multiplier   Multiplier;
        Input        Input;
        Notification Notification;

        static void Init();

        static const Config &Get();
    };
}
