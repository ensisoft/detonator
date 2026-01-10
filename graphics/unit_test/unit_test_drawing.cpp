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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <iostream>
#include <unordered_map>
#include <string>
#include <any>

#include "base/math.h"
#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "data/json.h"
#include "data/io.h"
#include "graphics/types.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/device.h"
#include "graphics/drawable.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/shader_program.h"
#include "graphics/shader_programs.h"
#include "graphics/shader_source.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/particle_engine.h"
#include "graphics/drawcmd.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_bitmap_buffer_source.h"
#include "graphics/shader_source.h"
#include "graphics/tool/polygon.h"

#include "test_device.cpp"

bool operator==(const gfx::Vertex2D& lhs, const gfx::Vertex2D& rhs)
{
    return real::equals(lhs.aPosition.x, rhs.aPosition.x) &&
           real::equals(lhs.aPosition.y, rhs.aPosition.y) &&
           real::equals(lhs.aTexCoord.x, rhs.aTexCoord.x) &&
           real::equals(lhs.aTexCoord.y, rhs.aTexCoord.y);
}

bool operator==(const gfx::Geometry::DrawCommand& lhs, const gfx::Geometry::DrawCommand& rhs)
{
    return lhs.type == rhs.type &&
           lhs.count == rhs.count &&
           lhs.offset == rhs.offset;

}

