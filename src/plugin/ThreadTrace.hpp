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

// TEMPORARY scaffolding that measured the thread matrix in docs/ARCHITECTURE.md; delete with its
// call sites once https://github.com/GrazedAnkle/SHR-SKSE64/issues/7 closes its last row.
//
// Each site tracks the DISTINCT threads it has been called on, not just the first: a site that
// usually arrives on the update thread but occasionally does not is the finding that matters, and
// a stable site still logs exactly once.

#include <mutex>

#include <Windows.h>

namespace SHR::ThreadTrace
{
    // A site exceeding this has already established that it is pool-dispatched.
    inline constexpr unsigned int MaxDistinctThreads = 8;

    struct Site
    {
        std::mutex    Mutex;
        unsigned long Threads[MaxDistinctThreads] = { };
        unsigned int  Count                       = 0;
        bool          Overflowed                  = false;
    };

    inline void Record(Site &site, const char *name)
    {
        const unsigned long current = ::GetCurrentThreadId();

        // Acceptable even on the XAudio callback: uncontended, held for a handful of comparisons.
        const std::scoped_lock lock(site.Mutex);

        for (unsigned int i = 0; i < site.Count; ++i)
        {
            if (site.Threads[i] == current)
            {
                return;
            }
        }

        if (site.Count == MaxDistinctThreads)
        {
            if (!site.Overflowed)
            {
                site.Overflowed = true;
                SKSE::log::warn(
                    FMT_STRING("[thread-matrix] {:s} exceeded {:d} distinct threads; no longer reporting"),
                    name,
                    MaxDistinctThreads
                );
            }
            return;
        }

        site.Threads[site.Count] = current;
        ++site.Count;

        // Decimal to match the %t the log pattern already stamps; two renderings of one number on
        // the same line read as two threads.
        if (site.Count == 1)
        {
            SKSE::log::info(FMT_STRING("[thread-matrix] {:s} first thread={:d}"), name, current);
        }
        else
        {
            SKSE::log::warn(
                FMT_STRING("[thread-matrix] {:s} additional thread={:d} (distinct so far={:d})"),
                name,
                current,
                site.Count
            );
        }
    }
}

// Each expansion gets its own site, so there is no shared registry.
#define SHR_TRACE_THREAD(name)                                  \
    do                                                          \
    {                                                           \
        static ::SHR::ThreadTrace::Site shrTraceSite_;          \
        ::SHR::ThreadTrace::Record(shrTraceSite_, (name));      \
    }                                                           \
    while (false)
