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

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring> // for memcpy

#include "base/assert.h"
#include "graphics/vertex.h"

namespace gfx
{
    // collection of utility classes for working with opaque (and typed)
    // vertex and index data.

    class VertexStream
    {
    public:
        VertexStream(VertexLayout layout, const void* buffer, size_t bytes) noexcept
          : mLayout(std::move(layout))
          , mBuffer((const uint8_t*)buffer)
          , mCount(GetCount(layout.vertex_struct_size, bytes))
        {}
        VertexStream(VertexLayout layout, const std::vector<uint8_t>& buffer) noexcept
          : mLayout(std::move(layout))
          , mBuffer(buffer.data())
          , mCount(GetCount(layout.vertex_struct_size, buffer.size()))
        {}

        template<typename Vertex>
        VertexStream(VertexLayout layout, const Vertex* buffer, size_t count) noexcept
          : mLayout(std::move(layout))
          , mBuffer((const uint8_t*)buffer)
          , mCount(count)
        {}
        template<typename Vertex>
        VertexStream(VertexLayout layout, const std::vector<Vertex>& buffer) noexcept
          : mLayout(std::move(layout))
          , mBuffer((const uint8_t*)buffer.data())
          , mCount(buffer.size())
        {}

        template<typename A>
        const A* GetAttribute(const char* name, size_t index) const noexcept
        {
            ASSERT(index < mCount);
            const auto& attribute = GetAttribute(name);
            const auto offset = attribute.offset + index * mLayout.vertex_struct_size;
            const auto size   = attribute.num_vector_components * sizeof(float);
            ASSERT(sizeof(A) == size);
            return reinterpret_cast<const A*>(mBuffer + offset);
        }
        template<typename T>
        const T* GetVertex(size_t index) const noexcept
        {
            ASSERT(index < mCount);
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            const auto offset = index * mLayout.vertex_struct_size;
            return reinterpret_cast<const T*>(mBuffer + offset);
        }
        const void* GetVertexPtr(size_t index) const noexcept
        {
            ASSERT(index < mCount);
            const auto offset = index * mLayout.vertex_struct_size;
            return static_cast<const void*>(mBuffer + offset);
        }
        bool HasAttribute(const char* name) const noexcept
        {  return FindAttribute(name) != nullptr; }
        size_t GetCount() const noexcept
        { return mCount; }
        bool IsValid() const noexcept
        { return mBuffer != nullptr; }

        void IntoJson(data::Writer& writer) const;

        const VertexLayout::Attribute* FindAttribute(const char* name) const noexcept
        {
            for (const auto& attr : mLayout.attributes)
            {
                if (attr.name == name)
                    return &attr;
            }
            return nullptr;
        }

    private:
        static size_t GetCount(size_t vertex_size_bytes, size_t buffer_size_bytes)
        {
            ASSERT((buffer_size_bytes % vertex_size_bytes) == 0);
            return buffer_size_bytes / vertex_size_bytes;
        }
        const VertexLayout::Attribute& GetAttribute(const char* name) const noexcept
        {
            for (const auto& attr : mLayout.attributes)
            {
                if (attr.name == name)
                    return attr;
            }
            BUG("No such vertex attribute was found.");
        }
    private:
        const VertexLayout mLayout;
        const uint8_t* mBuffer = nullptr;
        const size_t mCount = 0;
    };

    class VertexBuffer
    {
    public:
        VertexBuffer(VertexLayout layout, std::vector<uint8_t>* buffer) noexcept
          : mLayout(std::move(layout))
          , mBuffer(buffer)
        {}
        explicit VertexBuffer(VertexLayout layout) noexcept
          : mLayout(std::move(layout))
          , mBuffer(&mStorage)
        {}
        explicit VertexBuffer(std::vector<uint8_t>* buffer) noexcept
          : mBuffer(buffer)
        {}
        VertexBuffer()
          : mBuffer(&mStorage)
        {}

        void PushBack(const void* ptr)
        {
            ASSERT(mLayout.vertex_struct_size);
            const auto byte_offset = mBuffer->size();
            const auto byte_size = mLayout.vertex_struct_size;
            mBuffer->resize(byte_offset + byte_size);
            std::memcpy(&(*mBuffer)[byte_offset], ptr, byte_size);
        }

        const auto& GetLayout() const &  noexcept
        { return mLayout; }
        auto&& GetLayout() && noexcept
        { return std::move(mLayout); }

        const void* GetBufferPtr() const noexcept
        { return mBuffer->empty() ? nullptr : mBuffer->data(); }
        std::size_t GetBufferSize() const noexcept
        { return mBuffer->size(); }
        std::size_t GetCount() const noexcept
        { return mBuffer->size() / mLayout.vertex_struct_size; };
        std::size_t GetCapacity() const noexcept
        { return mBuffer->capacity() / mLayout.vertex_struct_size; }
        void SetVertexLayout(VertexLayout layout) noexcept
        { mLayout = std::move(layout); }

