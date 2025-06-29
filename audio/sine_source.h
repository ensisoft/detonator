// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <cmath>

namespace audio
{

#ifdef AUDIO_ENABLE_TEST_SOUND
    // Test source that produces simple sine wave. Useful for doing
    // a simple test when for example implementing a new device backend.
    class SineTestSource : public Source
    {
    public:
        SineTestSource(unsigned frequency, Format format = Format::Float32)
          : mFrequency(frequency)
          , mFormat(format)
        {}
        SineTestSource(unsigned frequency, unsigned millisecs, Format format = Format::Float32)
          : mFrequency(frequency)
          , mFormat(format)
          , mLimitDuration(true)
          , mDuration(millisecs)
        {}
        unsigned GetRateHz() const noexcept override
        { return 44100; }
        unsigned GetNumChannels() const noexcept override
        { return 1; }
        Format GetFormat() const noexcept override
        { return mFormat; }
        std::string GetName() const noexcept override
        { return "Sine"; }
        unsigned FillBuffer(void* buff, unsigned max_bytes) override
        {
            const auto num_channels = 1;
            const auto frame_size = num_channels * ByteSize(mFormat);
            const auto frames = max_bytes / frame_size;
            const auto radial_velocity = math::Pi * 2.0 * mFrequency;
            const auto sample_increment = 1.0/44100.0 * radial_velocity;

            for (unsigned i=0; i<frames; ++i)
            {
                // http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
                const float sample = std::sin(mSampleCounter++ * sample_increment);
                if (mFormat == Format::Float32)
                    ((float*)buff)[i] = sample;
                else if (mFormat == Format::Int32)
                    ((int*)buff)[i] = 0x7fffffff * sample;
                else if (mFormat == Format::Int16)
                    ((short*)buff)[i] = 0x7fff * sample;
            }
            return frames * frame_size;
        }
        bool HasMore(std::uint64_t) const noexcept override
        {
            if (!mLimitDuration) return true;
            const auto seconds = mSampleCounter / 44100.0f;
            const auto millis  = seconds * 1000.0f;
            return millis < mDuration;
        }
        void Shutdown() noexcept override
        { /* intentionally empty */ }
    private:
        const unsigned mFrequency = 0;
        const Format mFormat      = Format::Float32;
        const bool mLimitDuration = false;
        const unsigned mDuration  = 0;
        unsigned mSampleCounter   = 0;
    };
#endif // AUDIO_ENABLE_TEST_SOUND

} // namespace