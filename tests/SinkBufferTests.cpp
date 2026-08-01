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
#include "plugin/SinkBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>

namespace
{
    // Stands in for the encoded beat; the counters are what make a leak observable.
    struct CountedPayload
    {
        static inline int Live      = 0;
        static inline int Destroyed = 0;

        static void Reset()
        {
            Live      = 0;
            Destroyed = 0;
        }

        explicit CountedPayload(int value) : Value(value) { ++Live; }
        ~CountedPayload()
        {
            --Live;
            ++Destroyed;
        }

        CountedPayload(const CountedPayload &)            = delete;
        CountedPayload &operator=(const CountedPayload &) = delete;

        int Value;
    };

    // Only zero is meaningful to the protocol; the rejection code is XAudio2's for realism.
    constexpr std::int32_t Accepted = 0;
    constexpr std::int32_t Rejected = -2004353023; // XAUDIO2_E_INVALID_CALL
}

TEST_CASE("Accepted submission transfers the buffer to the sink", "[audio][sink]")
{
    CountedPayload::Reset();

    void *sinkContext = nullptr;
    const std::int32_t status = SHR::SubmitSinkBuffer(
        std::make_unique<CountedPayload>(7),
        [&sinkContext](const CountedPayload &payload, void *context) {
            CHECK(payload.Value == 7);
            // The sink round-trips this to its completion callback, so it must address the payload.
            CHECK(context == &payload);
            sinkContext = context;
            return Accepted;
        }
    );

    CHECK(status == Accepted);
    // Still alive: the sink owns it now, and reads it until its callback returns it.
    CHECK(CountedPayload::Live == 1);
    CHECK(CountedPayload::Destroyed == 0);

    SHR::DeleteSinkBuffer<CountedPayload>(sinkContext);

    CHECK(CountedPayload::Live == 0);
    CHECK(CountedPayload::Destroyed == 1);
}

TEST_CASE("Rejected submission frees the buffer the sink refused", "[audio][sink]")
{
    CountedPayload::Reset();

    bool submitted = false;
    const std::int32_t status = SHR::SubmitSinkBuffer(
        std::make_unique<CountedPayload>(7),
        [&submitted](const CountedPayload &, void *) {
            submitted = true;
            return Rejected;
        }
    );

    CHECK(status == Rejected);
    CHECK(submitted);
    // The sink never took it, so no completion callback will ever arrive to free it.
    CHECK(CountedPayload::Live == 0);
    CHECK(CountedPayload::Destroyed == 1);
}

TEST_CASE("Sustained rejection does not accumulate buffers", "[audio][sink]")
{
    CountedPayload::Reset();

    // The unbounded case: once the sink's queue stops draining, every subsequent beat is refused.
    constexpr int BeatCount = 200;
    for (int beat = 0; beat < BeatCount; ++beat)
    {
        const std::int32_t status = SHR::SubmitSinkBuffer(
            std::make_unique<CountedPayload>(beat),
            [](const CountedPayload &, void *) { return Rejected; }
        );
        CHECK(status == Rejected);
    }

    CHECK(CountedPayload::Live == 0);
    CHECK(CountedPayload::Destroyed == BeatCount);
}