        const auto& GetVertexBuffer() const noexcept
        { return *mBuffer; }

        auto& GetVertexBuffer() noexcept
        { return *mBuffer; }

        auto&& TransferVertexBuffer() noexcept
        { return std::move(*mBuffer); }

        void* GetVertexPtr(size_t index) noexcept
        {
            ASSERT(index < GetCount());
            const auto offset = index * mLayout.vertex_struct_size;
            return &(*mBuffer)[offset];
        }
        const void* GetVertexPtr(size_t index) const noexcept
        {
            ASSERT(index < GetCount());
            const auto offset = index * mLayout.vertex_struct_size;
            return &(*mBuffer)[offset];
        }

        template<typename T>
        const T* GetVertex(size_t index) const noexcept
        {
            ASSERT(index < GetCount());
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            const auto offset = index * mLayout.vertex_struct_size;
            return reinterpret_cast<const T*>(mBuffer->data() + offset);
        }

        template<typename T>
        T* GetVertex(size_t index) noexcept
        {
            ASSERT(index < GetCount());
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            const auto offset = index * mLayout.vertex_struct_size;
            return reinterpret_cast<T*>(mBuffer->data() + offset);
        }

        template<typename T>
        void SetVertex(const T& value, size_t index)
        {
            ASSERT(index < GetCount());
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            const auto offset = index * mLayout.vertex_struct_size;
            std::memcpy(mBuffer->data() + offset, (const void*)&value, sizeof(T));
        }

        template<typename A>
        A* GetAttribute(const char* name, size_t index) noexcept
        {
            ASSERT(index < GetCount());
            const auto& attribute = GetAttribute(name);
            const auto offset = attribute.offset + index * mLayout.vertex_struct_size;
            const auto size   = attribute.num_vector_components * sizeof(float);
            ASSERT(sizeof(A) == size);
            return reinterpret_cast<A*>(mBuffer->data() + offset);
        }

        // Copy over vertex data. Note that the incoming data *may* be less
        // than what is the current vertex size per current vertex layout.
        // This lets us copy over vertex data that is  in a compatible format
        // for the byte_size bytes of the current format. Useful when the
        // vertex layout has had new attributes appended to it.
        void CopyVertex(const void* vertex, size_t byte_size, size_t index)
        {
            ASSERT(index < GetCount());
            ASSERT(byte_size <= mLayout.vertex_struct_size);
            void* ptr = GetVertexPtr(index);
            std::memcpy(ptr, vertex, byte_size);
        }

        const VertexLayout::Attribute* FindAttribute(const char* name) const noexcept
        {
            for (const auto& attr : mLayout.attributes)
            {
                if (attr.name == name)
                    return &attr;
            }
            return nullptr;
        }

        void Resize(size_t count)
        {
            ASSERT(mLayout.vertex_struct_size);
            const auto bytes = count * mLayout.vertex_struct_size;
            mBuffer->resize(bytes);
        }

        void Reserve(size_t count)
        {
            ASSERT(mLayout.vertex_struct_size);
            const auto bytes = count * mLayout.vertex_struct_size;
            mBuffer->reserve(bytes);
        }

        // Pushback one uninitialized vertex and return pointer to it.
        void* PushBack()
        {
            ASSERT(mLayout.vertex_struct_size);
            const auto size = mBuffer->size();
            mBuffer->resize(mBuffer->size() + mLayout.vertex_struct_size);
            return &(*mBuffer)[size];
        }

        template<typename T>
        std::vector<T> CopyBuffer() const
        {
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            std::vector<T> ret;
            ret.resize(GetCount());
            if (!ret.empty())
                std::memcpy(ret.data(), mBuffer->data(), mBuffer->size());
            return ret;
        }
        auto CopyBuffer() const
        {
            return std::vector<uint8_t>{*mBuffer};
        }

        auto TransferBuffer()
        {
            return std::move(*mBuffer);
        }

        bool Validate() const noexcept;

        bool FromJson(const data::Reader& reader);
    private:
        const VertexLayout::Attribute& GetAttribute(const char* name) const noexcept
        {
            for (const auto& attr : mLayout.attributes)
            {
                if (attr.name == name)
                    return attr;
            }
            BUG("No such vertex attribute was found.");
        }
    private:
        VertexLayout mLayout;
        std::vector<uint8_t> mStorage;
        std::vector<uint8_t>* mBuffer = nullptr;
    };

    template<typename T>
    class TypedVertexBuffer
    {
    public:
        TypedVertexBuffer(VertexLayout layout, std::vector<uint8_t>* buffer) noexcept
          : mBuffer(std::move(layout), buffer)
        {}
        TypedVertexBuffer(VertexLayout layout, std::vector<uint8_t>& buffer) noexcept
           : mBuffer(std::move(layout), &buffer)
        {}
        explicit TypedVertexBuffer(VertexLayout layout) noexcept
          : mBuffer(std::move(layout))
        {}
        explicit TypedVertexBuffer(std::vector<uint8_t>* buffer) noexcept
          : mBuffer(buffer)
        {}

