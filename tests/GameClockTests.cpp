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
#include "adapter/GameClock.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A fresh clock has nothing to difference against, so its first sample is zero", "[game-clock]")
{
    SHR::GameClock clock;

    CHECK(clock.Consume(120.5F) == 0.0F);
}

TEST_CASE("Successive samples difference into elapsed hours", "[game-clock]")
{
    SHR::GameClock clock;

    REQUIRE(clock.Consume(100.0F) == 0.0F);
    CHECK(clock.Consume(100.25F) == 0.25F);
    CHECK(clock.Consume(103.25F) == 3.0F);

    // Every sample rebases, so a delta is never re-reported.
    CHECK(clock.Consume(103.25F) == 0.0F);
}

TEST_CASE("A reset absorbs the step to the next timeline whichever way the clock moves", "[game-clock]")
{
    SHR::GameClock clock;

    REQUIRE(clock.Consume(500.0F) == 0.0F);
    REQUIRE(clock.Consume(506.0F) == 6.0F);

    SECTION("Loading a save from earlier in the playthrough")
    {
        clock.Reset();

        CHECK(clock.Consume(200.0F) == 0.0F);
        CHECK(clock.Consume(201.0F) == 1.0F);
    }

    SECTION("Loading a save from later in the playthrough")
    {
        clock.Reset();

        // Hours the character never lived are not drift this clock reports.
        CHECK(clock.Consume(900.0F) == 0.0F);
        CHECK(clock.Consume(900.5F) == 0.5F);
    }
}

TEST_CASE("Resetting a clock that never sampled leaves it fresh", "[game-clock]")
{
    SHR::GameClock clock;

    clock.Reset();

    CHECK(clock.Consume(42.0F) == 0.0F);
    CHECK(clock.Consume(43.0F) == 1.0F);
}

TEST_CASE("Peeking reports the held reading without rebasing it", "[game-clock]")
{
    SHR::GameClock clock;

    CHECK_FALSE(clock.Peek().has_value());

    REQUIRE(clock.Consume(300.0F) == 0.0F);
    REQUIRE(clock.Peek() == 300.0F);

    // Peeking must leave the next difference intact.
    CHECK(clock.Peek() == 300.0F);
    CHECK(clock.Consume(308.0F) == 8.0F);
    CHECK(clock.Peek() == 308.0F);

    clock.Reset();

    CHECK_FALSE(clock.Peek().has_value());
}
