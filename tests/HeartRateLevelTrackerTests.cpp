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
#include "adapter/HeartRateLevelTracker.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A fresh tracker starts at rest and stays silent while the rate remains there", "[level-tracker]")
{
    SHR::HeartRateLevelTracker tracker;

    CHECK(tracker.Current() == SHR::HeartRateLevel::Resting);
    CHECK_FALSE(tracker.Observe(50.0F));
    CHECK_FALSE(tracker.Observe(0.0F));
    CHECK_FALSE(tracker.Observe(SHR::IdleHeartRateThreshold));
    CHECK(tracker.Current() == SHR::HeartRateLevel::Resting);
}

TEST_CASE("Crossing a band reports once and then goes quiet within that band", "[level-tracker]")
{
    SHR::HeartRateLevelTracker tracker;

    const auto crossed = tracker.Observe(90.0F);
    REQUIRE(crossed);
    CHECK(*crossed == SHR::HeartRateLevel::Idle);
    CHECK(tracker.Current() == SHR::HeartRateLevel::Idle);

    // Same band, up to its upper edge: no repeat notification.
    CHECK_FALSE(tracker.Observe(91.0F));
    CHECK_FALSE(tracker.Observe(129.0F));
    CHECK_FALSE(tracker.Observe(SHR::ElevatedHeartRateThreshold));
}

TEST_CASE("Descending bands are reported the same as ascending ones", "[level-tracker]")
{
    SHR::HeartRateLevelTracker tracker;

    REQUIRE(tracker.Observe(200.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::Extreme);

    const auto descended = tracker.Observe(100.0F);
    REQUIRE(descended);
    CHECK(*descended == SHR::HeartRateLevel::Idle);
}

TEST_CASE("A band skipped entirely reports only the band actually reached", "[level-tracker]")
{
    SHR::HeartRateLevelTracker tracker;

    // Intermediate bands were never observed, so they are never reported.
    const auto jumped = tracker.Observe(250.0F);
    REQUIRE(jumped);
    CHECK(*jumped == SHR::HeartRateLevel::Extreme);
}

TEST_CASE("Every band boundary is an exclusive lower edge", "[level-tracker]")
{
    // Strict greater-than, so a rate exactly on a threshold belongs to the band below it.
    SHR::HeartRateLevelTracker tracker;

    CHECK_FALSE(tracker.Observe(SHR::IdleHeartRateThreshold));
    REQUIRE(tracker.Observe(SHR::IdleHeartRateThreshold + 1.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::Idle);

    CHECK_FALSE(tracker.Observe(SHR::ElevatedHeartRateThreshold));
    REQUIRE(tracker.Observe(SHR::ElevatedHeartRateThreshold + 1.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::Elevated);

    CHECK_FALSE(tracker.Observe(SHR::HighHeartRateThreshold));
    REQUIRE(tracker.Observe(SHR::HighHeartRateThreshold + 1.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::High);

    CHECK_FALSE(tracker.Observe(SHR::VeryHighHeartRateThreshold));
    REQUIRE(tracker.Observe(SHR::VeryHighHeartRateThreshold + 1.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::VeryHigh);

    CHECK_FALSE(tracker.Observe(SHR::ExtremeHeartRateThreshold));
    REQUIRE(tracker.Observe(SHR::ExtremeHeartRateThreshold + 1.0F));
    CHECK(tracker.Current() == SHR::HeartRateLevel::Extreme);
}
