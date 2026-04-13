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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <vector>

#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/drawcall.h"
#include "graphics/simple_shape.h"

#include "device.h"
#include "graphics/shader_source.h"
#include "graphics/vertex.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/geometry_algo.h"
#include "graphics/paint_log.h"
#include "graphics/drawable_effect.h"

namespace gfx {

std::size_t SimpleShapeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mShape);
    hash = base::hash_combine(hash, mArgs);
    return hash;
}

SimpleShapeClass::SpatialMode SimpleShapeClass::GetSpatialMode() const
{
    return GetSimpleShapeSpatialMode(mShape);
}

void SimpleShapeClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("shape", mShape);
}
bool SimpleShapeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id", &mId);
    ok &= data.Read("name", &mName);
    ok &= data.Read("shape", &mShape);
    return ok;
}

float SimpleShapeClass::GetShapeAttribute(ShapeAttribute attribute) const noexcept
{
    if (attribute == ShapeAttribute::CornerRadius)
    {
        if (const auto* p = std::get_if<detail::RoundRectShapeArgs>(&mArgs))
            return p->corner_radius;
        else if (const auto* p = std::get_if<detail::CapsuleArgs>(&mArgs))
            return p->radius;

        BUG("No such simple shape attribute");
    }
    else if (attribute == ShapeAttribute::Orientation)
    {
        ASSERT(std::holds_alternative<detail::CapsuleArgs>(mArgs));
        return static_cast<float>(std::get<detail::CapsuleArgs>(mArgs).orientation);
    }
    return 0.0f;
}

bool SimpleShapeInstance::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState& state) const
{
    if (const auto* instanced_draw = draw.Get<GenericInstancedDraw>())
    {
        auto instance_data = PrepareDrawCall(*instanced_draw, device, env.editing_mode);
        if (!instance_data)
            GFX_PAINT_ERROR("Failed to prepare instanced drawing instance data.");
        program.SetInstanceData(std::move(instance_data));
    }

    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kDrawableFlags", mFlags);

    if (mEffect)
        mEffect->SetState(geometry, program);

    return true;
}
ShaderSource SimpleShapeInstance::GetShader(const Environment& env, const Device& device) const
{
    const auto effects = mEffect.has_value();
    const auto shape = GetShape();

    if (Is2DShape(shape))
        return Drawable::CreateShader(env.use_instancing, effects, device, Shader::Simple2D);
    if (Is3DShape(shape))
        return Drawable::CreateShader(env.use_instancing, effects, device, Shader::Simple3D);

    BUG("Bug on shape type.");
}

DrawGeometryHandle SimpleShapeInstance::GetGeometry(const Environment& env, Device& device) const
{
    detail::SimpleShapeEnvironment shape_env;
    shape_env.model_matrix = env.model_matrix;

    if (mEffect)
    {
        ASSERT(Is2DShape(mClass->GetShapeType()));

        if (env.mesh_type == MeshType::DebugMesh)
        {
            GFX_PAINT_ERROR("Debug mesh is not supported on simple shape effect mesh.");
            return DrawGeometryHandle::Null;
        }

        auto id = detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType());

        const auto effect_mesh_type = mEffect->GetEffectMshType();
        if (effect_mesh_type == DrawableEffect::EffectMeshType::ShardMesh)
        {
            const auto args = mEffect->GetShardMeshArgs();
            id += base::FormatString("ShardMesh@%1", args.mesh_subdivision_count);

            if (env.mesh_type == MeshType::Wireframe)
                id += "Wireframe";

            auto shard_texture  = device.FindTexture(id);
            auto shard_geometry = device.FindGeometry(id);
            if (shard_texture && shard_geometry)
                return DrawGeometryHandle(std::move(shard_geometry), shard_texture);

            auto geometry_buffer = Construct(env);
            ASSERT(!geometry_buffer.IsNull());

            DrawableEffect::GeometryInfo info;
            info.content_id   = std::move(id);
            info.content_hash = 0;
            info.content_name = base::FormatString("%1 Shards", mClass->GetShapeType());
            info.usage        = BufferUsage::Static;
            return DrawableEffect::GetShardGeometry(info, geometry_buffer, device);
        }
        else BUG("Missing effect mesh type handling.");
    }

    std::string id;
    std::string name;
    if (env.mesh_type == MeshType::Wireframe)
    {
        id = detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType());
        id += "Wireframe";
        name = base::FormatString("%1 Wireframe", mClass->GetShapeType());
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        if (Is2DShape(mClass->GetShapeType()))
        {
            GFX_PAINT_ERROR("Debug mesh is not supported on 2D simple shape.");
            return DrawGeometryHandle::Null;
        }

        const auto normals    = env.mesh_flags.test(MeshFlags::DebugNormals);
        const auto tangents   = env.mesh_flags.test(MeshFlags::DebugTangents);
        const auto bitangents = env.mesh_flags.test(MeshFlags::DebugBitangents);
        id = detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType());
        id += "DebugMesh";
        if (normals)
            id += "+Normals";
        if (tangents)
            id += "+Tangents";
        if (bitangents)
            id += "+BiTangents";

        name = base::FormatString("%1 DebugMesh", mClass->GetShapeType());
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        id = detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), shape_env, mStyle, mClass->GetShapeType());
        if (mStyle == Style::Solid)
            name = base::FormatString("%1", mClass->GetShapeType());
        else if (mStyle == Style::Outline)
            name = base::FormatString("%1 Outline", mClass->GetShapeType());
    }

    if (auto geometry = device.FindGeometry(id))
        return std::move(geometry);

    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.buffer = buffer.TransferGeometryBuffer();
    args.usage  = BufferUsage::Static;
    args.content_hash = 0;
    args.content_name = std::move(name);
    return device.CreateGeometry(id, std::move(args));
}

