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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <type_traits>

#include "base/assert.h"
#include "data/fwd.h"
#include "graphics/color4f.h"
#include "device/vertex.h"
#include "device/enum.h"

namespace gfx
{
    // 16bit vertex index for indexed drawing.
    using Index16 = std::uint16_t;
    // 32bit vertex index for indexed drawing.
    using Index32 = std::uint32_t;

    using DrawType = dev::DrawType;
    using IndexType = dev::IndexType;

#pragma pack(push, 1)
    struct Vec1 {
        float x = 0.0f;
    };

    // 2 float vector data object. Use glm::vec2 for math.
    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
    };
    // 3 float vector data object. Use glm::vec3 for math.
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };
    // 4 float vector data object. Use glm::vec4 for math.
    struct Vec4 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };
#pragma pack(pop)

    inline Vec2 ToVec(const glm::vec2& vector) noexcept
    { return { vector.x, vector.y }; }
    inline Vec3 ToVec(const glm::vec3& vector) noexcept
    { return { vector.x, vector.y, vector.z }; }
    inline Vec4 ToVec(const glm::vec4& vector) noexcept
    { return { vector.x, vector.y, vector.z, vector.w }; }
    inline glm::vec2 ToVec(const Vec2& vec) noexcept
    { return { vec.x, vec.y }; }
    inline glm::vec3 ToVec(const Vec3& vec) noexcept
    { return { vec.x, vec.y, vec.z }; }
    inline glm::vec4 ToVec(const Vec4& vec) noexcept
    { return { vec.x, vec.y, vec.z, vec.w }; }

    inline Vec4 ToVec(const Color4f& color) noexcept
    {
        const auto linear = gfx::sRGB_Decode(color);
        return {linear.Red(), linear.Green(), linear.Blue(), linear.Alpha() };
    }

    // About texture coordinates.
    // In OpenGL the Y axis for texture coordinates goes so that
    // 0.0 is the first scan-row and 1.0 is the last scan-row of
    // the image. In other words st=0.0,0.0 is the first pixel
    // element of the texture and st=1.0,1.0 is the last pixel.
    // This is also reflected in the scan row memory order in
    // the call glTexImage2D which assumes that the first element
    // (pixel in the memory buffer) is the lower left corner.
    // This however is of course not the same order than what most
    // image loaders produce, they produce data chunks where the
    // first element is the first pixel of the first scan row which
    // is the "top" of the image. So this means that y=1.0f is then
    // the bottom of the image and y=0.0f is the top of the image.
    //
    // Currently all the 2D geometry shapes have "inversed" their
    // texture coordinates so that they use y=0.0f for the top
    // of the shape and y=1.0 for the bottom of the shape which then
    // produces the expected rendering.
    //
    // Complete solution requires making sure that both parts of the
    // system, i.e. the geometry part (drawables) and the material
    // part (which produces the texturing) understand and agree on
    // this.

#pragma pack(push, 1)
    // Vertex for 2D drawing on the XY plane.
    struct Vertex2D {
        // Coordinate / position of the vertex in the model space.
        Vec2 aPosition;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
    };

    // Vertex type for 2D sharded mesh effects.
    struct ShardVertex2D {
        // Coordinate / position of the vertex in the model space
        Vec2 aPosition;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
        // Index into shard data for this vertex.
        uint32_t aShardIndex = 0;
    };

    // Vertex for rendering 2D shapes, such as quads, where the content
    // of the 2D shape rendered (basically the texture) is partially
    // mapped into a 3D world.
    // The intended use case is "isometric tile rendering" where each tile
    // is rendered as a 2D billboard that is aligned to face the camera
    // but the contents of each tile ae perceptually 3D. In order to compute
    // effects such as lights better we cannot rely on the 2D objects geometry
    // but rather the lights must be computed in the "perceptual 3D space".
    struct Perceptual3DVertex {
        Vec2 aPosition;
        Vec2 aTexCoord;
        // coordinate in the "tile 3D space". i.e. relative to
        // the tile plane. We use this information to compute lights
        // in perceptual 3D space.
        Vec3 aLocalOffset;
        // Normal of the vertex in the tile 3D space.
        Vec3 aWorldNormal;
    };

    // Vertex for #D drawing in the XYZ space.
    struct Vertex3D {
        // Coordinate / position of the vertex in the model space.
        Vec3 aPosition;
        // Vertex normal
        Vec3 aNormal;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
        // Surface coordinate space right vector for normal mapping.
        Vec3 aTangent;
        // Surface coordinate space up vector for normal mapping.
        Vec3 aBitangent;
    };

    struct ModelVertex3D {
        Vec3 aPosition;
        Vec3 aNormal;
        Vec2 aTexCoord;
        Vec3 aTangent;
        Vec3 aBitangent;
    };

    struct InstanceAttribute {
        Vec4 iaModelVectorX;
        Vec4 iaModelVectorY;
        Vec4 iaModelVectorZ;
        Vec4 iaModelVectorW;
    };

