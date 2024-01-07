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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

#include "base/assert.h"
#include "data/fwd.h"

namespace gfx
{
    // 16bit vertex index for indexed drawing.
    using Index16 = std::uint16_t;
    // 32bit vertex index for indexed drawing.
    using Index32 = std::uint32_t;

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
#pragma pack(pop)

    // The offsetof macro is guaranteed to be usable only with types with standard layout.
    static_assert(std::is_standard_layout<Vertex3D>::value, "Vertex3D must meet standard layout.");
    static_assert(std::is_standard_layout<Vertex2D>::value, "Vertex2D must meet standard layout.");

    // memcpy, binary read/write as-is require trivially_copyable.
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

    class GeometryBuffer
    {
    public:
        // Define how the geometry is to be rasterized.
        enum class DrawType : uint32_t {
            // Draw the given vertices as triangles, i.e.
            // each 3 vertices make a single triangle.
            Triangles,
            // Draw each given vertex as a separate point
            Points,
            // Draw a series of triangles all connected to the
            // first vertex
            TriangleFan,
            // Draw the vertices as a series of connected lines
            // where each pair of adjacent vertices are connected
            // by a line.
            // In this draw the line width setting applies.
            Lines,
            // Draw a line between the given vertices looping back
            // from the last vertex to the first.
            LineLoop
        };

        enum class IndexType {
            Index16, Index32
        };

        // Define how the contents of the geometry object are expected
        // to be used.
        enum class Usage {
            // The geometry is updated once and drawn multiple times.
            Static,
            // The geometry is updated multiple times and drawn once/few times.
            Stream,
            // The buffer is updated multiple times and drawn multiple times.
            Dynamic
        };

        // the structure is packed for more compact and simpler
        // serialization purposes
#pragma pack(push, 1)
        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            uint32_t count  = 0;
            uint32_t offset = 0;
        };
#pragma pack(pop)

        void UploadVertices(const void* data, size_t bytes)
        {
            ASSERT((data && bytes) || (!data && !bytes));
            mVertexData.resize(bytes);
            if (data)
                std::memcpy(&mVertexData[0], data, bytes);
        }
        void UploadIndices(const void* data, size_t bytes, IndexType type)
        {
            ASSERT((data && bytes) || (!data && !bytes));
            mIndexData.resize(bytes);
            if (data)
                std::memcpy(&mIndexData[0], data, bytes);
            mIndexType  = type;
        }

        inline void ClearDraws() noexcept
        { mDrawCmds.clear(); }
        inline void AddDrawCmd(const DrawCommand& cmd)
        {  mDrawCmds.push_back(cmd); }
        inline void SetVertexLayout(const VertexLayout& layout)
        { mVertexLayout = layout; }
        inline size_t GetNumDrawCmds() const noexcept
        { return mDrawCmds.size(); }
        inline size_t GetVertexBytes() const noexcept
        { return mVertexData.size(); }
        inline size_t GetIndexBytes() const noexcept
        { return mIndexData.size(); }
        inline const void* GetVertexDataPtr() const noexcept
        { return mVertexData.empty() ? nullptr : &mVertexData[0]; }
        inline const void* GetIndexDataPtr() const noexcept
        { return mIndexData.empty() ? nullptr : &mIndexData[0]; }
        inline auto& GetLayout() const & noexcept
        { return mVertexLayout; }
        inline auto&& GetLayout() && noexcept
        { return std::move(mVertexLayout); }
        inline IndexType GetIndexType() const noexcept
        { return mIndexType; }
        inline const DrawCommand GetDrawCmd(size_t index) const
        { return mDrawCmds[index]; }

        // Update the geometry object's data buffer contents.
        template<typename Vertex>
        void SetVertexBuffer(const Vertex* vertices, std::size_t count)
        { UploadVertices(vertices, count * sizeof(Vertex)); }

        template<typename Vertex> inline
        void SetVertexBuffer(const std::vector<Vertex>& vertices)
        { UploadVertices(vertices.data(), vertices.size() * sizeof(Vertex)); }

        inline void SetVertexBuffer(std::vector<uint8_t>&& data) noexcept
        { mVertexData = std::move(data); }

