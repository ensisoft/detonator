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

#include "audio/format.h"

namespace audio
{
    class Buffer
    {
    public:
        using Format = audio::Format;
        virtual ~Buffer() = default;
        virtual Format GetFormat() const = 0;
        virtual const void* GetPtr() const = 0;
        virtual void* GetPtr() = 0;
        virtual size_t GetByteSize() const = 0;
    private:

    };

    // backing data storage. todo: add canary.
    template<typename T = std::uint8_t>
    class VectorBuffer : public Buffer
    {
    public:
        void SetFormat(const Format& format)
        { mFormat = format; }
        void AllocateBytes(size_t bytes)
        { mBuffer.resize(bytes / sizeof(T)); }
        void AllocateItems(size_t count)
        { mBuffer.resize(count); }
        void SetFromVector(std::vector<T>&& buffer)
        { mBuffer = std::move(buffer); }
        void ResizeBytes(size_t bytes)
        { mBuffer.resize(bytes / sizeof(T)); }
        void ResizeItems(size_t count)
        { mBuffer.resize(count); }

        virtual Format GetFormat() const override
        { return mFormat; }
        virtual const void* GetPtr() const override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        virtual void* GetPtr() override
        { return mBuffer.empty() ? nullptr : &mBuffer[0]; }
        size_t GetByteSize() const override
        { return mBuffer.size() * sizeof(T); }
    private:
        Format mFormat;
        std::vector<T> mBuffer;
    };

} // namespace