#pragma pack(pop)

    // The offsetof macro is guaranteed to be usable only with types with standard layout.
    static_assert(std::is_standard_layout<Vertex3D>::value, "Vertex3D must meet standard layout.");
    static_assert(std::is_standard_layout<Vertex2D>::value, "Vertex2D must meet standard layout.");

    // memcpy and binary read/write require trivially_copyable.
    static_assert(std::is_trivially_copyable<Vertex3D>::value, "Vertex3D must be trivial to copy.");
    static_assert(std::is_trivially_copyable<Vertex2D>::value, "Vertex2D must be trivial to copy.");

    using VertexLayout = dev::VertexLayout;
    using dev::operator!=;
    using dev::operator==;

    template<typename Vertex>
    const VertexLayout& GetVertexLayout();

    template<> inline
    const VertexLayout& GetVertexLayout<ShardVertex2D>()
    {
        using DataType = VertexLayout::Attribute::DataType;
        static const VertexLayout layout(sizeof(ShardVertex2D), {
            {"aPosition",   0, 2, 0, offsetof(ShardVertex2D, aPosition),   DataType::Float},
            {"aTexCoord",   0, 2, 0, offsetof(ShardVertex2D, aTexCoord),   DataType::Float},
            {"aShardIndex", 0, 1, 0, offsetof(ShardVertex2D, aShardIndex), DataType::UnsignedInt}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<Perceptual3DVertex>()
    {
        using DataType = VertexLayout::Attribute::DataType;
        static const VertexLayout layout(sizeof(Perceptual3DVertex), {
            {"aPosition",    0, 2, 0, offsetof(Perceptual3DVertex, aPosition),    DataType::Float},
            {"aTexCoord",    0, 2, 0, offsetof(Perceptual3DVertex, aTexCoord),    DataType::Float},
            {"aLocalOffset", 0, 3, 0, offsetof(Perceptual3DVertex, aLocalOffset), DataType::Float},
            {"aWorldNormal", 0, 3, 0, offsetof(Perceptual3DVertex, aWorldNormal), DataType::Float}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex2D>()
    {
        // todo: if using GLSL layout bindings then need to
        // specify the vertex attribute indices properly.
        // todo: if using instanced rendering then need to specify
        // the divisors properly.
        using DataType = VertexLayout::Attribute::DataType;
        static const VertexLayout layout(sizeof(Vertex2D), {
            {"aPosition", 0, 2, 0, offsetof(Vertex2D, aPosition), DataType::Float},
            {"aTexCoord", 0, 2, 0, offsetof(Vertex2D, aTexCoord), DataType::Float}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex3D>()
    {
        using DataType = VertexLayout::Attribute::DataType;
        static const VertexLayout layout(sizeof(Vertex3D), {
            {"aPosition",   0, 3, 0, offsetof(Vertex3D, aPosition),  DataType::Float},
            {"aNormal",     0, 3, 0, offsetof(Vertex3D, aNormal),    DataType::Float},
            {"aTexCoord",   0, 2, 0, offsetof(Vertex3D, aTexCoord),  DataType::Float},
            {"aTangent",    0, 3, 0, offsetof(Vertex3D, aTangent),   DataType::Float},
            {"aBitangent",  0, 3, 0, offsetof(Vertex3D, aBitangent), DataType::Float}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<ModelVertex3D>()
    {
        using DataType = VertexLayout::Attribute::DataType;
        static const VertexLayout layout(sizeof(ModelVertex3D), {
            {"aPosition",  0, 3, 0, offsetof(ModelVertex3D, aPosition),  DataType::Float},
            {"aNormal",    0, 3, 0, offsetof(ModelVertex3D, aNormal),    DataType::Float},
            {"aTexCoord",  0, 2, 0, offsetof(ModelVertex3D, aTexCoord),  DataType::Float},
            {"aTangent",   0, 3, 0, offsetof(ModelVertex3D, aTangent),   DataType::Float},
            {"aBitangent", 0, 3, 0, offsetof(ModelVertex3D, aBitangent), DataType::Float},
        });
        return layout;
    }

    using InstanceDataLayout = VertexLayout;

    template<typename Attribute>
    const InstanceDataLayout& GetInstanceDataLayout();

    template<> inline
    const InstanceDataLayout& GetInstanceDataLayout<InstanceAttribute>()
    {
        using DataType = VertexLayout::Attribute::DataType;
        static const InstanceDataLayout layout(sizeof(InstanceAttribute), {
            {"iaModelVectorX", 0, 4, 1, offsetof(InstanceAttribute, iaModelVectorX), DataType::Float},
            {"iaModelVectorY", 0, 4, 1, offsetof(InstanceAttribute, iaModelVectorY), DataType::Float},
            {"iaModelVectorZ", 0, 4, 1, offsetof(InstanceAttribute, iaModelVectorZ), DataType::Float},
            {"iaModelVectorW", 0, 4, 1, offsetof(InstanceAttribute, iaModelVectorW), DataType::Float}
        });
        return layout;
    }

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
