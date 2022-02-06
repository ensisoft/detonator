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
#include <cstddef>
#include <fstream>

#include "audio/decoder.h"
#include "audio/format.h"

typedef struct mpg123_handle_struct mpg123_handle;

namespace audio
{
    class SourceBuffer;

    class Mpg123IODevice
    {
    public:
        virtual ~Mpg123IODevice() = default;
        // The mpg123 IO abstraction functions are modeled after the
        // Posix read and lseek.
        virtual long Read(void* buffer,  size_t bytes) = 0;
        virtual off_t Seek(off_t offset, int whence) = 0;
        virtual std::string GetName() const = 0;
    private:
    };

    class Mpg123FileInputStream : public Mpg123IODevice
    {
    public:
        Mpg123FileInputStream(const std::string& filename);
        Mpg123FileInputStream() = default;

        bool OpenFile(const std::string& filename);

        void UseStream(const std::string& name, std::ifstream&& stream);

        virtual long Read(void* buffer,  size_t bytes) override;
        virtual off_t Seek(off_t where, int whence) override;
        virtual std::string GetName() const override
        { return mFilename; }
    private:
        mutable std::ifstream mStream;
        std::string mFilename;
        std::int64_t mStreamSize;
    };

    class Mpg123Buffer : public Mpg123IODevice
    {
    public:
        Mpg123Buffer(const std::string& name, std::unique_ptr<SourceBuffer> buffer)
          : mName(name)
          , mBuffer(std::move(buffer))
        {}
        Mpg123Buffer(const std::string& name, std::shared_ptr<const SourceBuffer> buffer)
          : mName(name)
          , mBuffer(buffer)
        {}
        virtual long Read(void* buffer,  size_t bytes) override;
        virtual off_t Seek(off_t offset, int whence) override;
        virtual std::string GetName() const override
        { return mName; }
    private:
        const std::string mName;
        std::shared_ptr<const SourceBuffer> mBuffer;
        off_t mOffset = 0;
    };

    class Mpg123Decoder : public Decoder
    {
    public:
        Mpg123Decoder();
        Mpg123Decoder(std::unique_ptr<Mpg123IODevice> io, SampleType format = SampleType::Int16);
       ~Mpg123Decoder();
        virtual unsigned GetSampleRate() const override;
        virtual unsigned GetNumChannels() const override;
        virtual unsigned GetNumFrames() const override;
        virtual size_t ReadFrames(float* ptr, size_t frames) override;
        virtual size_t ReadFrames(short* ptr, size_t frames) override;
        virtual size_t ReadFrames(int* ptr, size_t frames) override;
        virtual void Reset() override;

        bool Open(std::unique_ptr<Mpg123IODevice> io, SampleType format = SampleType::Int16);
    private:
        template<typename T>
        size_t ReadFrames(T* ptr, size_t frames);
    private:
        struct Library;
        std::shared_ptr<Library> mLibrary;
        std::unique_ptr<Mpg123IODevice> mDevice;
        mpg123_handle* mHandle = nullptr;
        unsigned mSampleRate = 0;
        unsigned mFrameCount = 0;
        unsigned mOutFormat  = 0;
        bool mIsOpen = false;
    };
} // namespace
