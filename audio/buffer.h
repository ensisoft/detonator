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
    // Interface for accessing and dealing with buffers of PCM audio data.
    // Each buffer contains the actual PCM data and some meta information
    // related to the contents of the buffer, such as the PCM format
    // and also information about the audio elements that have produced
    // or processed the buffer data.
    class Buffer
    {
    public:
        using Format = audio::Format;
        // Collection of information regarding the element
        // that has touched/produced the buffer and its contents.
        struct InfoTag {
            // Details of the element.
            struct Element {
                std::string name;
                std::string id;
                bool source = false;
                bool source_done = false;
            } element;
        };
        virtual ~Buffer() = default;
        // Set the current format for the contents of the buffer.
        virtual void SetFormat(const Format& format) = 0;
        // Get the PCM audio format of the buffer.
        // The format is only valid when the buffer contains PCM data.
        // Other times the format will be NotSet.
        virtual Format GetFormat() const = 0;
        // Get the read pointer for the contents of the buffer.
        virtual const void* GetPtr() const = 0;
        // Get the pointer for writing the buffer contents. It's possible
        // that this would be a nullptr when the buffer doesn't support
        // writing into.
        virtual void* GetPtr() = 0;
        // Set the size of the buffer's content in bytes.
        virtual void SetByteSize(size_t bytes) = 0;
        // Get the size of the buffer's contents in bytes.
        virtual size_t GetByteSize() const = 0;
        // Get the capacity of the buffer in bytes.
        virtual size_t GetCapacity() const = 0;
        // Get the number of info tags associated with this buffer.
        // the info tags are accumulated in the buffer as it passes
        // from one element to another for processing.
        virtual size_t GetNumInfoTags() const = 0;
        // Add (append) a new info tag to this buffer.
        virtual void AddInfoTag(const InfoTag& tag) = 0;
        // Get an info tag at the specified index.
        virtual const InfoTag& GetInfoTag(size_t index) const = 0;

        // copy the buffer info tags from source buffer into the
        // destination buffer.
        static void CopyInfoTags(const Buffer& src, Buffer& dst)
        {
            for (size_t i=0; i<src.GetNumInfoTags(); ++i)
                dst.AddInfoTag(src.GetInfoTag(i));
        }
    private:
    };
    using BufferHandle = std::shared_ptr<Buffer>;

    class BufferView : public Buffer
    {
    public:
        BufferView(void* data, size_t capacity)
          : mCapacity(capacity)
          , mData(data)
        {}
        virtual void SetFormat(const Format& format) override
        { mFormat = format; }
        virtual Format GetFormat() const override
        { return mFormat; }
        virtual const void* GetPtr() const override
        { return mData; }
        virtual void* GetPtr() override
        { return mData; }
        virtual size_t GetByteSize() const override
        { return mSize; }
        virtual size_t GetCapacity() const override
        { return mCapacity; }
        virtual void SetByteSize(size_t bytes) override
        {
            ASSERT(bytes <= mCapacity);
            mSize = bytes;
        }
        virtual size_t GetNumInfoTags() const override
        { return mInfos.size(); }
        virtual void AddInfoTag(const InfoTag& tag) override
        { mInfos.push_back(tag); }
        virtual const InfoTag& GetInfoTag(size_t index) const override
        { return base::SafeIndex(mInfos, index); }
    private:
        const std::size_t mCapacity = 0;
        void* mData = nullptr;
        std::size_t mSize = 0;
        std::vector<InfoTag> mInfos;
        Format mFormat;
    };

    // backing data storage.
    class VectorBuffer : public Buffer
    {
    public:
        VectorBuffer(size_t capacity)
        { Resize(capacity); }
       ~VectorBuffer()
        {
            ASSERT(mBuffer.size() >= sizeof(canary));
            const auto canary_offset = mBuffer.size() - sizeof(canary);
            ASSERT(!std::memcmp(&mBuffer[canary_offset], &canary, sizeof(canary)) &&
                   "Audio buffer out of bounds write detected.");
        }
        virtual void SetFormat(const Format& format) override
        { mFormat = format; }
        virtual Format GetFormat() const override
        { return mFormat; }
        virtual const void* GetPtr() const override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        virtual void* GetPtr() override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        virtual size_t GetByteSize() const override
        { return mSize; }
        virtual size_t GetCapacity() const override
        { return mSize - sizeof(canary); }
        virtual void SetByteSize(size_t bytes) override
        {
            const auto limit = mBuffer.size() - sizeof(canary);
            ASSERT(bytes <= limit);
            mSize = bytes;
        }
        virtual size_t GetNumInfoTags() const override
        { return mInfos.size(); }
        virtual void AddInfoTag(const InfoTag& tag) override
        { mInfos.push_back(tag); }
        virtual const InfoTag& GetInfoTag(size_t index) const override
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
        // Buffer content format for the PCM data.
        Format mFormat;
        // the backing store for the buffer's PMC data.
        std::vector<std::uint8_t> mBuffer;
        // The collection of info tags accumulated as the buffer
        // has passed over a number of audio elements.
        std::vector<InfoTag> mInfos;
        // Size of PCM data content in bytes. can be less than
        // the buffer's actual capacity.
        std::size_t mSize = 0;
    };

    class BufferAllocator
    {
    public:
        virtual BufferHandle Allocate(size_t bytes)
        { return std::make_shared<VectorBuffer>(bytes); }
    private:
    };

} // namespace
