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

namespace gfx
{
    // 16bit vertex index for indexed drawing.
    using Index16 = std::uint16_t;
    // 32bit vertex index for indexed drawing.
    using Index32 = std::uint32_t;

    enum class IndexType {
        Index16, Index32
    };

    // Map the type of the index to index size in bytes.
    inline size_t GetIndexByteSize(IndexType type)
    {
        if (type == IndexType::Index16)
            return 2;
        else if (type == IndexType::Index32)
            return 4;
        else BUG("Missing index type.");
        return 0;
    }

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

    // Vertex for #D drawing in the XYZ space.
    struct Vertex3D {
        // Coordinate / position of the vertex in the model space.
        Vec3 aPosition;
        // Vertex normal
        Vec3 aNormal;
        // Texture coordinate for the vertex.
        Vec2 aTexCoord;
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

    struct VertexLayout {
        struct Attribute {
            // name of the attribute in the shader code.
            std::string name;
            // the index of the attribute.
            // use glsl syntax
            // layout (binding=x) in vec3 myAttrib;
            unsigned index = 0;
            // number of vector components
            // must be one of [1, 2, 3, 4]
            unsigned num_vector_components = 0;
            // the attribute divisor. if this is 0 the
            // attribute updates for every vertex and instancing is off.
            // ignored for geometry attributes.
            unsigned divisor = 0;
            // relative offset in the vertex data
            // typically offsetof(MyVertex, member)
            unsigned offset = 0;
        };
        unsigned vertex_struct_size = 0;
        std::vector<Attribute> attributes;

        VertexLayout() = default;
        VertexLayout(std::size_t struct_size,
                     std::initializer_list<Attribute> attrs)
                : vertex_struct_size(struct_size)
                , attributes(std::move(attrs))
        {}

        bool FromJson(const data::Reader& reader) noexcept;
        void IntoJson(data::Writer& writer) const;
        size_t GetHash() const noexcept;
    };

    bool operator==(const VertexLayout& lhs, const VertexLayout& rhs) noexcept;
    bool operator!=(const VertexLayout& lhs, const VertexLayout& rhs) noexcept;

