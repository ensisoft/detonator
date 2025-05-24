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

#include "config.h"

#include "warnpush.h"
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

namespace engine
{
    // smooth the frame deltas using a simple moving average
    // based on the historical (previous) frame stamp values.
    // the problem is that the frame delta has small jitter
    // which causes micro stutter when animating.
    // https://medium.com/@alen.ladavac/the-elusive-frame-timing-168f899aec92
    class FrameTimer
    {
    public:
        FrameTimer() : mSamples(10)
        {}

        void AddSample(float dt)
        {
            if (mSamples.full())
            {
                const auto old = mSamples.front();
                mSamples.push_back(dt);
                mSum -= old;
                mSum += dt;
            }
            else
            {
                mSamples.push_back(dt);
                mSum += dt;
            }
            mAverage = mSum / float(mSamples.size());
        }
        float GetAverage() const noexcept
        {
            return mAverage;
        }
    private:
        boost::circular_buffer<float> mSamples;
        double mSum = 0.0;
        double mAverage = 0.0;
    };

} // namespace