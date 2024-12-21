// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>

#include "base/assert.h"
#include "graphics/types.h"

namespace gfx
{
    // helper class to accumulate uniform block data in a type unsafe data
    // buffer such as a vector.
    template<typename T>
    class UniformBlockData
    {
    public:
        static_assert(std::is_trivially_copyable<T>::value,
                "Uniform block type must be trivially copyable.");
        static_assert(std::is_standard_layout<T>::value,
                "Uniform block type must have standard layout.");

        explicit UniformBlockData(std::vector<uint8_t>* buffer) noexcept
          : mBuffer(buffer)
        {}
        UniformBlockData() noexcept
          : mBuffer(&mStorage)
        {}
        void PushBack(const T& block_data)
        {
            const auto buff_offset_bytes = mBuffer->size();
            const auto item_size_bytes = sizeof(T);
            mBuffer->resize(buff_offset_bytes + item_size_bytes);
            std::memcpy(&(*mBuffer)[buff_offset_bytes], &block_data, item_size_bytes);
        }
        size_t GetSize() const noexcept
        {
            ASSERT((mBuffer->size() % sizeof(T))  == 0);
            const auto item_size_bytes = sizeof(T);
            const auto buff_size_bytes = mBuffer->size();
            return buff_size_bytes / item_size_bytes;
        }
        inline void Resize(size_t size)
        {
            const auto byte_size = sizeof(T) * size;
            mBuffer->resize(byte_size);
        }
        const T& operator[](size_t index) const noexcept
        {
            ASSERT(index < GetSize());
            const auto* base = mBuffer->data() + index * sizeof(T);
            return *static_cast<const T*>((const void*)base);
        }
        T& operator[](size_t index) noexcept
        {
            ASSERT(index < GetSize());
            auto* base = mBuffer->data() + index * sizeof(T);
            return *static_cast<T*>((void*)base);
        }

        inline auto TransferBuffer() noexcept
        {
            return std::move(*mBuffer);
        }

        inline const auto& GetBuffer() const noexcept
        {
            return *mBuffer;
        }
        inline auto& GetBuffer() noexcept
        {
            return *mBuffer;
        }

    private:
        std::vector<uint8_t>* mBuffer;
        std::vector<uint8_t> mStorage;
    };


    class UniformBlock
    {
    public:
        UniformBlock() = default;

        explicit UniformBlock(std::string name)
          : mBlockName(name)
        {}

        UniformBlock(std::string name, std::vector<uint8_t>&& data) noexcept
          : mBlockName(std::move(name))
          , mData(std::move(data))
        {}
        UniformBlock(std::string name, const std::vector<uint8_t>& data)
          : mBlockName(std::move(name))
          , mData(data)
        {}
        template<typename T>
        explicit UniformBlock(std::string name, UniformBlockData<T>&& data) noexcept
          : mBlockName(std::move(name))
          , mData(std::move(data.TransferBuffer()))
        {}

        template<typename T>
        explicit UniformBlock(std::string name, const UniformBlockData<T>& data)
          : mBlockName(std::move(name))
          , mData(data.GetBuffer())
        {}

        template<typename T>
        UniformBlock(std::string name, const T& value)
          : mBlockName(std::move(name))
        {
            UniformBlockData<T> data(&mData);
            data.Resize(1);
            data[0] = value;
        }
        template<typename T>
        UniformBlock(std::string name, T&& value)
          : mBlockName(std::move(name))
        {
            UniformBlockData<T> data(&mData);
            data.Resize(1);
            data[0] = std::move(value);
        }

        template<typename T> inline
        UniformBlockData<T> GetData() noexcept
        { return UniformBlockData<T> { &mData }; }

        template<typename T> inline
        void SetData(UniformBlockData<T>&& data) noexcept
        { mData = std::move(data.TransferBuffer()); }

        template<typename T>
        void SetData(const UniformBlockData<T>& data)
        { mData = data.GetBuffer(); }

        void SetData(std::vector<uint8_t>&& data) noexcept
        { mData = std::move(data); }
        void SetData(const std::vector<uint8_t>& data)
        { mData = data; }

        inline size_t GetByteSize() const noexcept
        { return mData.size(); }

        inline auto TransferBuffer() noexcept
        { return std::move(mData); }

        inline const auto& GetBuffer() const noexcept
        { return mData; }

        inline void SetName(std::string name) noexcept
        { mBlockName = std::move(name); }

        inline std::string GetName() const
        { return mBlockName; }

        template<typename T>
        inline T& GetAs() noexcept
        {
            UniformBlockData<T> data(&mData);
            return data[0];
        }
        template<typename T>
        inline const T& GetAs() const noexcept
        {
            const UniformBlockData<T> data(&mData);
            return data[0];
        }

    private:
        std::string mBlockName;
        std::vector<uint8_t> mData;
    };

} // namespace