        TypedVertexBuffer() = default;

        T* GetVertex(size_t index) noexcept
        { return mBuffer.GetVertex<T>(index); }

        const T* GetVertex(size_t index) const noexcept
        { return mBuffer.GetVertex<T>(index); }

        void Append(const T& value)
        { mBuffer.PushBack(&value); }

        void SetVertexLayout(VertexLayout layout) noexcept
        { mBuffer.SetVertexLayout(std::move(layout)); }

        void Resize(size_t count)
        { mBuffer.Resize(count); }

        std::vector<T> CopyBuffer() const
        {
            ASSERT(sizeof(T) == mBuffer.GetLayout().vertex_struct_size);

            std::vector<T> ret;
            ret.resize(mBuffer.GetCount());
            if (ret.empty())
                return ret;

            std::memcpy(ret.data(), mBuffer.GetBufferPtr(), mBuffer.GetBufferSize());
            return ret;
        }

        T& operator[](size_t index) noexcept
        { return *GetVertex(index); }

        const T& operator[](size_t index) const noexcept
        { return *GetVertex(index); }

        auto CopyRawBuffer()
        { return mBuffer.CopyBuffer(); }

        auto TransferRawBuffer() noexcept
        { return std::move(mBuffer.TransferBuffer()); }

    private:
        VertexBuffer mBuffer;
    };

    class IndexStream
    {
    public:
        using Type = IndexType;

        IndexStream(const void* buffer, size_t bytes, Type type)
          : mBuffer((const uint8_t*)buffer)
          , mCount(GetCount(bytes, type))
          , mType(type)
        {}
        IndexStream(const std::vector<uint8_t>& buffer, Type type)
          : mBuffer(buffer.data())
          , mCount(GetCount(buffer.size(), type))
          , mType(type)
        {}

        inline uint32_t GetIndex(size_t index) const noexcept
        {
            ASSERT(index < mCount);

            if (mType == Type::Index16)
            {
                uint16_t ret =0;
                memcpy(&ret, mBuffer + index * 2, 2);
                return ret;
            }
            else if (mType == Type::Index32)
            {
                uint32_t ret = 0;
                memcpy(&ret, mBuffer + index * 4, 4);
                return ret;
            } else BUG("Missing index type.");
        }
        inline size_t GetCount() const noexcept
        { return mCount; }
        inline bool IsValid() const noexcept
        { return mBuffer != nullptr; }

        void IntoJson(data::Writer& writer) const;

    private:
        static size_t GetCount(size_t bytes, Type type)
        {
            const auto size = GetIndexByteSize(type);
            ASSERT((bytes % size) == 0);
            return bytes / size;
        }
    private:
        const uint8_t* mBuffer = nullptr;
        const size_t mCount = 0;
        const Type mType;
    };

    class IndexBuffer
    {
    public:
        using Type = IndexType;

        explicit IndexBuffer(Type type, std::vector<uint8_t>* buffer) noexcept
          : mType(type)
          , mBuffer(buffer)
        {}
        explicit IndexBuffer(Type type) noexcept
          : mType(type)
          , mBuffer(&mStorage)
        {}
        explicit IndexBuffer(std::vector<uint8_t>* buffer) noexcept
          : mBuffer(buffer)
        {}
        IndexBuffer() noexcept
          : mBuffer(&mStorage)
        {}

        void SetType(Type type) noexcept
        { mType = type;}

        size_t GetCount() const noexcept
        { return mBuffer->size() / GetIndexByteSize(mType); }

        const void* GetBufferPtr() const noexcept
        { return mBuffer->data(); }

        size_t GetBufferSize() const noexcept
        { return mBuffer->size(); }

        Type GetType()  const noexcept
        { return mType; }

        const auto& GetIndexBuffer() const & noexcept
        { return *mBuffer; }

        auto&& GetIndexBuffer() && noexcept
        { return std::move(*mBuffer); }

        void PushBack(Index16 value) noexcept
        {
            ASSERT(mType == Type::Index16);
            const auto offset = mBuffer->size();
            mBuffer->resize(offset + sizeof(value));
            std::memcpy(&(*mBuffer)[offset], &value, sizeof(value));
        }

        void PushBack(Index32 value) noexcept
        {
            ASSERT(mType == Type::Index32);
            const auto offset = mBuffer->size();
            mBuffer->resize(offset + sizeof(value));
            std::memcpy(&(*mBuffer)[offset], &value, sizeof(value));
        }

        bool FromJson(const data::Reader& reader);

    private:
        Type mType = Type::Index16;
        std::vector<uint8_t> mStorage;
        std::vector<uint8_t>* mBuffer = nullptr;
    };


} // namespace