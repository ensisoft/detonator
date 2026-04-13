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

#include "base/assert.h"
#include "graphics/debug_drawable.h"

namespace gfx
{

bool DebugDrawableBase::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState& state) const
{
    return mDrawable->ApplyDynamicState(GetEnvironment(env), draw, geometry, device, program, state);
}

ShaderSource DebugDrawableBase::GetShader(const Environment& env, const Device& device) const
{
    return mDrawable->GetShader(GetEnvironment(env), device);
}
std::string DebugDrawableBase::GetShaderId(const Environment& env) const
{
    return mDrawable->GetShaderId(GetEnvironment(env));
}
std::string DebugDrawableBase::GetShaderName(const Environment& env) const
{
    return mDrawable->GetShaderName(GetEnvironment(env));
}

DrawGeometryHandle DebugDrawableBase::GetGeometry(const Environment& env, Device& device) const
{
    return mDrawable->GetGeometry(GetEnvironment(env), device);
}

DrawGeometryBuffer DebugDrawableBase::Construct(const Environment& env) const
{
    return mDrawable->Construct(GetEnvironment(env));
}

Drawable::Environment DebugDrawableBase::GetEnvironment(const Environment& env) const
{
    Environment e = env;
    if (mFeature == Feature::Wireframe)
    {
        e.mesh_type = MeshType::Wireframe;
    }
    else if (mFeature == Feature::NormalMesh)
    {
        e.mesh_type = MeshType::DebugMesh;
        e.mesh_flags.set(MeshFlags::DebugNormals, mFlags.test(Flags::Normals));
        e.mesh_flags.set(MeshFlags::DebugTangents, mFlags.test(Flags::Tangents));
        e.mesh_flags.set(MeshFlags::DebugBitangents, mFlags.test(Flags::Bitangents));
    }
    else BUG("Missing debug feature handling.");
    return e;
}

SpatialMode DebugDrawableBase::GetSpatialMode() const
{
    return mDrawable->GetSpatialMode();
}

} // namespace