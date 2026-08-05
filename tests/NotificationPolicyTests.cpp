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
#include "adapter/NotificationPolicy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace
{
    SHR::Notification CompleteNotification()
    {
        return {
            .Enabled = true,
            .Pulse   = {
                { "resting" },
                { "idle" },
                { "elevated" },
                { "high" },
                { "very high" },
                { "extreme" },
            },
            .Dying      = { "dying" },
            .Dead       = { "dead" },
            .Arrhythmia = { "arrhythmia" },
        };
    }

    // Any draw at all; a one-message pool answers the same for every value.
    constexpr std::size_t AnyDraw = 0;
}

TEST_CASE("Disabled and incomplete configurations suppress every notification", "[notification]")
{
    SHR::Notification notification = CompleteNotification();
    notification.Enabled = false;
    notification.Pulse.resize(1);

    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 40.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 0.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification, AnyDraw));

    notification.Enabled = true;
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification, AnyDraw));
}

TEST_CASE("A band with an empty pool has no message to draw", "[notification]")
{
    SHR::Notification notification = CompleteNotification();
    notification.Pulse[std::to_underlying(SHR::HeartRateLevel::High)] = { };

    const auto empty = SHR::NotificationPolicy::FindEmptyBand(notification);
    REQUIRE(empty);
    CHECK(*empty == std::to_underlying(SHR::HeartRateLevel::High));

    // One unusable band disables the lot, so no band can be shown a blank message.
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle, AnyDraw));

    // A seventh band is never selected, so its contents cannot disable anything.
    notification = CompleteNotification();
    notification.Pulse.emplace_back();
    CHECK_FALSE(SHR::NotificationPolicy::FindEmptyBand(notification));
    CHECK(SHR::NotificationPolicy::IsEnabled(notification));
}

TEST_CASE("The death and arrhythmia slots are silent when left unset", "[notification]")
{
    SHR::Notification notification = CompleteNotification();
    notification.Dying      = { };
    notification.Dead       = { };
    notification.Arrhythmia = { };

    // Still enabled - the six bands are what notifications require.
    REQUIRE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 40.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, true, 0.0F, AnyDraw));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification, AnyDraw));
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
        const auto *selected = SHR::NotificationPolicy::SelectPulse(notification, level, AnyDraw);
        REQUIRE(selected);
        CHECK(*selected == expected);
    }
}

TEST_CASE("Complete configurations select death and arrhythmia notifications", "[notification]")
{
    const SHR::Notification notification = CompleteNotification();

    const auto *dying = SHR::NotificationPolicy::SelectStatus(notification, true, 40.0F, AnyDraw);
    REQUIRE(dying);
    CHECK(*dying == "dying");

    const auto *dead = SHR::NotificationPolicy::SelectStatus(notification, true, 0.0F, AnyDraw);
    REQUIRE(dead);
    CHECK(*dead == "dead");

    const auto *arrhythmia = SHR::NotificationPolicy::SelectArrhythmia(notification, AnyDraw);
    REQUIRE(arrhythmia);
    CHECK(*arrhythmia == "arrhythmia");
}

TEST_CASE("A draw picks one message out of a pool", "[notification]")
{
    SHR::Notification notification = CompleteNotification();
    notification.Pulse[std::to_underlying(SHR::HeartRateLevel::Idle)] = { "first", "second", "third" };

    // Every entry is reachable, and the draw wraps rather than running off the end.
    constexpr std::array expected{ "first", "second", "third", "first" };
    for (std::size_t draw = 0; draw < expected.size(); ++draw)
    {
        CAPTURE(draw);
        const auto *selected =
            SHR::NotificationPolicy::SelectPulse(notification, SHR::HeartRateLevel::Idle, draw);
        REQUIRE(selected);
        CHECK(*selected == expected[draw]);
    }
}

TEST_CASE("Pool indices reduce a draw and can exclude the last message shown", "[notification]")
{
    using SHR::NotificationPolicy::SelectIndex;
    using SHR::NotificationPolicy::SelectIndexExcluding;

    CHECK(SelectIndex(3, 7) == 1);
    // An index already chosen survives being reduced again, which is what lets an excluding draw
    // pass straight through the Select functions unchanged.
    CHECK(SelectIndex(3, SelectIndexExcluding(3, 7, 1)) == SelectIndexExcluding(3, 7, 1));
    // An empty pool has no index to give; the Select functions answer with no message at all.
    CHECK(SelectIndex(0, 7) == 0);

    SECTION("Every draw skips the excluded entry and reaches everything else")
    {
        constexpr std::size_t poolSize = 4;
        constexpr std::size_t previous = 2;

        std::array<bool, poolSize> reached{ };
        for (std::size_t draw = 0; draw < poolSize * 2; ++draw)
        {
            CAPTURE(draw);
            const std::size_t index = SelectIndexExcluding(poolSize, draw, previous);
            REQUIRE(index < poolSize);
            CHECK(index != previous);
            reached[index] = true;
        }

        CHECK(reached == std::array{ true, true, false, true });
    }

    SECTION("Nothing to alternate with means the exclusion is ignored rather than starving")
    {
        CHECK(SelectIndexExcluding(1, 0, 0) == 0);
        CHECK(SelectIndexExcluding(1, 7, 0) == 0);
        CHECK(SelectIndexExcluding(0, 7, 0) == 0);
    }

    SECTION("An out-of-range previous is a first draw, or a pool that shrank under one")
    {
        constexpr std::size_t unset = std::numeric_limits<std::size_t>::max();
        CHECK(SelectIndexExcluding(3, 0, unset) == 0);
        CHECK(SelectIndexExcluding(3, 7, unset) == SelectIndex(3, 7));
    }
}
