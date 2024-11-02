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

#include <cstdint>
#include <cstring> // for memcpy

#include "base/math.h"

namespace base
{
    class NoiseGenerator
    {
    public:
        NoiseGenerator(float frequency, uint32_t prime1, uint32_t prime2, uint32_t prime3) noexcept
          : mPrime1(prime1)
          , mPrime2(prime2)
          , mPrime3(prime3)
          , mFrequency(frequency)
        {}

        // 1 dimensional noise,
        // returns noise value in the range [0, 1.0f]
        float GetSample(float x) const
        {
            const float period = 1.0f / mFrequency;
            const float x0 = int(x / period) * period;
            const float x1 = x0 + period;
            const auto y0 = Random(x0);
            const auto y1 = Random(x1);
            const float t = (x - x0) / period;
            return interpolate(y0, y1, t, math::Interpolation::Cosine);
        }

        // 2 dimensional noise
        // returns a noise value in the range [0, 1.0f]
        float GetSample(float x, float y) const
        {
            const float period = 1.0f / mFrequency;
            const float x0 = int(x / period) * period;
            const float x1 = x0 + period;
            const float y0 = int(y / period) * period;
            const float y1 = y0 + period;
            const float samples[4] = {
                    Random(x0, y0),
                    Random(x1, y0),
                    Random(x0, y1),
                    Random(x1, y1)
            };
            const auto t = (x - x0) / period;
            const float xbot = interpolate(samples[0], samples[1], t, math::Interpolation::Cosine);
            const float xtop = interpolate(samples[2], samples[3], t, math::Interpolation::Cosine);
            return interpolate(xbot, xtop, (y-y0) / period, math::Interpolation::Cosine);
        }
    private:
        float Random(float x) const
        {
            uint32_t bits = 0;  // std::bit_cast in cpp20
            std::memcpy(&bits, &x, sizeof(x));
            const uint32_t mask = (~(uint32_t)0) >> 1;
            const uint32_t val  = (bits << 13) ^ bits;
            return ((val * (val * val * mPrime1 + mPrime2) + mPrime3) & mask) / (float)mask;
        }
        float Random(float x, float y) const
        {
            return Random(x + y * 57);
        }

    private:
        uint32_t mPrime1 = 0;
        uint32_t mPrime2 = 0;
        uint32_t mPrime3 = 0;
        float mFrequency = 0;
    };

} // namespace