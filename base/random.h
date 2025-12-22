// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "config.h"

#include <ctime>
#include <type_traits>

#include "warnpush.h"
#  include <boost/random/mersenne_twister.hpp>
#  include <boost/random/uniform_int_distribution.hpp>
#  include <boost/random/uniform_real_distribution.hpp>
#include "warnpop.h"

namespace base
{
    // Generate pseudo random numbers based on the given seed.
    template<unsigned Seed, typename T>
    T rand(const T min, const T max) noexcept
    {
        // boost uniform distribution has an assert for the condition
        // that min < max. We have code that calls  random with (for example
        // 1.0f, 1.0f) and we must keep this working, so simply return min.
        if (min >= max)
            return min;

        static boost::random::mt19937 engine(Seed);
        if constexpr (std::is_floating_point<T>::value) {
            boost::random::uniform_real_distribution<T> dist(min, max);
            return dist(engine);
        } else {
            boost::random::uniform_int_distribution<T> dist(min, max);
            return dist(engine);
        }
    }

    // generate a random number in the range of min max (inclusive)
    // the random number generator is automatically seeded.
    template<typename T>
    T rand(T min, T max) noexcept
    {
        // boost uniform distribution has an assert for the condition
        // that min < max. We have code that calls  random with (for example
        // 1.0f, 1.0f) and we must keep this working, so simply return min.
        if (min >= max)
            return min;

        // if we enable this flag we always give out a deterministic
        // sequence by initializing the engine with a predetermined seed.
        // this is convenient for example testing purposes, enable this
        // flag and always get the same sequence without having to change
        // the calling code that doesn't really care.
#if defined(MATH_FORCE_DETERMINISTIC_RANDOM)
        static boost::random::mt19937 engine(0xdeadbeef);
#else
        static boost::random::mt19937 engine(std::time(nullptr));
#endif
        if constexpr (std::is_floating_point<T>::value) {
            boost::random::uniform_real_distribution<T> dist(min, max);
            return dist(engine);
        } else {
            boost::random::uniform_int_distribution<T> dist(min, max);
            return dist(engine);
        }
    }

    template<typename T, unsigned int Seed>
    struct RandomGenerator {
        RandomGenerator()
        {
            mersenne_twister_ = boost::mt19937(Seed);
        }
        RandomGenerator(T min, T max) noexcept
          : min_(min), max_(max)
        {
            mersenne_twister_ = boost::mt19937(Seed);
        }
        T operator()() const noexcept
        {
            return Generate(min_, max_);
        }
        T operator()(T min, T max) const noexcept
        {
            return Generate(min, max);
        }
        static T rand(T min, T max) noexcept
        {
            return base::rand<Seed, T>(min, max);
        }
    private:
        T Generate(T min, T max) const noexcept
        {
            if (min >= max)
                return min;

            if constexpr (std::is_floating_point<T>::value) {
                boost::random::uniform_real_distribution<T> dist(min, max);
                return dist(mersenne_twister_);
            } else {
                boost::random::uniform_int_distribution<T> dist(min, max);
                return dist(mersenne_twister_);
            }
        }
    private:
        T min_ = T();
        T max_ = T();
        mutable boost::mt19937 mersenne_twister_;
    };

} // namespace