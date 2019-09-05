// Copyright (c) 2014-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "base/assert.h"

#include <string>
#include <vector>

namespace audio
{
    // audiosample is class to encapsulate loading and the details
    // of simple audio samples such as wav.
    class AudioSample
    {
    public:
        using u8 = std::uint8_t;

        enum class format {
            S16LE
        };

        // load sample from the provided byte buffer
        AudioSample(const u8* ptr, std::size_t len, const std::string& name);

        // load sample from a file.
        AudioSample(const std::string& file, const std::string& name);

        // return the sample rate in hz
        unsigned rate() const
        { return sample_rate_; }

        // return the number of channels in the sample
        unsigned channels() const
        { return num_channels_; }

        unsigned frames() const
        { return num_frames_; }

        unsigned size() const
        { return buffer_.size(); }

        const void* data(std::size_t offset) const
        {
            ASSERT(offset < buffer_.size());
            return &buffer_[offset];
        }

        std::string name() const
        { return name_; }

    private:
        std::string name_;
        std::vector<u8> buffer_;
        unsigned sample_rate_  = 0;
        unsigned num_channels_ = 0;
        unsigned num_frames_   = 0;
    };

} // namespace