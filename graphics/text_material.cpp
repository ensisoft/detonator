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

#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/shadersource.h"
#include "graphics/text_material.h"

namespace gfx
{

TextMaterial::TextMaterial(const TextBuffer& text)  : mText(text)
{}
TextMaterial::TextMaterial(TextBuffer&& text)
  : mText(std::move(text))
{}

bool TextMaterial::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const
{
    raster.blending = RasterState::Blending::Transparent;

    const auto hash = mText.GetHash();
    const auto& gpu_id = std::to_string(hash);
    auto* texture = device.FindTexture(gpu_id);
    if (!texture)
    {
        // current text rendering use cases for this TextMaterial
        // are such that we expect the rendered geometry to match
        // the underlying rasterized text texture size almost exactly.
        // this means that we can skip the mipmap generation and use
        // a simple fast nearest/linear texture filter without mips.
        const bool mips = false;

        const auto format = mText.GetRasterFormat();
        if (format == TextBuffer::RasterFormat::Bitmap)
        {
            // create the texture object first. The if check above
            // will then act as a throttle and prevent superfluous
            // attempts to rasterize when the contents of the text
            // buffer have not changed.
            texture = device.MakeTexture(gpu_id);
            texture->SetTransient(true); // set transient flag up front to tone down DEBUG noise
            texture->SetName("TextMaterialTexture");

            auto bitmap = mText.RasterizeBitmap();
            if (!bitmap)
                return false;
            const auto width = bitmap->GetWidth();
            const auto height = bitmap->GetHeight();
            texture->Upload(bitmap->GetDataPtr(), width, height, gfx::Texture::Format::AlphaMask, mips);
        }
        else if (format == TextBuffer::RasterFormat::Texture)
        {
            // this is a dynamic text texture, i.e. texture that is used
            // to show text and then discarded when no longer needed
            const auto transient = true;

            texture = mText.RasterizeTexture(gpu_id, "TextMaterialTexture", device, transient);
            if (!texture)
                return false;
            texture->SetTransient(true);
            texture->SetName("TextMaterialTexture");
            // texture->GenerateMips(); << this would be the place to generate mips if needed.
        } else if (format == TextBuffer::RasterFormat::None)
            return false;
        else BUG("Unhandled texture raster format.");

        texture->SetContentHash(hash);
        texture->SetWrapX(Texture::Wrapping::Clamp);
        texture->SetWrapY(Texture::Wrapping::Clamp);
        // see the comment above about mipmaps. it's relevant regarding
        // the possible filtering settings that we can use here.

        // see issue 207
        // https://github.com/ensisoft/detonator/issues/207
#if 0
        if (mPointSampling)
        {
            texture->SetFilter(Texture::MagFilter::Nearest);
            texture->SetFilter(Texture::MinFilter::Nearest);
        }
        else
        {
            texture->SetFilter(Texture::MagFilter::Linear);
            texture->SetFilter(Texture::MinFilter::Linear);
        }
#endif
        texture->SetFilter(Texture::MinFilter::Linear);
        texture->SetFilter(Texture::MagFilter::Linear);

    }
    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kColor", mColor);
    return true;
}

void TextMaterial::ApplyStaticState(const Environment& env, Device& device, gfx::ProgramState& program) const
{}

ShaderSource TextMaterial::GetShader(const Environment& env, const Device& device) const
{
    const auto format = mText.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
    {
        ShaderSource source;
        source.SetType(ShaderSource::Type::Fragment);
        source.SetPrecision(ShaderSource::Precision::High);
        source.SetVersion(ShaderSource::Version::GLSL_300);
        source.AddUniform("kTexture", ShaderSource::UniformType::Sampler2D);
        source.AddUniform("kColor", ShaderSource::UniformType::Color4f);
        source.AddUniform("kTime", ShaderSource::UniformType::Float);
        source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
        source.AddSource(R"(
void FragmentShaderMain() {
   float alpha = texture(kTexture, vTexCoord).a;
   vec4 color = vec4(kColor.r, kColor.g, kColor.b, kColor.a * alpha);
   fs_out.color = color;
}
        )");
        return source;
    }
    else if (format == TextBuffer::RasterFormat::Texture)
    {
        ShaderSource source;
        source.SetType(ShaderSource::Type::Fragment);
        source.SetPrecision(ShaderSource::Precision::High);
        source.SetVersion(ShaderSource::Version::GLSL_300);
        source.AddUniform("kTexture", ShaderSource::UniformType::Sampler2D);
        source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
        source.AddSource(R"(
void FragmentShaderMain() {
    mat3 flip = mat3(vec3(1.0,  0.0, 0.0),
                     vec3(0.0, -1.0, 0.0),
                     vec3(0.0,  1.0, 0.0));
    vec3 tex = flip * vec3(vTexCoord.xy, 1.0);
    vec4 color = texture(kTexture, tex.xy);
    fs_out.color = color;
}
    )");
        return source;
    }
    else if (format == TextBuffer::RasterFormat::None)
        return ShaderSource();
    else BUG("Unhandled texture raster format.");
    return ShaderSource();
}

std::string TextMaterial::GetShaderId(const Environment& env) const
{
    size_t hash = 0;

    const auto format = mText.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
        hash = base::hash_combine(hash, "text-shader-bitmap");
    else if (format == TextBuffer::RasterFormat::Texture)
        hash = base::hash_combine(hash, "text-shader-texture");
    else if (format == TextBuffer::RasterFormat::None)
        hash = base::hash_combine(hash, "text-shader-none");
    else BUG("Unhandled texture raster format.");
    return std::to_string(hash);
}

std::string TextMaterial::GetShaderName(const Environment&) const
{
    const auto format = mText.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
        return "BitmapTextShader";
    else if (format == TextBuffer::RasterFormat::Texture)
        return "TextureTextShader";
    else BUG("Unhandled texture raster format.");
    return "";
}

void TextMaterial::ComputeTextMetrics(unsigned int* width, unsigned int* height) const
{
    mText.ComputeTextMetrics(width, height);
}

TextMaterial CreateMaterialFromText(const std::string& text,
                                    const std::string& font,
                                    const gfx::Color4f& color,
                                    unsigned font_size_px,
                                    unsigned raster_width,
                                    unsigned raster_height,
                                    unsigned text_align,
                                    unsigned text_prop,
                                    float line_height)
{
    TextBuffer buff(raster_width, raster_height);
    if ((text_align & 0xf) == TextAlign::AlignTop)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignTop);
    else if((text_align & 0xf) == TextAlign::AlignVCenter)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignCenter);
    else if ((text_align & 0xf) == TextAlign::AlignBottom)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignBottom);

    if ((text_align & 0xf0) == TextAlign::AlignLeft)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignLeft);
    else if ((text_align & 0xf0) == TextAlign::AlignHCenter)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignCenter);
    else if ((text_align & 0xf0) == TextAlign::AlignRight)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignRight);

    const bool underline = text_prop & TextProp::Underline;
    const bool blinking  = text_prop & TextProp::Blinking;

    // Add blob of text in the buffer.
    TextBuffer::Text text_and_style;
    text_and_style.text       = text;
    text_and_style.font       = font;
    text_and_style.fontsize   = font_size_px;
    text_and_style.underline  = underline;
    text_and_style.lineheight = line_height;
    buff.SetText(text_and_style);

    TextMaterial material(std::move(buff));
    material.SetPointSampling(true);
    material.SetColor(color);
    return material;
}

std::unique_ptr<TextMaterial> CreateMaterialInstance(const TextBuffer& text)
{
    return std::make_unique<TextMaterial>(text);
}
std::unique_ptr<TextMaterial> CreateMaterialInstance(TextBuffer&& text)
{
    return std::make_unique<TextMaterial>(std::move(text));
}

} // namespace
