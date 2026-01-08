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
#include <type_traits>

#include "base/math.h"
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

    inline Vec2 Lerp(const Vec2& one, const Vec2& two, float t) noexcept
    {
        Vec2 ret;
        ret.x = math::lerp(one.x, two.x, t);
        ret.y = math::lerp(one.y, two.y, t);
        return ret;
    }

    inline Vec3 Lerp(const Vec3& one, const Vec3 two, float t) noexcept
    {
        Vec3 ret;
        ret.x = math::lerp(one.x, two.x, t);
        ret.y = math::lerp(one.y, two.y, t);
        ret.z = math::lerp(one.z, two.z, t);
        return ret;
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

} // namespace
