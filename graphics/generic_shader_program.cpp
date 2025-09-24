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

#include "generic_shader_program.h"

#include "config.h"

#include <cstddef>

#include "base/logging.h"
#include "base/utility.h"
#include "base/format.h"
#include "graphics/enum.h"
#include "graphics/shader_source.h"
#include "graphics/generic_shader_program.h"

#include "renderpass.h"

namespace {
    enum class LightingFlags : uint32_t {
        EnableShadows = 0x1
    };
}
namespace gfx
{

std::string GenericShaderProgram::GetShaderId(const Material& material, const Material::Environment& env) const
{
    std::string id;
    id += "Pass:";
    id += base::ToString(env.render_pass);
    if (env.render_pass == RenderPass::ColorPass)
    {
        id += "Lit";
        id += TestFeature(ShadingFeatures::BasicLight) ? "yes": "no";
        id += "Fog";
        id += TestFeature(ShadingFeatures::BasicFog)   ? "yes" : "no";
        id += "Bloom:";
        id += TestFeature(OutputFeatures::WriteBloomTarget) ? "yes" : "no";
        id += "Color:";
        id += TestFeature(OutputFeatures::WriteColorTarget) ? "yes" : "no";
    }
    else if (env.render_pass == RenderPass::StencilPass)
    {
        // todo: see below do we need this??
        id += "Color:";
        id += TestFeature(OutputFeatures::WriteColorTarget) ? "yes" : "no";
    }
    id += "Material:";
    id += material.GetShaderId(env);
    return id;
}
std::string GenericShaderProgram::GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const
{
    std::string id;
    id += "Pass:";
    id += base::ToString(env.render_pass);
    if (env.render_pass == RenderPass::ColorPass)
    {
        id += "Lit:";
        id += TestFeature(ShadingFeatures::BasicLight) ? "yes" : "no";
        id += "Fog:";
        id += TestFeature(ShadingFeatures::BasicFog)   ? "yes" : "no";
    }
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
    source.AddPreprocessorDefinition("BASIC_LIGHTING_FLAGS_ENABLE_SHADOWS",    static_cast<unsigned>(LightingFlags::EnableShadows));

    source.AddPreprocessorDefinition("BASIC_FOG_MODE_LINEAR", static_cast<unsigned>(FogMode::Linear));
    source.AddPreprocessorDefinition("BASIC_FOG_MODE_EXP1", static_cast<unsigned>(FogMode::Exponential1));
    source.AddPreprocessorDefinition("BASIC_FOG_MODE_EXP2", static_cast<unsigned>(FogMode::Exponential2));

    if (env.render_pass == RenderPass::ColorPass)
    {
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
            source.AddPreprocessorDefinition("ENABLE_BLOOM_OUT");
        if (TestFeature(OutputFeatures::WriteColorTarget))
            source.AddPreprocessorDefinition("ENABLE_COLOR_OUT");

        source.AddPreprocessorDefinition("FRAGMENT_SHADER_MAIN_RENDER_PASS");
    }
    else if (env.render_pass == RenderPass::StencilPass)
    {
        // todo: probably should not need this?? if we're doing stencil
        // pass then we dont need to write to color target actually..
        if (TestFeature(OutputFeatures::WriteColorTarget))
            source.AddPreprocessorDefinition("ENABLE_COLOR_OUT");

        source.AddPreprocessorDefinition("FRAGMENT_SHADER_STENCIL_MASK_RENDER_PASS");
    }
    else if (env.render_pass == RenderPass::ShadowMapPass)
    {
        source.AddPreprocessorDefinition("FRAGMENT_SHADER_SHADOW_MAP_RENDER_PASS");
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

    if (env.render_pass == RenderPass::ColorPass)
    {
        source.AddPreprocessorDefinition("VERTEX_SHADER_MAIN_RENDER_PASS");

        if (TestFeature(ShadingFeatures::BasicLight))
            source.AddPreprocessorDefinition("ENABLE_BASIC_LIGHT");
        if (TestFeature(ShadingFeatures::BasicFog))
            source.AddPreprocessorDefinition("ENABLE_BASIC_FOG");
    }
    else if (env.render_pass == RenderPass::ShadowMapPass)
    {
        source.AddPreprocessorDefinition("VERTEX_SHADER_SHADOW_MAP_RENDER_PASS");
    }
    else if (env.render_pass == RenderPass::StencilPass)
    {
        source.AddPreprocessorDefinition("VERTEX_SHADER_STENCIL_MASK_RENDER_PASS");
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
    struct LightArrayUniformBlock {
        Light lights[MAX_LIGHTS];
        Vec3 camera_center;
        uint32_t light_count;
        uint32_t flags;
    };
#pragma pack(pop)
    static_assert((sizeof(Light) % 16) == 0);
    static_assert((offsetof(LightArrayUniformBlock, camera_center) % 16) == 0,
                  "incorrect std140 layout");

    uint32_t lighting_flags = 0;
    if (TestFeature(ShadingFeatures::BasicShadows))
        lighting_flags |= static_cast<uint32_t>(LightingFlags::EnableShadows);

    UniformBlockData<LightArrayUniformBlock> data;
    data.Resize(1);
    data[0].light_count   = light_count;
    data[0].camera_center = ToVec(mCameraCenter);
    data[0].flags         = lighting_flags;

    for (unsigned i=0; i<light_count; ++i)
    {
        const auto& light = mLights[i];
        data[0].lights[i].diffuse_color  = ToVec(light->diffuse_color);
        data[0].lights[i].ambient_color  = ToVec(light->ambient_color);
        data[0].lights[i].specular_color = ToVec(light->specular_color);
        data[0].lights[i].direction      = ToVec(light->view_direction);
        data[0].lights[i].position       = ToVec(light->view_position);
        data[0].lights[i].constant_attenuation  = light->constant_attenuation;
        data[0].lights[i].linear_attenuation    = light->linear_attenuation;
        data[0].lights[i].quadratic_attenuation = light->quadratic_attenuation;
        data[0].lights[i].spot_half_angle = light->spot_half_angle.ToRadians();
        data[0].lights[i].type = static_cast<int32_t>(light->type);
    }
    program.SetUniformBlock(UniformBlock("LightArray", std::move(data)));
}

void GenericShaderProgram::ApplyDynamicState(const Device &device, const Environment &env, ProgramState &program, Device::RasterState &state, void *user) const
{
#pragma pack(push, 1)
    struct LightTransform {
        Vec4 vector0;
        Vec4 vector1;
        Vec4 vector2;
        Vec4 vector3;
    };
    struct LightTransformUniformBlock {
        LightTransform transforms[MAX_LIGHTS];
    };
#pragma pack(pop)
    static_assert((sizeof(LightTransform) % 16) == 0);

    if (env.render_pass == RenderPass::ColorPass &&
        TestFeature(ShadingFeatures::BasicLight) &&
        TestFeature(ShadingFeatures::BasicShadows))
    {
        const auto light_count = std::min(MAX_LIGHTS, static_cast<int>(mLights.size()));

        UniformBlockData<LightTransformUniformBlock> data;
        data.Resize(1);

        const auto& view_to_world = glm::inverse(*env.view_matrix);

        const auto sampler_count = program.GetSamplerCount();
        const auto texture_count = program.GetTextureCount();

        // for each light create a transformation matrix that will transform a
        // a coordinate from view space to the light coordinate space for
        // shadow mapping.
        for (unsigned light_index=0; light_index<light_count; ++light_index)
        {
            const auto& light = mLights[light_index];
            if (light->type == BasicLightType::Ambient)
                continue;

            const auto& world_to_light = GetLightViewMatrix(light_index);
            const auto& light_projection = GetLightProjectionMatrix(light_index);
            const auto& view_to_light = world_to_light * view_to_world;
            const auto& light_to_map  =  light_projection * view_to_light;
            data[0].transforms[light_index].vector0 = ToVec(light_to_map[0]);
            data[0].transforms[light_index].vector1 = ToVec(light_to_map[1]);
            data[0].transforms[light_index].vector2 = ToVec(light_to_map[2]);
            data[0].transforms[light_index].vector3 = ToVec(light_to_map[3]);
        }

        const auto sampler_index = sampler_count + 0;
        const auto* shadow_map = GetLightShadowMap(device);
        ASSERT(shadow_map);

        program.SetUniformBlock(UniformBlock("LightTransformArray", std::move(data)));
        program.SetTexture("kShadowMap", sampler_index, *shadow_map);
        program.SetTextureCount(texture_count + 1);
    }
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

glm::mat4 GenericShaderProgram::GetLightViewMatrix(unsigned light_index) const
{
    const auto& light = *mLights[light_index];

    // remember that when using glm::lookAt if the view direction (the direction
    // the camera is pointed at) is collinear with the "up" vector the math breaks
    // so we need to choose a different basis vector for the lookAt to use in order
    // to compute its cross product.
    glm::vec3 reference = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(light.world_direction, reference)) > 0.99f)
        reference = glm::vec3(1.0f, 0.0f, 0.0f); // go with god.. no I mean with x axis

    const auto world_to_light = glm::lookAt(light.world_position,
        light.world_position + light.world_direction * 1.0f, reference);
    return world_to_light;
}

glm::mat4 GenericShaderProgram::GetLightProjectionMatrix(unsigned light_index) const
{
    const auto light = mLights[light_index];
    const auto near_plane = light->near_plane;
    const auto far_plane  = light->far_plane;
    const auto aspect = float(mShadowMapWidth) /  float(mShadowMapHeight);

    if (light->type == BasicLightType::Directional)
        return glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
    else if (light->type == BasicLightType::Point)
        return glm::perspective(glm::radians(65.0f),  aspect, near_plane, far_plane);
    else if (light->type == BasicLightType::Spot)
        return glm::perspective(glm::radians(65.0f),  aspect, near_plane, far_plane);
    else BUG("Bug on light projection.");
}

GenericShaderProgram::LightProjectionType GenericShaderProgram::GetLightProjectionType(unsigned light_index) const
{
    const auto& light = mLights[light_index];
    if (light->type == BasicLightType::Ambient)
        return LightProjectionType::Invalid;
    else if (light->type == BasicLightType::Directional)
        return LightProjectionType::Orthographic;
    else if (light->type == BasicLightType::Point)
        return LightProjectionType::Perspective;
    else if (light->type == BasicLightType::Spot)
        return LightProjectionType::Perspective;
    BUG("Bug on light projection type.");
}

const Texture* GenericShaderProgram::GetLightShadowMap(const Device& device) const
{
    return device.FindTexture(mRendererName + "/ShadowMap");
}

float GenericShaderProgram::GetLightProjectionNearPlane(unsigned light_index) const
{
    return mLights[light_index]->near_plane;
}
float GenericShaderProgram::GetLightProjectionFarPlane(unsigned light_index) const
{
    return mLights[light_index]->far_plane;
}

} // namespace