void unit_test_material_uniforms()
{
    TEST_CASE(test::Type::Feature)

    // test dynamic program uniforms.
    {
        TestDevice device;
        gfx::ProgramState state;
        gfx::MaterialClass test(gfx::MaterialClass::Type::Color);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(false);

        // check that the dynamic state is set as expected.
        // this should mean that both static uniforms  and dynamic
        // uniforms are set.
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, state);

        gfx::Color4f base_color;
        TEST_REQUIRE(state.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(base_color == gfx::Color::Green);
    }

    {
        TestDevice device;
        gfx::ProgramState state;
        gfx::MaterialClass test(gfx::MaterialClass::Type::BasicLight);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetStatic(false);

        test.SetAmbientColor(gfx::Color::Red);
        test.SetDiffuseColor(gfx::Color::Green);
        test.SetSpecularColor(gfx::Color::Blue);
        test.SetSpecularExponent(128.0f);

        // check that the dynamic state is set as expected.
        // this should mean that both static uniforms  and dynamic
        // uniforms are set.
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, state);

        gfx::Color4f diffuse_color;
        gfx::Color4f ambient_color;
        gfx::Color4f specular_color;
        float specular_exponent = 0.0f;
        TEST_REQUIRE(state.GetUniform("kDiffuseColor", &diffuse_color));
        TEST_REQUIRE(state.GetUniform("kAmbientColor", &ambient_color));
        TEST_REQUIRE(state.GetUniform("kSpecularColor", &specular_color));
        TEST_REQUIRE(state.GetUniform("kSpecularExponent", &specular_exponent));
        TEST_REQUIRE(ambient_color == gfx::Color::Red);
        TEST_REQUIRE(diffuse_color == gfx::Color::Green);
        TEST_REQUIRE(specular_color == gfx::Color::Blue);
        TEST_REQUIRE(specular_exponent == real::float32(128.0f));
    }

    {
        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Gradient);
        test.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::GradientColor0);
        test.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::GradientColor1);
        test.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::GradientColor2);
        test.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::GradientColor3);

        test.SetStatic(false);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        test.ApplyDynamicState(env, device, program);

        gfx::Color4f color0;
        gfx::Color4f color1;
        gfx::Color4f color2;
        gfx::Color4f color3;
        TEST_REQUIRE(program.GetUniform("kGradientColor0", &color0));
        TEST_REQUIRE(program.GetUniform("kGradientColor1", &color1));
        TEST_REQUIRE(program.GetUniform("kGradientColor2", &color2));
        TEST_REQUIRE(program.GetUniform("kGradientColor3", &color3));
        TEST_REQUIRE(color0 == gfx::Color::DarkGreen);
        TEST_REQUIRE(color1 == gfx::Color::DarkGray);
        TEST_REQUIRE(color2 == gfx::Color::DarkBlue);
        TEST_REQUIRE(color3 == gfx::Color::DarkMagenta);
    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Texture);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(false);
        test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.draw_category = gfx::DrawCategory::Basic;
        env.draw_primitive = gfx::DrawPrimitive::Triangles;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);

        glm::vec2 texture_scale;
        glm::vec3 texture_velocity;
        int particle_effect;
        float runtime;
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocity", &texture_velocity));
        TEST_REQUIRE(program.GetUniform("kTime", &runtime));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity == glm::vec3(4.0f, 5.0f, -1.0f));
        TEST_REQUIRE(runtime == 2.0f);

        env.draw_category  = gfx::DrawCategory::Particles;
        env.draw_primitive = gfx::DrawPrimitive::Points;
        test.SetParticleEffect(gfx::MaterialClass::ParticleEffect::Rotate);
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(program.GetUniform("kParticleEffect", &particle_effect));
        TEST_REQUIRE(particle_effect == static_cast<int>(gfx::MaterialClass::ParticleEffect::Rotate));

    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Sprite);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(false);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.draw_category = gfx::DrawCategory::Basic;
        env.draw_primitive = gfx::DrawPrimitive::Triangles;
        env.material_time = 2.0f;
        test.ApplyDynamicState(env, device, program);

        glm::vec2 texture_scale;
        glm::vec3 texture_velocity;
        int particle_effect = 0;
        float runtime;
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocity", &texture_velocity));
        TEST_REQUIRE(program.GetUniform("kTime", &runtime));
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity == glm::vec3(4.0f, 5.0f, -1.0f));
        TEST_REQUIRE(runtime == 2.0f);
        TEST_REQUIRE(base_color == gfx::Color::Green);

        env.draw_category = gfx::DrawCategory::Particles;
        env.draw_primitive = gfx::DrawPrimitive::Points;
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(program.GetUniform("kParticleEffect", &particle_effect));
        TEST_REQUIRE(particle_effect == 0);

    }

    // test static program uniforms.
    {
        TestDevice device;
        gfx::ProgramState program;

        gfx::ColorClass test(gfx::MaterialClass::Type::Color);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetBaseColor(gfx::Color::Green);
        test.SetStatic(true);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;

        test.ApplyStaticState(env, device, program);
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(base_color == gfx::Color::Green);

        program.Clear();
        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kBaseColor"));
    }


    {
        TestDevice device;
        gfx::ProgramState state;
        gfx::MaterialClass test(gfx::MaterialClass::Type::BasicLight);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetStatic(true);

        test.SetAmbientColor(gfx::Color::Red);
        test.SetDiffuseColor(gfx::Color::Green);
        test.SetSpecularColor(gfx::Color::Blue);
        test.SetSpecularExponent(128.0f);

        // check that the dynamic state is set as expected.
        // this should mean that both static uniforms  and dynamic
        // uniforms are set.
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        test.ApplyStaticState(env, device, state);

        gfx::Color4f diffuse_color;
        gfx::Color4f ambient_color;
        gfx::Color4f specular_color;
        float specular_exponent = 0.0f;
        TEST_REQUIRE(state.GetUniform("kDiffuseColor", &diffuse_color));
        TEST_REQUIRE(state.GetUniform("kAmbientColor", &ambient_color));
        TEST_REQUIRE(state.GetUniform("kSpecularColor", &specular_color));
        TEST_REQUIRE(state.GetUniform("kSpecularExponent", &specular_exponent));
        TEST_REQUIRE(ambient_color == gfx::Color::Red);
        TEST_REQUIRE(diffuse_color == gfx::Color::Green);
        TEST_REQUIRE(specular_color == gfx::Color::Blue);
        TEST_REQUIRE(specular_exponent == real::float32(128.0f));

        state.Clear();
        test.ApplyDynamicState(env, device, state);
        TEST_REQUIRE(!state.HasUniform("kDiffuseColor"));
        TEST_REQUIRE(!state.HasUniform("kAmbientColor"));
        TEST_REQUIRE(!state.HasUniform("kSpecularColor"));
        TEST_REQUIRE(!state.HasUniform("kSpecularExponent"));
    }

    {
        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Gradient);
        test.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::GradientColor0);
        test.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::GradientColor1);
        test.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::GradientColor2);
        test.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::GradientColor3);
        test.SetStatic(true);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;

        test.ApplyStaticState(env, device, program);
        gfx::Color4f color0;
        gfx::Color4f color1;
        gfx::Color4f color2;
        gfx::Color4f color3;
        TEST_REQUIRE(program.GetUniform("kGradientColor0", &color0));
        TEST_REQUIRE(program.GetUniform("kGradientColor1", &color1));
        TEST_REQUIRE(program.GetUniform("kGradientColor2", &color2));
        TEST_REQUIRE(program.GetUniform("kGradientColor3", &color3));
        TEST_REQUIRE(color0 == gfx::Color::DarkGreen);
        TEST_REQUIRE(color1 == gfx::Color::DarkGray);
        TEST_REQUIRE(color2 == gfx::Color::DarkBlue);
        TEST_REQUIRE(color3 == gfx::Color::DarkMagenta);

        program.Clear();

        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kColor0"));
        TEST_REQUIRE(!program.HasUniform("kColor1"));
        TEST_REQUIRE(!program.HasUniform("kColor2"));
        TEST_REQUIRE(!program.HasUniform("kColor3"));
    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Texture);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(true);
        test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 2.0f;

        test.ApplyStaticState(env, device, program);
        glm::vec2 texture_scale;
        glm::vec3 texture_velocity;
        glm::vec1 particle_rotation_flag;
        glm::vec1 render_points_flag;
        glm::vec1 runtime;
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocity", &texture_velocity));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity == glm::vec3(4.0f, 5.0f, -1.0));

        program.Clear();

        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kTextureScale"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityXY"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityZ"));
    }

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(2, 2);

        TestDevice device;
        gfx::ProgramState program;

        gfx::MaterialClass test(gfx::MaterialClass::Type::Sprite);
        test.SetTextureScaleX(2.0f);
        test.SetTextureScaleY(3.0f);
        test.SetTextureVelocityX(4.0f);
        test.SetTextureVelocityY(5.0f);
        test.SetTextureVelocityZ(-1.0f);
        test.SetStatic(true);
        test.SetBaseColor(gfx::Color::Red);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 2.0f;

        test.ApplyStaticState(env, device, program);
        glm::vec2 texture_scale;
        glm::vec3 texture_velocity;
        gfx::Color4f base_color;
        TEST_REQUIRE(program.GetUniform("kTextureScale", &texture_scale));
        TEST_REQUIRE(program.GetUniform("kTextureVelocity", &texture_velocity));
        TEST_REQUIRE(program.GetUniform("kBaseColor", &base_color));
        TEST_REQUIRE(texture_scale == glm::vec2(2.0f, 3.0f));
        TEST_REQUIRE(texture_velocity == glm::vec3(4.0f, 5.0f, -1.0f));
        TEST_REQUIRE(base_color == gfx::Color::Red);

        program.Clear();

        test.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(!program.HasUniform("kTextureScale"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityXY"));
        TEST_REQUIRE(!program.HasUniform("kTextureVelocityZ"));
        TEST_REQUIRE(!program.HasUniform("kBaseColor"));
    }

    // test that static programs generate different program ID
    // based on their static state even if the underlying shader
    // program has the same type.
    {
        gfx::MaterialClass foo(gfx::MaterialClass::Type::Color);
        foo.SetStatic(true);
        foo.SetBaseColor(gfx::Color::Red);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State state;

        auto bar = foo;
        TEST_REQUIRE(foo.GetShaderId(state) == bar.GetShaderId(state));

        bar.SetBaseColor(gfx::Color::Green);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
    }

    {
        gfx::MaterialClass foo(gfx::MaterialClass::Type::BasicLight);
        foo.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        foo.SetStatic(true);

        foo.SetAmbientColor(gfx::Color::Red);
        foo.SetDiffuseColor(gfx::Color::Green);
        foo.SetSpecularColor(gfx::Color::Blue);
        foo.SetSpecularExponent(128.0f);

        gfx::MaterialClass::State state;

        auto bar = foo;
        TEST_REQUIRE(foo.GetShaderId(state) == bar.GetShaderId(state));

        foo.SetAmbientColor(gfx::Color::HotPink);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));

        bar = foo;
        foo.SetDiffuseColor(gfx::Color::HotPink);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));

        bar = foo;
        foo.SetSpecularColor(gfx::Color::HotPink);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));

        bar = foo;
        foo.SetSpecularExponent(8.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));

    }


    {
        gfx::MaterialClass foo(gfx::MaterialClass::Type::Gradient);
        foo.SetStatic(true);
        foo.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::GradientColor2);
        foo.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::GradientColor0);
        foo.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::GradientColor3);
        foo.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::GradientColor1);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State state;

        auto bar = foo;
        TEST_REQUIRE(foo.GetShaderId(state) == bar.GetShaderId(state));

        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::GradientColor2);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetColor(gfx::Color::White,gfx::GradientClass::ColorIndex::GradientColor3);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::GradientColor0);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::GradientColor1);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
    }

    {
        gfx::MaterialClass foo(gfx::MaterialClass::Type::Texture);
        foo.SetStatic(true);
        foo.SetTextureScaleX(2.0f);
        foo.SetTextureScaleY(3.0f);
        foo.SetTextureVelocityX(4.0f);
        foo.SetTextureVelocityY(5.0f);
        foo.SetTextureVelocityZ(-1.0f);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State state;

        auto bar = foo;
        TEST_REQUIRE(bar.GetShaderId(state) == foo.GetShaderId(state));
        bar = foo;
        foo.SetTextureScaleX(2.2f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureScaleY(2.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityX(4.1f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityY(-5.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityZ(1.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
    }

    {
        gfx::SpriteClass foo(gfx::MaterialClass::Type::Sprite);
        foo.SetStatic(true);
        foo.SetTextureScaleX(2.0f);
        foo.SetTextureScaleY(3.0f);
        foo.SetTextureVelocityX(4.0f);
        foo.SetTextureVelocityY(5.0f);
        foo.SetTextureVelocityZ(-1.0f);
        foo.SetBaseColor(gfx::Color::Red);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State state;

        auto bar = foo;
        TEST_REQUIRE(bar.GetShaderId(state) == foo.GetShaderId(state));
        bar = foo;
        foo.SetTextureScaleX(2.2f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureScaleY(2.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityX(4.1f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityY(-5.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetTextureVelocityZ(1.0f);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
        bar = foo;
        foo.SetBaseColor(gfx::Color::Blue);
        TEST_REQUIRE(foo.GetShaderId(state) != bar.GetShaderId(state));
    }
}

void unit_test_material_texture()
{
    TEST_CASE(test::Type::Feature)

    TestDevice device;
    gfx::ProgramState program;

    gfx::TextureMap2DClass test(gfx::MaterialClass::Type::Texture);
    test.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
    test.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    test.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Clamp);
    test.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Clamp);

    gfx::RgbBitmap bitmap;
    bitmap.Resize(100, 80);
    test.SetTexture(gfx::CreateTextureFromBitmap(bitmap));

    gfx::FlatShadedColorProgram pass;
    gfx::MaterialClass::State env;
    env.material_time = 1.0;
    test.ApplyDynamicState(env, device, program);

    const auto& texture = device.GetTexture(0);
    TEST_REQUIRE(texture.GetHeight() == 80);
    TEST_REQUIRE(texture.GetWidth() == 100);
    TEST_REQUIRE(texture.GetFormat() == gfx::Texture::Format::sRGB);
    TEST_REQUIRE(texture.GetMinFilter() == gfx::Texture::MinFilter::Trilinear);
    TEST_REQUIRE(texture.GetMagFilter() == gfx::Texture::MagFilter::Nearest);
    TEST_REQUIRE(texture.GetWrapX() == gfx::Texture::Wrapping::Clamp);
    TEST_REQUIRE(texture.GetWrapY() == gfx::Texture::Wrapping::Clamp);
    TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
    TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
}

void unit_test_sprite_texture_blending()
{
    TEST_CASE(test::Type::Feature)

    gfx::RgbBitmap bitmap;
    bitmap.Resize(10, 10);

    gfx::SpriteClass test(gfx::MaterialClass::Type::Sprite);
    test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
    test.GetTextureMap(0)->SetSpriteFrameRate(1.0f);
    test.SetBlendFrames(false);

    auto GetBlendFactor = [&test](double time) {
        TestDevice device;
        gfx::ProgramState program;
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = time;
        test.ApplyDynamicState(env, device, program);
        float blend_factor = 0.0f;
        program.GetUniform("kBlendCoeff", &blend_factor);
        return blend_factor;
    };

    // time in seconds.
    TEST_REQUIRE(GetBlendFactor(0.00) == 0.0f);
    TEST_REQUIRE(GetBlendFactor(0.50) == 0.0f);
    TEST_REQUIRE(GetBlendFactor(0.98) == 0.0f);
    TEST_REQUIRE(GetBlendFactor(1.25) == 0.0f);

    test.SetBlendFrames(true);
    TEST_REQUIRE(GetBlendFactor(0.00) == 0.5f);
    TEST_REQUIRE(math::equals(GetBlendFactor(0.49), 1.0f, 0.01f));
    TEST_REQUIRE(math::equals(GetBlendFactor(0.51), 0.0f, 0.01f));
    TEST_REQUIRE(math::equals(GetBlendFactor(1.00), 0.5f, 0.01f));
}

void unit_test_sprite_texture_binding()
{
    TEST_CASE(test::Type::Feature)

    TestDevice device;
    gfx::ProgramState program;

    auto BindTextures = [&device, &program](double time, gfx::MaterialClass& test) {
        device.Clear();
        program.Clear();
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = time;
        test.ApplyDynamicState(env, device, program);
    };

    // test cycling through sprite textures.
    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(10, 10);

        gfx::MaterialClass test(gfx::MaterialClass::Type::Sprite);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap));
        test.GetTextureMap(0)->SetSpriteFrameRate(1.0f);

        test.SetBlendFrames(false);
        test.GetTextureMap(0)->SetSpriteLooping(false);

        // start
        {
            BindTextures(0.0, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }

        // middle
        {
            BindTextures(0.5, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }

        // middle
        {
            BindTextures(1.5, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }

        test.GetTextureMap(0)->SetSpriteLooping(true);

        // start
        {
            BindTextures(0.0, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }

        // middle
        {
            BindTextures(0.5, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }

        // middle
        {
            BindTextures(1.5, test);
            const auto& texture = device.GetTexture(0);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture);
        }
    }

    {
        gfx::RgbBitmap bitmap0;
        bitmap0.Resize(10, 10);

        gfx::RgbBitmap bitmap1;
        bitmap1.Resize(20, 20);

        gfx::MaterialClass test(gfx::MaterialClass::Type::Sprite);
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap0));
        test.AddTexture(gfx::CreateTextureFromBitmap(bitmap1));
        test.GetTextureMap(0)->SetSpriteFrameRate(1.0f);

        test.SetBlendFrames(false);
        test.GetTextureMap(0)->SetSpriteLooping(false);

        // start
        {
            BindTextures(0.0, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 10);
            TEST_REQUIRE(texture1.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        // mid,
        {
            BindTextures(0.5, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 10);
            TEST_REQUIRE(texture1.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        // end
        {
            BindTextures(1.0, test);
            const auto& texture0 = device.GetTexture(0);
            TEST_REQUIRE(texture0.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture0);
        }

        // end, clamp
        {
            BindTextures(1.5, test);
            const auto& texture0 = device.GetTexture(0);
            TEST_REQUIRE(texture0.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture0);
        }

        test.GetTextureMap(0)->SetSpriteLooping(true);

        // with sprite set to looping everything else should be the same
        // as above, but when the time exceeds the duration of the sprite
        // animation cycle we are going to wrap over.

        // end
        {
            BindTextures(1.0, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 20);
            TEST_REQUIRE(texture1.GetWidth() == 10);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        // looping over
        {
            BindTextures(2.1, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 10);
            TEST_REQUIRE(texture1.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        test.GetTextureMap(0)->SetSpriteLooping(false);
        test.SetBlendFrames(true);

        // with blending when time is 0.0 we're actually blending
        // between the last and the first frame. so the texture binding
        // changes a little bit.
        {
            BindTextures(0.0, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 20);
            TEST_REQUIRE(texture1.GetWidth() == 10);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        //
        {
            BindTextures(0.5, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 10);
            TEST_REQUIRE(texture1.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        // end
        {
            BindTextures(1.0, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 10);
            TEST_REQUIRE(texture1.GetWidth() == 20);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }

        // wrap over
        test.GetTextureMap(0)->SetSpriteLooping(true);

        // end
        {
            BindTextures(2.0, test);
            const auto& texture0 = device.GetTexture(0);
            const auto& texture1 = device.GetTexture(1);
            TEST_REQUIRE(texture0.GetWidth() == 20);
            TEST_REQUIRE(texture1.GetWidth() == 10);
            TEST_REQUIRE(program.GetSamplerSetting(0).unit == 0);
            TEST_REQUIRE(program.GetSamplerSetting(0).texture == &texture0);
            TEST_REQUIRE(program.GetSamplerSetting(1).unit == 1);
            TEST_REQUIRE(program.GetSamplerSetting(1).texture == &texture1);
        }
    }
}

void unit_test_material_textures_bind_fail()
{

    TEST_CASE(test::Type::Feature)

    TestDevice device;
    gfx::ProgramState program;

    // test setting basic texture properties.
    {
        gfx::TextureMap2DClass test(gfx::MaterialClass::Type::Texture);
        test.SetTexture(gfx::LoadTextureFromFile("no-such-file.png"));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }

    {
        gfx::SpriteClass test(gfx::MaterialClass::Type::Sprite);
        test.AddTexture( gfx::LoadTextureFromFile("no-such-file.png"));

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }

    {
        gfx::CustomMaterialClass test(gfx::MaterialClass::Type::Sprite);
        gfx::SpriteMap sprite;
        sprite.SetType(gfx::SpriteMap::Type::Sprite);
        sprite.SetName("huhu");
        sprite.SetNumTextures(1);
        sprite.SetTextureSource(0, gfx::LoadTextureFromFile("no-such-file.png"));
        test.SetNumTextureMaps(1);
        test.SetTextureMap(0, sprite);

        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 1.0;
        // no crash
        test.ApplyDynamicState(env, device, program);
    }
}

void unit_test_material_uniform_folding()
{
    TEST_CASE(test::Type::Feature)

    gfx::FlatShadedColorProgram pass;
    gfx::MaterialClass::State state;

    // fold uniforms into consts in the GLSL when the material is
    // marked static.
    {
        TestDevice device;

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Color);
        klass.SetBaseColor(gfx::Color::White);
        klass.SetStatic(true);
        const auto& source = klass.GetShader(state, device);
        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kBaseColor;") == false);
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kBaseColor = vec4(1.00,1.00,1.00,1.00);"));
    }

    {
        TestDevice device;
        gfx::MaterialClass test(gfx::MaterialClass::Type::BasicLight);
        test.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        test.SetStatic(true);
        test.SetAmbientColor(gfx::Color::Red);
        test.SetDiffuseColor(gfx::Color::Green);
        test.SetSpecularColor(gfx::Color::Blue);
        test.SetSpecularExponent(128.0f);

        const auto& source = test.GetShader(state, device);
        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kAmbientColor = vec4(1.00,0.00,0.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kDiffuseColor = vec4(0.00,1.00,0.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kSpecularColor = vec4(0.00,0.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const float kSpecularExponent = 128.00;"));
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kAmbientColor") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kDiffuseColor") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kSpecularColor") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform float kSpecularExponent") == false);
    }

    {
        TestDevice device;

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Gradient);
        klass.SetColor(gfx::Color::Blue,  gfx::GradientClass::ColorIndex::GradientColor2);
        klass.SetColor(gfx::Color::Green, gfx::GradientClass::ColorIndex::GradientColor0);
        klass.SetColor(gfx::Color::Red,   gfx::GradientClass::ColorIndex::GradientColor3);
        klass.SetColor(gfx::Color::White, gfx::GradientClass::ColorIndex::GradientColor1);
        klass.SetStatic(true);
        const auto& source = klass.GetShader(state, device);
        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kGradientColor0;") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kGradientColor1;") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kGradientColor2;") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kGradientColor3;") == false);
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kGradientColor0 = vec4(0.00,1.00,0.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kGradientColor1 = vec4(1.00,1.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kGradientColor2 = vec4(0.00,0.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kGradientColor3 = vec4(1.00,0.00,0.00,1.00);"));
    }

    {
        TestDevice device;

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture);
        klass.SetStatic(true);
        klass.SetBaseColor(gfx::Color::White);
        klass.SetTextureVelocityX(4.0f);
        klass.SetTextureVelocityY(5.0f);
        klass.SetTextureVelocityZ(-1.0f);
        klass.SetTextureScaleX(2.0);
        klass.SetTextureScaleY(3.0);
        const auto& source = klass.GetShader(state, device);
        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "uniform vec4 kBaseColor;") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kTextureScale") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kTextureVelocityXY") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform float kTextureVelocityZ") == false);
        TEST_REQUIRE(base::Contains(sauce, "const vec4 kBaseColor = vec4(1.00,1.00,1.00,1.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec2 kTextureScale = vec2(2.00,3.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec3 kTextureVelocity = vec3(4.00,5.00,-1.00);"));
    }

    {
        TestDevice device;

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Sprite);
        klass.SetStatic(true);
        klass.SetTextureVelocityX(4.0f);
        klass.SetTextureVelocityY(5.0f);
        klass.SetTextureVelocityZ(-1.0f);
        klass.SetTextureScaleX(2.0);
        klass.SetTextureScaleY(3.0);
        const auto& source = klass.GetShader(state, device);
        const auto& sauce = source.GetSource();
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kTextureScale") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform vec2 kTextureVelocityXY") == false);
        TEST_REQUIRE(base::Contains(sauce, "uniform float kTextureVelocityZ") == false);
        TEST_REQUIRE(base::Contains(sauce, "const vec2 kTextureScale = vec2(2.00,3.00);"));
        TEST_REQUIRE(base::Contains(sauce, "const vec3 kTextureVelocity = vec3(4.00,5.00,-1.00);"));
    }
}

void unit_test_custom_shader_source()
{
    TEST_CASE(test::Type::Feature)

    gfx::MaterialClass klass(gfx::MaterialClass::Type::Custom);
    klass.SetShaderSrc(R"(
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

void FragmentShaderMain() {
  fs_out.color = vec4(1.0);
}
        )");

    TestDevice device;
    gfx::MaterialClass::State state;
    const auto& source = klass.GetShader(state, device);
    const auto& sauce = source.GetSource();
    TEST_REQUIRE(base::Contains(sauce, "#version 100"));
    TEST_REQUIRE(base::Contains(sauce, "attribute vec2 aVec2;"));
    TEST_REQUIRE(base::Contains(sauce, "attribute vec3 aVec3;"));
    TEST_REQUIRE(base::Contains(sauce, "attribute vec4 aVec4;"));
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
}

void unit_test_custom_uniforms()
{
    TEST_CASE(test::Type::Feature)

    gfx::CustomMaterialClass klass(gfx::MaterialClass::Type::Custom);
    klass.SetUniform("float", 56.0f);
    klass.SetUniform("int", 123);
    klass.SetUniform("vec2", glm::vec2(1.0f, 2.0f));
    klass.SetUniform("vec3", glm::vec3(1.0f, 2.0f, 3.0f));
    klass.SetUniform("vec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    klass.SetUniform("color", gfx::Color::DarkCyan);

    TestDevice device;
    gfx::ProgramState program;
    gfx::FlatShadedColorProgram pass;
    gfx::MaterialClass::State env;
    env.material_time = 0.0f;
    klass.ApplyDynamicState(env, device, program);

    int ivec1_;
    float vec1_;
    glm::vec2 vec2_;
    glm::vec3 vec3_;
    glm::vec4 vec4_;
    gfx::Color4f color_;
    TEST_REQUIRE(program.GetUniform("float", &vec1_));
    TEST_REQUIRE(program.GetUniform("int", &ivec1_));
    TEST_REQUIRE(program.GetUniform("vec2", &vec2_));
    TEST_REQUIRE(program.GetUniform("vec3", &vec3_));
    TEST_REQUIRE(program.GetUniform("vec4", &vec4_));
    TEST_REQUIRE(program.GetUniform("color", &color_));
    TEST_REQUIRE(ivec1_   == 123);
    TEST_REQUIRE(vec1_ == real::float32(56.0f));
    TEST_REQUIRE(vec2_  == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(vec3_  == glm::vec3(1.0f, 2.0f, 3.0f));
    TEST_REQUIRE(vec4_  == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    TEST_REQUIRE(color_  == gfx::Color::DarkCyan);

}

void unit_test_custom_textures()
{
    TEST_CASE(test::Type::Feature)

    gfx::CustomMaterialClass klass(gfx::MaterialClass::Type::Custom);
    klass.SetNumTextureMaps(2);
    klass.SetBlendFrames(true);

    {
        gfx::RgbBitmap bitmap;
        bitmap.Resize(10, 10);

        gfx::TextureMap texture;
        texture.SetName("texture");
        texture.SetType(gfx::TextureMap::Type::Texture2D);
        texture.SetNumTextures(1);
        texture.SetTextureSource(0, gfx::CreateTextureFromBitmap(bitmap));
        texture.SetTextureRect(0, gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        texture.SetSamplerName("kFoobar");
        texture.SetRectUniformName("kFoobarRect");
        klass.SetTextureMap(0, texture);
    }

    {
        gfx::RgbBitmap frame0, frame1;
        frame0.Resize(20, 20);
        frame1.Resize(30, 30);

        gfx::TextureMap sprite;
        sprite.SetName("sprite");
        sprite.SetType(gfx::TextureMap::Type::Sprite);
        sprite.SetSpriteFrameRate(10.0f);
        sprite.SetNumTextures(2);
        sprite.SetSpriteLooping(false);
        sprite.SetTextureSource(0, gfx::CreateTextureFromBitmap(frame0));
        sprite.SetTextureSource(1, gfx::CreateTextureFromBitmap(frame1));
        sprite.SetTextureRect(size_t(0), gfx::FRect(1.0f, 2.0f, 3.0f, 4.0f));
        sprite.SetTextureRect(size_t(1), gfx::FRect(4.0f, 3.0f, 2.0f, 1.0f));
        sprite.SetSamplerName("kTexture0", 0);
        sprite.SetSamplerName("kTexture1", 1);
        sprite.SetRectUniformName("kTextureRect0", 0);
        sprite.SetRectUniformName("kTextureRect1", 1);
        klass.SetTextureMap(1, sprite);
    }

    TestDevice device;
    gfx::ProgramState program;

    gfx::FlatShadedColorProgram pass;
    gfx::MaterialClass::State env;
    env.material_time = 0.0;
    klass.ApplyDynamicState(env, device, program);
    // these textures should be bound to these samplers. check the textures based on their sizes.
    TEST_REQUIRE(program.FindTextureBinding("kFoobar")->texture->GetWidth() == 10);
    TEST_REQUIRE(program.FindTextureBinding("kFoobar")->texture->GetHeight() == 10);
    TEST_REQUIRE(program.FindTextureBinding("kTexture0")->texture->GetWidth()  == 30);
    TEST_REQUIRE(program.FindTextureBinding("kTexture0")->texture->GetHeight() == 30);
    TEST_REQUIRE(program.FindTextureBinding("kTexture1")->texture->GetWidth()  == 20);
    TEST_REQUIRE(program.FindTextureBinding("kTexture1")->texture->GetHeight() == 20);

    // check the texture rects.
    glm::vec4 kFoobarRect;
    TEST_REQUIRE(program.GetUniform("kFoobarRect", &kFoobarRect));
    TEST_REQUIRE(kFoobarRect == glm::vec4(0.5f, 0.6f, 0.7f, 0.8f));

    glm::vec4 kTextureRect0;
    glm::vec4 kTextureRect1;
    TEST_REQUIRE(program.GetUniform("kTextureRect0", &kTextureRect0));
    TEST_REQUIRE(program.GetUniform("kTextureRect1", &kTextureRect1));
    TEST_REQUIRE(kTextureRect0 == glm::vec4(4.0f, 3.0f, 2.0f, 1.0f));
    TEST_REQUIRE(kTextureRect1 == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
}

void unit_test_polygon_inline_data()
{
    TEST_CASE(test::Type::Feature)

    gfx::tool::PolygonBuilder2D builder;
    gfx::PolygonMeshClass poly;
    TEST_REQUIRE(poly.GetContentHash() == 0);

    const gfx::Vertex2D verts[3] = {
       { { 10.0f,  10.0f}, {0.5f, 1.0f} },
       { {-10.0f, -10.0f}, {0.0f, 0.0f} },
       { { 10.0f,  10.0f}, {1.0f, 0.0f} }
    };
    gfx::Geometry::DrawCommand cmd;
    cmd.offset = 0;
    cmd.count  = 3;
    cmd.type   = gfx::Geometry::DrawType::TriangleFan;
    builder.AddVertices(verts, 3);
    builder.AddDrawCommand(cmd);
    builder.BuildPoly(poly);

    const auto hash1 = poly.GetContentHash();
    TEST_REQUIRE(hash1);

    {
        gfx::Geometry::CreateArgs args;
        gfx::DrawableClass::Environment env;
        env.editing_mode = true;

        auto& geom = args.buffer;
        TEST_REQUIRE(poly.Construct(env, args));
        TEST_REQUIRE(geom.GetNumDrawCmds() == 1);
        TEST_REQUIRE(geom.GetDrawCmd(0) == cmd);
        TEST_REQUIRE(geom.GetVertexCount() == 3);
        TEST_REQUIRE(geom.GetVertexBytes() == sizeof(verts));

        const gfx::VertexStream stream(geom.GetLayout(), geom.GetVertexBuffer());
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
    }

    // change the content (simulate editing)
    builder.AddVertices(verts, 3);
    builder.AddDrawCommand(cmd);
    builder.BuildPoly(poly);

    const auto hash2 = poly.GetContentHash();
    TEST_REQUIRE(hash2);
    TEST_REQUIRE(hash1 != hash2);

    {
        gfx::Geometry::CreateArgs args;
        gfx::DrawableClass::Environment env;
        env.editing_mode = true;

        auto& geom = args.buffer;
        TEST_REQUIRE(poly.Construct(env, args));
        TEST_REQUIRE(geom.GetNumDrawCmds() == 2);
        TEST_REQUIRE(geom.GetDrawCmd(0) == cmd);
        TEST_REQUIRE(geom.GetDrawCmd(1) == cmd);
        TEST_REQUIRE(geom.GetVertexBytes() == sizeof(verts) * 2);
        TEST_REQUIRE(geom.GetVertexCount() == 6);

        const gfx::VertexStream stream(geom.GetLayout(), geom.GetVertexBuffer());
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(3) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(4) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(5) == verts[2]);
    }

}

void unit_test_polygon_mesh()
{
    TEST_CASE(test::Type::Feature)

    // generate some content
    gfx::Vertex2D verts[3];
    verts[0].aPosition = gfx::Vec2 {  1.0f,  2.0f };
    verts[0].aTexCoord = gfx::Vec2 {  0.5f,  0.5f };
    verts[1].aPosition = gfx::Vec2 { -1.0f, -2.0f };
    verts[1].aTexCoord = gfx::Vec2 {  1.0f,  1.0f };
    verts[2].aPosition = gfx::Vec2 {  0.0f,  0.0f };
    verts[2].aTexCoord = gfx::Vec2 { -0.5f, -0.5f };

    gfx::Index16 indices[3];
    indices[0] = 123;
    indices[1] = 100;
    indices[2] = 1;

    gfx::Geometry::DrawCommand cmds[1];
    cmds[0].type   = gfx::Geometry::DrawType::TriangleFan;
    cmds[0].count  = 123;
    cmds[0].offset = 0;

    {
        const gfx::VertexStream vertex_stream(gfx::GetVertexLayout<gfx::Vertex2D>(), verts, 3);
        const gfx::IndexStream index_stream(indices, sizeof(indices), gfx::Geometry::IndexType::Index16);
        const gfx::CommandStream command_stream(cmds, 1);

        data::JsonObject json;
        vertex_stream.IntoJson(json);
        index_stream.IntoJson(json);
        command_stream.IntoJson(json);

        data::FileDevice file;
        file.Open("mesh-test.json");
        json.Dump(file);

        file.Close();
    }

    {
        gfx::PolygonMeshClass poly;
        poly.SetContentUri("mesh-test.json");

        gfx::Geometry::CreateArgs args;
        gfx::DrawableClass::Environment env;
        env.editing_mode = false;

        auto& geom = args.buffer;

        TEST_REQUIRE(poly.Construct(env, args));
        TEST_REQUIRE(geom.GetNumDrawCmds() == 1);
        TEST_REQUIRE(geom.GetDrawCmd(0) == cmds[0]);
        TEST_REQUIRE(geom.GetVertexBytes() == sizeof(verts));
        TEST_REQUIRE(geom.GetVertexCount() == 3);

        const gfx::VertexStream stream(geom.GetLayout(), geom.GetVertexBuffer());
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(0) == verts[0]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(1) == verts[1]);
        TEST_REQUIRE(*stream.GetVertex<gfx::Vertex2D>(2) == verts[2]);
    }
}

void unit_test_polygon_shader()
{
    TEST_CASE(test::Type::Feature)

    // test shader ID generation
    {
        gfx::PolygonMeshClass klass0;
        klass0.SetName("klass0");
        klass0.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
        klass0.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());

        gfx::PolygonMeshClass klass1;
        klass1.SetName("klass1");
        klass1.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
        klass1.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());

        gfx::DrawableClass::Environment env;
        env.use_instancing = false;
        TEST_REQUIRE(klass0.GetShaderId(env) == klass1.GetShaderId(env));

        env.use_instancing = true;
        TEST_REQUIRE(klass0.GetShaderId(env) == klass1.GetShaderId(env));

        env.use_instancing = true;
        std::string id0 = klass0.GetShaderId(env);
        env.use_instancing = false;
        std::string id1 = klass1.GetShaderId(env);
        TEST_REQUIRE(id0 != id1);


        klass0.SetShaderSrc(R"(
void CustomVertexTransform(inout VertexData vs) {
  vs.vertex = vec4(0.0);
}
        )");
        env.use_instancing = false;
        TEST_REQUIRE(klass0.GetShaderId(env) != klass1.GetShaderId(env));

    }

    // test shader source generation
    {
        gfx::PolygonMeshClass klass;
        klass.SetName("klass0");
        klass.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
        klass.SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        klass.SetShaderSrc(R"(
void CustomVertexTransform(inout VertexData vs) {
  // bla
  vs.vertex = vec4(0.0);
}
        )");

        TestDevice dev;
        gfx::DrawableClass::Environment env;
        env.use_instancing = false;

        const auto& src = klass.GetShader(env, dev);
        const auto& source = src.GetSource();
        //std::cout << source;
        TEST_REQUIRE(base::Contains(source, "#define CUSTOM_VERTEX_TRANSFORM"));
        TEST_REQUIRE(base::Contains(source, "void CustomVertexTransform(inout VertexData vs"));
        TEST_REQUIRE(base::Contains(source, "// bla"));

    }

}

void unit_test_local_particles()
{
    TEST_CASE(test::Type::Feature)

    using K = gfx::ParticleEngineClass;
    using P = gfx::ParticleEngineClass::Params;

    struct ParticleVertex {
        gfx::Vec2 aPosition;
        gfx::Vec2 aDirection;
        gfx::Vec4 aData;
    };

    // emitter position and spawning inside rectangle
    {
        gfx::ParticleEngineClass::Params p;
        p.mode             = K::SpawnPolicy::Once;
        p.placement        = K::Placement::Inside;
        p.shape            = K::EmitterShape::Rectangle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Outwards;
        p.init_rect_height = 0.5f;
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            TEST_REQUIRE(v.aPosition.x >= 0.25f);
            TEST_REQUIRE(v.aPosition.y >= 0.25f);
            TEST_REQUIRE(v.aPosition.x <= 0.25f + 0.5f);
            TEST_REQUIRE(v.aPosition.y <= 0.25f + 0.5f);
        }
    }
    // emitter position and spawning outside rectangle
    {
        gfx::ParticleEngineClass::Params p;
        p.mode             = K::SpawnPolicy::Once;
        p.placement        = K::Placement::Outside;
        p.shape            = K::EmitterShape::Rectangle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Outwards;
        p.init_rect_height = 0.5f;
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);


        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);

        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            const bool inside_box = (v.aPosition.x > 0.25f && v.aPosition.x < 0.75f) &&
                                    (v.aPosition.y > 0.25f && v.aPosition.y < 0.75f);
            TEST_REQUIRE(!inside_box);
        }
    }

    // emitter position and spawning edge of rectangle
    {
        gfx::ParticleEngineClass::Params p;
        p.mode             = K::SpawnPolicy::Once;
        p.placement        = K::Placement::Edge;
        p.shape            = K::EmitterShape::Rectangle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Outwards;
        p.init_rect_height = 0.5f;
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);

        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            const auto on_left_edge   = math::equals(v.aPosition.x, 0.25f)   && v.aPosition.y >= 0.25f && v.aPosition.y <= 0.75f;
            const auto on_right_edge  = math::equals(v.aPosition.x, 0.75f)  && v.aPosition.y >= 0.25f && v.aPosition.y <= 0.75f;
            const auto on_top_edge    = math::equals(v.aPosition.y, 0.25f)    && v.aPosition.x >= 0.25f && v.aPosition.x <= 0.75f;
            const auto on_bottom_edge = math::equals(v.aPosition.y, 0.75f) && v.aPosition.x >= 0.25f && v.aPosition.x <= 0.75f;
            TEST_REQUIRE(on_left_edge || on_right_edge || on_top_edge || on_bottom_edge);
        }
    }

    // emitter position and spawning center of rectangle
    {
        gfx::ParticleEngineClass::Params p;
        p.mode             = K::SpawnPolicy::Once;
        p.placement        = K::Placement::Center;
        p.shape            = K::EmitterShape::Rectangle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Outwards;
        p.init_rect_height = 0.5f;
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);

        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            TEST_REQUIRE(math::equals(v.aPosition.x, 0.5f));
            TEST_REQUIRE(math::equals(v.aPosition.y, 0.5f));
        }
    }

    // emitter position and spawning when using circle
    {
        const K::Placement placements[] = {
            K::Placement::Inside,
            K::Placement::Center,
            K::Placement::Edge,
            K::Placement::Outside
        };
        for (auto placement : placements)
        {
            gfx::ParticleEngineClass::Params p;
            p.placement        = placement;
            p.mode             = K::SpawnPolicy::Once;
            p.shape            = K::EmitterShape::Circle;
            p.coordinate_space = K::CoordinateSpace::Local;
            p.direction        = K::Direction::Outwards;
            p.init_rect_height = 0.5f; // radius will be 0.25f
            p.init_rect_width  = 0.5f;
            p.init_rect_xpos   = 0.25f;
            p.init_rect_ypos   = 0.25f;
            p.num_particles    = 10;
            gfx::ParticleEngineClass klass(p);
            gfx::ParticleEngineInstance eng(klass);

            gfx::Geometry::CreateArgs args;
            gfx::FlatShadedColorProgram pass;
            gfx::DrawableClass::Environment env;

            eng.Restart(env);

            TestDevice dev;
            TEST_REQUIRE(eng.Construct(env, dev, args));
            TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

            const gfx::VertexStream stream(args.buffer.GetLayout(),
                                           args.buffer.GetVertexBuffer());

            for (size_t i=0; i<p.num_particles; ++i)
            {
                const auto& v = *stream.GetVertex<ParticleVertex>(i);
                const auto r = glm::length(glm::vec2(0.5f, 0.5f) - glm::vec2(v.aPosition.x, v.aPosition.y));
                if (placement == K::Placement::Inside)
                    TEST_REQUIRE(math::equals(r, 0.25f) || r < 0.25f);
                else if (placement == K::Placement::Outside)
                    TEST_REQUIRE(math::equals(r, 0.25f) || r > 0.25f);
                else if  (placement == K::Placement::Edge)
                    TEST_REQUIRE(math::equals(r, 0.25f));
                else if (placement == K::Placement::Center)
                    TEST_REQUIRE(math::equals(r, 0.0f));
            }
        }
    }

    // direction of travel outwards from circle edge.
    {
        gfx::ParticleEngineClass::Params p;
        p.placement        = K::Placement::Edge;
        p.mode             = K::SpawnPolicy::Once;
        p.shape            = K::EmitterShape::Circle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Outwards;
        p.boundary         = K::BoundaryPolicy::Clamp;
        p.init_rect_height = 0.5f; // radius will be 0.25f
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        p.min_velocity     = 1.0f;
        p.max_velocity     = 1.0f;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        eng.Update(env, 1.0/60.0f);

        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            const auto r = glm::length(glm::vec2(0.5f, 0.5f) - glm::vec2(v.aPosition.x, v.aPosition.y));
            TEST_REQUIRE( r > 0.25f);
        }
    }

    // direction of travel inwards from circle edge.
    {
        gfx::ParticleEngineClass::Params p;
        p.placement        = K::Placement::Edge;
        p.mode             = K::SpawnPolicy::Once;
        p.shape            = K::EmitterShape::Circle;
        p.coordinate_space = K::CoordinateSpace::Local;
        p.direction        = K::Direction::Inwards;
        p.boundary         = K::BoundaryPolicy::Clamp;
        p.init_rect_height = 0.5f; // radius will be 0.25f
        p.init_rect_width  = 0.5f;
        p.init_rect_xpos   = 0.25f;
        p.init_rect_ypos   = 0.25f;
        p.num_particles    = 10;
        p.min_velocity     = 1.0f;
        p.max_velocity     = 1.0f;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        gfx::Geometry::CreateArgs args;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        eng.Update(env, 1.0/60.0f);

        TestDevice dev;
        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            const auto r = glm::length(glm::vec2(0.5f, 0.5f) - glm::vec2(v.aPosition.x, v.aPosition.y));
            TEST_REQUIRE( r < 0.25f);
        }
    }
    // todo: direction of travel with sector

}