DrawGeometryBuffer SimpleShapeInstance::Construct(const Environment& env) const
{
    detail::SimpleShapeEnvironment shape_env;
    shape_env.model_matrix = env.model_matrix;

    if (mEffect)
    {
        ASSERT(Is2DShape(mClass->GetShapeType()));

        // not supported.
        if (env.mesh_type == MeshType::DebugMesh)
            return DrawGeometryBuffer::Null;

        GeometryBuffer buffer;
        detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType(), buffer);

        const auto effect_mesh_type = mEffect->GetEffectMshType();
        if (effect_mesh_type == DrawableEffect::EffectMeshType::ShardMesh)
        {
            const auto args = mEffect->GetShardMeshArgs();
            GeometryBuffer shard_geom_buffer;
            TextureBuffer shard_data_buffer;
            // should not fail since the data is all statically defined.
            ASSERT(DrawableEffect::ConstructShardEffectMesh(std::move(buffer),
                                                          &shard_geom_buffer,
                                                          &shard_data_buffer,
                                                          args.mesh_subdivision_count,
                                                          args.discard_skinny_slivers));

            if (env.mesh_type == MeshType::PaintMesh)
            {
                DEBUG("Created shard effect mesh on simple shape. [shape=%1]", mClass->GetShapeType());
                return DrawGeometryBuffer(std::move(shard_geom_buffer), std::move(shard_data_buffer));
            }
            else if (env.mesh_type == MeshType::Wireframe)
            {
                GeometryBuffer wireframe;
                CreateWireframe(shard_geom_buffer, wireframe);

                DEBUG("Created wireframe shard effect mesh on simple shape. [shape=%1]", mClass->GetShapeType());
                return DrawGeometryBuffer(std::move(wireframe), std::move(shard_data_buffer));
            }
        }
        else BUG("Missing effect mesh type handling.");
        return DrawGeometryBuffer::Null;
    }

    if (env.mesh_type == MeshType::Wireframe)
    {
        GeometryBuffer buffer;
        GeometryBuffer wireframe;
        detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType(), buffer);
        CreateWireframe(buffer, wireframe);

        DEBUG("Created wireframe mesh on simple shape. [shape=%1]", mClass->GetShapeType());
        return std::move(wireframe);
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        // not supported.
        if (Is2DShape(mClass->GetShapeType()))
            return DrawGeometryBuffer::Null;

        const auto normals    = env.mesh_flags.test(MeshFlags::DebugNormals);
        const auto tangents   = env.mesh_flags.test(MeshFlags::DebugTangents);
        const auto bitangents = env.mesh_flags.test(MeshFlags::DebugBitangents);

        unsigned flags = 0;
        if (normals) flags |= DebugMeshFlags::Normals;
        if (tangents) flags |= DebugMeshFlags::Tangents;
        if (bitangents) flags |= DebugMeshFlags::Bitangents;

        GeometryBuffer buffer;
        GeometryBuffer debug_mesh;
        detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, Style::Solid, mClass->GetShapeType(), buffer);
        CreateDebugMesh(buffer, debug_mesh, flags);

        DEBUG("Created debug mesh on simple shape. [shape=%1]", mClass->GetShapeType());
        return std::move(debug_mesh);
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        GeometryBuffer buffer;
        detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, mStyle, mClass->GetShapeType(), buffer);

        DEBUG("Created paint mesh on simple shape. [shape=%1, style=%2]", mClass->GetShapeType(), mStyle);
        return std::move(buffer);
    }
    else BUG("Missing mesh type handling.");
    return GeometryBuffer{};
}

