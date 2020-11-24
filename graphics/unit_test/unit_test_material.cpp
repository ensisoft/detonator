// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "graphics/types.h"
#include "graphics/material.h"

template<typename To, typename From>
const To& SameCast(const From& from, const To& to)
{
    return dynamic_cast<const To&>(from);
}

bool operator==(const gfx::Color4f& lhs, const gfx::Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}

bool operator==(const gfx::FRect& lhs, const gfx::FRect& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY()) &&
           real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}

void unit_test_data()
{
    gfx::MaterialClass klass;
    klass.SetType(gfx::MaterialClass::Type::Sprite);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Emissive);
    klass.SetGamma(1.5f);
    klass.SetFps(3.0f);
    klass.SetStatic(false);
    klass.SetBlendFrames(false);
    klass.SetBaseColor(gfx::Color::DarkGreen);
    klass.SetColorMapColor(gfx::Color::DarkBlue, gfx::MaterialClass::ColorIndex::BottomLeft);
    klass.SetColorMapColor(gfx::Color::DarkGreen,gfx::MaterialClass::ColorIndex::TopLeft);
    klass.SetColorMapColor(gfx::Color::DarkMagenta, gfx::MaterialClass::ColorIndex::BottomRight);
    klass.SetColorMapColor(gfx::Color::DarkGray, gfx::MaterialClass::ColorIndex::TopRight);
    klass.SetShaderFile("my/shader/file.glsl");
    klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Trilinear);
    klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter::Nearest);
    klass.SetTextureWrapX(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureWrapY(gfx::MaterialClass::TextureWrapping::Repeat);
    klass.SetTextureScaleX(2.0f);
    klass.SetTextureScaleY(3.0f);
    klass.SetTextureVelocityX(4.0f);
    klass.SetTextureVelocityY(5.0f);
    klass.SetTextureVelocityZ(-1.0f);
    klass.SetParticleAction(gfx::MaterialClass::ParticleAction::Rotate);

    gfx::detail::TextureFileSource texture;
    texture.SetFileName("file.png");
    texture.SetName("file");
    klass.AddTexture(texture);
    klass.SetTextureGc(0, true);
    klass.SetTextureRect(0, gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

    gfx::TextBuffer text;
    text.SetBufferSize(100, 100);
    text.AddText("hello world", "font.otf", 23);
    gfx::detail::TextureTextBufferSource buffer;
    buffer.SetTextBuffer(text);
    buffer.SetName("text");
    klass.AddTexture(buffer);

    gfx::RgbBitmap rgb;
    rgb.Resize(2, 2);
    rgb.SetPixel(0, 0, gfx::Color::Red);
    rgb.SetPixel(0, 1, gfx::Color::White);
    rgb.SetPixel(1, 0, gfx::Color::Green);
    rgb.SetPixel(1, 1, gfx::Color::Blue);
    gfx::detail::TextureBitmapBufferSource bitmap;
    bitmap.SetName("bitmap");
    bitmap.SetBitmap(rgb);
    klass.AddTexture(bitmap);

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
    klass.AddTexture(generator);

    const auto& json = klass.ToJson();
    //std::cout << json.dump(2);
    //std::cout << std::endl;

    {
        auto ret = gfx::MaterialClass::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        const auto& copy = ret.value();
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetType() == gfx::MaterialClass::Type::Sprite);
        TEST_REQUIRE(copy.GetSurfaceType() == gfx::MaterialClass::SurfaceType::Emissive);
        TEST_REQUIRE(copy.GetGamma() == real::float32(1.5f));
        TEST_REQUIRE(copy.GetFps() == real::float32(3.0f));
        TEST_REQUIRE(copy.IsStatic() == false);
        TEST_REQUIRE(copy.GetBlendFrames() == false);
        TEST_REQUIRE(copy.GetBaseColor() == gfx::Color::DarkGreen);
        TEST_REQUIRE(copy.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomLeft) == gfx::Color::DarkBlue);
        TEST_REQUIRE(copy.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopLeft) == gfx::Color::DarkGreen);
        TEST_REQUIRE(copy.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomRight) == gfx::Color::DarkMagenta);
        TEST_REQUIRE(copy.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopRight) == gfx::Color::DarkGray);
        TEST_REQUIRE(copy.GetShaderFile() == "my/shader/file.glsl");
        TEST_REQUIRE(copy.GetMinTextureFilter() == gfx::MaterialClass::MinTextureFilter::Trilinear);
        TEST_REQUIRE(copy.GetMagTextureFilter() == gfx::MaterialClass::MagTextureFilter::Nearest);
        TEST_REQUIRE(copy.GetTextureWrapX() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(copy.GetTextureWrapY() == gfx::MaterialClass::TextureWrapping::Repeat);
        TEST_REQUIRE(copy.GetTextureScaleX() == real::float32(2.0f));
        TEST_REQUIRE(copy.GetTextureScaleY() == real::float32(3.0f));
        TEST_REQUIRE(copy.GetTextureVelocityX() == real::float32(4.0f));
        TEST_REQUIRE(copy.GetTextureVelocityY() == real::float32(5.0f));
        TEST_REQUIRE(copy.GetTextureVelocityZ() == real::float32(-1.0f));
        TEST_REQUIRE(copy.GetParticleAction() == gfx::MaterialClass::ParticleAction::Rotate);

        TEST_REQUIRE(copy.GetNumTextures() == 4);
        TEST_REQUIRE(copy.GetTextureSource(0).GetSourceType() == gfx::TextureSource::Source::Filesystem);
        TEST_REQUIRE(copy.GetTextureSource(0).GetHash() == texture.GetHash());
        TEST_REQUIRE(copy.GetTextureSource(0).GetId() == texture.GetId());
        TEST_REQUIRE(copy.GetTextureSource(0).GetName() == texture.GetName());
        TEST_REQUIRE(copy.GetTextureGc(0) == true);
        TEST_REQUIRE(copy.GetTextureRect(0) == gfx::FRect(0.5f, 0.6f, 0.7f, 0.8f));

        TEST_REQUIRE(copy.GetTextureSource(1).GetSourceType() == gfx::TextureSource::Source::TextBuffer);
        TEST_REQUIRE(copy.GetTextureSource(1).GetHash() == buffer.GetHash());
        TEST_REQUIRE(copy.GetTextureSource(1).GetId() == buffer.GetId());
        TEST_REQUIRE(copy.GetTextureSource(1).GetName() == buffer.GetName());
        {
            const auto& other = SameCast(copy.GetTextureSource(1), buffer);
            TEST_REQUIRE(other.GetTextBuffer().GetBufferWidth() == 100);
            TEST_REQUIRE(other.GetTextBuffer().GetBufferHeight() == 100);
            TEST_REQUIRE(other.GetTextBuffer().GetNumTexts() == 1);
            TEST_REQUIRE(other.GetTextBuffer().GetText(0).text == "hello world");
            TEST_REQUIRE(other.GetTextBuffer().GetText(0).font == "font.otf");
            TEST_REQUIRE(other.GetTextBuffer().GetText(0).fontsize == 23);
        }

        TEST_REQUIRE(copy.GetTextureSource(2).GetSourceType() == gfx::TextureSource::Source::BitmapBuffer);
        TEST_REQUIRE(copy.GetTextureSource(2).GetHash() == bitmap.GetHash());
        TEST_REQUIRE(copy.GetTextureSource(2).GetId() == bitmap.GetId());
        TEST_REQUIRE(copy.GetTextureSource(2).GetName() == bitmap.GetName());
        {
            const auto& other = SameCast(copy.GetTextureSource(2), bitmap);
            TEST_REQUIRE(other.GetBitmap().GetHeight() == 2);
            TEST_REQUIRE(other.GetBitmap().GetWidth() == 2);
            const gfx::RgbBitmap* bmp = nullptr;
            TEST_REQUIRE(other.GetBitmap(&bmp));
            TEST_REQUIRE(bmp->GetPixel(0, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp->GetPixel(0, 1) == gfx::Color::White);
            TEST_REQUIRE(bmp->GetPixel(1, 0) == gfx::Color::Green);
            TEST_REQUIRE(bmp->GetPixel(1, 1) == gfx::Color::Blue);
        }

        TEST_REQUIRE(copy.GetTextureSource(3).GetSourceType() == gfx::TextureSource::Source::BitmapGenerator);
        TEST_REQUIRE(copy.GetTextureSource(3).GetHash() == generator.GetHash());
        TEST_REQUIRE(copy.GetTextureSource(3).GetId() == generator.GetId());
        TEST_REQUIRE(copy.GetTextureSource(3).GetName() == generator.GetName());
        {
            const auto& other = SameCast(copy.GetTextureSource(3), generator);
            const auto& test = SameCast(other.GetGenerator(), noise);
            TEST_REQUIRE(test.GetWidth() == noise.GetWidth());
            TEST_REQUIRE(test.GetHeight() == noise.GetHeight());
            TEST_REQUIRE(test.GetNumLayers() == noise.GetNumLayers());
            for (size_t i=0; i<test.GetNumLayers(); ++i)
            {
                TEST_REQUIRE(test.GetLayer(i).prime0 == noise.GetLayer(i).prime0);
                TEST_REQUIRE(test.GetLayer(i).prime1 == noise.GetLayer(i).prime1);
                TEST_REQUIRE(test.GetLayer(i).prime2 == noise.GetLayer(i).prime2);
                TEST_REQUIRE(test.GetLayer(i).frequency == noise.GetLayer(i).frequency);
                TEST_REQUIRE(test.GetLayer(i).amplitude == noise.GetLayer(i).amplitude);
            }
        }
    }

    // test assignment
    gfx::MaterialClass copy;
    copy = klass;
    TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    TEST_REQUIRE(copy.GetId() == klass.GetId());

}

int test_main(int argc, char* argv[])
{
    unit_test_data();
    return 0;
}