void unit_test_global_particles()
{
    TEST_CASE(test::Type::Feature)

    struct ParticleVertex {
        gfx::Vec2 aPosition;
        gfx::Vec2 aDirection;
        gfx::Vec4 aData;
    };


    // global sector direction
    {
        gfx::ParticleEngineClass::Params p;
        p.coordinate_space = gfx::ParticleEngineClass::CoordinateSpace::Global;
        p.init_rect_width  = 1.0;
        p.init_rect_height = 1.0;
        p.init_rect_xpos   = 0;
        p.init_rect_ypos   = 0;
        p.num_particles    = 10;
        p.mode             = gfx::ParticleEngineClass::SpawnPolicy::Once;
        p.direction        = gfx::ParticleEngineClass::Direction::Sector;
        p.placement        = gfx::ParticleEngineClass::Placement::Center;
        p.direction_sector_start_angle = math::DegreesToRadians(135.0);
        p.direction_sector_size = 0.0;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        TestDevice dev;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;
        gfx::Geometry::CreateArgs args;

        base::Transform transform;
        transform.Resize(200.0f, 6.0f);
        const auto& model_to_world = transform.GetAsMatrix();

        env.model_matrix = &model_to_world;

        eng.Restart(env);
        eng.Update(env, 1.0/60.0f);

        TEST_REQUIRE(eng.Construct(env, dev, args));
        TEST_REQUIRE(args.buffer.GetVertexCount() == p.num_particles);

        const gfx::VertexStream stream(args.buffer.GetLayout(),
                                       args.buffer.GetVertexBuffer());

        for (size_t i=0; i<p.num_particles; ++i)
        {
            const auto& v = *stream.GetVertex<ParticleVertex>(i);
            TEST_REQUIRE(math::equals(100.0f, v.aPosition.x, 0.1f));
            TEST_REQUIRE(math::equals(3.0f, v.aPosition.y, 0.1f));

            const auto result_angle = math::FindVectorRotationAroundZ(gfx::ToVec(v.aDirection));
            const auto target_angle = math::DegreesToRadians(135.0f);
            const auto epsilon = 0.1f;
            TEST_REQUIRE(math::equals(target_angle, result_angle, epsilon));
        }
    }

}

