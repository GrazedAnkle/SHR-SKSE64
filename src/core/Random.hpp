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

#include <cstddef>
#include <random>

namespace Detail
{
    inline std::mt19937 &rng()
    {
        thread_local std::mt19937 engine(std::random_device{ }());
        return engine;
    }
}

template<typename Float>
inline Float Random(Float min, Float max)
{
    std::uniform_real_distribution<Float> distribution(min, max);
    return distribution(Detail::rng());
}

inline float RandomNormal()
{
    std::normal_distribution<float> distribution(0.0F, 1.0F);
    return distribution(Detail::rng());
}

// A uniform draw for choosing among interchangeable alternatives, such as a notification message.
// Deliberately not RhythmRandom, which is seeded so offline renders reproduce.
inline std::size_t RandomDraw()
{
    std::uniform_int_distribution<std::size_t> distribution;
    return distribution(Detail::rng());
}
