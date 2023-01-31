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

template<typename To, typename From>
const To* SameCast(const From* from, const To* to)
{
    return dynamic_cast<const To*>(from);
}

void unit_test_maps()
{
    {
        gfx::TextureMap2D texture;
        texture.SetTexture(gfx::LoadTextureFromFile("file.png"));
        texture.SetSamplerName("kFoobar");
        texture.SetRectUniformName("kFoobarRect");
        texture.SetTextureRect(gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        data::JsonObject json;
        texture.IntoJson(json);

        gfx::TextureMap2D other;
        other.FromJson(json);
        TEST_REQUIRE(other.GetHash() == texture.GetHash());
        TEST_REQUIRE(other.GetSamplerName() == "kFoobar");
        TEST_REQUIRE(other.GetRectUniformName() == "kFoobarRect");
        TEST_REQUIRE(other.GetTextureRect() == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        gfx::TextureMap2D copy(other);
        TEST_REQUIRE(copy.GetHash() == texture.GetHash());
        TEST_REQUIRE(copy.GetSamplerName() == "kFoobar");
        TEST_REQUIRE(copy.GetRectUniformName() == "kFoobarRect");
        TEST_REQUIRE(copy.GetTextureRect() == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        copy = texture;
        TEST_REQUIRE(copy.GetHash() == texture.GetHash());
        TEST_REQUIRE(copy.GetSamplerName() == "kFoobar");
        TEST_REQUIRE(copy.GetRectUniformName() == "kFoobarRect");
        TEST_REQUIRE(copy.GetTextureRect() == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
    }

    {
        gfx::SpriteMap sprite;
        sprite.AddTexture(gfx::LoadTextureFromFile("frame_0.png"));
        sprite.AddTexture(gfx::LoadTextureFromFile("frame_1.png"));
        sprite.SetSamplerName("kTexture0", 0);
        sprite.SetSamplerName("kTexture1", 1);
        sprite.SetRectUniformName("kTextureRect0", 0);
        sprite.SetRectUniformName("kTextureRect1", 1);
        sprite.SetFps(10.0f);

        data::JsonObject json;
        sprite.IntoJson(json);

        gfx::SpriteMap other;
        TEST_REQUIRE(other.FromJson(json));
        TEST_REQUIRE(other.GetHash() == sprite.GetHash());
        TEST_REQUIRE(other.GetFps() == real::float32(10.0f));
        TEST_REQUIRE(other.GetSamplerName(0) == "kTexture0");
        TEST_REQUIRE(other.GetSamplerName(1) == "kTexture1");
        TEST_REQUIRE(other.GetRectUniformName(0) == "kTextureRect0");
        TEST_REQUIRE(other.GetRectUniformName(1) == "kTextureRect1");
        TEST_REQUIRE(other.GetNumTextures() == 2);
        TEST_REQUIRE(other.GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(other.GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::Filesystem);

        gfx::SpriteMap copy(other);
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

void unit_test_color()
{
    gfx::ColorClass klass;
    klass.SetGamma(1.5f);
    klass.SetStatic(false);
    klass.SetBaseColor(gfx::Color::DarkGreen);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetName("my color");

    // serialization
    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId() == klass.GetId());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        //TEST_REQUIRE(ret->Getshader() == gfx::MaterialClass::Shader::Sprite);
        TEST_REQUIRE(ret->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->AsColor()->GetBaseColor() == gfx::Color::DarkGreen);
        TEST_REQUIRE(ret->AsColor()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(ret->AsColor()->IsStatic() == false);
        TEST_REQUIRE(ret->PremultipliedAlpha() == true);
    }
    // copy and assignment
    {
        gfx::ColorClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());

        gfx::ColorClass temp;
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
        TEST_REQUIRE(clone->AsColor()->GetBaseColor() == gfx::Color::DarkGreen);
        TEST_REQUIRE(clone->AsColor()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(clone->AsColor()->IsStatic() == false);
    }

}

void unit_test_gradient()
{
    gfx::GradientClass klass;
    klass.SetGamma(1.5f);
    klass.SetStatic(false);
    klass.SetColor(gfx::Color::DarkBlue,    gfx::GradientClass::ColorIndex::BottomLeft);
    klass.SetColor(gfx::Color::DarkGreen,   gfx::GradientClass::ColorIndex::TopLeft);
    klass.SetColor(gfx::Color::DarkMagenta, gfx::GradientClass::ColorIndex::BottomRight);
    klass.SetColor(gfx::Color::DarkGray,    gfx::GradientClass::ColorIndex::TopRight);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetName("my gradient");

    // serialization
    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId() == klass.GetId());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        //TEST_REQUIRE(ret->Getshader() == gfx::MaterialClass::Shader::Sprite);
        TEST_REQUIRE(ret->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::BottomLeft) == gfx::Color::DarkBlue);
        TEST_REQUIRE(ret->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::TopLeft) == gfx::Color::DarkGreen);
        TEST_REQUIRE(ret->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::BottomRight) == gfx::Color::DarkMagenta);
        TEST_REQUIRE(ret->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::TopRight) == gfx::Color::DarkGray);
        TEST_REQUIRE(ret->AsGradient()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(ret->AsGradient()->IsStatic() == false);
        TEST_REQUIRE(ret->PremultipliedAlpha());
    }
    // copy and assignment
    {
        gfx::GradientClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());

        gfx::GradientClass temp;
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
        TEST_REQUIRE(clone->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::BottomLeft) == gfx::Color::DarkBlue);
        TEST_REQUIRE(clone->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::TopLeft) == gfx::Color::DarkGreen);
        TEST_REQUIRE(clone->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::BottomRight) == gfx::Color::DarkMagenta);
        TEST_REQUIRE(clone->AsGradient()->GetColor(gfx::GradientClass::ColorIndex::TopRight) == gfx::Color::DarkGray);
        TEST_REQUIRE(clone->AsGradient()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(clone->AsGradient()->IsStatic() == false);
        TEST_REQUIRE(clone->PremultipliedAlpha());
    }
}