void unit_test_particles()
{
    TEST_CASE(test::Type::Feature)

    // emission mode once.
    {
        gfx::ParticleEngineClass::Params p;
        p.num_particles = 100;
        p.max_lifetime  = 1.0f;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Once;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        TestDevice dev;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 100);

        eng.Update(env, 1.5f);
        TEST_REQUIRE(eng.IsAlive() == false);
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 0);
    }


    // emission mode maintain, new particles are spawned to compensate
    // for ones that died.
    {
        gfx::ParticleEngineClass::Params p;
        p.num_particles = 100;
        p.max_lifetime  = 1.0f;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Maintain;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        TestDevice dev;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 100);

        eng.Update(env, 1.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 100);

        eng.Update(env, 1.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 100);
    }

    // continuously spawn new particles. num_particles is the spawn
    // rate of particles in particles/second
    {
        gfx::ParticleEngineClass::Params p;
        p.num_particles = 10; // 10 particles per second.
        p.min_lifetime  = 10.0f;
        p.max_lifetime  = 10.0f;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Continuous;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        TestDevice dev;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        // we're starting with 0 particles and on every update
        // spawn new particles within the spawn rate.
        eng.Restart(env);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 0);

        eng.Update(env, 0.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 5);

        eng.Update(env, 0.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 10);

        eng.Update(env, 0.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 15);
    }

    // Spawn on command only
    {
        gfx::ParticleEngineClass::Params p;
        p.num_particles = 10; // 10 particles per second.
        p.min_lifetime  = 10.0f;
        p.max_lifetime  = 10.0f;
        p.mode = gfx::ParticleEngineClass::SpawnPolicy::Command;
        gfx::ParticleEngineClass klass(p);
        gfx::ParticleEngineInstance eng(klass);

        TestDevice dev;
        gfx::FlatShadedColorProgram pass;
        gfx::DrawableClass::Environment env;

        eng.Restart(env);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 0);

        gfx::Drawable::Command cmd;
        cmd.name = "EmitParticles";
        cmd.args["count"] = 10;
        eng.Execute(env, cmd);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 10);

        // update should not affect particle spawning since it's on command now
        eng.Update(env, 0.5f);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 10);

        eng.Execute(env, cmd);
        TEST_REQUIRE(eng.IsAlive());
        TEST_REQUIRE(eng.GetNumParticlesAlive() == 20);
    }


    // todo: test the following:
    // - emission mode
    // - min and max duration of the simulation
    // - delay
    // - min/max properties
}

