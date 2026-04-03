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
#include "graphics/shader_source.h"
#include "graphics/vertex.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/geometry_algo.h"
#include "graphics/paint_log.h"

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

bool SimpleShapeInstance::ApplyDynamicState(const Environment& env, const DrawCall& draw, Device& device, ProgramState& program, RasterState& state) const
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
ShaderSource SimpleShapeInstance::GetShader(const Environment& env, const Device& device) const
{
    const auto shape = GetShape();
    if (Is2DShape(shape))
        return Drawable::CreateShader(env, device, Shader::Simple2D);
    if (Is3DShape(shape))
        return Drawable::CreateShader(env, device, Shader::Simple3D);

    BUG("Bug on shape type.");
}
std::string SimpleShapeInstance::GetGeometryId(const Environment& env) const
{
    const detail::SimpleShapeEnvironment shape_env = {env.model_matrix};
    return detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), shape_env, mStyle, mClass->GetShapeType());
}

bool SimpleShapeInstance::Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const
{
    if (env.mesh_type == MeshType::ShardedEffectMesh)
    {
        if (!Is2DShape(mClass->GetShapeType()))
            return false;
        if (mStyle != Style::Solid)
            return false;

        constexpr auto discard_skinny_slivers = true;
        const auto& args = std::get<ShardedEffectMeshArgs>(env.mesh_args);
        return ConstructShardMesh(env, device, geometry,args.mesh_subdivision_count, discard_skinny_slivers);
    }

    const detail::SimpleShapeEnvironment shape_env = {env.model_matrix};

    geometry.usage = Geometry::Usage::Static;
    geometry.content_name = base::ToString(mClass->GetShapeType());
    detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, mStyle, mClass->GetShapeType(), geometry.buffer);
    return true;
}

std::string SimpleShapeInstance::GetShaderId(const Environment& env) const
{
    const auto shape = GetShape();
    if (Is2DShape(shape))
        return Drawable::GetShaderId(env, Shader::Simple2D);
    if (Is3DShape(shape))
        return Drawable::GetShaderId(env, Shader::Simple3D);

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

    if (mStyle == Style::Outline)
        return DrawPrimitive::Lines;

    return DrawPrimitive::Triangles;
}

Drawable::Usage SimpleShapeInstance::GetGeometryUsage() const
{
    return Usage::Static;
}

SpatialMode SimpleShapeInstance::GetSpatialMode() const
{
    return mClass->GetSpatialMode();
}

bool SimpleShapeInstance::ConstructShardMesh(const Environment& env, Device& device, Geometry::CreateArgs& create,
    unsigned mesh_subdivision_count, bool discard_skinny_slivers) const
{
    Geometry::CreateArgs temp;
    temp.content_name = base::ToString(mClass->GetShapeType());
    temp.usage = Geometry::Usage::Static;
    const detail::SimpleShapeEnvironment shape_env = {env.model_matrix};
    detail::ConstructSimpleShape(mClass->GetShapeArgs(), shape_env, mStyle, mClass->GetShapeType(), temp.buffer);

    // the triangle mesh computation produces a  mesh that  has the same
    // vertex layout as the original drawables geometry  buffer.
    GeometryBuffer shard_geometry_buffer;
    if (!CreateShardEffectMesh(temp.buffer, &shard_geometry_buffer, mesh_subdivision_count, discard_skinny_slivers))
        return false;

    const auto vertex_count = shard_geometry_buffer.GetVertexCount();
    const auto triangle_count = vertex_count / 3;

    create.buffer       = std::move(shard_geometry_buffer);
    create.usage        = temp.usage;
    create.content_hash = temp.content_hash;
    create.content_name = temp.content_name;
    DEBUG("Successfully constructed simple shape shard mesh. [shape=%1, triangles=%2]",
        mClass->GetShapeType(), triangle_count);
    return true;
}

bool SimpleShape::ApplyDynamicState(const Environment& env, const DrawCall& draw, Device& device, ProgramState& program, RasterState& state) const
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
    // not supporting the effect mesh operation in this render path right now
    // since it's not needed.
    ASSERT(env.mesh_type == MeshType::NormalRenderMesh);

    if (Is2DShape(mShape))
        return CreateShader(env, device, Shader::Simple2D);
    if (Is3DShape(mShape))
        return CreateShader(env, device, Shader::Simple3D);

    BUG("Bug on shape type.");
}
std::string SimpleShape::GetShaderId(const Environment& env) const
{
    if (Is2DShape(mShape))
        return Drawable::GetShaderId(env, Shader::Simple2D);
    if (Is3DShape(mShape))
        return Drawable::GetShaderId(env, Shader::Simple3D);

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

std::string SimpleShape::GetGeometryId(const Environment& env) const
{
    const detail::SimpleShapeEnvironment shape_env = {env.model_matrix};
    return detail::GetSimpleShapeGeometryId(mArgs, shape_env, mStyle, mShape);
}

bool SimpleShape::Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const
{
    const detail::SimpleShapeEnvironment shape_env = {env.model_matrix};

    geometry.content_name = base::ToString(mShape);
    geometry.usage = Geometry::Usage::Static;
    detail::ConstructSimpleShape(mArgs, shape_env, mStyle, mShape, geometry.buffer);

    if (Is3DShape(mShape))
        ASSERT(ComputeTangents(geometry.buffer));

    return true;
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

Drawable::Usage SimpleShape::GetGeometryUsage() const
{
    return Usage::Static;
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
