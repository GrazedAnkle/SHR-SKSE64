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

#include <cstdint>
#include <memory>
#include <utility>

namespace SHR
{
    // Ownership transfer across an audio sink boundary. The sink receives a raw context pointer and
    // returns it through a completion callback, so ownership may only move once the sink has accepted
    // the buffer.

    // Pairs with a successful SubmitSinkBuffer.
    template <typename Payload>
    void DeleteSinkBuffer(void *context)
    {
        delete static_cast<Payload *>(context);
    }

    // `submit(const Payload &, void *context)` returns the sink's status code, zero on success. It
    // receives the payload so it can describe the bytes, and the context the sink will hand back.
    template <typename Payload, typename Submit>
    std::int32_t SubmitSinkBuffer(std::unique_ptr<Payload> payload, Submit &&submit)
    {
        const std::int32_t status = std::forward<Submit>(submit)(
            std::as_const(*payload),
            static_cast<void *>(payload.get())
        );
        if (status == 0)
        {
            (void)payload.release();
        }
        return status;
    }
}