// test that new programs are built out of vertex and
// fragment shaders only when the shaders change not
// when the high level class types changes. For example
// a rect and a circle can both use the same vertex shader
// and when with a single material only a single program
// needs to be created.
void unit_test_painter_shape_material_pairing()
{
    TEST_CASE(test::Type::Feature)

    TestDevice device;

    auto painter = gfx::Painter::Create(&device);
    auto color = gfx::CreateMaterialFromColor(gfx::Color::Red);
    gfx::Transform transform;

    painter->Draw(gfx::Rectangle(), transform, color);
    TEST_REQUIRE(device.GetNumShaders() == 2);
    TEST_REQUIRE(device.GetNumPrograms() == 1);

    painter->Draw(gfx::Circle(), transform, color);
    TEST_REQUIRE(device.GetNumShaders() == 2);
    TEST_REQUIRE(device.GetNumPrograms() == 1);

    auto gradient = gfx::CreateMaterialFromColor(gfx::Color::Red, gfx::Color::Red,
                                                 gfx::Color::Green, gfx::Color::Green);
    painter->Draw(gfx::Rectangle(), transform, gradient);
    TEST_REQUIRE(device.GetNumShaders() == 3);
    TEST_REQUIRE(device.GetNumPrograms() == 2);
    painter->Draw(gfx::Circle(), transform, color);
    TEST_REQUIRE(device.GetNumShaders() == 3);
    TEST_REQUIRE(device.GetNumPrograms() == 2);

}

