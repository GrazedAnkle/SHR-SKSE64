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
#include "NotificationPolicy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

namespace
{
    SHR::Notification CompleteNotification()
    {
        return {
            .Enabled    = true,
            .Pulse      = { "resting", "idle", "elevated", "high", "very high", "extreme" },
            .Dying      = "dying",
            .Dead       = "dead",
            .Arrhythmia = "arrhythmia",
        };
    }
}

TEST_CASE("Disabled and incomplete configurations suppress every notification", "[notification]")
{
    SHR::Notification notification = CompleteNotification();
    notification.Enabled = false;
    notification.Pulse.resize(1);

    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 40.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 0.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification));

    notification.Enabled = true;
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification));
}

TEST_CASE("Notification selection covers every heart-rate level", "[notification]")
{
    const SHR::Notification notification = CompleteNotification();
    constexpr std::array levels{
        std::pair{ SHR::HeartRateLevel::Resting,  "resting" },
        std::pair{ SHR::HeartRateLevel::Idle,     "idle" },
        std::pair{ SHR::HeartRateLevel::Elevated, "elevated" },
        std::pair{ SHR::HeartRateLevel::High,     "high" },
        std::pair{ SHR::HeartRateLevel::VeryHigh, "very high" },
        std::pair{ SHR::HeartRateLevel::Extreme,  "extreme" },
    };

    for (const auto &[level, expected] : levels)
    {
        CAPTURE(level);
        const auto selected = SHR::NotificationPolicy::SelectPulse(notification, level);
        REQUIRE(selected);
        CHECK(*selected == expected);
    }
}

TEST_CASE("Complete configurations select death and arrhythmia notifications", "[notification]")
{
    const SHR::Notification notification = CompleteNotification();

    const auto dying = SHR::NotificationPolicy::SelectStatus(notification, true, 40.0F);
    REQUIRE(dying);
    CHECK(*dying == "dying");

    const auto dead = SHR::NotificationPolicy::SelectStatus(notification, true, 0.0F);
    REQUIRE(dead);
    CHECK(*dead == "dead");

    const auto arrhythmia = SHR::NotificationPolicy::SelectArrhythmia(notification);
    REQUIRE(arrhythmia);
    CHECK(*arrhythmia == "arrhythmia");
}
