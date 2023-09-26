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

#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "data/json.h"
#include "graphics/types.h"
#include "graphics/material.h"

void unit_test_maps()
{
    TEST_CASE(test::Type::Feature)

    {
        gfx::TextureMap texture;
        texture.SetName("hehe");
        texture.SetType(gfx::TextureMap::Type::Texture2D);
        texture.SetNumTextures(1);
        texture.SetTextureSource(0, gfx::LoadTextureFromFile("file.png"));
        texture.SetTextureRect(0, gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        texture.SetSamplerName("kFoobar", 0);
        texture.SetRectUniformName("kFoobarRect", 0);

        data::JsonObject json;
        texture.IntoJson(json);

        gfx::TextureMap other;
        other.FromJson(json);
        TEST_REQUIRE(other.GetHash() == texture.GetHash());
        TEST_REQUIRE(other.GetName() == "hehe");
        TEST_REQUIRE(other.GetSamplerName(0) == "kFoobar");
        TEST_REQUIRE(other.GetRectUniformName(0) == "kFoobarRect");
        TEST_REQUIRE(other.GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        gfx::TextureMap copy(other);
        TEST_REQUIRE(copy.GetHash() == texture.GetHash());
        TEST_REQUIRE(copy.GetName() == "hehe");
        TEST_REQUIRE(copy.GetSamplerName(0) == "kFoobar");
        TEST_REQUIRE(copy.GetRectUniformName(0) == "kFoobarRect");
        TEST_REQUIRE(copy.GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        copy = texture;
        TEST_REQUIRE(copy.GetHash() == texture.GetHash());
        TEST_REQUIRE(copy.GetName() == "hehe");
        TEST_REQUIRE(copy.GetSamplerName(0) == "kFoobar");
        TEST_REQUIRE(copy.GetRectUniformName(0) == "kFoobarRect");
        TEST_REQUIRE(copy.GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
    }

    {
        gfx::TextureMap sprite;
        sprite.SetName("sprite0");
        sprite.SetType(gfx::TextureMap::Type::Sprite);
        sprite.SetNumTextures(2);
        sprite.SetTextureSource(0, gfx::LoadTextureFromFile("frame_0.png"));
        sprite.SetTextureSource(1, gfx::LoadTextureFromFile("frame_1.png"));
        sprite.SetSamplerName("kTexture0", 0);
        sprite.SetSamplerName("kTexture1", 1);
        sprite.SetRectUniformName("kTextureRect0", 0);
        sprite.SetRectUniformName("kTextureRect1", 1);
        sprite.SetFps(10.0f);

        data::JsonObject json;
        sprite.IntoJson(json);

        gfx::TextureMap other;
        TEST_REQUIRE(other.FromJson(json));
        TEST_REQUIRE(other.GetName() == "sprite0");
        TEST_REQUIRE(other.GetHash() == sprite.GetHash());
        TEST_REQUIRE(other.GetFps() == real::float32(10.0f));
        TEST_REQUIRE(other.GetSamplerName(0) == "kTexture0");
        TEST_REQUIRE(other.GetSamplerName(1) == "kTexture1");
        TEST_REQUIRE(other.GetRectUniformName(0) == "kTextureRect0");
        TEST_REQUIRE(other.GetRectUniformName(1) == "kTextureRect1");
        TEST_REQUIRE(other.GetNumTextures() == 2);
        TEST_REQUIRE(other.GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(other.GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::Filesystem);

        gfx::TextureMap copy(other);
        TEST_REQUIRE(copy.GetHash() == sprite.GetHash());
        TEST_REQUIRE(copy.GetFps() == real::float32(10.0f));
        TEST_REQUIRE(copy.GetSamplerName(0) == "kTexture0");
        TEST_REQUIRE(copy.GetSamplerName(1) == "kTexture1");
        TEST_REQUIRE(copy.GetRectUniformName(0) == "kTextureRect0");
        TEST_REQUIRE(copy.GetRectUniformName(1) == "kTextureRect1");
        TEST_REQUIRE(copy.GetNumTextures() == 2);
        TEST_REQUIRE(copy.GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(copy.GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::Filesystem);

        copy = sprite;
        TEST_REQUIRE(copy.GetHash() == sprite.GetHash());
        TEST_REQUIRE(copy.GetFps() == real::float32(10.0f));
        TEST_REQUIRE(copy.GetSamplerName(0) == "kTexture0");
        TEST_REQUIRE(copy.GetSamplerName(1) == "kTexture1");
        TEST_REQUIRE(copy.GetRectUniformName(0) == "kTextureRect0");
        TEST_REQUIRE(copy.GetRectUniformName(1) == "kTextureRect1");
        TEST_REQUIRE(copy.GetNumTextures() == 2);
        TEST_REQUIRE(copy.GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(copy.GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::Filesystem);

        auto x = sprite.Copy();
        TEST_REQUIRE(x->GetHash() == sprite.GetHash());
        x = sprite.Clone();
        TEST_REQUIRE(x->GetHash() != sprite.GetHash());
        TEST_REQUIRE(x->AsSpriteMap()->GetFps() == real::float32(10.0f));
        TEST_REQUIRE(x->AsSpriteMap()->GetSamplerName(0) == "kTexture0");
        TEST_REQUIRE(x->AsSpriteMap()->GetSamplerName(1) == "kTexture1");
        TEST_REQUIRE(x->AsSpriteMap()->GetRectUniformName(0) == "kTextureRect0");
        TEST_REQUIRE(x->AsSpriteMap()->GetRectUniformName(1) == "kTextureRect1");
        TEST_REQUIRE(x->AsSpriteMap()->GetNumTextures() == 2);
        TEST_REQUIRE(x->AsSpriteMap()->GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(x->AsSpriteMap()->GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
    }
}

void unit_test_material_class()
{
    TEST_CASE(test::Type::Feature)

    gfx::ColorClass klass(gfx::MaterialClass::Type::Color);
    klass.SetGamma(1.5f);
    klass.SetStatic(false);
    klass.SetColor(gfx::Color::HotPink,     gfx::MaterialClass::ColorIndex::BaseColor);
    klass.SetColor(gfx::Color::DarkBlue,    gfx::MaterialClass::ColorIndex::BottomLeft);
    klass.SetColor(gfx::Color::DarkGreen,   gfx::MaterialClass::ColorIndex::TopLeft);
    klass.SetColor(gfx::Color::DarkMagenta, gfx::MaterialClass::ColorIndex::BottomRight);
    klass.SetColor(gfx::Color::DarkGray,    gfx::MaterialClass::ColorIndex::TopRight);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
    klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetParticleAction(gfx::TextureMap2DClass::ParticleAction::Rotate);
    klass.SetTextureScaleX(2.0f);
    klass.SetTextureScaleY(3.0f);
    klass.SetTextureVelocityX(4.0f);
    klass.SetTextureVelocityY(5.0f);
    klass.SetTextureVelocityZ(-1.0f);
    klass.SetTextureRotation(2.5f);
    klass.SetName("my material");
    klass.SetShaderUri("my_shader.glsl");
    klass.SetShaderSrc("some shader source");
    klass.SetUniform("float", 56.0f);
    klass.SetUniform("int", 123);
    klass.SetUniform("vec2", glm::vec2(1.0f, 2.0f));
    klass.SetUniform("vec3", glm::vec3(1.0f, 2.0f, 3.0f));
    klass.SetUniform("vec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    klass.SetUniform("color", gfx::Color::DarkCyan);
    klass.SetActiveTextureMap("123abc");

    auto texture_file_source = std::make_unique<gfx::detail::TextureFileSource>();
    texture_file_source->SetFileName("file.png");
    texture_file_source->SetName("file");
    auto texture_map = std::make_unique<gfx::TextureMap>();
    texture_map->SetName("sprite");
    texture_map->SetNumTextures(1);
    texture_map->SetTextureRect(0, gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
    texture_map->SetTextureSource(0, std::move(texture_file_source));

    klass.SetNumTextureMaps(1);
    klass.SetTextureMap(0, std::move(texture_map));

    // serialization
    {
        data::JsonObject json;
        klass.IntoJson(json);
        //std::cout << json.ToString();
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId()   == klass.GetId());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(ret->GetColor(gfx::MaterialClass::ColorIndex::BaseColor)   == gfx::Color::HotPink);
        TEST_REQUIRE(ret->GetColor(gfx::MaterialClass::ColorIndex::BottomLeft)  == gfx::Color::DarkBlue);
        TEST_REQUIRE(ret->GetColor(gfx::MaterialClass::ColorIndex::TopLeft)     == gfx::Color::DarkGreen);
        TEST_REQUIRE(ret->GetColor(gfx::MaterialClass::ColorIndex::BottomRight) == gfx::Color::DarkMagenta);
        TEST_REQUIRE(ret->GetColor(gfx::MaterialClass::ColorIndex::TopRight)    == gfx::Color::DarkGray);
        TEST_REQUIRE(ret->IsStatic()            == false);
        TEST_REQUIRE(ret->GetSurfaceType()      == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->GetParticleAction()   == gfx::MaterialClass::ParticleAction::Rotate);
        TEST_REQUIRE(ret->GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(ret->GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Nearest);
        TEST_REQUIRE(ret->GetTextureWrapX()     == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->GetTextureWrapY()     == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->GetGamma()            == real::float32(1.5f));
        TEST_REQUIRE(ret->GetTextureScaleX()    == real::float32(2.0f));
        TEST_REQUIRE(ret->GetTextureScaleY()    == real::float32(3.0f));
        TEST_REQUIRE(ret->GetTextureVelocityX() == real::float32(4.0f));
        TEST_REQUIRE(ret->GetTextureVelocityY() == real::float32(5.0f));
        TEST_REQUIRE(ret->GetTextureVelocityZ() == real::float32(-1.0f));
        TEST_REQUIRE(ret->GetTextureRotation()  == real::float32(2.5f));
        TEST_REQUIRE(ret->GetShaderUri() == "my_shader.glsl");
        TEST_REQUIRE(ret->GetShaderSrc() == "some shader source");
        TEST_REQUIRE(ret->GetActiveTextureMap() == "123abc");
        TEST_REQUIRE(*ret->FindUniformValue<int>("int") == 123);
        TEST_REQUIRE(*ret->FindUniformValue<float>("float") == real::float32(56.0f));
        TEST_REQUIRE(*ret->FindUniformValue<glm::vec2>("vec2") == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(*ret->FindUniformValue<glm::vec3>("vec3") == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(*ret->FindUniformValue<glm::vec4>("vec4") == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(*ret->FindUniformValue<gfx::Color4f>("color") == gfx::Color::DarkCyan);

        TEST_REQUIRE(ret->GetNumTextureMaps()   == 1);
        const auto* map = ret->GetTextureMap(0);
        TEST_REQUIRE(map->GetNumTextures()  == 1);
        TEST_REQUIRE(map->GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        TEST_REQUIRE(map->GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);

    }
    // copy and assignment
    {
        gfx::ColorClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());

        gfx::ColorClass temp(gfx::MaterialClass::Type::Color);
        temp =  klass;
        TEST_REQUIRE(temp.GetHash() == klass.GetHash());
        TEST_REQUIRE(temp.GetId() == klass.GetId());
    }
    // clone
    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetHash() != klass.GetHash());
        TEST_REQUIRE(clone->GetId() != klass.GetId());
        TEST_REQUIRE(clone->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(clone->GetBaseColor() == gfx::Color::HotPink);
        TEST_REQUIRE(clone->GetColor(gfx::MaterialClass::ColorIndex::BaseColor) == gfx::Color::HotPink);
        TEST_REQUIRE(clone->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(clone->IsStatic() == false);
    }
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_maps();
    unit_test_material_class();
    return 0;
}
) // TEST_MAIN