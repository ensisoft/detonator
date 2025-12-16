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

#include <iostream>

#include "base/test_minimal.h"
#include "graphics/shader_source.h"

std::string CleanStr(const std::string& str)
{
    std::string ret;
    for (auto c : str)
    {
        if (c == ' ' || c == '\r' || c == '\n')
            continue;
        ret += c;
    }
    return ret;
}

void unit_test_raw_source_es100()
{
    TEST_CASE(test::Type::Feature)

    using ddt = gfx::ShaderSource::ShaderDataDeclarationType;
    using dt = gfx::ShaderSource::ShaderDataType;

    // vertex shader
    {
        const auto& ret = gfx::ShaderSource::FromRawSource(R"(
#version 100

attribute vec2 aVec2;
attribute vec3 aVec3;
attribute vec4 aVec4;

varying vec2 vVec2;
varying vec3 vVec3;
varying vec4 vVec4;

void main() {
  gl_Position = vec4(1.0);
}
        )", gfx::ShaderSource::Type::Vertex);
        TEST_REQUIRE(ret.GetVersion() == gfx::ShaderSource::Version::GLSL_100);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->decl_type == ddt::Varying);

        const auto& sauce = ret.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "attribute vec2 aVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "attribute vec3 aVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "attribute vec4 aVec4;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec2 vVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec3 vVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec4 vVec4;"));
        TEST_REQUIRE(base::Contains(sauce, R"(void main() {
  gl_Position = vec4(1.0);
})"));

    }

    // fragment shader
    {
        const auto& ret = gfx::ShaderSource::FromRawSource(R"(
#version 100

#define PI 3.145
#define MY_SHADER_FOO

uniform int kInt;
uniform float kFloat;
uniform vec2 kVec2;
uniform vec3 kVec3;
uniform vec4 kVec4;

uniform mat2 kMat2;
uniform mat3 kMat3;
uniform mat4 kMat4;

uniform sampler2D kSampler;

varying vec2 vVec2;
varying vec3 vVec3;
varying vec4 vVec4;

void main() {
  gl_FragColor = vec4(1.0);
}
        )", gfx::ShaderSource::Type::Fragment);

        TEST_REQUIRE(ret.GetVersion() == gfx::ShaderSource::Version::GLSL_100);
        TEST_REQUIRE(ret.FindShaderBlock("PI")->type == gfx::ShaderSource::ShaderBlockType::PreprocessorDefine);
        TEST_REQUIRE(ret.FindShaderBlock("PI")->data == "#define PI 3.145");
        TEST_REQUIRE(ret.FindShaderBlock("MY_SHADER_FOO")->type == gfx::ShaderSource::ShaderBlockType::PreprocessorDefine);
        TEST_REQUIRE(ret.FindShaderBlock("MY_SHADER_FOO")->data == "#define MY_SHADER_FOO");
        TEST_REQUIRE(ret.FindDataDeclaration("kInt")->data_type == dt::Int);
        TEST_REQUIRE(ret.FindDataDeclaration("kInt")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec2")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec3")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec4")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat2")->data_type == dt::Mat2f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat2")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat3")->data_type == dt::Mat3f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat3")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat4")->data_type == dt::Mat4f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat4")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kSampler")->data_type == dt::Sampler2D);
        TEST_REQUIRE(ret.FindDataDeclaration("kSampler")->decl_type == ddt::Uniform);

        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->decl_type == ddt::Varying);

        const auto& sauce = ret.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "#version 100"));
        TEST_REQUIRE(base::Contains(sauce, "#define PI 3.145"));
        TEST_REQUIRE(base::Contains(sauce, "#define MY_SHADER_FOO"));
        TEST_REQUIRE(base::Contains(sauce, "uniform int kInt;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform float kFloat;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec3 kVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kVec4;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat2 kMat2;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat3 kMat3;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat4 kMat4;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform sampler2D kSampler;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec2 vVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec3 vVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "varying vec4 vVec4;"));
        TEST_REQUIRE(base::Contains(sauce, R"(void main() {
  gl_FragColor = vec4(1.0);
}
)"));

    }
}