// Test that when a shader fails to load or compile the painter produces
// a fallback shader and stops trying to recreate the shader on every paint.
void unit_test_painter_fallback_material_shader()
{
    TEST_CASE(test::Type::Feature)

    // shader fails with compile error.
    {
        TestDevice device;

        auto painter = gfx::Painter::Create(&device);

        gfx::MaterialClass material_class(gfx::MaterialClass::Type::Color);
        // add junk source
        material_class.SetShaderSrc(R"(
// junk shader
asdgljsaglsja
        )");

        {
            painter->Draw(gfx::Circle(), gfx::Transform(), gfx::MaterialInstance(material_class));
            TEST_REQUIRE(device.GetNumShaders() == 1);
            auto& shader = device.GetShader(0);
            TEST_REQUIRE(shader.IsFallback() == false);
            TEST_REQUIRE(shader.IsValid() == false);
            shader.SetName("junk shader");
        }

        // draw again, the previous shader should still
        // exist and not get overwritten
        {
            painter->Draw(gfx::Circle(), gfx::Transform(), gfx::MaterialInstance(material_class));
            TEST_REQUIRE(device.GetNumShaders() == 1);
            auto& shader = device.GetShader(0);
            TEST_REQUIRE(shader.IsFallback() == false);
            TEST_REQUIRE(shader.IsValid() == false);
            TEST_REQUIRE(shader.GetName() == "junk shader");
        }
    }

    // shader fails to load (shader source version error)
    {
        TestDevice device;

        auto painter = gfx::Painter::Create(&device);

        gfx::MaterialClass material_class(gfx::MaterialClass::Type::Custom);
        // unsupported version
        material_class.SetShaderSrc(R"(
#version 100
int main() { gl_FragColor = vec4(1.0); }
            )");


        // fallback gets created as a valid shader object
        {
            painter->Draw(gfx::Circle(), gfx::Transform(), gfx::MaterialInstance(material_class));
            TEST_REQUIRE(device.GetNumShaders() == 1);
            auto& shader = device.GetShader(0);
            TEST_REQUIRE(shader.IsFallback() == true);
            TEST_REQUIRE(shader.IsValid() == true);
            shader.SetName("fallback shader");
        }

        // draw again, the previous shader should still
        // exist and not get ovewrwritten
        {
            painter->Draw(gfx::Circle(), gfx::Transform(), gfx::MaterialInstance(material_class));
            TEST_REQUIRE(device.GetNumShaders() == 1);
            auto& shader = device.GetShader(0);
            TEST_REQUIRE(shader.IsFallback() == true);
            TEST_REQUIRE(shader.IsValid() == true);
            TEST_REQUIRE(shader.GetName() == "fallback shader");
        }
    }
}