void unit_test_texture()
{
    gfx::detail::TextureFileSource texture;
    texture.SetFileName("file.png");
    texture.SetName("file");

    gfx::TextureMap2DClass klass;
    klass.SetGamma(1.5f);
    klass.SetStatic(false);
    klass.SetBaseColor(gfx::Color::DarkGreen);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetTexture(texture.Copy());
    klass.SetTextureRect(gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
    klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
    klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureScaleX(2.0f);
    klass.SetTextureScaleY(3.0f);
    klass.SetTextureVelocityX(4.0f);
    klass.SetTextureVelocityY(5.0f);
    klass.SetTextureVelocityZ(-1.0f);
    klass.SetTextureRotation(2.5f);
    klass.SetParticleAction(gfx::TextureMap2DClass::ParticleAction::Rotate);
    klass.SetName("my texture");

    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId() == klass.GetId());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(ret->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->PremultipliedAlpha());
        TEST_REQUIRE(ret->AsTexture()->GetBaseColor() == gfx::Color::DarkGreen);
        TEST_REQUIRE(ret->AsTexture()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(ret->AsTexture()->IsStatic() == false);
        TEST_REQUIRE(ret->AsTexture()->GetTextureSource()->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(ret->AsTexture()->GetTextureSource()->GetHash() == texture.GetHash());
        TEST_REQUIRE(ret->AsTexture()->GetTextureSource()->GetId()   == texture.GetId());
        TEST_REQUIRE(ret->AsTexture()->GetTextureSource()->GetName() == texture.GetName());
        TEST_REQUIRE(ret->AsTexture()->GetTextureRect() == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(ret->AsTexture()->GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Nearest);
        TEST_REQUIRE(ret->AsTexture()->GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsTexture()->GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsTexture()->GetTextureScaleX() == real::float32(2.0f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureScaleY() == real::float32(3.0f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureVelocityX() == real::float32(4.0f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureVelocityY() == real::float32(5.0f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureVelocityZ() == real::float32(-1.0f));
        TEST_REQUIRE(ret->AsTexture()->GetTextureRotation() == real::float32(2.5f));
        TEST_REQUIRE(ret->AsTexture()->GetParticleAction() == gfx::BuiltInMaterialClass::ParticleAction::Rotate);
    }

    {
        gfx::TextureMap2DClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());

        gfx::TextureMap2DClass temp;
        temp =  klass;
        TEST_REQUIRE(temp.GetHash() == klass.GetHash());
        TEST_REQUIRE(temp.GetId() == klass.GetId());
    }

    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetId() != klass.GetId());
        TEST_REQUIRE(clone->GetHash() != klass.GetHash());
        TEST_REQUIRE(clone->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(clone->PremultipliedAlpha());
        TEST_REQUIRE(clone->AsTexture()->GetBaseColor() == gfx::Color::DarkGreen);
        TEST_REQUIRE(clone->AsTexture()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(clone->AsTexture()->IsStatic() == false);
        TEST_REQUIRE(clone->AsTexture()->GetTextureSource()->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(clone->AsTexture()->GetTextureSource()->GetHash() != texture.GetHash());
        TEST_REQUIRE(clone->AsTexture()->GetTextureSource()->GetId()   != texture.GetId());
        TEST_REQUIRE(clone->AsTexture()->GetTextureSource()->GetName() == texture.GetName());
        TEST_REQUIRE(clone->AsTexture()->GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(clone->AsTexture()->GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Nearest);
        TEST_REQUIRE(clone->AsTexture()->GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(clone->AsTexture()->GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(clone->AsTexture()->GetTextureScaleX() == real::float32(2.0f));
        TEST_REQUIRE(clone->AsTexture()->GetTextureScaleY() == real::float32(3.0f));
        TEST_REQUIRE(clone->AsTexture()->GetTextureVelocityX() == real::float32(4.0f));
        TEST_REQUIRE(clone->AsTexture()->GetTextureVelocityY() == real::float32(5.0f));
        TEST_REQUIRE(clone->AsTexture()->GetTextureVelocityZ() == real::float32(-1.0f));
        TEST_REQUIRE(clone->AsTexture()->GetParticleAction() == gfx::BuiltInMaterialClass::ParticleAction::Rotate);
    }
}
void unit_test_sprite()
{
    gfx::SpriteClass klass;
    klass.SetGamma(1.5f);
    klass.SetStatic(false);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetBaseColor(gfx::Color::Blue);
    klass.SetFps(3.0f);
    klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
    klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureScaleX(2.0f);
    klass.SetTextureScaleY(3.0f);
    klass.SetTextureVelocityX(4.0f);
    klass.SetTextureVelocityY(5.0f);
    klass.SetTextureVelocityZ(-1.0f);
    klass.SetTextureRotation(2.5f);
    klass.SetParticleAction(gfx::TextureMap2DClass::ParticleAction::Rotate);
    klass.SetName("my sprite");

    gfx::detail::TextureFileSource texture;
    texture.SetFileName("file.png");
    texture.SetName("file");
    klass.AddTexture(texture.Copy());
    klass.SetTextureRect(0, gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

    gfx::TextBuffer text;
    text.SetBufferSize(100, 100);
    text.SetText("hello world", "font.otf", 23);
    gfx::detail::TextureTextBufferSource buffer;
    buffer.SetTextBuffer(text);
    buffer.SetName("text");
    klass.AddTexture(buffer.Copy());

    gfx::RgbBitmap rgb;
    rgb.Resize(2, 2);
    rgb.SetPixel(0, 0, gfx::Color::Red);
    rgb.SetPixel(0, 1, gfx::Color::White);
    rgb.SetPixel(1, 0, gfx::Color::Green);
    rgb.SetPixel(1, 1, gfx::Color::Blue);
    gfx::detail::TextureBitmapBufferSource bitmap;
    bitmap.SetName("bitmap");
    bitmap.SetBitmap(rgb);
    klass.AddTexture(bitmap.Copy());

    gfx::NoiseBitmapGenerator noise;
    noise.SetWidth(100);
    noise.SetHeight(100);
    gfx::NoiseBitmapGenerator::Layer layer;
    layer.prime0 = 123;
    layer.prime1 = 7;
    layer.prime2 = 7777;
    layer.amplitude = 5.0f;
    layer.frequency = 6.0f;
    noise.AddLayer(layer);
    gfx::detail::TextureBitmapGeneratorSource generator;
    generator.SetGenerator(noise);
    generator.SetName("noise");
    klass.AddTexture(generator.Copy());

    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId() == klass.GetId());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(ret->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->PremultipliedAlpha());
        TEST_REQUIRE(ret->AsSprite()->GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(ret->AsSprite()->GetBaseColor() == gfx::Color::Blue);
        TEST_REQUIRE(ret->AsSprite()->IsStatic() == false);
        TEST_REQUIRE(ret->AsSprite()->GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(ret->AsSprite()->GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Nearest);
        TEST_REQUIRE(ret->AsSprite()->GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsSprite()->GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsSprite()->GetTextureScaleX() == real::float32(2.0f));
        TEST_REQUIRE(ret->AsSprite()->GetTextureScaleY() == real::float32(3.0f));
        TEST_REQUIRE(ret->AsSprite()->GetTextureVelocityX() == real::float32(4.0f));
        TEST_REQUIRE(ret->AsSprite()->GetTextureVelocityY() == real::float32(5.0f));
        TEST_REQUIRE(ret->AsSprite()->GetTextureVelocityZ() == real::float32(-1.0f));
        TEST_REQUIRE(ret->AsSprite()->GetTextureRotation() == real::float32(2.5f));
        TEST_REQUIRE(ret->AsSprite()->GetParticleAction() == gfx::BuiltInMaterialClass::ParticleAction::Rotate);

        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(0)->GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(0)->GetHash() == texture.GetHash());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(0)->GetId()   == texture.GetId());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(0)->GetName() == texture.GetName());
        TEST_REQUIRE(ret->AsSprite()->GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(1)->GetSourceType() == gfx::TextureSource::Source::TextBuffer);
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(1)->GetHash() == buffer.GetHash());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(1)->GetId() == buffer.GetId());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(1)->GetName() == buffer.GetName());
        {
            const auto* other = SameCast(ret->AsSprite()->GetTextureSource(1), &buffer);
            TEST_REQUIRE(other->GetTextBuffer().GetBufferWidth() == 100);
            TEST_REQUIRE(other->GetTextBuffer().GetBufferHeight() == 100);
            TEST_REQUIRE(other->GetTextBuffer().GetText().text == "hello world");
            TEST_REQUIRE(other->GetTextBuffer().GetText().font == "font.otf");
            TEST_REQUIRE(other->GetTextBuffer().GetText().fontsize == 23);
        }

        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(2)->GetSourceType() == gfx::TextureSource::Source::BitmapBuffer);
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(2)->GetHash() == bitmap.GetHash());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(2)->GetId() == bitmap.GetId());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(2)->GetName() == bitmap.GetName());
        {
            const auto* other = SameCast(ret->AsSprite()->GetTextureSource(2), &bitmap);
            TEST_REQUIRE(other->GetBitmap().GetHeight() == 2);
            TEST_REQUIRE(other->GetBitmap().GetWidth() == 2);
            const gfx::RgbBitmap* bmp = nullptr;
            TEST_REQUIRE(other->GetBitmap(&bmp));
            TEST_REQUIRE(bmp->GetPixel(0, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp->GetPixel(0, 1) == gfx::Color::White);
            TEST_REQUIRE(bmp->GetPixel(1, 0) == gfx::Color::Green);
            TEST_REQUIRE(bmp->GetPixel(1, 1) == gfx::Color::Blue);
        }

        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(3)->GetSourceType() == gfx::TextureSource::Source::BitmapGenerator);
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(3)->GetHash() == generator.GetHash());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(3)->GetId() == generator.GetId());
        TEST_REQUIRE(ret->AsSprite()->GetTextureSource(3)->GetName() == generator.GetName());
        {
            const auto* other = SameCast(ret->AsSprite()->GetTextureSource(3), &generator);
            const auto* test = SameCast(&other->GetGenerator(), &noise);
            TEST_REQUIRE(test->GetWidth() == noise.GetWidth());
            TEST_REQUIRE(test->GetHeight() == noise.GetHeight());
            TEST_REQUIRE(test->GetNumLayers() == noise.GetNumLayers());
            for (size_t i=0; i<test->GetNumLayers(); ++i)
            {
                TEST_REQUIRE(test->GetLayer(i).prime0 == noise.GetLayer(i).prime0);
                TEST_REQUIRE(test->GetLayer(i).prime1 == noise.GetLayer(i).prime1);
                TEST_REQUIRE(test->GetLayer(i).prime2 == noise.GetLayer(i).prime2);
                TEST_REQUIRE(test->GetLayer(i).frequency == noise.GetLayer(i).frequency);
                TEST_REQUIRE(test->GetLayer(i).amplitude == noise.GetLayer(i).amplitude);
            }
        }
    }

    {
        gfx::SpriteClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());

        gfx::SpriteClass temp;
        temp =  klass;
        TEST_REQUIRE(temp.GetHash() == klass.GetHash());
        TEST_REQUIRE(temp.GetId() == klass.GetId());
    }

    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetId() != klass.GetId());
        TEST_REQUIRE(clone->GetHash() != klass.GetHash());
        TEST_REQUIRE(clone->AsSprite()->GetNumTextures() == 4);
    }
}