void SimpleShapeInstance::Update(const Environment& env, float dt)
{
    if (mEffect)
        mEffect->Update(dt);
}

std::string SimpleShapeInstance::GetShaderId(const Environment& env) const
{
    const auto effects = mEffect.has_value();
    const auto shape = GetShape();

    if (Is2DShape(shape))
        return Drawable::GetShaderId(env.use_instancing, effects, Shader::Simple2D);
    if (Is3DShape(shape))
        return Drawable::GetShaderId(env.use_instancing, effects, Shader::Simple3D);

    BUG("Bug on shape type.");
}

std::string SimpleShapeInstance::GetShaderName(const Environment& env) const
{
    const auto shape = GetShape();
    if (Is2DShape(shape))
        return Drawable::GetShaderName(env, Shader::Simple2D);
    if (Is3DShape(shape))
        return Drawable::GetShaderName(env, Shader::Simple3D);

    BUG("Bug on shape type.");
}

Drawable::Type SimpleShapeInstance::GetType() const
{
    return Type::SimpleShape;
}

Drawable::DrawPrimitive SimpleShapeInstance::GetDrawPrimitive() const
{
    if (Is3DShape(mClass->GetShapeType()))
        return DrawPrimitive::Triangles;

    if (mEffect)
        return DrawPrimitive::Triangles;

    if (mStyle == Style::Outline)
        return DrawPrimitive::Lines;

    return DrawPrimitive::Triangles;
}

SpatialMode SimpleShapeInstance::GetSpatialMode() const
{
    return mClass->GetSpatialMode();
}

bool SimpleShapeInstance::SetEffect(DrawableEffect effect)
{
    if (effect.GetEffectMshType() == DrawableEffect::EffectMeshType::ShardMesh)
    {
        if (Is2DShape(mClass->GetShapeType()))
        {
            mEffect = effect;
            DEBUG("Set drawable effect on simple shape instance. [effect=%1]", mEffect->GetEffectType());
            return true;
        }
    }
    ERROR("Drawable effect is not compatible with the simple shape type. [shape=%1, effect=%2]",
        mClass->GetShapeType(), effect.GetEffectType());
    return false;
}

bool SimpleShape::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState& state) const
{
    if (const auto* instanced_draw = draw.Get<GenericInstancedDraw>())
    {
        auto instance_data = PrepareDrawCall(*instanced_draw, device, env.editing_mode);
        if (!instance_data)
            GFX_PAINT_ERROR("Failed to prepare instanced drawing instance data.");
        program.SetInstanceData(std::move(instance_data));
    }

    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kDrawableFlags", mFlags);
    return true;
}
ShaderSource SimpleShape::GetShader(const Environment& env, const Device& device) const
{
    if (Is2DShape(mShape))
        return CreateShader(env.use_instancing, false, device, Shader::Simple2D);
    if (Is3DShape(mShape))
        return CreateShader(env.use_instancing, false, device, Shader::Simple3D);

    BUG("Bug on shape type.");
}
std::string SimpleShape::GetShaderId(const Environment& env) const
{
    if (Is2DShape(mShape))
        return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple2D);
    if (Is3DShape(mShape))
        return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple3D);

    BUG("Bug on shape type.");
}

std::string SimpleShape::GetShaderName(const Environment& env) const
{
    if (Is2DShape(mShape))
        return Drawable::GetShaderName(env, Shader::Simple2D);
    if (Is3DShape(mShape))
        return Drawable::GetShaderName(env, Shader::Simple3D);

    BUG("Bug on shape type.");
}