void unit_test_raw_source_es300()
{
    TEST_CASE(test::Type::Feature)

    using ddt = gfx::ShaderSource::ShaderDataDeclarationType;
    using dt = gfx::ShaderSource::ShaderDataType;

    // vertex shader
    {
        const auto& ret = gfx::ShaderSource::FromRawSource(R"(
#version 300 es

in vec2 aVec2;
in vec3 aVec3;
in vec4 aVec4;

out vec2 vVec2;
out vec3 vVec3;
out vec4 vVec4;

void main() {
  gl_Position = vec4(1.0);
}
        )", gfx::ShaderSource::Type::Vertex);

        TEST_REQUIRE(ret.GetVersion() == gfx::ShaderSource::Version::GLSL_300);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->decl_type == ddt::Attribute);

        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->decl_type == ddt::Varying);

        const auto& sauce = ret.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "#version 300 es"));
        TEST_REQUIRE(base::Contains(sauce, "in vec2 aVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec3 aVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec4 aVec4;"));
        TEST_REQUIRE(base::Contains(sauce, "out vec2 vVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "out vec3 vVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "out vec4 vVec4;"));
        TEST_REQUIRE(base::Contains(sauce, R"(void main() {
  gl_Position = vec4(1.0);
}
)"));
    }

    // fragment shader
    {
        const auto& ret = gfx::ShaderSource::FromRawSource(R"(
#version 300 es

#define PI 3.145
#define MY_SHADER_FOO

uniform int kInt;
uniform float kFloat;
uniform vec2 kVec2;
uniform vec3 kVec3;
uniform vec4 kVec4;

uniform mat2 kMat2;
uniform mat3 kMat3;
uniform mat4 kMat4;

uniform sampler2D kSampler;


in vec2 vVec2;
in vec3 vVec3;
in vec4 vVec4;

void main() {
  gl_FragColor = vec4(1.0);
}
        )", gfx::ShaderSource::Type::Fragment);

        TEST_REQUIRE(ret.GetVersion() == gfx::ShaderSource::Version::GLSL_300);
        TEST_REQUIRE(ret.FindShaderBlock("PI")->type == gfx::ShaderSource::ShaderBlockType::PreprocessorDefine);
        TEST_REQUIRE(ret.FindShaderBlock("PI")->data == "#define PI 3.145");
        TEST_REQUIRE(ret.FindShaderBlock("MY_SHADER_FOO")->type == gfx::ShaderSource::ShaderBlockType::PreprocessorDefine);
        TEST_REQUIRE(ret.FindShaderBlock("MY_SHADER_FOO")->data == "#define MY_SHADER_FOO");

        TEST_REQUIRE(ret.FindDataDeclaration("kInt")->data_type == dt::Int);
        TEST_REQUIRE(ret.FindDataDeclaration("kInt")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec2")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec3")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("kVec4")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat2")->data_type == dt::Mat2f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat2")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat3")->data_type == dt::Mat3f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat3")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat4")->data_type == dt::Mat4f);
        TEST_REQUIRE(ret.FindDataDeclaration("kMat4")->decl_type == ddt::Uniform);
        TEST_REQUIRE(ret.FindDataDeclaration("kSampler")->data_type == dt::Sampler2D);
        TEST_REQUIRE(ret.FindDataDeclaration("kSampler")->decl_type == ddt::Uniform);

        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec2")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec3")->decl_type == ddt::Varying);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("vVec4")->decl_type == ddt::Varying);

        const auto& sauce = ret.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "#version 300 es"));
        TEST_REQUIRE(base::Contains(sauce, "#define PI 3.145"));
        TEST_REQUIRE(base::Contains(sauce, "#define MY_SHADER_FOO"));
        TEST_REQUIRE(base::Contains(sauce, "uniform int kInt;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform float kFloat;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec3 kVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kVec4;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat2 kMat2;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat3 kMat3;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform mat4 kMat4;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform sampler2D kSampler;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec2 vVec2;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec3 vVec3;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec4 vVec4;"));
        TEST_REQUIRE(base::Contains(sauce, R"(void main() {
  gl_FragColor = vec4(1.0);
}
)"));
    }

}

void unit_test_generation()
{
    TEST_CASE(test::Type::Feature)

    {
        gfx::ShaderSource source;
        source.SetPrecision(gfx::ShaderSource::Precision::High);
        source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
        source.SetType(gfx::ShaderSource::Type::Fragment);
        source.AddPreprocessorDefinition("PI", 3.143f);
        source.AddConstant("kFoobar", 123);
        source.AddVarying("vColor", gfx::ShaderSource::VaryingType::Vec4f);

        source.AddSource(R"(
void main() {
    gl_FragColor = vec4(1.0)
}
    )");

        const auto& sauce = source.GetSource();
        //std::cout << sauce;
        TEST_REQUIRE(base::Contains(sauce, "#version 300 es"));
        TEST_REQUIRE(base::Contains(sauce, "precision highp float;"));
        TEST_REQUIRE(base::Contains(sauce, "const int kFoobar = 123;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec4 vColor;"));
    }

    {
        gfx::ShaderSource source;
        source.SetPrecision(gfx::ShaderSource::Precision::High);
        source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
        source.SetType(gfx::ShaderSource::Type::Vertex);
        source.AddPreprocessorDefinition("PI", 3.143f);
        source.AddConstant("kFoobar", 123);
        source.AddAttribute("aPosition", gfx::ShaderSource::AttributeType::Vec4f);
        source.AddVarying("vColor", gfx::ShaderSource::VaryingType::Vec4f);
        source.AddSource(R"(
void main() {
    gl_Position = vec4(1.0);
}
        )");

        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "#version 300 es"));
        TEST_REQUIRE(base::Contains(sauce, "const int kFoobar = 123;"));
        TEST_REQUIRE(base::Contains(sauce, "in vec4 aPosition;"));
        TEST_REQUIRE(base::Contains(sauce, "out vec4 vColor;"));
    }

}

