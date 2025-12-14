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

#include "base/format.h"
#include "graphics/debug_drawable.h"
#include "graphics/shader_source.h"
#include "graphics/program.h"
#include "graphics/geometry.h"

namespace gfx
{

bool DebugDrawableBase::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const
{
    return mDrawable->ApplyDynamicState(env, device, program, state);
}

ShaderSource DebugDrawableBase::GetShader(const Environment& env, const Device& device) const
{
    return mDrawable->GetShader(env, device);
}
std::string DebugDrawableBase::GetShaderId(const Environment& env) const
{
    return mDrawable->GetShaderId(env);
}
std::string DebugDrawableBase::GetShaderName(const Environment& env) const
{
    return mDrawable->GetShaderName(env);
}
std::string DebugDrawableBase::GetGeometryId(const Environment& env) const
{
    const auto normals = mFlags.test(Flags::Normals);
    const auto tangents = mFlags.test(Flags::Tangents);
    const auto bitangents = mFlags.test(Flags::Bitangents);

    std::string id;
    id += mDrawable->GetGeometryId(env);
    id += base::ToString(mFeature);
    id += normals ? "Normals" : "";
    id += tangents ? "Tangents" : "";
    id += bitangents ? "Bitangents" : "";
    return id;
}

bool DebugDrawableBase::Construct(const Environment& env, Device& device, Geometry::CreateArgs& create) const
{
    if (mDrawable->GetDrawPrimitive() != DrawPrimitive::Triangles)
    {
        return mDrawable->Construct(env, device, create);
    }

    Geometry::CreateArgs temp;
    if (!mDrawable->Construct(env, device, temp))
        return false;

    if (!temp.buffer.HasVertexData())
        return false;

    if (mFeature == Feature::Wireframe)
    {
        GeometryBuffer wireframe;
        CreateWireframe(temp.buffer, wireframe);

        create.buffer = std::move(wireframe);
        create.content_name = "Wireframe/" + temp.content_name;
        create.content_hash = temp.content_hash;
    }
    else if (mFeature == Feature::NormalMesh)
    {
        const auto normals = mFlags.test(Flags::Normals);
        const auto tangents = mFlags.test(Flags::Tangents);
        const auto bitangents = mFlags.test(Flags::Bitangents);

        unsigned flags = 0;
        if (normals) flags |= NormalMeshFlags::Normals;
        if (tangents) flags |= NormalMeshFlags::Tangents;
        if (bitangents) flags |= NormalMeshFlags::Bitangents;

        GeometryBuffer buffer;
        if (!CreateNormalMesh(temp.buffer, buffer, flags))
            return false;

        create.buffer = std::move(buffer);
        create.content_name = base::FormatString("%1%2%3Mesh/%4",
                normals ? "Normal" : "",
                tangents ? "Tangent" : "",
                bitangents ? "Bitangent" : "",
                temp.content_name);
        create.content_hash = temp.content_hash;
    }
    return true;
}

Drawable::Usage DebugDrawableBase::GetGeometryUsage() const
{
    return mDrawable->GetGeometryUsage();
}

SpatialMode DebugDrawableBase::GetSpatialMode() const
{
    return mDrawable->GetSpatialMode();
}

size_t DebugDrawableBase::GetGeometryHash() const
{
    return mDrawable->GetGeometryHash();
}

} // namespace