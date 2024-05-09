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

#include "graphics/shaderprogram.h"
#include "graphics/shadersource.h"
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
ShaderSource ShaderProgram::GetShader(const Material& material, const Material::Environment& env, const Device& device) const
{
    ShaderSource source;
    source.SetType(ShaderSource::Type::Fragment);
    source.SetVersion(ShaderSource::Version::GLSL_100);
    source.SetPrecision(ShaderSource::Precision::High);

    source.AddSource(R"(
struct FS_OUT {
   vec4 color;
} fs_out;

void FragmentShaderMain();

float sRGB_encode(float value)
{
   return value <= 0.0031308
       ? value * 12.92
       : pow(value, 1.0/2.4) * 1.055 - 0.055;
}
vec4 sRGB_encode(vec4 color)
{
   vec4 ret;
   ret.r = sRGB_encode(color.r);
   ret.g = sRGB_encode(color.g);
   ret.b = sRGB_encode(color.b);
   ret.a = color.a; // alpha is always linear
   return ret;
}

void main() {
  FragmentShaderMain();

  gl_FragColor = sRGB_encode(fs_out.color);
}
)");

    source.Merge(material.GetShader(env, device));
    return source;
}
ShaderSource ShaderProgram::GetShader(const Drawable& drawable, const Drawable::Environment& env, const Device& device) const
{
    // careful here, WebGL shits itself if the source is concatenated together
    // so that the vertex source (with varyings) comes after the main.
    ShaderSource source;
    source.SetType(ShaderSource::Type::Vertex);
    source.SetVersion(ShaderSource::Version::GLSL_100);

    source.AddSource(R"(
struct VS_OUT {
   vec4 position;
} vs_out;

void VertexShaderMain();

)");

    source.Merge(drawable.GetShader(env, device));
    source.AddSource(R"(

void main() {
   VertexShaderMain();
}
)");

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