void unit_test_custom()
{
    gfx::CustomMaterialClass klass;
    klass.SetShaderUri("my_shader.glsl");
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, true);
    klass.SetUniform("float", 56.0f);
    klass.SetUniform("int", 123);
    klass.SetUniform("vec2", glm::vec2(1.0f, 2.0f));
    klass.SetUniform("vec3", glm::vec3(1.0f, 2.0f, 3.0f));
    klass.SetUniform("vec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    klass.SetUniform("color", gfx::Color::DarkCyan);
    klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Linear);
    klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetName("my material");

    {
        gfx::TextureMap2D texture;
        texture.SetTexture(gfx::LoadTextureFromFile("file.png"));
        texture.SetSamplerName("kFoobar");
        texture.SetRectUniformName("kFoobarRect");
        texture.SetTextureRect(gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));
        klass.SetTextureMap("texture", texture);
    }

    {
        gfx::SpriteMap sprite;
        sprite.SetFps(10.0f);
        sprite.AddTexture(gfx::LoadTextureFromFile("frame_0.png"));
        sprite.AddTexture(gfx::LoadTextureFromFile("frame_1.png"));
        sprite.SetSamplerName("kTexture0", 0);
        sprite.SetSamplerName("kTexture1", 1);
        sprite.SetRectUniformName("kTextureRect0", 0);
        sprite.SetRectUniformName("kTextureRect1", 1);
        klass.SetTextureMap("sprite", sprite);
    }


    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = gfx::MaterialClass::ClassFromJson(json);
        TEST_REQUIRE(ret);
        TEST_REQUIRE(ret->GetName() == klass.GetName());
        TEST_REQUIRE(ret->GetId() == klass.GetId());
        TEST_REQUIRE(ret->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(ret->PremultipliedAlpha());
        TEST_REQUIRE(ret->AsCustom()->GetShaderUri() == "my_shader.glsl");
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<float>("float") == real::float32(56.0f));
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<int>("int") == 123);
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<glm::vec2>("vec2") == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<glm::vec3>("vec3") == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<glm::vec4>("vec4") == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(*ret->AsCustom()->GetUniformValue<gfx::Color4f>("color") == gfx::Color::DarkCyan);
        TEST_REQUIRE(ret->AsCustom()->GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Linear);
        TEST_REQUIRE(ret->AsCustom()->GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(ret->AsCustom()->GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsCustom()->GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(ret->AsCustom()->FindTextureMap("texture")->GetHash() == klass.FindTextureMap("texture")->GetHash());
        TEST_REQUIRE(ret->AsCustom()->FindTextureMap("sprite")->GetHash() == klass.FindTextureMap("sprite")->GetHash());
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
    }

    {
        gfx::CustomMaterialClass copy(klass);
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.FindTextureMap("texture")->GetHash() == klass.FindTextureMap("texture")->GetHash());
        TEST_REQUIRE(copy.FindTextureMap("sprite")->GetHash() == klass.FindTextureMap("sprite")->GetHash());
        TEST_REQUIRE(copy.GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(copy.GetShaderUri() == "my_shader.glsl");
        TEST_REQUIRE(*copy.GetUniformValue<float>("float") == real::float32(56.0f));
        TEST_REQUIRE(*copy.GetUniformValue<int>("int") == 123);
        TEST_REQUIRE(*copy.GetUniformValue<glm::vec2>("vec2") == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(*copy.GetUniformValue<glm::vec3>("vec3") == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(*copy.GetUniformValue<glm::vec4>("vec4") == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(*copy.GetUniformValue<gfx::Color4f>("color") == gfx::Color::DarkCyan);
        TEST_REQUIRE(copy.GetTextureMagFilter() == gfx::MaterialClass::MagTextureFilter::Linear);
        TEST_REQUIRE(copy.GetTextureMinFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(copy.GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(copy.GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());


        gfx::CustomMaterialClass temp;
        temp =  klass;
        TEST_REQUIRE(temp.GetHash() == klass.GetHash());
        TEST_REQUIRE(temp.GetId() == klass.GetId());
    }

    {
        {
            auto clone = klass.Clone();
            TEST_REQUIRE(clone->GetId() != klass.GetId());
            TEST_REQUIRE(clone->GetHash() != klass.GetHash());
            TEST_REQUIRE(clone->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
            TEST_REQUIRE(clone->AsCustom()->GetShaderUri() == "my_shader.glsl");
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<float>("float") == real::float32(56.0f));
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<int>("int") == 123);
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<glm::vec2>("vec2") == glm::vec2(1.0f, 2.0f));
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<glm::vec3>("vec3") == glm::vec3(1.0f, 2.0f, 3.0f));
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<glm::vec4>("vec4") == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
            TEST_REQUIRE(*clone->AsCustom()->GetUniformValue<gfx::Color4f>("color") == gfx::Color::DarkCyan);

        }
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_maps();
    unit_test_color();
    unit_test_gradient();
    unit_test_texture();
    unit_test_sprite();
    unit_test_custom();
    return 0;
}