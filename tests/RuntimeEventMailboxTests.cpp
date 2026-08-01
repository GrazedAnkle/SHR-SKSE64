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
#include "core/RuntimeEventMailbox.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace
{
    using Buffer = std::array<SHR::RuntimeEvent, SHR::RuntimeEventMailbox::Capacity>;
}

TEST_CASE("An empty mailbox drains nothing", "[mailbox]")
{
    SHR::RuntimeEventMailbox mailbox;
    Buffer buffer;

    CHECK(mailbox.Drain(buffer) == 0);
    CHECK(mailbox.DroppedCount() == 0);
}

TEST_CASE("Events drain in post order and only once", "[mailbox]")
{
    SHR::RuntimeEventMailbox mailbox;
    Buffer buffer;

    REQUIRE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }));
    REQUIRE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Sleep, .Duration = 42.0F }));
    REQUIRE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Jump }));

    REQUIRE(mailbox.Drain(buffer) == 3);
    CHECK(buffer[0].Kind == SHR::RuntimeEventKind::Hit);
    CHECK(buffer[1].Kind == SHR::RuntimeEventKind::Sleep);
    CHECK(buffer[1].Duration == 42.0F);
    CHECK(buffer[2].Kind == SHR::RuntimeEventKind::Jump);

    // A second drain must not replay events into the simulation.
    CHECK(mailbox.Drain(buffer) == 0);
}

TEST_CASE("Overflow drops rather than blocking, and is counted", "[mailbox]")
{
    SHR::RuntimeEventMailbox mailbox;
    Buffer buffer;

    for (std::size_t i = 0; i < SHR::RuntimeEventMailbox::Capacity; ++i)
    {
        REQUIRE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }));
    }

    CHECK_FALSE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }));
    CHECK_FALSE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }));
    CHECK(mailbox.DroppedCount() == 2);

    // The drain restores capacity; the drop tally is cumulative rather than reset.
    REQUIRE(mailbox.Drain(buffer) == SHR::RuntimeEventMailbox::Capacity);
    CHECK(mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }));
    CHECK(mailbox.DroppedCount() == 2);
}

TEST_CASE("Clear discards pending events without counting them as drops", "[mailbox]")
{
    SHR::RuntimeEventMailbox mailbox;
    Buffer buffer;

    REQUIRE(mailbox.Post({ .Kind = SHR::RuntimeEventKind::CombatEntry }));
    mailbox.Clear();

    CHECK(mailbox.Drain(buffer) == 0);
    CHECK(mailbox.DroppedCount() == 0);
}

TEST_CASE("Concurrent producers lose no events to a racing consumer", "[mailbox][stress]")
{
    // Several workers posting while the update thread drains: every posted event is either drained
    // exactly once or counted as dropped, never silently lost or duplicated.
    constexpr int ProducerCount     = 6;
    constexpr int EventsPerProducer = 2000;
    constexpr int TotalPosted       = ProducerCount * EventsPerProducer;

    SHR::RuntimeEventMailbox mailbox;
    std::atomic_int          accepted = 0;
    std::atomic_bool         done     = false;
    std::vector<std::thread> producers;

    for (int p = 0; p < ProducerCount; ++p)
    {
        producers.emplace_back([&mailbox, &accepted] {
            for (int i = 0; i < EventsPerProducer; ++i)
            {
                if (mailbox.Post({ .Kind = SHR::RuntimeEventKind::Hit }))
                {
                    accepted.fetch_add(1);
                }
            }
        });
    }

    int    drained = 0;
    Buffer buffer;
    while (!done.load())
    {
        drained += static_cast<int>(mailbox.Drain(buffer));
        if (accepted.load() + static_cast<int>(mailbox.DroppedCount()) == TotalPosted)
        {
            done.store(true);
        }
    }

    for (std::thread &producer : producers)
    {
        producer.join();
    }
    drained += static_cast<int>(mailbox.Drain(buffer));

    const int dropped = static_cast<int>(mailbox.DroppedCount());
    CHECK(accepted.load() + dropped == TotalPosted);
    CHECK(drained == accepted.load());
}
