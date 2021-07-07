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

// todo: maybe move these wrappers out of the audio namespace?

typedef	struct SNDFILE_tag	SNDFILE ;

namespace audio
{
    class SndFileIODevice
    {
    public:
        virtual ~SndFileIODevice() = default;
        virtual std::int64_t GetLength() const = 0;
        virtual std::int64_t Seek(std::int64_t offset, int whence) = 0;
        virtual std::int64_t Read(void* ptr, std::int64_t count) = 0;
        virtual std::int64_t Tell() const = 0;
    private:
    };

    class SndFileVirtualDevice
    {
    public:
        SndFileVirtualDevice(std::unique_ptr<SndFileIODevice> device);
       ~SndFileVirtualDevice();
        unsigned GetSampleRate() const
        { return mSampleRate; }
        unsigned GetNumChannels() const
        { return mChannels; }
        unsigned GetNumFrames() const
        { return mFrames; }

        size_t ReadFrames(float* ptr, size_t frames);
        size_t ReadFrames(double* ptr, size_t frames);
        size_t ReadFrames(short* ptr, size_t frames);
        size_t ReadFrames(int* ptr, size_t frames);
    private:
        std::unique_ptr<SndFileIODevice> mDevice;
        SNDFILE* mFile = nullptr;
        unsigned mSampleRate = 0;
        unsigned mFrames     = 0;
        unsigned mChannels   = 0;
    };

    class SndFileInputStream : public SndFileIODevice
    {
    public:
         SndFileInputStream(const std::string& filename);
         SndFileInputStream() = default;
        ~SndFileInputStream();

        bool OpenFile(const std::string& filename);

        virtual std::int64_t GetLength() const override;
        virtual std::int64_t Seek(std::int64_t offset, int whence) override;
        virtual std::int64_t Read(void* ptr, std::int64_t count) override;
        virtual std::int64_t Tell() const override;
    private:
        mutable std::ifstream mStream;
        std::string mFilename;
        std::int64_t mStreamSize = 0;
    };

    class SndFileBuffer : public SndFileIODevice
    {
    public:
        SndFileBuffer(const std::vector<std::uint8_t>& buffer)
            : mBuffer(buffer)
        {}
        SndFileBuffer(std::vector<std::uint8_t>&& buffer)
            : mBuffer(std::move(buffer))
        {}
        virtual std::int64_t GetLength() const override;
        virtual std::int64_t Seek(std::int64_t offset, int whence) override;
        virtual std::int64_t Read(void* ptr, std::int64_t count) override;
        virtual std::int64_t Tell() const override;
    private:
        std::vector<std::uint8_t> mBuffer;
        std::int64_t mOffset = 0;
    };

} // namespace
