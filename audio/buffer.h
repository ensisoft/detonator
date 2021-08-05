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
#include <vector>
#include <cstring>

#include "base/assert.h"
#include "base/utility.h"
#include "audio/format.h"

namespace audio
{
    class Buffer
    {
    public:
        using Format = audio::Format;
        struct InfoTag {
            struct Element {
                std::string name;
                std::string id;
                bool source = false;
                bool source_done = false;
            } element;
        };
        virtual ~Buffer() = default;
        // Get the PCM audio format of the buffer.
        // The format is only valid when the buffer contains PCM data.
        // Other times the format will be NotSet.
        virtual Format GetFormat() const = 0;
        // Get the read pointer for the contents of the buffer.
        virtual const void* GetPtr() const = 0;
        // Get the write pointer for the buffer contents. It's possible
        // that this would be a nullptr when the buffer doesn't support
        // writing into.
        virtual void* GetPtr() = 0;
        // Get the size of the buffer in bytes for both reading and writing.
        virtual size_t GetByteSize() const = 0;
        virtual size_t GetNumInfoTags() const = 0;
        virtual void AddInfoTag(const InfoTag& tag) = 0;
        virtual const InfoTag& GetInfoTag(size_t index) const = 0;
    private:

    };

    // backing data storage.
    class VectorBuffer : public Buffer
    {
    public:
       ~VectorBuffer()
        {
            ASSERT(mBuffer.size() >= sizeof(canary));
            ASSERT(!std::memcmp(&mBuffer[GetByteSize()], &canary, sizeof(canary)) &&
                   "Audio buffer out of bounds write detected.");
        }

        void SetFormat(const Format& format)
        { mFormat = format; }
        void AllocateBytes(size_t bytes)
        { Resize(bytes); }
        void ResizeBytes(size_t bytes)
        { Resize(bytes); }

        virtual Format GetFormat() const override
        { return mFormat; }
        virtual const void* GetPtr() const override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        virtual void* GetPtr() override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        size_t GetByteSize() const override
        { return mBuffer.size() - sizeof(canary); }
        virtual size_t GetNumInfoTags() const override
        { return mInfos.size(); }
        virtual void AddInfoTag(const InfoTag& tag) override
        { mInfos.push_back(tag); }
        virtual const InfoTag& GetInfoTag(size_t index) const
        { return base::SafeIndex(mInfos, index); }
    private:
        void Resize(size_t bytes)
        {
            const auto actual = bytes + sizeof(canary);
            mBuffer.resize(actual);
            std::memcpy(&mBuffer[bytes], &canary, sizeof(canary));
        }
    private:
        static constexpr auto canary = 0xF4F5ABCD;
    private:
        Format mFormat;
        std::vector<std::uint8_t> mBuffer;
        std::vector<InfoTag> mInfos;
    };

} // namespace
