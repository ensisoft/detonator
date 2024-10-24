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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <base64/base64.h>
#include "warnpop.h"

#include <functional> // for hash
#include <cmath>
#include <cstdio>

#include "base/logging.h"
#include "base/utility.h"
#include "base/threadpool.h"
#include "base/math.h"
#include "base/json.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"
#include "graphics/drawcmd.h"
#include "graphics/drawable.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"
#include "graphics/resource.h"
#include "graphics/transform.h"
#include "graphics/loader.h"
#include "graphics/shadersource.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"
#include "graphics/utility.h"

namespace gfx {

void DebugDrawableBase::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const
{
    mDrawable->ApplyDynamicState(env, program, state);
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
    std::string id;
    id += mDrawable->GetGeometryId(env);
    id += base::ToString(mFeature);
    return id;
}

bool DebugDrawableBase::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    if (mDrawable->GetPrimitive() != Primitive::Triangles)
    {
        return mDrawable->Construct(env, create);
    }

    if (mFeature == Feature::Wireframe)
    {
        Geometry::CreateArgs temp;
        if (!mDrawable->Construct(env, temp))
            return false;

        if (temp.buffer.HasData())
        {
            GeometryBuffer wireframe;
            CreateWireframe(temp.buffer, wireframe);

            create.buffer = std::move(wireframe);
            create.content_name = "Wireframe/" + temp.content_name;
            create.content_hash = temp.content_hash;
        }
    }
    return true;
}

Drawable::Usage DebugDrawableBase::GetUsage() const
{
    return mDrawable->GetUsage();
}

size_t DebugDrawableBase::GetContentHash() const
{
    return mDrawable->GetContentHash();
}

bool Is3DShape(const Drawable& drawable) noexcept
{
    const auto type = drawable.GetType();
    if (type == Drawable::Type::Polygon)
    {
        if (const auto* instance = dynamic_cast<const PolygonMeshInstance*>(&drawable))
        {
            const auto mesh = instance->GetMeshType();
            if (mesh == PolygonMeshInstance::MeshType::Simple3D ||
                mesh == PolygonMeshInstance::MeshType::Model3D)
                return true;
        }
    }

    if (type != Drawable::Type::SimpleShape)
        return false;
    if (const auto* instance = dynamic_cast<const SimpleShapeInstance*>(&drawable))
        return Is3DShape(instance->GetShape());
    else if (const auto* instance = dynamic_cast<const SimpleShape*>(&drawable))
        return Is3DShape(instance->GetShape());
    else BUG("Unknown drawable shape type.");
}

bool Is3DShape(const DrawableClass& klass) noexcept
{
    const auto type = klass.GetType();
    if (type == Drawable::Type::Polygon)
    {
        if (const auto* polygon = dynamic_cast<const PolygonMeshClass*>(&klass))
        {
            const auto mesh = polygon->GetMeshType();
            if (mesh == PolygonMeshClass::MeshType::Simple3D ||
                mesh == PolygonMeshClass::MeshType::Model3D)
                return true;
        }
    }
    if (type != DrawableClass::Type::SimpleShape)
        return false;
    if (const auto* simple = dynamic_cast<const SimpleShapeClass*>(&klass))
        return Is3DShape(simple->GetShapeType());
    else BUG("Unknown drawable shape type");
}

std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass)
{
    // factory function based on type switching.
    // Alternative way would be to have a virtual function in the DrawableClass
    // but this would have 2 problems: creating based shared_ptr of the drawable
    // class would require shared_from_this which I consider to be bad practice and
    // it'd cause some problems at some point.
    // secondly it'd create a circular dependency between class and the instance types
    // which is going to cause some problems at some point.
    const auto type = klass->GetType();

    if (type == DrawableClass::Type::SimpleShape)
        return std::make_unique<SimpleShapeInstance>(std::static_pointer_cast<const SimpleShapeClass>(klass));
    else if (type == DrawableClass::Type::ParticleEngine)
        return std::make_unique<ParticleEngineInstance>(std::static_pointer_cast<const ParticleEngineClass>(klass));
    else if (type == DrawableClass::Type::Polygon)
        return std::make_unique<PolygonMeshInstance>(std::static_pointer_cast<const PolygonMeshClass>(klass));
    else BUG("Unhandled drawable class type");
    return nullptr;
}

} // namespace