DrawGeometryHandle SimpleShape::GetGeometry(const Environment& env, Device& device) const
{
    detail::SimpleShapeEnvironment shape_env;
    shape_env.model_matrix = env.model_matrix;

    std::string id;
    std::string name;
    if (env.mesh_type == MeshType::Wireframe)
    {
        id = detail::GetSimpleShapeGeometryId(mArgs, shape_env, Style::Solid, mShape);
        id += "Wireframe";
        name = base::FormatString("%1 Wireframe", mShape);
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        if (Is2DShape(mShape))
        {
            GFX_PAINT_ERROR("Debug mesh is not supported on 2D simple shape.");
            return DrawGeometryHandle::Null;
        }

        const auto normals    = env.mesh_flags.test(MeshFlags::DebugNormals);
        const auto tangents   = env.mesh_flags.test(MeshFlags::DebugTangents);
        const auto bitangents = env.mesh_flags.test(MeshFlags::DebugBitangents);
        id = detail::GetSimpleShapeGeometryId(mArgs, shape_env, Style::Solid, mShape);
        id += "DebugMesh";
        if (normals)
            id += "+Normals";
        if (tangents)
            id += "+Tangents";
        if (bitangents)
            id += "+BiTangents";

        name = base::FormatString("%1 DebugMesh", mShape);
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        id = detail::GetSimpleShapeGeometryId(mArgs, shape_env, mStyle, mShape);
        if (mStyle == Style::Solid)
            name = base::FormatString("%1", mShape);
        else if (mStyle == Style::Outline)
            name = base::FormatString("%1 Outline", mShape);
    }

    if (auto geometry = device.FindGeometry(id))
        return std::move(geometry);

    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.buffer = buffer.TransferGeometryBuffer();
    args.usage  = BufferUsage::Static;
    args.content_hash = 0;
    args.content_name = std::move(name);
    return device.CreateGeometry(id, std::move(args));
}

DrawGeometryBuffer SimpleShape::Construct(const Environment& env) const
{
    detail::SimpleShapeEnvironment shape_env;
    shape_env.model_matrix = env.model_matrix;

    if (env.mesh_type == MeshType::Wireframe)
    {
        GeometryBuffer buffer;
        GeometryBuffer wireframe;
        detail::ConstructSimpleShape(mArgs, shape_env, Style::Solid, mShape, buffer);
        CreateWireframe(buffer, wireframe);

        DEBUG("Created wireframe mesh on simple shape. [shape=%1]", mShape);
        return std::move(wireframe);
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        // not supported.
        if (Is2DShape(mShape))
            return DrawGeometryBuffer::Null;

        const auto normals    = env.mesh_flags.test(MeshFlags::DebugNormals);
        const auto tangents   = env.mesh_flags.test(MeshFlags::DebugTangents);
        const auto bitangents = env.mesh_flags.test(MeshFlags::DebugBitangents);

        unsigned flags = 0;
        if (normals) flags |= DebugMeshFlags::Normals;
        if (tangents) flags |= DebugMeshFlags::Tangents;
        if (bitangents) flags |= DebugMeshFlags::Bitangents;

        GeometryBuffer buffer;
        GeometryBuffer debug_mesh;
        detail::ConstructSimpleShape(mArgs, shape_env, Style::Solid, mShape, buffer);
        CreateDebugMesh(buffer, debug_mesh, flags);

        DEBUG("Created debug mesh on simple shape. [shape=%1]", mShape);
        return std::move(debug_mesh);
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        GeometryBuffer buffer;
        detail::ConstructSimpleShape(mArgs, shape_env, mStyle, mShape, buffer);

        DEBUG("Created paint mesh on simple shape. [shape=%1, style=%2]", mShape, mStyle);
        return std::move(buffer);
    }
    else BUG("Missing mesh type handling.");
    return GeometryBuffer{};
}

Drawable::Type SimpleShape::GetType() const
{
    return Type::SimpleShape;
}

Drawable::DrawPrimitive SimpleShape::GetDrawPrimitive() const
{
    if (Is3DShape(mShape))
        return DrawPrimitive::Triangles;

    if (mStyle == Style::Outline)
        return DrawPrimitive::Lines;

    return DrawPrimitive::Triangles;
}

SpatialMode SimpleShape::GetSpatialMode() const
{
    return GetSimpleShapeSpatialMode(mShape);
}

float SimpleShape::GetShapeAttribute(ShapeAttribute attribute) const noexcept
{
    if (attribute == ShapeAttribute::CornerRadius)
    {
        if (const auto* p = std::get_if<detail::RoundRectShapeArgs>(&mArgs))
            return p->corner_radius;
        else if (const auto* p = std::get_if<detail::CapsuleArgs>(&mArgs))
            return p->radius;

        BUG("No such simple shape attribute");
    }
    else if (attribute == ShapeAttribute::Orientation)
    {
        ASSERT(std::holds_alternative<detail::CapsuleArgs>(mArgs));
        return static_cast<float>(std::get<detail::CapsuleArgs>(mArgs).orientation);
    }
    return 0.0f;
}


} // namespace gfx
