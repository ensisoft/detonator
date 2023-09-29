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

#include "graphics/shaderpass.h"
#include "graphics/device.h"

namespace gfx {

std::string ShaderProgram::GetShaderId(const Material& material, const Material::Environment& env) const
{
    return material.GetShaderId(env);
}
std::string ShaderProgram::GetShaderId(const Drawable& drawable, const Drawable::Environment& env) const
{
    return drawable.GetShaderId(env);
}
std::string ShaderProgram::GetShader(const Material& material, const Material::Environment& env, const Device& device) const
{
    std::string source(R"(
#version 100
precision highp float;

struct FS_OUT {
   vec4 color;
} fs_out;

void FragmentShaderMain();

void main() {
  FragmentShaderMain();

  gl_FragColor = fs_out.color;
})");
    source.append(material.GetShader(env, device));
    return source;
}
std::string ShaderProgram::GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const
{
    // careful here, WebGL shits itself if the source is concatenated together
    // so that the vertex source (with varyings) comes after the main.

    std::string source(R"(
#version 100

struct VS_OUT {
   vec4 position;
} vs_out;

void VertexShaderMain();

)");

source.append(drawable.GetShader(env, device));
source.append(R"(

void main() {
   VertexShaderMain();
})");

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