void unit_test_painter_fallback_drawable_shader()
{
    TEST_CASE(test::Type::Feature)

    // shader fails with compile error
    {
        TestDevice device;

        auto painter = gfx::Painter::Create(&device);

        gfx::PolygonMeshClass drawable_class;
        drawable_class.SetMeshType(gfx::PolygonMeshClass::MeshType::Simple2DRenderMesh);
        drawable_class.SetShaderSrc(R"(
// junk shader
asdgljsaglsja
        )");

        {
            painter->Draw(gfx::PolygonMeshInstance(drawable_class), gfx::Transform(),
                gfx::CreateMaterialFromColor(gfx::Color::Red));
            // index 0 is the material (fragment) shader, index 1 is the drawable (vertex) shader
            TEST_REQUIRE(device.GetNumShaders() == 2);
            auto& shader = device.GetShader(1);
            TEST_REQUIRE(shader.IsFallback() == false);
            TEST_REQUIRE(shader.IsValid() == false);
            shader.SetName("junk shader");
        }

        // draw again, the previous shader should still
        // exist and not get overwritten
        {
            painter->Draw(gfx::PolygonMeshInstance(drawable_class), gfx::Transform(),
                gfx::CreateMaterialFromColor(gfx::Color::Red));
            // index 0 is the material (fragment) shader, index 1 is the drawable (vertex) shader
            TEST_REQUIRE(device.GetNumShaders() == 2);
            auto& shader = device.GetShader(1);
            TEST_REQUIRE(shader.IsFallback() == false);
            TEST_REQUIRE(shader.IsValid() == false);
            TEST_REQUIRE(shader.GetName() == "junk shader");
        }
    }

    // currently there's no way for the user to write their own
    // vertex shaders, only customize them. this means loading
    // cannot really fail, so this test (see material)
    // doesn't exist.
}