void unit_test_raw_source_combine()
{
    TEST_CASE(test::Type::Feature)

    gfx::ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Fragment);

    source.LoadRawSource(R"(
float SomeFunction() {
   return 1.0;
}
)");

    source.LoadRawSource(R"(
#version 300 es

precision highp float;

// The incoming color value.
uniform vec4 kBaseColor;

// Incoming per particle alpha value.
in float vParticleAlpha;

void FragmentShaderMain() {

    vec4 color = kBaseColor;

    // modulate by alpha
    color.a *= vParticleAlpha;

    // out value.
    fs_out.color = color;
}

)");
    auto src = source.GetSource();
    TEST_REQUIRE(CleanStr(src) ==
CleanStr(R"(#version 300 es
precision highp float;
uniform vec4 kBaseColor;
in float vParticleAlpha;

float SomeFunction() {
   return 1.0;
}
void FragmentShaderMain() {
    vec4 color = kBaseColor;

    color.a *= vParticleAlpha;

    fs_out.color = color;
}

)"));

}


void unit_test_conditional_data()
{
    TEST_CASE(test::Type::Feature)

    gfx::ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Vertex);

    source.LoadRawSource(R"(

#version 300 es

// @attributes

in vec4 kBleh;

#ifdef FOOBAR
  in vec4 kFoobar;
#endif

float SomeFunction() {
   return 1.0;
}
)");
    auto src = source.GetSource();
    //std::cout << src;
    TEST_REQUIRE(CleanStr(src) == CleanStr(R"(#version 300 es
in vec4 kBleh;
#ifdef FOOBAR
  in vec4 kFoobar;
#endif

float SomeFunction() {
   return 1.0;
}
)"));

}

void unit_test_uniform_blocks()
{
    TEST_CASE(test::Type::Feature)
gfx::ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Fragment);
    TEST_REQUIRE(source.LoadRawSource(R"(
#version 300 es

// @types
struct Light {
  vec4 color;
};

// @uniforms
layout (std140) uniform LightArray {
  Light lights[10];
};

uniform highp sampler2DArray kShadowMap;

)"));
    const auto& blocks = source.ListImportantUniformBlocks();
    TEST_REQUIRE(blocks.size() == 2);
    TEST_REQUIRE(blocks[0].data_decl.has_value());
    TEST_REQUIRE(blocks[1].data_decl.has_value());
    TEST_REQUIRE(blocks[0].data_decl->decl_type == gfx::ShaderSource::ShaderDataDeclarationType::UniformBlock);
    TEST_REQUIRE(blocks[0].data_decl->data_type == gfx::ShaderSource::ShaderDataType::UserDefinedStruct);
    TEST_REQUIRE(blocks[0].data_decl->name == "LightArray");
    TEST_REQUIRE(blocks[1].data_decl->decl_type == gfx::ShaderSource::ShaderDataDeclarationType::Uniform);
    TEST_REQUIRE(blocks[1].data_decl->data_type == gfx::ShaderSource::ShaderDataType::Sampler2DArray);
    TEST_REQUIRE(blocks[1].data_decl->name == "kShadowMap");

}

void unit_test_token_replacement()
{
    TEST_CASE(test::Type::Feature)
    gfx::ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Vertex);
    source.ReplaceToken("MY_TOKEN", "vec4 x = vec4(1.0);");
    source.LoadRawSource(R"(
#version 300 es

// @attributes
in vec4 kBleh;

#ifdef FOOBAR
  in vec4 kFoobar;
#endif

float SomeFunction() {
   return 1.0;

  // $MY_TOKEN
}
)");
    auto src = source.GetSource();
    //std::cout << src;
    TEST_REQUIRE(CleanStr(src) == CleanStr(R"(#version 300 es
in vec4 kBleh;
#ifdef FOOBAR
  in vec4 kFoobar;
#endif

float SomeFunction() {
   return 1.0;
vec4 x = vec4(1.0);
}
)"));
}

void unit_test_sampler2DArray_bug()
{
    TEST_CASE(test::Type::Feature)

    gfx::ShaderSource source;
    source.SetType(gfx::ShaderSource::Type::Fragment);
    source.LoadRawSource(R"(
#version 300 es
uniform highp sampler2DArray kSamplerArray;
void FragmentShaderMain() {
}
)");

    TEST_REQUIRE(source.HasUniform("kSamplerArray"));
    TEST_REQUIRE(source.FindShaderBlock("kSamplerArray")->data == "uniform highp sampler2DArray kSamplerArray;");

    const auto& src = source.GetSource();
    TEST_REQUIRE(CleanStr(src) == CleanStr(R"(
#version 300 es
uniform highp sampler2DArray kSamplerArray;
void FragmentShaderMain() {
}
)"));

}


EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_raw_source_es100();
    unit_test_raw_source_es300();
    unit_test_generation();
    unit_test_raw_source_combine();
    unit_test_conditional_data();
    unit_test_token_replacement();
    unit_test_uniform_blocks();

    unit_test_sampler2DArray_bug();
    return 0;
}
) // EXPORT_TEST_MAIN