        inline void SetIndexData(std::vector<uint8_t>&& data) noexcept
        { mIndexData = std::move(data); }
        void SetIndexBuffer(const Index16* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index16), IndexType::Index16); }

        void SetIndexBuffer(const Index32* indices, size_t count)
        { UploadIndices(indices, count * sizeof(Index32), IndexType::Index32); }

        void SetIndexBuffer(const std::vector<Index16>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        void SetIndexBuffer(const std::vector<Index32>& indices)
        { SetIndexBuffer(indices.data(), indices.size()); }

        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        inline void AddDrawCmd(DrawType type)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = 0;
            cmd.count  = std::numeric_limits<uint32_t>::max();
            AddDrawCmd(cmd);
        }

        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        inline void AddDrawCmd(DrawType type, uint32_t offset, size_t count)
        {
            DrawCommand cmd;
            cmd.type   = type;
            cmd.offset = offset;
            cmd.count  = count;
            AddDrawCmd(cmd);
        }

        inline void SetDrawCommands(const std::vector<DrawCommand>& commands)
        { mDrawCmds = commands; }
        inline void SetDrawCmds(std::vector<DrawCommand>&& commands)
        { mDrawCmds = std::move(commands); }

        inline const auto& GetDrawCommands() const & noexcept
        { return mDrawCmds; }

        inline auto GetDrawCommands() && noexcept
        { return std::move(mDrawCmds); }

        inline bool HasData() const noexcept
        { return !mVertexData.empty(); }

        inline size_t GetVertexCount() const noexcept
        {
            ASSERT(mVertexLayout.vertex_struct_size);
            return mVertexData.size() / mVertexLayout.vertex_struct_size;
        }

        template<typename Vertex>
        Vertex GetVertexAt(size_t index) const noexcept
        {
            ASSERT(sizeof(Vertex) == mVertexLayout.vertex_struct_size);
            const auto count = GetVertexCount();
            ASSERT(index < count);
            const auto offset = index * mVertexLayout.vertex_struct_size;
            Vertex vertex;
            const auto* src = (const char*)mVertexData.data();
            std::memcpy(&vertex, src + offset, sizeof(vertex));
            return vertex;

        }
    private:
        VertexLayout mVertexLayout;
        std::vector<DrawCommand> mDrawCmds;
        std::vector<uint8_t> mVertexData;
        std::vector<uint8_t> mIndexData;
        IndexType mIndexType = IndexType::Index16;
    };


    // Encapsulate information about a particular geometry and how
    // that geometry is to be rendered and rasterized. A geometry
    // object contains a set of vertex data and then multiple draw
    // commands each command addressing some subset of the vertices.
    class Geometry
    {
    public:
        using Usage       = GeometryBuffer::Usage;
        using DrawType    = GeometryBuffer::DrawType;
        using DrawCommand = GeometryBuffer::DrawCommand;
        using IndexType   = GeometryBuffer::IndexType;

        struct CreateArgs {
            GeometryBuffer buffer;
            // Set the expected usage of the geometry. Should be set before
            // calling any methods to upload the data.
            Usage usage = Usage::Static;
            // Set the (human-readable) name of the geometry.
            // This has debug significance only.
            std::string content_name;
            // Get the hash value based on the buffer contents.
            std::size_t content_hash = 0;
        };

        virtual ~Geometry() = default;

        // Get the human-readable geometry name.
        virtual std::string GetName() const = 0;
        // Get the current usage set on the geometry.
        virtual Usage GetUsage() const = 0;
        // Get the hash value computed from the geometry buffer.
        virtual std::size_t GetContentHash() const  = 0;
        // Get the number of draw commands set on the geometry.
        virtual size_t GetNumDrawCmds() const = 0;
        // Get the draw command at the specified index.
        virtual DrawCommand GetDrawCmd(size_t index) const = 0;

        // Map the type of the index to index size in bytes.
        static size_t GetIndexByteSize(IndexType type)
        {
            if (type == IndexType::Index16)
                return 2;
            else if (type == IndexType::Index32)
                return 4;
            else BUG("Missing index type.");
            return 0;
        }
    private:
    };

    using GeometryPtr = std::shared_ptr<const Geometry>;

    class GeometryDrawCommand
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        GeometryDrawCommand(const Geometry& geometry) noexcept
          : mGeometry(&geometry)
          , mCmdStart(0)
          , mCmdCount(geometry.GetNumDrawCmds())
        {}
        GeometryDrawCommand(const Geometry& geometry, size_t cmd_start, size_t cmd_count)
          : mGeometry(&geometry)
          , mCmdStart(cmd_start)
          , mCmdCount(ResolveCount(geometry, cmd_count))
         {}
        inline size_t GetNumDrawCmds() const noexcept
        { return mCmdCount; }
        inline DrawCommand GetDrawCmd(size_t index) const noexcept
        { return mGeometry->GetDrawCmd(mCmdStart + index); }
        inline const Geometry* GetGeometry() const noexcept
        { return mGeometry;}

        static size_t ResolveCount(const Geometry& geometry, size_t count) noexcept
        {
            if (count == std::numeric_limits<size_t>::max())
                return geometry.GetNumDrawCmds();
            return count;
        }

    private:
        const Geometry* mGeometry = nullptr;
        const size_t mCmdStart = 0;
        const size_t mCmdCount = 0;
    };


    class IndexStream
    {
    public:
        using Type = Geometry::IndexType;

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
            const auto size = Geometry::GetIndexByteSize(type);
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
        explicit IndexBuffer(Geometry::IndexType type, std::vector<uint8_t>* buffer)
          : mType(type)
          , mBuffer(buffer)
        {}
        explicit IndexBuffer(Geometry::IndexType type)
          : mType(type)
          , mBuffer(&mStorage)
        {}
        IndexBuffer(std::vector<uint8_t>* buffer)
          : mBuffer(buffer)
        {}
        IndexBuffer()
          : mBuffer(&mStorage)
        {}

        inline void SetType(Geometry::IndexType type) noexcept
        { mType = type;}

        inline size_t GetCount() const noexcept
        { return mBuffer->size() / Geometry::GetIndexByteSize(mType); }

        const void* GetBufferPtr() const noexcept
        { return mBuffer->data(); }

        size_t GetBufferSize() const noexcept
        { return mBuffer->size(); }

        Geometry::IndexType GetType()  const noexcept
        { return mType; }

        inline const auto& GetIndexBuffer() const & noexcept
        { return *mBuffer; }

        inline auto&& GetIndexBuffer() && noexcept
        { return std::move(*mBuffer); }

        void PushBack(Index16 value) noexcept
        {
            ASSERT(mType == Geometry::IndexType::Index16);
            const auto offset = mBuffer->size();
            mBuffer->resize(offset + sizeof(value));
            std::memcpy(&(*mBuffer)[offset], &value, sizeof(value));
        }

        void PushBack(Index32 value) noexcept
        {
            ASSERT(mType == Geometry::IndexType::Index32);
            const auto offset = mBuffer->size();
            mBuffer->resize(offset + sizeof(value));
            std::memcpy(&(*mBuffer)[offset], &value, sizeof(value));
        }


        bool FromJson(const data::Reader& reader);

    private:
        Geometry::IndexType mType = Geometry::IndexType::Index16;
        std::vector<uint8_t> mStorage;
        std::vector<uint8_t>* mBuffer = nullptr;
    };

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

    class CommandStream
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        explicit CommandStream(const std::vector<DrawCommand>& commands)
          : mCommands(commands.data())
          , mCount(commands.size())
        {}
        CommandStream(const DrawCommand* commands, size_t count)
          : mCommands(commands)
          , mCount(count)
        {}

        inline size_t GetCount() const noexcept
        { return mCount; }
        inline DrawCommand GetCommand(size_t index) const noexcept
        {
            ASSERT(index < mCount);
            return mCommands[index];
        }

        void IntoJson(data::Writer& writer) const;

    private:
        const DrawCommand* mCommands = nullptr;
        const size_t mCount = 0;
    };

    class CommandBuffer
    {
    public:
        using DrawCommand = Geometry::DrawCommand;

        explicit CommandBuffer(std::vector<DrawCommand>* commands)
          : mBuffer(commands)
        {}
        CommandBuffer()
          : mBuffer(&mStorage)
        {}

        inline size_t GetCount() const noexcept
        { return mBuffer->size(); }

        inline DrawCommand GetCommand(size_t index) const noexcept
        {
            ASSERT(index < mBuffer->size());
            return (*mBuffer)[index];
        }

        inline const auto& GetCommandBuffer() const & noexcept
        { return *mBuffer; }
        inline auto&& GetCommandBuffer() && noexcept
        { return std::move(*mBuffer); }

        inline void PushBack(const DrawCommand& cmd)
        { mBuffer->push_back(cmd); }

        bool FromJson(const data::Reader& reader);

    private:
        std::vector<DrawCommand> mStorage;
        std::vector<DrawCommand>* mBuffer = nullptr;
    };


    void CreateWireframe(const GeometryBuffer& geometry, GeometryBuffer& wireframe);

} // namespace
