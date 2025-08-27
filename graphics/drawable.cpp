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

#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"


namespace gfx {

DrawCategory DrawableClass::MapDrawableCategory(Type type)
{
    if (type == Type::ParticleEngine)
        return DrawCategory::Particles;
    else if (type == Type::TileBatch)
        return DrawCategory::TileBatch;
    else if (type == Type::SimpleShape ||
             type == Type::Polygon ||
             type == Type::DebugDrawable ||
             type == Type::LineBatch3D ||
             type == Type::LineBatch2D ||
             type == Type::GuideGrid)
        return DrawCategory::Basic;
    BUG("Bug on draw category mapping based on drawable type.");
    return DrawCategory::Basic;
}

bool Is3DShape(const Drawable& drawable) noexcept
{
    const auto type = drawable.GetType();
    if (type == Drawable::Type::Polygon)
    {
        if (const auto* instance = dynamic_cast<const PolygonMeshInstance*>(&drawable))
        {
            const auto mesh = instance->GetRenderMeshType();
            if (mesh == PolygonMeshInstance::RenderMeshType::Simple3D ||
                mesh == PolygonMeshInstance::RenderMeshType::Model3D)
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
            const auto mesh = polygon->GetRenderMeshType();
            if (mesh == PolygonMeshClass::RenderMeshType::Simple3D ||
                mesh == PolygonMeshClass::RenderMeshType::Model3D)
                return true;
        }
    }
    if (type != DrawableClass::Type::SimpleShape)
        return false;
    if (const auto* simple = dynamic_cast<const SimpleShapeClass*>(&klass))
        return Is3DShape(simple->GetShapeType());
    else BUG("Unknown drawable shape type");
}

bool Is2DShape(const Drawable& drawable) noexcept
{
    return !Is3DShape(drawable);
}

bool Is2DShape(const DrawableClass& klass) noexcept
{
    return !Is3DShape(klass);
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
