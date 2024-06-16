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

#include "base/test_minimal.h"
#include "graphics/shadersource.h"

void unit_test_raw_source()
{
    TEST_CASE(test::Type::Feature)

    using ddt = gfx::ShaderSource::ShaderDataDeclarationType;
    using dt = gfx::ShaderSource::ShaderDataType;

    {
        const auto ret = gfx::ShaderSource::FromRawSource(R"(
#version 100

attribute vec2 aVec2;
attribute vec3 aVec3;
attribute vec4 aVec4;

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
        )");

        TEST_REQUIRE(ret.GetVersion() == gfx::ShaderSource::Version::GLSL_100);

        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->data_type == dt::Vec2f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec2")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->data_type == dt::Vec3f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec3")->decl_type == ddt::Attribute);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->data_type == dt::Vec4f);
        TEST_REQUIRE(ret.FindDataDeclaration("aVec4")->decl_type == ddt::Attribute);

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

        TEST_REQUIRE(ret.GetSourceCount() == 1);
        TEST_REQUIRE(ret.GetSource(0) == R"(void main() {
  gl_FragColor = vec4(1.0);
}
)");
    }
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_raw_source();

    return 0;
}
) // EXPORT_TEST_MAIN