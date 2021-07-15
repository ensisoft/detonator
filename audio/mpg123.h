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

#include <string>
#include <memory>

#include "audio/decoder.h"
#include "audio/format.h"

typedef struct mpg123_handle_struct mpg123_handle;

namespace audio
{
    // todo: provide IO abstractions similar to sndfile here.

    class Mpg123Decoder : public Decoder
    {
    public:
        Mpg123Decoder();
       ~Mpg123Decoder();
        virtual unsigned GetSampleRate() const override;
        virtual unsigned GetNumChannels() const override;
        virtual unsigned GetNumFrames() const override;
        virtual size_t ReadFrames(float* ptr, size_t frames) override;
        virtual size_t ReadFrames(short* ptr, size_t frames) override;
        virtual size_t ReadFrames(int* ptr, size_t frames) override;
        bool Open(const std::string& file, SampleType format = SampleType::Int16);
    private:
        template<typename T>
        size_t ReadFrames(T* ptr, size_t frames);
    private:
        struct Library;
        std::shared_ptr<Library> mLibrary;
        mpg123_handle* mHandle = nullptr;
        unsigned mSampleRate = 0;
        unsigned mFrameCount = 0;
        unsigned mOutFormat  = 0;
        bool mIsOpen = false;
    };
} // namespace