    template<typename Vertex>
    const VertexLayout& GetVertexLayout();

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex2D>()
    {
        // todo: if using GLSL layout bindings then need to
        // specify the vertex attribute indices properly.
        // todo: if using instanced rendering then need to specify
        // the divisors properly.
        static const VertexLayout layout(sizeof(Vertex2D), {
            {"aPosition", 0, 2, 0, offsetof(Vertex2D, aPosition)},
            {"aTexCoord", 0, 2, 0, offsetof(Vertex2D, aTexCoord)}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<Vertex3D>()
    {
        static const VertexLayout layout(sizeof(Vertex3D), {
            {"aPosition", 0, 3, 0, offsetof(Vertex3D, aPosition)},
            {"aNormal",   0, 3, 0, offsetof(Vertex3D, aNormal)},
            {"aTexCoord", 0, 2, 0, offsetof(Vertex3D, aTexCoord)}
        });
        return layout;
    }

    template<> inline
    const VertexLayout& GetVertexLayout<ModelVertex3D>()
    {
        static const VertexLayout layout(sizeof(ModelVertex3D), {
            {"aPosition",  0, 3, 0, offsetof(ModelVertex3D, aPosition)},
            {"aNormal",    0, 3, 0, offsetof(ModelVertex3D, aNormal)},
            {"aTexCoord",  0, 2, 0, offsetof(ModelVertex3D, aTexCoord)},
            {"aTangent",   0, 3, 0, offsetof(ModelVertex3D, aTangent)},
            {"aBitangent", 0, 3, 0, offsetof(ModelVertex3D, aBitangent)},
        });
        return layout;
    }

    using InstanceDataLayout = VertexLayout;

    template<typename Attribute>
    const InstanceDataLayout& GetInstanceDataLayout();

    template<> inline
    const InstanceDataLayout& GetInstanceDataLayout<InstanceAttribute>()
    {
        static const InstanceDataLayout layout(sizeof(InstanceAttribute), {
            {"iaModelVectorX", 0, 4, 0, offsetof(InstanceAttribute, iaModelVectorX)},
            {"iaModelVectorY", 0, 4, 0, offsetof(InstanceAttribute, iaModelVectorY)},
            {"iaModelVectorZ", 0, 4, 0, offsetof(InstanceAttribute, iaModelVectorZ)},
            {"iaModelVectorW", 0, 4, 0, offsetof(InstanceAttribute, iaModelVectorW)}
        });
        return layout;
    }

    class VertexStream
    {
    public:
        VertexStream(const VertexLayout& layout, const void* buffer, size_t bytes)
          : mLayout(layout)
          , mBuffer((const uint8_t*)buffer)
          , mCount(GetCount(layout.vertex_struct_size, bytes))
        {}
        VertexStream(const VertexLayout& layout, const std::vector<uint8_t>& buffer)
          : mLayout(layout)
          , mBuffer(buffer.data())
          , mCount(GetCount(layout.vertex_struct_size, buffer.size()))
        {}

        template<typename Vertex>
        VertexStream(const VertexLayout& layout, const Vertex* buffer, size_t count)
          : mLayout(layout)
          , mBuffer((const uint8_t*)buffer)
          , mCount(count)
        {}
        template<typename Vertex>
        VertexStream(const VertexLayout& layout, const std::vector<Vertex>& buffer)
          : mLayout(layout)
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
        inline bool HasAttribute(const char* name) const noexcept
        {  return FindAttribute(name) != nullptr; }
        inline size_t GetCount() const noexcept
        { return mCount; }
        inline bool IsValid() const noexcept
        { return mBuffer != nullptr; }

        void IntoJson(data::Writer& writer) const;

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
        const VertexLayout mLayout;
        const uint8_t* mBuffer = nullptr;
        const size_t mCount = 0;
    };

    class VertexBuffer
    {
    public:
        VertexBuffer(const VertexLayout& layout, std::vector<uint8_t>* buffer)
          : mLayout(layout)
          , mBuffer(buffer)
        {}
        explicit VertexBuffer(const VertexLayout& layout)
          : mLayout(layout)
          , mBuffer(&mStorage)
        {}
        explicit VertexBuffer(std::vector<uint8_t>* buffer)
          : mBuffer(buffer)
        {}
        VertexBuffer()
          : mBuffer(&mStorage)
        {}

        inline void PushBack(const void* ptr)
        {
            ASSERT(mLayout.vertex_struct_size);
            const auto byte_offset = mBuffer->size();
            const auto byte_size = mLayout.vertex_struct_size;
            mBuffer->resize(byte_offset + byte_size);
            std::memcpy(&(*mBuffer)[byte_offset], ptr, byte_size);
        }

        inline const auto& GetLayout() const &  noexcept
        { return mLayout; }
        inline auto&& GetLayout() && noexcept
        { return std::move(mLayout); }

        inline const void* GetBufferPtr() const noexcept
        { return mBuffer->empty() ? nullptr : mBuffer->data(); }
        inline std::size_t GetBufferSize() const noexcept
        { return mBuffer->size(); }
        inline std::size_t GetCount() const noexcept
        { return mBuffer->size() / mLayout.vertex_struct_size; };
        inline void SetVertexLayout(VertexLayout layout) noexcept
        { mLayout = std::move(layout); }

        inline const auto& GetVertexBuffer() const & noexcept
        { return *mBuffer; }
        inline auto&& GetVertexBuffer() && noexcept
        { return std::move(*mBuffer); }


        template<typename T>
        const T* GetVertex(size_t index) const noexcept
        {
            ASSERT(index < GetCount());
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            const auto offset = index * mLayout.vertex_struct_size;
            return reinterpret_cast<const T*>(mBuffer->data() + offset);
        }

        template<typename T>
        std::vector<T> Copy() const
        {
            ASSERT(sizeof(T) == mLayout.vertex_struct_size);
            std::vector<T> ret;
            ret.resize(GetCount());
            if (!ret.empty())
                std::memcpy(ret.data(), mBuffer->data(), mBuffer->size());
            return ret;
        }

        bool Validate() const noexcept;

        bool FromJson(const data::Reader& reader);
    private:
        VertexLayout mLayout;
        std::vector<uint8_t> mStorage;
        std::vector<uint8_t>* mBuffer = nullptr;
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

        explicit IndexBuffer(Type type, std::vector<uint8_t>* buffer)
          : mType(type)
          , mBuffer(buffer)
        {}
        explicit IndexBuffer(Type type)
          : mType(type)
          , mBuffer(&mStorage)
        {}
        IndexBuffer(std::vector<uint8_t>* buffer)
          : mBuffer(buffer)
        {}
        IndexBuffer()
          : mBuffer(&mStorage)
        {}

        inline void SetType(Type type) noexcept
        { mType = type;}

        inline size_t GetCount() const noexcept
        { return mBuffer->size() / GetIndexByteSize(mType); }

        inline const void* GetBufferPtr() const noexcept
        { return mBuffer->data(); }

        inline size_t GetBufferSize() const noexcept
        { return mBuffer->size(); }

        inline Type GetType()  const noexcept
        { return mType; }

        inline const auto& GetIndexBuffer() const & noexcept
        { return *mBuffer; }

        inline auto&& GetIndexBuffer() && noexcept
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
