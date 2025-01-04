// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "base/logging.h"
#include "graphics/shaderprogram.h"
#include "graphics/shadersource.h"
#include "graphics/device.h"
#include "graphics/material_class.h"

namespace gfx {

std::string ShaderProgram::GetShaderId(const Material& material, const Material::Environment& env) const
{
    return material.GetShaderId(env);
}
std::string ShaderProgram::GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const
{
    return drawable.GetShaderId(env);
}
ShaderSource ShaderProgram::GetShader(const Material& material, const Material::Environment& env, const Device& device) const
{
    static const char* fragment_main = {
#include "shaders/generic_main_fragment_shader.glsl"
    };
    static const char* utility_func = {
#include "shaders/srgb_functions.glsl"
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

    source.LoadRawSource(utility_func);
    source.LoadRawSource(fragment_main);
    source.AddShaderSourceUri("shaders/srgb_functions.glsl");
    source.AddShaderSourceUri("shaders/generic_main_fragment_shader.glsl");
    return source;
}
ShaderSource ShaderProgram::GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const
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
    source.LoadRawSource(vertex_main);
    source.AddShaderSourceUri("shaders/generic_main_vertex_shader.glsl");
    return source;
}
std::string ShaderProgram::GetShaderName(const Material& material, const Material::Environment& env) const
{
    return material.GetShaderName(env);
}
std::string ShaderProgram::GetShaderName(const Drawable& drawable, const Drawable::Environment& env) const
{
    return drawable.GetShaderName(env);
}

} // namespace