// multiple materials with textures should only load the
// same texture object once onto the device.
void unit_test_packed_texture_bug()
{
    TEST_CASE(test::Type::Feature)

    gfx::RgbaBitmap bmp;
    bmp.Resize(10, 10);
    bmp.Fill(gfx::Color::HotPink);
    gfx::WritePNG(bmp, "test-texture.png");

    // several materials
    {
        gfx::TextureMap2DClass material0(gfx::MaterialClass::Type::Texture);
        material0.SetTexture(gfx::LoadTextureFromFile("test-texture.png"));
        gfx::TextureMap2DClass material1(gfx::MaterialClass::Type::Texture);
        material1.SetTexture(gfx::LoadTextureFromFile("test-texture.png"));

        TestDevice device;
        gfx::ProgramState program;
        gfx::FlatShadedColorProgram pass;
        gfx::MaterialClass::State env;
        env.material_time = 0.0f;
        env.editing_mode  = false;
        material0.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(device.GetNumTextures() == 1);

        material1.ApplyDynamicState(env, device, program);
        TEST_REQUIRE(device.GetNumTextures() == 1);
    }

    // single material but with multiple texture maps
    {

    }
}

// static flag should generate new program IDs since
// the static uniforms get folded into the shader code.
// however not having static should not generate new IDs
void unit_test_gpu_id_bug()
{
    TEST_CASE(test::Type::Feature)

    gfx::FlatShadedColorProgram pass;
    gfx::MaterialClass::State env;
    env.material_time = 0.0f;
    env.editing_mode  = false;

    // each one of these different materials objects with the
    // same type maps to the same underlying shader object.
    // for example two color shaders that arent static can share
    // the same shader/program object and set their state dynamically
    // through uniform settings.
    // however if they're marked static the uniforms are folded in the
    // shader source which means they now must be different shader objects.

    // not -static, i.e. dynamic
    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Color);
        klass.SetStatic(false);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);

        const auto& initial = klass.GetShaderId(env);
        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        TEST_REQUIRE(klass.GetShaderId(env) == initial);
    }

    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture);
        klass.SetStatic(false);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.0f);
        klass.SetTextureScaleY(1.0f);
        klass.SetTextureVelocityX(0.0f);
        klass.SetTextureVelocityY(0.0f);
        klass.SetTextureRotation(0.0f);
        const auto& initial = klass.GetShaderId(env);

        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.5f);
        klass.SetTextureScaleY(1.5f);
        klass.SetTextureVelocityX(1.0f);
        klass.SetTextureVelocityY(1.0f);
        klass.SetTextureRotation(1.0f);
        TEST_REQUIRE(klass.GetShaderId(env) == initial);
    }

    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Sprite);
        klass.SetStatic(false);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.0f);
        klass.SetTextureScaleY(1.0f);
        klass.SetTextureVelocityX(0.0f);
        klass.SetTextureVelocityY(0.0f);
        klass.SetTextureRotation(0.0f);
        const auto& initial = klass.GetShaderId(env);

        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.5f);
        klass.SetTextureScaleY(1.5f);
        klass.SetTextureVelocityX(1.0f);
        klass.SetTextureVelocityY(1.0f);
        klass.SetTextureRotation(1.0f);
        TEST_REQUIRE(klass.GetShaderId(env) == initial);
    }

    // static
    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Color);
        klass.SetStatic(true);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);

        const auto& initial = klass.GetShaderId(env);
        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        TEST_REQUIRE(klass.GetShaderId(env) != initial);
    }

    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Texture);
        klass.SetStatic(true);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.0f);
        klass.SetTextureScaleY(1.0f);
        klass.SetTextureVelocityX(0.0f);
        klass.SetTextureVelocityY(0.0f);
        klass.SetTextureRotation(0.0f);
        const auto& initial = klass.GetShaderId(env);

        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.5f);
        klass.SetTextureScaleY(1.5f);
        klass.SetTextureVelocityX(1.0f);
        klass.SetTextureVelocityY(1.0f);
        klass.SetTextureRotation(1.0f);
        TEST_REQUIRE(klass.GetShaderId(env) != initial);
    }

    {
        gfx::MaterialClass klass(gfx::MaterialClass::Type::Sprite);
        klass.SetStatic(true);
        klass.SetColor(gfx::Color::White, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.0f);
        klass.SetTextureScaleY(1.0f);
        klass.SetTextureVelocityX(0.0f);
        klass.SetTextureVelocityY(0.0f);
        klass.SetTextureRotation(0.0f);
        const auto& initial = klass.GetShaderId(env);

        klass.SetColor(gfx::Color::Red, gfx::MaterialClass::ColorIndex::BaseColor);
        klass.SetTextureScaleX(1.5f);
        klass.SetTextureScaleY(1.5f);
        klass.SetTextureVelocityX(1.0f);
        klass.SetTextureVelocityY(1.0f);
        klass.SetTextureRotation(1.0f);
        TEST_REQUIRE(klass.GetShaderId(env) != initial);
    }

}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_material_uniforms();
    unit_test_material_texture();
    unit_test_sprite_texture_blending();
    unit_test_sprite_texture_binding();
    unit_test_material_textures_bind_fail();
    unit_test_material_uniform_folding();
    unit_test_custom_shader_source();
    unit_test_custom_uniforms();
    unit_test_custom_textures();
    unit_test_polygon_inline_data();
    unit_test_polygon_mesh();
    unit_test_polygon_shader();
    unit_test_local_particles();
    unit_test_global_particles();
    unit_test_particles();
    unit_test_painter_shape_material_pairing();
    unit_test_painter_fallback_material_shader();
    unit_test_painter_fallback_drawable_shader();

    unit_test_packed_texture_bug();
    unit_test_gpu_id_bug();
    return 0;
}
) // TEST_MAIN
