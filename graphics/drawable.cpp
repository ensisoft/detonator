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
#include "base/format.h"
#include "base/hash.h"
#include "base/logging.h"
#include "graphics/drawable.h"
#include "graphics/shader_code.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"

namespace gfx {

DrawCategory DrawableClass::MapDrawableCategory(Type type) noexcept
{
    if (type == Type::ParticleEngine)
        return DrawCategory::Particles;
    else if (type == Type::TileBatch)
        return DrawCategory::TileBatch;
    else if (type == Type::SimpleShape ||
             type == Type::Polygon ||
             type == Type::DebugDrawable ||
             type == Type::EffectsDrawable ||
             type == Type::LineBatch3D ||
             type == Type::LineBatch2D ||
             type == Type::GuideGrid ||
             type == Type::Other)
        return DrawCategory::Basic;
    BUG("Bug on draw category mapping based on drawable type.");
    return DrawCategory::Basic;
}

// static
ShaderSource Drawable::CreateShader(const Environment& environment, const Device& device, Shader shader)
{
    ShaderSource source;
    source.SetType(ShaderSource::Type::Vertex);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddDebugInfo("Instancing", environment.use_instancing ? "yes" : "no");
    if (environment.use_instancing)
        source.AddPreprocessorDefinition("INSTANCED_DRAW");

    if  (shader == Shader::Simple2D)
    {
        if (environment.mesh_type == MeshType::ShardedEffectMesh)
        {
            source.LoadRawSource(glsl::vertex_2d_effect);
            source.AddShaderSourceUri("shaders/vertex_2d_effect.glsl");
            source.AddPreprocessorDefinition("VERTEX_HAS_SHARD_INDEX_ATTRIBUTE");
            source.AddPreprocessorDefinition("APPLY_SHARD_MESH_EFFECT");
            source.AddPreprocessorDefinition("MESH_EFFECT_TYPE_SHARD_EXPLOSION", static_cast<int>(MeshEffectType::ShardedMeshExplosion));
        }
        else if (environment.mesh_type == MeshType::NormalRenderMesh)
        {
            // nothing to do here for now
        }
        else BUG("Bug no render mesh type.");

        source.LoadRawSource(glsl::vertex_base);
        source.LoadRawSource(glsl::vertex_2d_simple);
        source.AddShaderSourceUri("shaders/vertex_base.glsl");
        source.AddShaderSourceUri("shaders/vertex_2d_simple_shader.glsl");
        source.AddDebugInfo("Mesh", base::ToString(environment.mesh_type));
    }
    else if (shader == Shader::Simple3D)
    {
        source.LoadRawSource(glsl::vertex_base);
        source.LoadRawSource(glsl::vertex_3d_simple);
        source.AddShaderSourceUri("shaders/vertex_base.glsl");
        source.AddShaderSourceUri("shaders/vertex_3d_simple_shader.glsl");
    }
    else if (shader == Shader::Model3D)
    {
        source.LoadRawSource(glsl::vertex_base);
        source.LoadRawSource(glsl::vertex_3d_model);
        source.AddShaderSourceUri("shaders/vertex_base.glsl");
        source.AddShaderSourceUri("shaders/vertex_3d_model_shader.glsl");
    }
    else if (shader == Shader::Perceptual3D)
    {
        source.LoadRawSource(glsl::vertex_base);
        source.LoadRawSource(glsl::vertex_3d_perceptual);
        source.AddShaderSourceUri("shaders/vertex_base.glsl");
        source.AddShaderSourceUri("shaders/vertex_perceptual_3d_shader.glsl");
    }
    else BUG("Bug on shape type.");

    return source;
}

// static
std::string Drawable::GetShaderId(const Environment& env, Shader shader)
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, env.use_instancing);
    hash = base::hash_combine(hash, env.mesh_type);
    hash = base::hash_combine(hash, shader);
    return std::to_string(hash);
}

//static
std::string Drawable::GetShaderName(const Environment& env, Shader shader)
{
    return base::FormatString("%1 Vertex Shader", shader);
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
