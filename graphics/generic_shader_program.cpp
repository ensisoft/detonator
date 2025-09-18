// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <cstddef>

#include "base/logging.h"
#include "base/utility.h"
#include "graphics/enum.h"
#include "graphics/shader_source.h"
#include "graphics/generic_shader_program.h"

namespace gfx
{

std::string GenericShaderProgram::GetShaderId(const Material& material, const Material::Environment& env) const
{
    std::string id;
    id += TestFeature(ShadingFeatures::BasicLight) ? "Lit:yes" : "Lit:no";
    id += TestFeature(ShadingFeatures::BasicFog)   ? "Fog:Yes" : "Fog:no";
    id += TestFeature(OutputFeatures::WriteBloomTarget) ? "Bloom:yes" : "Bloom:no";
    id += TestFeature(OutputFeatures::WriteColorTarget) ? "Color:yes" : "Color:no";
    id += "Material:";
    id += material.GetShaderId(env);
    return id;
}
std::string GenericShaderProgram::GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const
{
    std::string id;
    id += TestFeature(ShadingFeatures::BasicLight) ? "Lit:yes" : "Lit:no";
    id += TestFeature(ShadingFeatures::BasicFog)   ? "Fog:yes" : "Fog:no";
    id += "Drawable:";
    id += drawable.GetShaderId(env);
    return id;
}
ShaderSource GenericShaderProgram::GetShader(const Material& material, const Material::Environment& env, const Device& device) const
{
    static const char* fragment_main = {
#include "shaders/generic_main_fragment_shader.glsl"
    };
    static const char* utility_func = {
#include "shaders/srgb_functions.glsl"
    };
    static const char* basic_light = {
#include "shaders/basic_light.glsl"
    };
    static const char* basic_fog = {
#include "shaders/basic_fog.glsl"
    };

    auto source = material.GetShader(env, device);
    if (source.GetType() != ShaderSource::Type::Fragment)
    {
        ERROR("Non supported GLSL shader type. Type must be 'fragment'.  [shader='%1']", source.GetShaderName());
        return {};
    }
    if (source.GetVersion() != ShaderSource::Version::GLSL_300)
    {
        ERROR("Non supported GLSL version. Version must be 300 es. [shader='%1']", source.GetShaderName());
        return {};
    }
    if (source.GetPrecision() == ShaderSource::Precision::NotSet)
        source.SetPrecision(ShaderSource::Precision::High);

    if (!source.HasShaderBlock("PI", ShaderSource::ShaderBlockType::PreprocessorDefine))
        source.AddPreprocessorDefinition("PI", "3.1415926");
    if (!source.HasShaderBlock("E", ShaderSource::ShaderBlockType::PreprocessorDefine))
        source.AddPreprocessorDefinition("E", "2.71828182");
    if (!source.HasShaderBlock("MATERIAL_FLAGS_ENABLE_BLOOM", ShaderSource::ShaderBlockType::PreprocessorDefine))
        source.AddPreprocessorDefinition("MATERIAL_FLAGS_ENABLE_BLOOM", static_cast<unsigned>(MaterialFlags::EnableBloom));

    source.AddPreprocessorDefinition("BASIC_LIGHT_MAX_LIGHTS", static_cast<unsigned>(MAX_LIGHTS));
    source.AddPreprocessorDefinition("BASIC_LIGHT_TYPE_AMBIENT",     static_cast<unsigned>(LightType::Ambient));
    source.AddPreprocessorDefinition("BASIC_LIGHT_TYPE_DIRECTIONAL", static_cast<unsigned>(LightType::Directional));
    source.AddPreprocessorDefinition("BASIC_LIGHT_TYPE_SPOT",        static_cast<unsigned>(LightType::Spot));
    source.AddPreprocessorDefinition("BASIC_LIGHT_TYPE_POINT",       static_cast<unsigned>(LightType::Point));

    source.AddPreprocessorDefinition("BASIC_FOG_MODE_LINEAR", static_cast<unsigned>(FogMode::Linear));
    source.AddPreprocessorDefinition("BASIC_FOG_MODE_EXP1", static_cast<unsigned>(FogMode::Exponential1));
    source.AddPreprocessorDefinition("BASIC_FOG_MODE_EXP2", static_cast<unsigned>(FogMode::Exponential2));

    if (TestFeature(ShadingFeatures::BasicLight))
    {
        source.AddPreprocessorDefinition("ENABLE_BASIC_LIGHT");
        source.LoadRawSource(basic_light);
        source.AddShaderSourceUri("shaders/basic_light.glsl");
    }
    if (TestFeature(ShadingFeatures::BasicFog))
    {
        source.AddPreprocessorDefinition("ENABLE_BASIC_FOG");
        source.LoadRawSource(basic_fog);
        source.AddShaderSourceUri("shaders/basic_fog.glsl");
    }

    if (TestFeature(OutputFeatures::WriteBloomTarget))
    {
        source.AddPreprocessorDefinition("ENABLE_BLOOM_OUT");
    }
    if (TestFeature(OutputFeatures::WriteColorTarget))
    {
        source.AddPreprocessorDefinition("ENABLE_COLOR_OUT");
    }

    source.LoadRawSource(utility_func);
    source.LoadRawSource(fragment_main);
    source.AddShaderSourceUri("shaders/srgb_functions.glsl");
    source.AddShaderSourceUri("shaders/generic_main_fragment_shader.glsl");
    return source;
}
ShaderSource GenericShaderProgram::GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const
{
    // careful here, WebGL shits itself if the source is concatenated together
    // so that the vertex source (with varyings) comes after the main.
    static const char* vertex_main = {
#include "shaders/generic_main_vertex_shader.glsl"
    };

    auto source = drawable.GetShader(env, device);
    if (source.GetType() != ShaderSource::Type::Vertex)
    {
        ERROR("Non supported GLSL shader type. Type must be 'vertex'. [shader='%1']", source.GetShaderName());
        return {};
    }
    if (source.GetVersion() != ShaderSource::Version::GLSL_300)
    {
        ERROR("Non supported GLSL version. Version must be 300 es. [shader='%1']", source.GetShaderName());
        return {};
    }
    if (TestFeature(ShadingFeatures::BasicLight))
    {
        source.AddPreprocessorDefinition("ENABLE_BASIC_LIGHT");
    }
    if (TestFeature(ShadingFeatures::BasicFog))
    {
        source.AddPreprocessorDefinition("ENABLE_BASIC_FOG");
    }

    source.LoadRawSource(vertex_main);
    source.AddShaderSourceUri("shaders/generic_main_vertex_shader.glsl");
    source.AddPreprocessorDefinition("DRAWABLE_FLAGS_FLIP_UV_VERTICALLY", static_cast<unsigned>(DrawableFlags::Flip_UV_Vertically));
    source.AddPreprocessorDefinition("DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY", static_cast<unsigned>(DrawableFlags::Flip_UV_Horizontally));
    return source;
}

void GenericShaderProgram::ApplyDynamicState(const Device& device, ProgramState& program) const
{
    if (TestFeature(ShadingFeatures::BasicLight))
        ApplyLightState(device, program);
    if (TestFeature(ShadingFeatures::BasicFog))
        ApplyFogState(device, program);

    if (TestFeature(OutputFeatures::WriteBloomTarget))
    {
        program.SetUniform("kBloomThreshold", mBloomThreshold);
        program.SetUniform("kBloomColor", mBloomColor);
    }
}

void GenericShaderProgram::ApplyLightState(const Device& device, ProgramState& program) const
{
    const auto light_count = std::min(MAX_LIGHTS, static_cast<int>(mLights.size()));

    // this type and the binary layout must be reflected in the
    // GLSL source !
#pragma pack(push, 1)
    struct Light {
        Vec4 diffuse_color;
        Vec4 ambient_color;
        Vec4 specular_color;
        Vec3 direction;
        float spot_half_angle;
        Vec3 position;
        float constant_attenuation;
        float linear_attenuation;
        float quadratic_attenuation;
        uint32_t type;
        float padding[1];
    };
    static_assert((sizeof(Light) % 16) == 0);

    struct LightArrayUniformBlock {
        Light lights[MAX_LIGHTS];
        Vec3 camera_center;
        uint32_t light_count;
        float padding1_[1];
    };

    static_assert((offsetof(LightArrayUniformBlock, camera_center) % 16) == 0,
                  "incorrect std140 layout");

#pragma pack(pop)

    UniformBlockData<LightArrayUniformBlock> data;
    data.Resize(1);
    data[0].light_count   = light_count;
    data[0].camera_center = ToVec(mCameraCenter);

    for (unsigned i=0; i<light_count; ++i)
    {
        const auto& light = mLights[i];
        data[0].lights[i].diffuse_color  = ToVec(light->diffuse_color);
        data[0].lights[i].ambient_color  = ToVec(light->ambient_color);
        data[0].lights[i].specular_color = ToVec(light->specular_color);
        data[0].lights[i].direction = ToVec(glm::normalize(light->direction));
        data[0].lights[i].position  = ToVec(light->position);
        data[0].lights[i].constant_attenuation  = light->constant_attenuation;
        data[0].lights[i].linear_attenuation    = light->linear_attenuation;
        data[0].lights[i].quadratic_attenuation = light->quadratic_attenuation;
        data[0].lights[i].spot_half_angle = light->spot_half_angle.ToRadians();
        data[0].lights[i].type = static_cast<int32_t>(light->type);
    }
    program.SetUniformBlock(UniformBlock("LightArray", std::move(data)));
}

void GenericShaderProgram::ApplyFogState(const Device& device, ProgramState& program) const
{
#pragma pack(push, 1)
    struct Fog {
        Vec4 color;
        Vec3 camera;
        float density;
        float start_depth;
        float end_depth;
        uint32_t mode;
    };
#pragma pack(pop)

    UniformBlockData<Fog> data;
    data.Resize(1);
    data[0].color  = ToVec(mFog.color);
    // actually we don't need the camera position, easier to say
    // the camera is at 0.0, 0.0, 0.0, the world moves around. (as it is
    // with the view transform).
    data[0].camera =  Vec3{0.0f, 0.0f, 0.0f}; //ToVec(mCameraCenter);
    data[0].density = mFog.density;
    data[0].start_depth = mFog.start_depth;
    data[0].end_depth   = mFog.end_depth;
    data[0].mode  = static_cast<uint32_t>(mFog.mode);
    program.SetUniformBlock(UniformBlock("FogData", std::move(data)));

}

} // namespace