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
#include <memory>
#include <fstream>

#include "audio/decoder.h"

// todo: maybe move these wrappers out of the audio namespace?

typedef	struct SNDFILE_tag	SNDFILE ;

namespace audio
{
    class SourceStream;

    class SndFileDecoder : public Decoder
    {
    public:
        SndFileDecoder() = default;
        SndFileDecoder(std::shared_ptr<const SourceStream> io);
       ~SndFileDecoder();
        virtual unsigned GetSampleRate() const override
        { return mSampleRate; }
        virtual unsigned GetNumChannels() const override
        { return mChannels; }
        virtual unsigned GetNumFrames() const override
        { return mFrames; }
        virtual size_t ReadFrames(float* ptr, size_t frames) override;
        virtual size_t ReadFrames(short* ptr, size_t frames) override;
        virtual size_t ReadFrames(int* ptr, size_t frames) override;
        virtual void Reset() override;

        // Try to open the decoder based on the given IO device.
        // Returns true if successful. Errors are logged.
        bool Open(std::shared_ptr<const SourceStream> io);
    private:
        std::int64_t GetLength() const;
        std::int64_t Seek(std::int64_t offset, int whence);
        std::int64_t Read(void* ptr, std::int64_t count);
        std::int64_t Tell() const;
    private:
        std::shared_ptr<const SourceStream> mSource;
        SNDFILE* mFile = nullptr;
        unsigned mSampleRate = 0;
        unsigned mFrames     = 0;
        unsigned mChannels   = 0;
        std::int64_t mOffset = 0;
    };

} // namespace
