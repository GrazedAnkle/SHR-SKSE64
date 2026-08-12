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
#include "adapter/CombatEngagementTracker.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A fresh tracker is disengaged and stays silent while nothing engages", "[combat-tracker]")
{
    SHR::CombatEngagementTracker tracker;

    CHECK_FALSE(tracker.Engaged());
    CHECK_FALSE(tracker.Observe(false));
    CHECK_FALSE(tracker.Engaged());
}

TEST_CASE("Entering combat reports once", "[combat-tracker]")
{
    SHR::CombatEngagementTracker tracker;

    CHECK(tracker.Observe(true));
    CHECK(tracker.Engaged());
}

TEST_CASE("Repeated engaged observations within one encounter report nothing further", "[combat-tracker]")
{
    // The searching/combat alternation, and one observation per hostile actor, both arrive here as a
    // run of engaged states. Only the first is an entry.
    SHR::CombatEngagementTracker tracker;

    REQUIRE(tracker.Observe(true));
    CHECK_FALSE(tracker.Observe(true));
    CHECK_FALSE(tracker.Observe(true));
    CHECK_FALSE(tracker.Observe(true));
    CHECK(tracker.Engaged());
}

TEST_CASE("A later encounter reports again once combat has been left", "[combat-tracker]")
{
    SHR::CombatEngagementTracker tracker;

    REQUIRE(tracker.Observe(true));
    CHECK_FALSE(tracker.Observe(false));
    CHECK_FALSE(tracker.Engaged());

    CHECK(tracker.Observe(true));
    CHECK(tracker.Engaged());
}

TEST_CASE("Leaving combat is never itself an entry", "[combat-tracker]")
{
    SHR::CombatEngagementTracker tracker;

    REQUIRE(tracker.Observe(true));
    CHECK_FALSE(tracker.Observe(false));
    CHECK_FALSE(tracker.Observe(false));
}
