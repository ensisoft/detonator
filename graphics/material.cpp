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
#  include <base64/base64.h>
#include "warnpop.h"

#include <set>

#include "base/logging.h"
#include "base/format.h"
#include "base/hash.h"
#include "base/json.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/material.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "graphics/program.h"
#include "graphics/resource.h"
#include "graphics/loader.h"
#include "graphics/shaderpass.h"
#include "graphics/algo.h"

//                  == Notes about shaders ==
// 1. Shaders are specific to a device within compatibility constraints
// but a GLES 2 device context cannot use GL ES 3 shaders for example.
//
// 2. Logically shaders are part of the higher level graphics system, i.e.
// they express some particular algorithm/technique in a device dependant
// way, so logically they belong to the higher layer (i.e. roughly painter
// material level of abstraction).
//
// Basically this means that the device abstraction breaks down because
// we need this device specific (shader) code that belongs to the higher
// level of the system.
//
// A fancy way of solving this would be to use a shader translator, i.e.
// a device that can transform shader from one language version to another
// language version or even from one shader language to another, for example
// from GLSL to HLSL/MSL/SPIR-V.  In fact this is exactly what libANGLE does
// when it implements GL ES2/3 on top of DX9/11/Vulkan/GL/Metal.
// But this only works when the particular underlying implementations can
// support similar feature sets. For example instanced rendering is not part
// of ES 2 but >= ES 3 so therefore it's not possible to translate ES3 shaders
// using instanced rendering into ES2 shaders directly but the whole rendering
// path must be changed to not use instanced rendering.
//
// This then means that if a system was using a shader translation layer (such
// as libANGLE) any decent system would still require several different rendering
// paths. For example a primary "high end path" and a "fallback path" for low end
// devices. These different paths would use different feature sets (such as instanced
// rendering) and also (in many cases) require different shaders to be written to
// fully take advantage of the graphics API features.
// Then these two different rendering paths would/could use a shader translation
// layer in order to use some specific graphics API. (through libANGLE)
//
//            <<Device>>
//              |    |
//     <ES2 Device> <ES3 Device>
//             |      |
//            <libANGLE>
//                 |
//      [GL/DX9/DX11/VULKAN/Metal]
//
// Where "ES2 Device" provides the low end graphics support and "ES3 Device"
// device provides the "high end" graphics rendering path support.
// Both device implementations would require their own shaders which would then need
// to be translated to the device specific shaders matching the feature level.
//
// Finally, who should own the shaders and who should edit them ?
//   a) The person who is using the graphics library ?
//   b) The person who is writing the graphics library ?
//
// The answer seems to be mostly the latter (b), i.e. in most cases the
// graphics functionality should work "out of the box" and the graphics
// library should *just work* without the user having to write shaders.
// However there's a need that user might want to write their own special
// shaders because they're cool to do so and want some special customized
// shader effect.
//

namespace {
    using namespace base;
    std::string ToConst(float value)
    { return base::ToChars(value); }
    std::string ToConst(const gfx::Color4f& color)
    {
        return base::FormatString("vec4(%1,%2,%3,%4)",
                                  ToChars(color.Red()),
                                  ToChars(color.Green()),
                                  ToChars(color.Blue()),
                                  ToChars(color.Alpha()));
    }
    std::string ToConst(const glm::vec2& vec2)
    { return base::FormatString("vec2(%1,%2)", ToChars(vec2.x), ToChars(vec2.y)); }
    std::string ToConst(const glm::vec3& vec3)
    { return base::FormatString("vec3(%1,%2,%3)", ToChars(vec3.x), ToChars(vec3.y), ToChars(vec3.z)); }
    std::string ToConst(const glm::vec4& vec4)
    { return base::FormatString("vec4(%1,%2,%3,%4)", ToChars(vec4.x), ToChars(vec4.y), ToChars(vec4.z), ToChars(vec4.w)); }

    struct ShaderData {
        float gamma = 0.0f;
        float texture_rotation = 0;
        glm::vec2 texture_scale;
        glm::vec3 texture_velocity;
        gfx::Color4f base_color;
        gfx::Color4f color_map[4];
        glm::vec2 gradient_offset;
    };
    std::string FoldUniforms(const std::string& src, const ShaderData& data)
    {
        std::string code;
        std::string line;
        std::stringstream ss(src);
        while (std::getline(ss, line))
        {
            if (base::Contains(line, "uniform"))
            {
                const auto original = line;
                if (base::Contains(line, "kGamma"))
                    line = base::FormatString("const float kGamma = %1;", ToConst(data.gamma));
                else if (base::Contains(line, "kBaseColor"))
                    line = base::FormatString("const vec4 kBaseColor = %1;", ToConst(data.base_color));
                else if (base::Contains(line, "kTextureScale"))
                    line = base::FormatString("const vec2 kTextureScale = %1;", ToConst(data.texture_scale));
                else if (base::Contains(line, "kTextureVelocityXY"))
                    line = base::FormatString("const vec2 kTextureVelocityXY = %1;", ToConst(glm::vec2(data.texture_velocity)));
                else if (base::Contains(line, "kTextureVelocityZ"))
                    line = base::FormatString("const float kTextureVelocityZ = %1;", ToConst(data.texture_velocity.z));
                else if (base::Contains(line, "kTextureRotation"))
                    line = base::FormatString("const float kTextureRotation = %1;", ToConst(data.texture_rotation));
                else if (base::Contains(line, "kColor0"))
                    line = base::FormatString("const vec4 kColor0 = %1;", ToConst(data.color_map[0]));
                else if (base::Contains(line, "kColor1"))
                    line = base::FormatString("const vec4 kColor1 = %1;", ToConst(data.color_map[1]));
                else if (base::Contains(line, "kColor2"))
                    line = base::FormatString("const vec4 kColor2 = %1;", ToConst(data.color_map[2]));
                else if (base::Contains(line, "kColor3"))
                    line = base::FormatString("const vec4 kColor3 = %1;", ToConst(data.color_map[3]));
                else if (base::Contains(line, "kOffset"))
                    line = base::FormatString("const vec2 kOffset = %1;", ToConst(data.gradient_offset));
                if (original != line)
                    DEBUG("'%1' => '%2", original, line);
            }
            code.append(line);
            code.append("\n");
        }
        return code;
    }
} // namespace

namespace gfx
{

Texture* detail::TextureTextureSource::Upload(const Environment& env, Device& device) const
{
    return device.FindTexture(mGpuId);
}

void detail::TextureTextureSource::IntoJson(data::Writer& data) const
{
    data.Write("id",     mId);
    data.Write("name",   mName);
    data.Write("gpu_id", mGpuId);
}

bool detail::TextureTextureSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",     &mId);
    ok &= data.Read("name",   &mName);
    ok &= data.Read("gpu_id", &mGpuId);
    return ok;
}

size_t detail::TextureFileSource::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFile);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mColorSpace);
    hash = base::hash_combine(hash, mEffects);
    return hash;
}

Texture* detail::TextureFileSource::Upload(const Environment& env, Device& device) const
{
    // using the mFile URI is *not* enough to uniquely
    // identify this texture object on the GPU because it's
    // possible that the *same* texture object (same underlying file)
    // is used with *different* flags in another material.
    // in other words, "foo.png with premultiplied alpha" must be
    // a different GPU texture object than "foo.png with straight alpha".
    size_t gpu_hash = 0;
    gpu_hash = base::hash_combine(gpu_hash, mFile);
    gpu_hash = base::hash_combine(gpu_hash, mColorSpace);
    gpu_hash = base::hash_combine(gpu_hash, mFlags);
    gpu_hash = base::hash_combine(gpu_hash, mEffects);
    const auto& gpu_id = std::to_string(gpu_hash);

    auto* texture = device.FindTexture(gpu_id);
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content) {
        content_hash = base::hash_combine(0, mFile);
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }

    if (!texture) {
        texture = device.MakeTexture(gpu_id);
        texture->SetName(mName);
    }

    if (const auto& bitmap = GetData())
    {
        constexpr auto generate_mips = true;
        constexpr auto skip_mips = false;
        const auto sRGB = mColorSpace == ColorSpace::sRGB;
        texture->SetContentHash(content_hash);
        texture->Upload(bitmap->GetDataPtr(), bitmap->GetWidth(), bitmap->GetHeight(),
            Texture::DepthToFormat(bitmap->GetDepthBits(), sRGB), generate_mips);

        if (mEffects.test(Effect::Blur))
        {
            const auto format = texture->GetFormat();
            if (format == gfx::Texture::Format::RGBA ||
                format == gfx::Texture::Format::sRGBA)
            {
                texture->SetFilter(Texture::MinFilter::Linear);
                texture->SetFilter(Texture::MagFilter::Linear);
                algo::ApplyBlur(gpu_id, texture, &device);
                texture->GenerateMips();
            } else WARN("Texture blur is not supported on texture format. [file='%1', format='%2']", mFile, format);
        }
        return texture;
    }
    return nullptr;
}

std::shared_ptr<IBitmap> detail::TextureFileSource::GetData() const
{
    DEBUG("Loading texture file. [file='%1']", mFile);
    Image file(mFile);
    if (!file.IsValid())
    {
        ERROR("Failed to load texture. [file='%1']", mFile);
        return nullptr;
    }

    if (file.GetDepthBits() == 8)
        return std::make_shared<AlphaMask>(file.AsBitmap<Grayscale>());
    else if (file.GetDepthBits() == 24)
        return std::make_shared<RgbBitmap>(file.AsBitmap<RGB>());
    else if (file.GetDepthBits() == 32)
    {
        if (!TestFlag(Flags::PremulAlpha))
            return std::make_shared<RgbaBitmap>(file.AsBitmap<RGBA>());

        auto view = file.GetPixelReadView<RGBA>();
        auto ret = std::make_shared<Bitmap<RGBA>>();
        ret->Resize(view.GetWidth(), view.GetHeight());
        DEBUG("Performing alpha premultiply on texture. [file='%1']", mFile);
        PremultiplyAlpha(ret->GetPixelWriteView(), view, true /* srgb */);
        return ret;
    }
    else ERROR("Unexpected texture bit depth. [file='%1']", mFile);

    ERROR("Failed to load texture. [file='%1']", mFile);
    return nullptr;
}
void detail::TextureFileSource::IntoJson(data::Writer& data) const
{
    data.Write("id",         mId);
    data.Write("file",       mFile);
    data.Write("name",       mName);
    data.Write("flags",      mFlags);
    data.Write("colorspace", mColorSpace);
    data.Write("effects",    mEffects);
}
bool detail::TextureFileSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",         &mId);
    ok &= data.Read("file",       &mFile);
    ok &= data.Read("name",       &mName);
    ok &= data.Read("flags",      &mFlags);
    ok &= data.Read("colorspace", &mColorSpace);
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);
    return ok;
}

Texture* detail::TextureBitmapBufferSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(mId);
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content) {
        content_hash = base::hash_combine(content_hash, mBitmap->GetWidth());
        content_hash = base::hash_combine(content_hash, mBitmap->GetHeight());
        content_hash = base::hash_combine(content_hash, mBitmap->GetHash());
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    if (!texture) {
        texture = device.MakeTexture(mId);
        texture->SetName(mName);
    }

    // todo: assuming linear color space now.
    constexpr auto sRGB = false;
    constexpr auto generate_mips = true;

    texture->SetContentHash(content_hash);
    texture->Upload(mBitmap->GetDataPtr(), mBitmap->GetWidth(), mBitmap->GetHeight(),
        Texture::DepthToFormat(mBitmap->GetDepthBits(), sRGB), generate_mips);
    return texture;
}

void detail::TextureBitmapBufferSource::IntoJson(data::Writer& data) const
{
    const auto depth = mBitmap->GetDepthBits() / 8;
    const auto width = mBitmap->GetWidth();
    const auto height = mBitmap->GetHeight();
    const auto bytes = width * height * depth;
    data.Write("id",     mId);
    data.Write("name",   mName);
    data.Write("width",  width);
    data.Write("height", height);
    data.Write("depth",  depth);
    data.Write("data", base64::Encode((const unsigned char*)mBitmap->GetDataPtr(), bytes));
}
bool detail::TextureBitmapBufferSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    unsigned width = 0;
    unsigned height = 0;
    unsigned depth = 0;
    std::string base64;
    ok &= data.Read("id",     &mId);
    ok &= data.Read("name",   &mName);
    ok &= data.Read("width",  &width);
    ok &= data.Read("height", &height);
    ok &= data.Read("depth",  &depth);
    ok &= data.Read("data",   &base64);

    const auto& bits = base64::Decode(base64);
    if (depth == 1 && bits.size() == width*height)
        mBitmap = std::make_shared<GrayscaleBitmap>((const Grayscale*) &bits[0], width, height);
    else if (depth == 3 && bits.size() == width*height*3)
        mBitmap = std::make_shared<RgbBitmap>((const RGB*) &bits[0], width, height);
    else if (depth == 4 && bits.size() == width*height*4)
        mBitmap = std::make_shared<RgbaBitmap>((const RGBA*) &bits[0], width, height);
    else return false;
    return ok;
}

Texture* detail::TextureBitmapGeneratorSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(mId);
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content) {
        content_hash = mGenerator->GetHash();
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    if (!texture) {
        texture = device.MakeTexture(mId);
        texture->SetName(mName);
    }

    // todo: assuming linear color space now
    constexpr auto sRGB = false;
    constexpr auto generate_mips = true;

    if (const auto& bitmap = mGenerator->Generate())
    {
        texture->SetContentHash(content_hash);
        texture->Upload(bitmap->GetDataPtr(), bitmap->GetWidth(), bitmap->GetHeight(),
            Texture::DepthToFormat(bitmap->GetDepthBits(), sRGB), generate_mips);
        return texture;
    }
    return nullptr;
}

void detail::TextureBitmapGeneratorSource::IntoJson(data::Writer& data) const
{
    auto chunk = data.NewWriteChunk();
    mGenerator->IntoJson(*chunk);
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("function", mGenerator->GetFunction());
    data.Write("generator", std::move(chunk));
}
bool detail::TextureBitmapGeneratorSource::FromJson(const data::Reader& data)
{
    IBitmapGenerator::Function function;
    bool ok = true;
    ok &= data.Read("id",       &mId);
    ok &= data.Read("name",     &mName);
    if (!data.Read("function", &function))
        return false;

    if (function == IBitmapGenerator::Function::Noise)
        mGenerator = std::make_unique<NoiseBitmapGenerator>();
    else BUG("Unhandled bitmap generator type.");

    const auto& chunk = data.GetReadChunk("generator");
    if (!chunk || !mGenerator->FromJson(*chunk))
        return false;

    return ok;
}

std::shared_ptr<IBitmap> detail::TextureTextBufferSource::GetData() const
{
    // since this interface is returning a CPU side bitmap object
    // there's no way to use a texture based (bitmap) font here.
    if (mTextBuffer.GetRasterFormat() == TextBuffer::RasterFormat::Bitmap)
        return mTextBuffer.RasterizeBitmap();
    return nullptr;
}

Texture* detail::TextureTextBufferSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(mId);
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content) {
        content_hash = base::hash_combine(content_hash, mTextBuffer.GetHash());
        content_hash = base::hash_combine(content_hash, mEffects);
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    const auto format = mTextBuffer.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
    {
        if (!texture)
        {
            texture = device.MakeTexture(mId);
            texture->SetName(mName);
        }
        if (const auto& mask = mTextBuffer.RasterizeBitmap())
        {
            constexpr auto generate_mips = true;
            texture->SetContentHash(content_hash);
            texture->Upload(mask->GetDataPtr(), mask->GetWidth(), mask->GetHeight(), Texture::Format::Grayscale, generate_mips);
        }
    }
    else if (format == TextBuffer::RasterFormat::Texture)
    {
        if (auto* texture = mTextBuffer.RasterizeTexture(mId, mName, device))
        {
            DEBUG("Rasterized new texture from text buffer. [name='%1']", mName);
            texture->SetName(mName);
            texture->SetFilter(Texture::MinFilter::Linear);
            texture->SetFilter(Texture::MagFilter::Linear);
            texture->SetContentHash(content_hash);

            // The frame buffer render produces a texture that doesn't play nice with
            // model space texture coordinates right now. Simplest solution for now is
            // to simply flip it horizontally...
            algo::FlipTexture(mId, texture, &device, algo::FlipDirection::Horizontal);

            if (mEffects.test(Effect::Blur))
            {
                const auto format = texture->GetFormat();
                if (format == gfx::Texture::Format::RGBA || format == gfx::Texture::Format::sRGBA)
                    algo::ApplyBlur(mId, texture, &device);
                else WARN("Texture blur is not supported on texture format. [name='%1', format=%2]", mName, format);
            }
            texture->GenerateMips();
            return texture;
        } else ERROR("Failed to rasterize texture from text buffer. [name='%1']", mName);
        return nullptr;
    }
    return nullptr;
}

void detail::TextureTextBufferSource::IntoJson(data::Writer& data) const
{
    auto chunk = data.NewWriteChunk();
    mTextBuffer.IntoJson(*chunk);
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("effects", mEffects);
    data.Write("buffer", std::move(chunk));
}
bool detail::TextureTextBufferSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);

    const auto& chunk = data.GetReadChunk("buffer");
    if (!chunk)
        return false;

    ok &= mTextBuffer.FromJson(*chunk);
    return ok;
}

TextureMap::TextureMap(const TextureMap& other, bool copy)
{
    mId = copy ? other.mId : base::RandomString(10);
    mName               = other.mName;
    mType               = other.mType;
    mFps                = other.mFps;
    mLooping            = other.mLooping;
    mSamplerName[0]     = other.mSamplerName[0];
    mSamplerName[1]     = other.mSamplerName[1];
    mRectUniformName[0] = other.mRectUniformName[0];
    mRectUniformName[1] = other.mRectUniformName[1];
    mSpriteSheet        = other.mSpriteSheet;
    for (const auto& texture : other.mTextures)
    {
        Texture dupe;
        dupe.rect = texture.rect;
        if (texture.source)
            dupe.source = copy ? texture.source->Copy() : texture.source->Clone();
        mTextures.push_back(std::move(dupe));
    }
}

size_t TextureMap::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mFps);
    hash = base::hash_combine(hash, mSamplerName[0]);
    hash = base::hash_combine(hash, mSamplerName[1]);
    hash = base::hash_combine(hash, mRectUniformName[0]);
    hash = base::hash_combine(hash, mRectUniformName[1]);
    hash = base::hash_combine(hash, mLooping);
    hash = base::hash_combine(hash, mSpriteSheet);
    for (const auto& texture : mTextures)
    {
        const auto* source = texture.source.get();
        hash = base::hash_combine(hash, source ? source->GetHash() : 0);
        hash = base::hash_combine(hash, texture.rect);
    }
    return hash;
}

bool TextureMap::BindTextures(const BindingState& state, Device& device, BoundState& result) const
{
    if (mTextures.empty())
        return false;

    if (mType == Type::Sprite)
    {
        const auto frame_interval = 1.0f / std::max(mFps, 0.001f);
        const auto frame_fraction = std::fmod(state.current_time, frame_interval);
        const auto blend_coeff = frame_fraction/frame_interval;
        const auto first_index = (unsigned)(state.current_time/frame_interval);
        const auto frame_count =  GetSpriteSpriteFrameCount();
        const auto max_index = frame_count - 1;
        const auto texture_count = std::min(frame_count, 2u);
        const auto first_frame  = mLooping
                                  ? ((first_index + 0) % frame_count)
                                  : math::clamp(0u, max_index, first_index+0);
        const auto second_frame = mLooping
                                  ? ((first_index + 1) % frame_count)
                                  : math::clamp(0u, max_index, first_index+1);
        const unsigned frame_index[2] = {
                first_frame, second_frame
        };

        TextureSource::Environment texture_source_env;
        texture_source_env.dynamic_content = state.dynamic_content;

        // if we're using a single sprite sheet then the sprite frames
        // are sub-rects inside the first texture rect.
        if (const auto* spritesheet = GetSpriteSheet())
        {
            const auto& sprite = mTextures[0];
            const auto& source = sprite.source;
            const auto& rect   = sprite.rect;
            const auto tile_width = rect.GetWidth() / spritesheet->cols;
            const auto tile_height = rect.GetHeight() / spritesheet->rows;
            const auto tile_xpos = rect.GetX();
            const auto tile_ypos = rect.GetY();

            if (auto* texture = source->Upload(texture_source_env, device))
            {
                for (unsigned i = 0; i < 2; ++i)
                {
                    const auto tile_index = frame_index[i];
                    const auto tile_row = tile_index / spritesheet->cols;
                    const auto tile_col = tile_index % spritesheet->cols;
                    FRect tile_rect;
                    tile_rect.Resize(tile_width, tile_height);
                    tile_rect.Move(tile_xpos, tile_ypos);
                    tile_rect.Translate(tile_col * tile_width, tile_row * tile_height);

                    result.textures[i]      = texture;
                    result.rects[i]         = tile_rect;
                    result.sampler_names[i] = mSamplerName[i];
                    result.rect_names[i]    = mRectUniformName[i];
                }
            } else return false;
        }
        else
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                const auto& sprite = mTextures[frame_index[i]];
                const auto& source = sprite.source;
                if (auto* texture = source->Upload(texture_source_env, device))
                {
                    result.textures[i]      = texture;
                    result.rects[i]         = sprite.rect;
                    result.sampler_names[i] = mSamplerName[i];
                    result.rect_names[i]    = mRectUniformName[i];
                } else return false;
            }
        }
        result.blend_coefficient = blend_coeff;
    }
    else if (mType == Type::Texture2D)
    {
        TextureSource::Environment texture_source_env;
        texture_source_env.dynamic_content = state.dynamic_content;

        if (auto* texture = mTextures[0].source->Upload(texture_source_env, device))
        {
            result.textures[0]       = texture;
            result.rects[0]          = mTextures[0].rect;
            result.sampler_names[0]  = mSamplerName[0];
            result.rect_names[0]     = mRectUniformName[0];
            result.blend_coefficient = 0;
        } else  return false;
    }
    return true;
}

void TextureMap::IntoJson(data::Writer& data) const
{
    data.Write("id",            mId);
    data.Write("name",          mName);
    data.Write("type",          mType);
    data.Write("fps",           mFps);
    data.Write("sampler_name0", mSamplerName[0]);
    data.Write("sampler_name1", mSamplerName[1]);
    data.Write("rect_name0",    mRectUniformName[0]);
    data.Write("rect_name1",    mRectUniformName[1]);
    data.Write("looping",       mLooping);
    if (const auto* spritesheet = GetSpriteSheet())
    {
        data.Write("spritesheet_rows", spritesheet->rows);
        data.Write("spritesheet_cols", spritesheet->cols);
    }

    for (const auto& texture : mTextures)
    {
        auto chunk = data.NewWriteChunk();
        if (texture.source)
            texture.source->IntoJson(*chunk);
        ASSERT(!chunk->HasValue("type"));
        ASSERT(!chunk->HasValue("box"));
        chunk->Write("type",  texture.source->GetSourceType());
        chunk->Write("rect",  texture.rect);
        data.AppendChunk("textures", std::move(chunk));
    }
}

bool TextureMap::FromJson(const data::Reader& data)
{
    bool ok = true;
    if (data.HasValue("id"))
        ok &= data.Read("id", &mId);
    if (data.HasValue("name"))
        ok &= data.Read("name", &mName);

    ok &= data.Read("type",          &mType);
    ok &= data.Read("fps",           &mFps);
    ok &= data.Read("sampler_name0", &mSamplerName[0]);
    ok &= data.Read("sampler_name1", &mSamplerName[1]);
    ok &= data.Read("rect_name0",    &mRectUniformName[0]);
    ok &= data.Read("rect_name1",    &mRectUniformName[1]);
    ok &= data.Read("looping",       &mLooping);

    SpriteSheet spritesheet;
    if (data.HasValue("spritesheet_rows") &&
        data.HasValue("spritesheet_cols"))
    {
        ok &= data.Read("spritesheet_rows", &spritesheet.rows);
        ok &= data.Read("spritesheet_cols", &spritesheet.cols);
        mSpriteSheet = spritesheet;
    }

    for (unsigned i=0; i<data.GetNumChunks("textures"); ++i)
    {
        const auto& chunk = data.GetReadChunk("textures", i);
        TextureSource::Source type;
        if (chunk->Read("type", &type))
        {
            std::unique_ptr<TextureSource> source;
            if (type == TextureSource::Source::Filesystem)
                source = std::make_unique<detail::TextureFileSource>();
            else if (type == TextureSource::Source::TextBuffer)
                source = std::make_unique<detail::TextureTextBufferSource>();
            else if (type == TextureSource::Source::BitmapBuffer)
                source = std::make_unique<detail::TextureBitmapBufferSource>();
            else if (type == TextureSource::Source::BitmapGenerator)
                source = std::make_unique<detail::TextureBitmapGeneratorSource>();
            else if (type == TextureSource::Source::Texture)
                source = std::make_unique<detail::TextureTextureSource>();
            else BUG("Unhandled texture source type.");

            Texture texture;
            ok &= source->FromJson(*chunk);
            ok &= chunk->Read("rect", &texture.rect);
            texture.source = std::move(source);
            mTextures.push_back(std::move(texture));
        }
    }
    return ok;
}

bool TextureMap::FromLegacyJsonTexture2D(const data::Reader& data)
{
    bool ok = true;
    FRect rect;
    ok &= data.Read("rect",         &rect);
    ok &= data.Read("sampler_name", &mSamplerName[0]);
    ok &= data.Read("rect_name",    &mRectUniformName[0]);

    const auto& texture = data.GetReadChunk("texture");
    if (!texture)
        return false;

    TextureSource::Source type;
    if (!texture->Read("type", &type))
        return false;

    std::unique_ptr<TextureSource> source;
    if (type == TextureSource::Source::Filesystem)
        source = std::make_unique<detail::TextureFileSource>();
    else if (type == TextureSource::Source::TextBuffer)
        source = std::make_unique<detail::TextureTextBufferSource>();
    else if (type == TextureSource::Source::BitmapBuffer)
        source = std::make_unique<detail::TextureBitmapBufferSource>();
    else if (type == TextureSource::Source::BitmapGenerator)
        source = std::make_unique<detail::TextureBitmapGeneratorSource>();
    else if (type == TextureSource::Source::Texture)
        source = std::make_unique<detail::TextureTextureSource>();
    else BUG("Unhandled texture source type.");

    if (!source->FromJson(*texture))
        return false;

    if (mTextures.empty())
        mTextures.resize(1);
    mTextures[0].source = std::move(source);
    mTextures[0].rect   = rect;
    return ok;
}

size_t TextureMap::FindTextureSourceIndexById(const std::string& id) const
{
    size_t i=0;
    for (i=0; i<GetNumTextures(); ++i)
    {
        auto* source = GetTextureSource(i);
        if (source->GetId() == id)
            break;
    }
    return i;
}

size_t TextureMap::FindTextureSourceIndexByName(const std::string& name) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextures(); ++i)
    {
        auto* source = GetTextureSource(i);
        if (source->GetName() == name)
            break;
    }
    return i;
}

TextureMap* TextureMap::AsSpriteMap()
{
    if (mType == Type::Sprite)
        return this;
    return nullptr;
}
TextureMap* TextureMap::AsTextureMap2D()
{
    if (mType == Type::Texture2D)
        return this;
    return nullptr;
}

const TextureMap* TextureMap::AsSpriteMap() const
{
    if (mType == Type::Sprite)
        return this;
    return nullptr;
}
const TextureMap* TextureMap::AsTextureMap2D() const
{
    if (mType == Type::Texture2D)
        return this;
    return nullptr;
}

unsigned TextureMap::GetSpriteSpriteFrameCount() const
{
    if (mType != Type::Sprite)
        return 0;

    if (const auto* ptr = GetSpriteSheet())
        return ptr->cols * ptr->rows;

    return mTextures.size();
}

TextureMap& TextureMap::operator=(const TextureMap& other)
{
    if (this == &other)
        return *this;
    TextureMap tmp(other, true);
    std::swap(mId,                 tmp.mId);
    std::swap(mName,               tmp.mName);
    std::swap(mType,               tmp.mType);
    std::swap(mFps,                tmp.mFps);
    std::swap(mTextures,           tmp.mTextures);
    std::swap(mSamplerName[0],     tmp.mSamplerName[0]);
    std::swap(mSamplerName[1],     tmp.mSamplerName[1]);
    std::swap(mRectUniformName[0], tmp.mRectUniformName[0]);
    std::swap(mRectUniformName[1], tmp.mRectUniformName[1]);
    std::swap(mLooping,            tmp.mLooping);
    std::swap(mSpriteSheet,        tmp.mSpriteSheet);
    return *this;
}

MaterialClass::MaterialClass(const MaterialClass& other, bool copy)
{
    mClassId          = copy ? other.mClassId : base::RandomString(10);
    mName             = other.mName;
    mType             = other.mType;
    mGamma            = other.mGamma;
    mFlags            = other.mFlags;
    mShaderUri        = other.mShaderUri;
    mShaderSrc        = other.mShaderSrc;
    mActiveTextureMap = other.mActiveTextureMap;
    mParticleAction   = other.mParticleAction;
    mSurfaceType      = other.mSurfaceType;
    mTextureRotation  = other.mTextureRotation;
    mTextureScale     = other.mTextureScale;
    mTextureVelocity  = other.mTextureVelocity;
    mTextureMinFilter = other.mTextureMinFilter;
    mTextureMagFilter = other.mTextureMagFilter;
    mTextureWrapX     = other.mTextureWrapX;
    mTextureWrapY     = other.mTextureWrapY;
    mColorMap[0]      = other.mColorMap[0];
    mColorMap[1]      = other.mColorMap[1];
    mColorMap[2]      = other.mColorMap[2];
    mColorMap[3]      = other.mColorMap[3];
    mColorWeight      = other.mColorWeight;
    mUniforms         = other.mUniforms;

    for (const auto& map : other.mTextureMaps)
    {
        mTextureMaps.push_back(copy ? map->Copy() : map->Clone());
    }
}

std::string MaterialClass::GetProgramId(const State& state) const noexcept
{
    size_t hash = 0;
    hash = state.shader_pass->GetHash();
    hash = base::hash_combine(hash, mType);

    if (mType == Type::Color)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, mGamma);
            hash = base::hash_combine(hash, mColorMap[ColorIndex::BaseColor]);
        }
    }
    else if (mType == Type::Gradient)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, mGamma);
            hash = base::hash_combine(hash, mColorMap[0]);
            hash = base::hash_combine(hash, mColorMap[1]);
            hash = base::hash_combine(hash, mColorMap[2]);
            hash = base::hash_combine(hash, mColorMap[3]);
            hash = base::hash_combine(hash, mColorWeight);
        }
    }
    else if (mType == Type::Sprite)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, mGamma);
            hash = base::hash_combine(hash, mColorMap[ColorIndex::BaseColor]);
            hash = base::hash_combine(hash, mTextureScale);
            hash = base::hash_combine(hash, mTextureVelocity);
            hash = base::hash_combine(hash, mTextureRotation);
        }
    }
    else if (mType == Type::Texture)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mColorMap[ColorIndex::BaseColor]);
        hash = base::hash_combine(hash, mTextureScale);
        hash = base::hash_combine(hash, mTextureVelocity);
        hash = base::hash_combine(hash, mTextureRotation);
    }
    else if (mType == Type::Custom)
    {
        hash = base::hash_combine(hash, mShaderSrc);
        hash = base::hash_combine(hash, mShaderUri);
    } else BUG("Unknown material type.");

    return std::to_string(hash);
}

std::size_t MaterialClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mShaderUri);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mActiveTextureMap);
    hash = base::hash_combine(hash, mParticleAction);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mTextureRotation);
    hash = base::hash_combine(hash, mTextureScale);
    hash = base::hash_combine(hash, mTextureVelocity);
    hash = base::hash_combine(hash, mTextureMinFilter);
    hash = base::hash_combine(hash, mTextureMagFilter);
    hash = base::hash_combine(hash, mTextureWrapX);
    hash = base::hash_combine(hash, mTextureWrapY);
    hash = base::hash_combine(hash, mColorMap[0]);
    hash = base::hash_combine(hash, mColorMap[1]);
    hash = base::hash_combine(hash, mColorMap[2]);
    hash = base::hash_combine(hash, mColorMap[3]);
    hash = base::hash_combine(hash, mColorWeight);
    hash = base::hash_combine(hash, mFlags);

    // remember that the order of uniforms (and texturemaps)
    // can change between IntoJson/FromJson! This can result
    // in a different order of items in the *unordered* maps !
    // This then can result in a different hash value being
    // computed. To avoid this non-sense problem use a set
    // to give a consistent iteration order over the uniforms
    // and texture maps.
    std::set<std::string> keys;
    for (const auto& uniform : mUniforms)
        keys.insert(uniform.first);

    for (const auto& key : keys)
    {
        const auto* uniform = base::SafeFind(mUniforms, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *uniform);
    }

    for (const auto& map : mTextureMaps)
    {
        hash = base::hash_combine(hash, map->GetHash());
    }
    return hash;
}

Shader* MaterialClass::GetShader(const State& state, Device& device) const noexcept
{
    if (auto* shader = device.FindShader(GetProgramId(state)))
        return shader;

    if (mType == Type::Color)
        return GetColorShader(state, device);
    else if (mType == Type::Gradient)
        return GetGradientShader(state, device);
    else if (mType == Type::Sprite)
        return GetSpriteShader(state, device);
    else if (mType == Type::Texture)
        return GetTextureShader(state, device);
    else if (mType == Type::Custom)
        return GetCustomShader(state, device);
    else BUG("Unknown material type.");

    return nullptr;
}

bool MaterialClass::ApplyDynamicState(const State& state, Device& device, Program& program) const noexcept
{
    if (mType == Type::Color)
    {
        if (!IsStatic())
        {
            SetUniform("kGamma",     state.uniforms, mGamma,       program);
            SetUniform("kBaseColor", state.uniforms, mColorMap[0], program);
        }
    }
    else if (mType == Type::Gradient)
    {
        program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
        if (!IsStatic())
        {
            SetUniform("kGamma",  state.uniforms, mGamma,       program);
            SetUniform("kColor0", state.uniforms, mColorMap[0], program);
            SetUniform("kColor1", state.uniforms, mColorMap[1], program);
            SetUniform("kColor2", state.uniforms, mColorMap[2], program);
            SetUniform("kColor3", state.uniforms, mColorMap[3], program);
            SetUniform("kOffset", state.uniforms, mColorWeight, program);
        }
    }
    else if (mType == Type::Sprite)
        return ApplySpriteDynamicState(state, device, program);
    else if (mType == Type::Texture)
        return ApplyTextureDynamicState(state, device, program);
    else if (mType == Type::Custom)
        return ApplyCustomDynamicState(state, device, program);
    else BUG("Unknown material type.");

    return true;
}

void MaterialClass::ApplyStaticState(const State& state, Device& device, Program& program) const noexcept
{
    if (mType == Type::Color)
    {
        program.SetUniform("kGamma",     mGamma);
        program.SetUniform("kBaseColor", mColorMap[ColorIndex::BaseColor]);
    }
    else if (mType == Type::Gradient)
    {
        program.SetUniform("kGamma",  mGamma);
        program.SetUniform("kColor0", mColorMap[0]);
        program.SetUniform("kColor1", mColorMap[1]);
        program.SetUniform("kColor2", mColorMap[2]);
        program.SetUniform("kColor3", mColorMap[3]);
        program.SetUniform("kOffset", mColorWeight);
    }
    else if (mType == Type::Sprite)
    {
        program.SetUniform("kGamma",             mGamma);
        program.SetUniform("kBaseColor",         mColorMap[ColorIndex::BaseColor]);
        program.SetUniform("kTextureScale",      mTextureScale.x, mTextureScale.y);
        program.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
        program.SetUniform("kTextureVelocityZ",  mTextureVelocity.z);
        program.SetUniform("kTextureRotation",   mTextureRotation);
    }
    else if (mType == Type::Texture)
    {
        program.SetUniform("kGamma",             mGamma);
        program.SetUniform("kBaseColor",         mColorMap[ColorIndex::BaseColor]);
        program.SetUniform("kTextureScale",      mTextureScale.x, mTextureScale.y);
        program.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
        program.SetUniform("kTextureVelocityZ",  mTextureVelocity.z);
        program.SetUniform("kTextureRotation",   mTextureRotation);
    }
    else if (mType == Type::Custom)
    {
        // nothing to do here, static state should be in the shader
        // already, either by the shader programmer or by the shader
        // source generator.
    } else BUG("Unknown material type.");
}

void MaterialClass::IntoJson(data::Writer& data) const
{
    data.Write("type",               mType);
    data.Write("id",                 mClassId);
    data.Write("name",               mName);
    data.Write("shader_uri",         mShaderUri);
    data.Write("shader_src",         mShaderSrc);
    data.Write("active_texture_map", mActiveTextureMap);
    data.Write("surface",            mSurfaceType);
    data.Write("particle_action",    mParticleAction);
    data.Write("gamma",              mGamma);
    data.Write("texture_min_filter", mTextureMinFilter);
    data.Write("texture_mag_filter", mTextureMagFilter);
    data.Write("texture_wrap_x",     mTextureWrapX);
    data.Write("texture_wrap_y",     mTextureWrapY);
    data.Write("texture_scale",      mTextureScale);
    data.Write("texture_velocity",   mTextureVelocity);
    data.Write("texture_rotation",   mTextureRotation);
    data.Write("color_map0",         mColorMap[0]);
    data.Write("color_map1",         mColorMap[1]);
    data.Write("color_map2",         mColorMap[2]);
    data.Write("color_map3",         mColorMap[3]);
    data.Write("color_weight",       mColorWeight);
    data.Write("flags",              mFlags);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> uniform_keys;
    for (const auto& uniform : mUniforms)
        uniform_keys.insert(uniform.first);

    for (const auto& key : uniform_keys)
    {
        const auto& uniform = *base::SafeFind(mUniforms, key);
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        std::visit([&chunk](const auto& variant_value) {
            chunk->Write("value", variant_value);
        }, uniform);
        data.AppendChunk("uniforms", std::move(chunk));
    }

    for (const auto& map : mTextureMaps)
    {
        auto chunk = data.NewWriteChunk();
        map->IntoJson(*chunk);
        ASSERT(chunk->HasValue("name")); // moved into TextureMap::IntoJson
        ASSERT(chunk->HasValue("type")); // moved into TextureMap::IntoJson
        //chunk->Write("name", key);
        //chunk->Write("type", map->GetType());
        data.AppendChunk("texture_maps", std::move(chunk));
    }
}

bool MaterialClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("type",               &mType);
    ok &= data.Read("id",                 &mClassId);
    ok &= data.Read("name",               &mName);
    ok &= data.Read("shader_uri",         &mShaderUri);
    ok &= data.Read("shader_src",         &mShaderSrc);
    ok &= data.Read("active_texture_map", &mActiveTextureMap);
    ok &= data.Read("surface",            &mSurfaceType);
    ok &= data.Read("particle_action",    &mParticleAction);
    ok &= data.Read("gamma",              &mGamma);
    ok &= data.Read("color_map0",         &mColorMap[0]);
    ok &= data.Read("color_map1",         &mColorMap[1]);
    ok &= data.Read("color_map2",         &mColorMap[2]);
    ok &= data.Read("color_map3",         &mColorMap[3]);
    ok &= data.Read("texture_min_filter", &mTextureMinFilter);
    ok &= data.Read("texture_mag_filter", &mTextureMagFilter);
    ok &= data.Read("texture_wrap_x",     &mTextureWrapX);
    ok &= data.Read("texture_wrap_y",     &mTextureWrapY);
    ok &= data.Read("texture_scale",      &mTextureScale);
    ok &= data.Read("texture_velocity",   &mTextureVelocity);
    ok &= data.Read("texture_rotation",   &mTextureRotation);
    ok &= data.Read("color_weight",       &mColorWeight);
    ok &= data.Read("flags",              &mFlags);

    if (data.HasValue("static"))
    {
        bool static_content = false;
        ok &= data.Read("static", &static_content);
        mFlags.set(Flags::Static, static_content);
    }

    if (mType == Type::Color)
    {
        if (data.HasValue("color"))
            ok &= data.Read("color", &mColorMap[0]);
    }
    else if (mType == Type::Gradient)
    {
        if (data.HasValue("offset"))
            ok &= data.Read("offset", &mColorWeight);
    }
    else if (mType == Type::Texture)
    {
        if (data.HasValue("color"))
            ok &= data.Read("color", &mColorMap[0]);

        if (!data.HasArray("texture_maps"))
        {
            if (data.HasChunk("texture_map"))
            {
                const auto& chunk = data.GetReadChunk("texture_map");
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(*chunk);
            }
            else
            {
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromLegacyJsonTexture2D(data);
            }
            if (mTextureMaps.size() > 1)
                mTextureMaps.resize(1);
        }
    }
    else if (mType == Type::Sprite)
    {
        if (data.HasValue("color"))
            ok &= data.Read("color", &mColorMap[0]);
        if (data.HasValue("blending"))
        {
            bool blend_frames = false;
            ok &= data.Read("blending", &blend_frames);
            mFlags.set(Flags::BlendFrames, blend_frames);
        }
        else if (data.HasValue("blend_frames"))
        {
            bool blend_frames = false;
            ok &= data.Read("blend_frames", &blend_frames);
            mFlags.set(Flags::BlendFrames, blend_frames);
        }

        if (!data.HasArray("texture_maps"))
        {
            if (data.GetNumChunks("sprites"))
            {
                const auto& chunk = data.GetReadChunk("sprites", 0);
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(*chunk);
            }
            else
            {
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(data);
            }
        }
    }

    for (unsigned i=0; i<data.GetNumChunks("uniforms"); ++i)
    {
        Uniform uniform;
        const auto& chunk = data.GetReadChunk("uniforms", i);
        std::string name;
        ok &= chunk->Read("name", &name);
        ok &= chunk->Read("value", &uniform);
        mUniforms[std::move(name)] = std::move(uniform);
    }

    for (unsigned i=0; i<data.GetNumChunks("texture_maps"); ++i)
    {
        const auto& chunk = data.GetReadChunk("texture_maps", i);
        std::string name;
        TextureMap::Type type;
        if (chunk->Read("type", &type) && chunk->Read("name", &name))
        {
            auto map = std::make_unique<TextureMap>();

            if (type == TextureMap::Type::Texture2D)
            {
                if (!chunk->HasValue("sampler_name0"))
                    ok &= map->FromLegacyJsonTexture2D(*chunk);
                else ok &= map->FromJson(*chunk);
            }
            else
            {
                ok &= map->FromJson(*chunk);
            }
            mTextureMaps.push_back(std::move(map));
        } else ok = false;
    }
    return ok;
}

std::unique_ptr<MaterialClass> MaterialClass::Copy() const
{
    return std::make_unique<MaterialClass>(*this, true);
}

std::unique_ptr<MaterialClass> MaterialClass::Clone() const
{
    return std::make_unique<MaterialClass>(*this, false);
}

void MaterialClass::BeginPacking(TexturePacker* packer) const
{
    for (const auto& map : mTextureMaps)
    {
        for (size_t i=0; i<map->GetNumTextures(); ++i)
        {
            const auto& rect   = map->GetTextureRect(i);
            const auto* source = map->GetTextureSource(i);
            const TexturePacker::ObjectHandle handle = source;

            source->BeginPacking(packer);
            packer->SetTextureBox(handle, rect);

            // when texture rects are used to address a sub rect within the
            // texture wrapping on texture coordinates must be done "manually"
            // since the HW sampler coords are outside the sub rectangle coords.
            // for example if the wrapping is set to wrap on x and our box is
            // 0.25 units the HW sampler would not help us here to wrap when the
            // X coordinate is 0.26. Instead, we need to do the wrap manually.
            // However, this can cause rendering artifacts when texture sampling
            // is done depending on the current filter being used.
            bool can_combine = true;

            const auto& box = rect;
            const auto x = box.GetX();
            const auto y = box.GetY();
            const auto w = box.GetWidth();
            const auto h = box.GetHeight();
            const float eps = 0.001;
            // if the texture uses sub rect then we still have this problem and packing
            // won't make it any worse. in other words if the box is the normal 0.f - 1.0f
            // (meaning whole texture, not some part of it) then combining and using a
            // sub-rect can make the result worse if coordinate wrapping is in fact needed.
            if (math::equals(0.0f, x, eps) &&
                math::equals(0.0f, y, eps) &&
                math::equals(1.0f, w, eps) &&
                math::equals(1.0f, h, eps))
            {
                // is it possible for a texture to go beyond its range and require wrapping?
                // the only case we know here is when texture velocity is non-zero
                // or when texture scaling is used. we consider these properties to be
                // static and not be changed by the game by default at runtime.

                // velocity check
                const bool has_x_velocity = !math::equals(0.0f, mTextureVelocity.x, eps);
                const bool has_y_velocity = !math::equals(0.0f, mTextureVelocity.y, eps);
                if (has_x_velocity && mTextureWrapX == TextureWrapping::Repeat)
                    can_combine = false;
                else if (has_y_velocity && mTextureWrapY == TextureWrapping::Repeat)
                    can_combine = false;

                // scale check
                if (mTextureScale.x > 1.0f && mTextureWrapX == TextureWrapping::Repeat)
                    can_combine = false;
                else if (mTextureScale.y > 1.0f && mTextureWrapY == TextureWrapping::Repeat)
                    can_combine = false;
            }
            packer->SetTextureFlag(handle, TexturePacker::TextureFlags::CanCombine, can_combine);
        }
    }
}

void MaterialClass::FinishPacking(const TexturePacker* packer)
{
    for (auto& map : mTextureMaps)
    {
        for (size_t i=0; i<map->GetNumTextures(); ++i)
        {
            auto* source = map->GetTextureSource(i);
            const TexturePacker::ObjectHandle handle = source;
            source->FinishPacking(packer);
            map->SetTextureRect(i, packer->GetPackedTextureBox(handle));
        }
    }
}

unsigned MaterialClass::FindTextureMapIndexById(const std::string& id) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextureMaps(); ++i)
    {
        const auto* map = GetTextureMap(i);
        if (map->GetId() == id)
            break;
    }
    return i;
}

unsigned MaterialClass::FindTextureMapIndexByName(const std::string& name) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextureMaps(); ++i)
    {
        const auto* map = GetTextureMap(i);
        if (map->GetName() == name)
            break;
    }
    return i;
}

TextureMap* MaterialClass::FindTextureMapByName(const std::string& name)
{
    const auto index = FindTextureMapIndexByName(name);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}
TextureMap* MaterialClass::FindTextureMapById(const std::string& id)
{
    const auto index = FindTextureMapIndexById(id);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}
const TextureMap* MaterialClass::FindTextureMapByName(const std::string& name) const
{
    const auto index = FindTextureMapIndexByName(name);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}
const TextureMap* MaterialClass::FindTextureMapById(const std::string& id) const
{
    const auto index = FindTextureMapIndexById(id);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}

void MaterialClass::SetTexture(std::unique_ptr<TextureSource> source)
{
    if (mTextureMaps.size() != 1)
        mTextureMaps.resize(1);
    if (mTextureMaps[0] == nullptr)
    {
        mTextureMaps[0] = std::make_unique<TextureMap>();
        if (mType == Type::Sprite)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Sprite);
            mTextureMaps[0]->SetName("Sprite");
        }
        else if (mType == Type::Texture)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Texture2D);
            mTextureMaps[0]->SetName("Texture");
        }
    }
    mTextureMaps[0]->SetNumTextures(1);
    mTextureMaps[0]->SetTextureSource(0, std::move(source));
}

void MaterialClass::AddTexture(std::unique_ptr<TextureSource> source)
{
    if (mTextureMaps.size() != 1)
        mTextureMaps.resize(1);
    if (mTextureMaps[0] == nullptr)
    {
        mTextureMaps[0] = std::make_unique<TextureMap>();
        if (mType == Type::Sprite)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Sprite);
            mTextureMaps[0]->SetName("Sprite");
        }
        else if (mType == Type::Texture)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Texture2D);
            mTextureMaps[0]->SetName("Texture");
        }
    }
    const auto count = mTextureMaps[0]->GetNumTextures();
    mTextureMaps[0]->SetNumTextures(count+1);
    mTextureMaps[0]->SetTextureSource(count, std::move(source));
}

void MaterialClass::DeleteTextureMap(const std::string& id) noexcept
{
    base::EraseRemove(mTextureMaps, [&id](const auto& map) {
        return map->GetId() == id;
    });
}
void MaterialClass::DeleteTextureSrc(const std::string& id) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        map->DeleteTexture(index);
    }
}

TextureSource* MaterialClass::FindTextureSource(const std::string& id) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureSource(index);
    }
    return nullptr;
}

const TextureSource* MaterialClass::FindTextureSource(const std::string& id) const noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureSource(index);
    }
    return nullptr;
}

FRect MaterialClass::FindTextureRect(const std::string& id) const noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureRect(index);
    }
    return {};
}

void MaterialClass::SetTextureRect(const std::string& id, const gfx::FRect& rect) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        map->SetTextureRect(index, rect);
        return;
    }
}

// static
std::unique_ptr<MaterialClass> MaterialClass::ClassFromJson(const data::Reader& data)
{
    Type type;
    if (!data.Read("type", &type))
        return nullptr;

    auto klass = std::make_unique<MaterialClass>(type);

    // todo: change the API so that we can also return the OK value.
    bool ok = klass->FromJson(data);
    return klass;
}

MaterialClass& MaterialClass::operator=(const MaterialClass& other)
{
    if (this == &other)
        return *this;

    MaterialClass tmp(other, true);
    std::swap(mClassId         , tmp.mClassId);
    std::swap(mName            , tmp.mName);
    std::swap(mType            , tmp.mType);
    std::swap(mGamma           , tmp.mGamma);
    std::swap(mFlags           , tmp.mFlags);
    std::swap(mShaderUri       , tmp.mShaderUri);
    std::swap(mShaderSrc       , tmp.mShaderSrc);
    std::swap(mActiveTextureMap, tmp.mActiveTextureMap);
    std::swap(mParticleAction  , tmp.mParticleAction);
    std::swap(mSurfaceType     , tmp.mSurfaceType);
    std::swap(mTextureRotation , tmp.mTextureRotation);
    std::swap(mTextureScale    , tmp.mTextureScale);
    std::swap(mTextureVelocity , tmp.mTextureVelocity);
    std::swap(mTextureMinFilter, tmp.mTextureMinFilter);
    std::swap(mTextureMagFilter, tmp.mTextureMagFilter);
    std::swap(mTextureWrapX    , tmp.mTextureWrapX);
    std::swap(mTextureWrapY    , tmp.mTextureWrapY);
    std::swap(mColorMap[0]     , tmp.mColorMap[0]);
    std::swap(mColorMap[1]     , tmp.mColorMap[1]);
    std::swap(mColorMap[2]     , tmp.mColorMap[2]);
    std::swap(mColorMap[3]     , tmp.mColorMap[3]);
    std::swap(mColorWeight     , tmp.mColorWeight);
    std::swap(mUniforms        , tmp.mUniforms);
    std::swap(mTextureMaps     , tmp.mTextureMaps);
    return *this;
}

TextureMap* MaterialClass::SelectTextureMap(const State& state) const noexcept
{
    if (mTextureMaps.empty())
    {
        if (state.first_render)
            WARN("Material has no texture maps. [name='%1'", mName);
        return nullptr;
    }

    if (state.uniforms)
    {
        if (const auto* active_texture = base::SafeFind(*state.uniforms, std::string("active_texture_map")))
        {
            const auto* map_id = std::get_if<std::string>(active_texture);
            ASSERT(map_id  && "Active texture map selection has wrong (non-string) uniform type.");
            for (auto& map : mTextureMaps)
            {
                if (map->GetId() == *map_id)
                    return map.get();
            }
            if (state.first_render)
                WARN("No such texture map found in material. Falling back on default. [name='%1', map=%2]", mName, *map_id);
        }
    }

    // keep previous semantics, so default to the first map for the
    // material and sprite maps.
    if (mActiveTextureMap.empty())
        return mTextureMaps[0].get();

    for (auto& map : mTextureMaps)
    {
        if (map->GetId() == mActiveTextureMap)
            return map.get();
    }
    if (state.first_render)
        WARN("No such texture map found in material. Using first map. [name='%1', map=%2]", mName, mActiveTextureMap);

    return mTextureMaps[0].get();
}

Shader* MaterialClass::GetColorShader(const State& state, Device& device) const noexcept
{

std::string source(R"(
#version 100
precision mediump float;
uniform vec4 kBaseColor;
uniform float kGamma;
varying float vParticleAlpha;

vec4 ShaderPass(vec4 color);

void main()
{
  vec4 color = kBaseColor;
  color.a *= vParticleAlpha;

  // gamma (in)correction.
  color.rgb = pow(color.rgb, vec3(kGamma));

  gl_FragColor = ShaderPass(color);

}
)");

    if (IsStatic())
    {
        ShaderData data;
        data.gamma      = mGamma;
        data.base_color = mColorMap[ColorIndex::BaseColor];
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, std::move(source));

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(IsStatic() ? mName : "ColorShader");
    shader->CompileSource(source);
    return shader;
}

Shader* MaterialClass::GetGradientShader(const State& state, Device& device) const noexcept
{
std::string source(R"(
#version 100
precision highp float;

uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;
uniform vec2 kOffset;
uniform float kGamma;
uniform float kRenderPoints;

varying vec2 vTexCoord;
varying float vParticleAlpha;

vec4 ShaderPass(vec4 color);

vec4 MixGradient(vec2 coords)
{
  vec4 top = mix(kColor0, kColor1, coords.x);
  vec4 bot = mix(kColor2, kColor3, coords.x);
  vec4 color = mix(top, bot, coords.y);
  return color;
}

void main()
{
  vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
  coords = (coords - kOffset) + vec2(0.5, 0.5);
  coords = clamp(coords, vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec4 color  = MixGradient(coords);

  color.a *= vParticleAlpha;

  // gamma (in)correction
  color.rgb = pow(color.rgb, vec3(kGamma));

  gl_FragColor = ShaderPass(color);
}
)");
    if (IsStatic())
    {
        ShaderData data;
        data.gamma = mGamma;
        data.color_map[0] = mColorMap[0];
        data.color_map[1] = mColorMap[1];
        data.color_map[2] = mColorMap[2];
        data.color_map[3] = mColorMap[3];
        data.gradient_offset = mColorWeight;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, source);

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(IsStatic() ? mName : "GradientShader");
    shader->CompileSource(source);
    return shader;
}

Shader* MaterialClass::GetSpriteShader(const State& state, Device& device) const noexcept
{
    // todo: maybe pack some of shader uniforms
    std::string source(R"(
#version 100
precision highp float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;
uniform vec4 kBaseColor;
uniform float kRenderPoints;
uniform float kGamma;
uniform float kTime;
uniform float kBlendCoeff;
uniform float kApplyRandomParticleRotation;
uniform vec2 kTextureScale;
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
uniform float kTextureRotation;
uniform ivec2 kTextureWrap;
uniform vec2 kAlphaMask;

varying vec2 vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;

// Support texture coordinate wrapping (clamp or repeat)
// for cases when hardware texture sampler setting is
// insufficient, i.e. when sampling from a sub rectangle
// in a packed texture. (or whenever we're using texture rects)
// This however can introduce some sampling artifacts depending
// on fhe filter.
// TODO: any way to fix those artifacts ?
vec2 WrapTextureCoords(vec2 coords, vec2 box)
{
  float x = coords.x;
  float y = coords.y;

  if (kTextureWrap.x == 1)
    x = clamp(x, 0.0, box.x);
  else if (kTextureWrap.x == 2)
    x = fract(x / box.x) * box.x;

  if (kTextureWrap.y == 1)
    y = clamp(y, 0.0, box.y);
  else if (kTextureWrap.y == 2)
    y = fract(y / box.y) * box.y;

  return vec2(x, y);
}

vec2 RotateCoords(vec2 coords)
{
    float random_angle = mix(0.0, vParticleRandomValue, kApplyRandomParticleRotation);
    float angle = kTextureRotation + kTextureVelocityZ * kTime + random_angle * 3.1415926;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

vec4 ShaderPass(vec4 color);

void main()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    // note about gl_PointCoord:
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = RotateCoords(coords);

    coords += kTextureVelocityXY * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex0 = kTextureBox0.zw;
    vec2 scale_tex1 = kTextureBox1.zw;
    vec2 trans_tex0 = kTextureBox0.xy;
    vec2 trans_tex1 = kTextureBox1.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    vec2 c1 = WrapTextureCoords(coords * scale_tex0, scale_tex0) + trans_tex0;
    vec2 c2 = WrapTextureCoords(coords * scale_tex1, scale_tex1) + trans_tex1;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 tex0 = texture2D(kTexture0, c1);
    vec4 tex1 = texture2D(kTexture1, c2);

    vec4 col0 = mix(kBaseColor * tex0, vec4(kBaseColor.rgb, kBaseColor.a * tex0.a), kAlphaMask[0]);
    vec4 col1 = mix(kBaseColor * tex1, vec4(kBaseColor.rgb, kBaseColor.a * tex1.a), kAlphaMask[1]);

    vec4 color = mix(col0, col1, kBlendCoeff);
    color.a *= vParticleAlpha;

    // apply gamma (in)correction.
    color.rgb = pow(color.rgb, vec3(kGamma));

    gl_FragColor = ShaderPass(color);
}
)");
    if (IsStatic())
    {
        ShaderData data;
        data.gamma = mGamma;
        data.texture_scale = mTextureScale;
        data.texture_velocity = mTextureVelocity;
        data.texture_rotation = mTextureRotation;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, std::move(source));

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(IsStatic() ? mName : "SpriteShader");
    shader->CompileSource(source);
    return shader;
}

bool MaterialClass::ApplySpriteDynamicState(const State& state, Device& device, Program& program) const noexcept
{
    auto* map = SelectTextureMap(state);
    if (map == nullptr)
        return false;

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = state.material_time;
    ts.group_tag       = mClassId;

    TextureMap::BoundState binds;
    if (!map->BindTextures(ts, device,  binds))
        return false;

    glm::vec2 alpha_mask;

    bool need_software_wrap = true;
    for (unsigned i=0; i<2; ++i)
    {
        auto* texture = binds.textures[i];
        // set texture properties *before* setting it to the program.
        texture->SetFilter(mTextureMinFilter);
        texture->SetFilter(mTextureMagFilter);
        texture->SetWrapX(mTextureWrapX);
        texture->SetWrapY(mTextureWrapY);
        texture->SetGroup(mClassId);

        alpha_mask[i] = texture->GetFormat() == Texture::Format::Grayscale
                        ? 1.0f : 0.0f;

        const auto& box = binds.rects[i];
        const float x  = box.GetX();
        const float y  = box.GetY();
        const float sx = box.GetWidth();
        const float sy = box.GetHeight();
        const auto& kTextureName = "kTexture" + std::to_string(i);
        const auto& kTextureRect = "kTextureBox" + std::to_string(i);
        program.SetTexture(kTextureName.c_str(), i, *texture);
        program.SetUniform(kTextureRect.c_str(), x, y, sx, sy);

        // if a sub-rectangle is defined we need to use software (shader)
        // based wrapping/clamping in order to wrap/clamp properly within
        // the bounds of the sub rect.
        // we do this check here rather than introduce a specific flag for this purpose.
        const float eps = 0.001;
        if (math::equals(0.0f, x, eps) &&
            math::equals(0.0f, y, eps) &&
            math::equals(1.0f, sx, eps) &&
            math::equals(1.0f, sy, eps))
            need_software_wrap = false;
    }
    const float kMaterialTime     = state.material_time;
    const float kRenderPoints     = state.render_points ? 1.0f : 0.0f;
    const float kParticleRotation = state.render_points && mParticleAction == ParticleAction::Rotate ? 1.0f : 0.0f;
    const float kBlendCoeff       = BlendFrames() ? binds.blend_coefficient : 0.0f;
    program.SetTextureCount(2);
    program.SetUniform("kBlendCoeff",                  kBlendCoeff);
    program.SetUniform("kTime",                        kMaterialTime);
    program.SetUniform("kRenderPoints",                kRenderPoints);
    program.SetUniform("kAlphaMask",                   alpha_mask);
    program.SetUniform("kApplyRandomParticleRotation", kParticleRotation);

    // set software wrap/clamp. 0 = disabled.
    if (need_software_wrap)
    {
        const auto wrap_x = mTextureWrapX == TextureWrapping::Clamp ? 1 : 2;
        const auto wrap_y = mTextureWrapY == TextureWrapping::Clamp ? 1 : 2;
        program.SetUniform("kTextureWrap", wrap_x, wrap_y);
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
    }
    if (!IsStatic())
    {
        SetUniform("kBaseColor",         state.uniforms, mColorMap[ColorIndex::BaseColor], program);
        SetUniform("kGamma",             state.uniforms, mGamma, program);
        SetUniform("kTextureScale",      state.uniforms, glm::vec2(mTextureScale.x, mTextureScale.y), program);
        SetUniform("kTextureVelocityXY", state.uniforms, glm::vec2(mTextureVelocity.x, mTextureVelocity.y), program);
        SetUniform("kTextureVelocityZ",  state.uniforms, mTextureVelocity.z, program);
        SetUniform("kTextureRotation",   state.uniforms, mTextureRotation, program);
    }
    return true;
}

Shader* MaterialClass::GetTextureShader(const State& state, Device& device) const noexcept
{
// todo: pack some of the uniforms ?
    std::string source(R"(
#version 100
precision highp float;

uniform sampler2D kTexture;
uniform vec4 kTextureBox;
uniform float kAlphaMask;
uniform float kRenderPoints;
uniform float kGamma;
uniform float kApplyRandomParticleRotation;
uniform float kTime;
uniform vec2 kTextureScale;
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
uniform float kTextureRotation;
uniform vec4 kBaseColor;
// 0 disabled, 1 clamp, 2 wrap
uniform ivec2 kTextureWrap;

varying vec2 vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;

// Support texture coordinate wrapping (clamp or repeat)
// for cases when hardware texture sampler setting is
// insufficient, i.e. when sampling from a sub rectangle
// in a packed texture. (or whenever we're using texture rects)
// This however can introduce some sampling artifacts depending
// on fhe filter.
// TODO: any way to fix those artifacs ?
vec2 WrapTextureCoords(vec2 coords, vec2 box)
{
  float x = coords.x;
  float y = coords.y;

  if (kTextureWrap.x == 1)
    x = clamp(x, 0.0, box.x);
  else if (kTextureWrap.x == 2)
    x = fract(x / box.x) * box.x;

  if (kTextureWrap.y == 1)
    y = clamp(y, 0.0, box.y);
  else if (kTextureWrap.y == 2)
    y = fract(y / box.y) * box.y;

  return vec2(x, y);
}

vec2 RotateCoords(vec2 coords)
{
    float random_angle = mix(0.0, vParticleRandomValue, kApplyRandomParticleRotation);
    float angle = kTextureRotation + kTextureVelocityZ * kTime + random_angle * 3.1415926;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

vec4 ShaderPass(vec4 color);

void main()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    // note about gl_PointCoord:
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = RotateCoords(coords);
    coords += kTextureVelocityXY * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex = kTextureBox.zw;
    vec2 trans_tex = kTextureBox.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    coords = WrapTextureCoords(coords * scale_tex, scale_tex) + trans_tex;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 texel = texture2D(kTexture, coords);

    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 color = mix(kBaseColor * texel, vec4(kBaseColor.rgb, kBaseColor.a * texel.a), kAlphaMask);
    color.a *= vParticleAlpha;

    // apply gamma (in)correction.
    color.rgb = pow(color.rgb, vec3(kGamma));

    gl_FragColor = ShaderPass(color);
}
)");
    if (IsStatic())
    {
        ShaderData data;
        data.gamma            = mGamma;
        data.base_color       = mColorMap[ColorIndex::BaseColor];
        data.texture_scale    = mTextureScale;
        data.texture_velocity = mTextureVelocity;
        data.texture_rotation = mTextureRotation;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, std::move(source));

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(IsStatic() ? mName : "Texture2DShader");
    shader->CompileSource(source);
    return shader;
}

bool MaterialClass::ApplyTextureDynamicState(const State& state, Device& device, Program& program) const noexcept
{
    auto* map = SelectTextureMap(state);
    if (map == nullptr)
        return false;

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = 0.0;

    TextureMap::BoundState binds;
    if (!map->BindTextures(ts, device, binds))
        return false;

    auto* texture = binds.textures[0];
    texture->SetFilter(mTextureMinFilter);
    texture->SetFilter(mTextureMagFilter);
    texture->SetWrapX(mTextureWrapX);
    texture->SetWrapY(mTextureWrapY);

    const auto rect = binds.rects[0];
    const float x  = rect.GetX();
    const float y  = rect.GetY();
    const float sx = rect.GetWidth();
    const float sy = rect.GetHeight();

    bool need_software_wrap = true;
    // if a sub-rectangle is defined we need to use software (shader)
    // based wrapping/clamping in order to wrap/clamp properly within
    // the bounds of the sub rect.
    // we do this check here rather than introduce a specific flag for this purpose.
    const float eps = 0.001;
    if (math::equals(0.0f, x, eps) &&
        math::equals(0.0f, y, eps) &&
        math::equals(1.0f, sx, eps) &&
        math::equals(1.0f, sy, eps))
        need_software_wrap = false;

    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kTextureBox", x, y, sx, sy);
    program.SetTextureCount(1);
    program.SetUniform("kApplyRandomParticleRotation",
                    state.render_points && mParticleAction == ParticleAction::Rotate ? 1.0f : 0.0f);
    program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
    program.SetUniform("kTime", (float)state.material_time);
    program.SetUniform("kAlphaMask", binds.textures[0]->GetFormat() == Texture::Format::Grayscale ? 1.0f : 0.0f);

    // set software wrap/clamp. 0 = disabled.
    if (need_software_wrap)
    {
        const auto wrap_x = mTextureWrapX == TextureWrapping::Clamp ? 1 : 2;
        const auto wrap_y = mTextureWrapY == TextureWrapping::Clamp ? 1 : 2;
        program.SetUniform("kTextureWrap", wrap_x, wrap_y);
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
    }
    if (!IsStatic())
    {
        SetUniform("kGamma",             state.uniforms, mGamma, program);
        SetUniform("kBaseColor",         state.uniforms, mColorMap[ColorIndex::BaseColor], program);
        SetUniform("kTextureScale",      state.uniforms, glm::vec2(mTextureScale.x, mTextureScale.y), program);
        SetUniform("kTextureVelocityXY", state.uniforms, glm::vec2(mTextureVelocity.x, mTextureVelocity.y), program);
        SetUniform("kTextureVelocityZ",  state.uniforms, mTextureVelocity.z, program);
        SetUniform("kTextureRotation",   state.uniforms, mTextureRotation, program);
    }
    return true;
}

Shader* MaterialClass::GetCustomShader(const State& state, Device& device) const noexcept
{
    auto* shader = device.MakeShader(GetProgramId(state));
    if (!mShaderSrc.empty())
    {
        shader->SetName("CustomShaderSource");
        if (!shader->CompileSource(state.shader_pass->ModifyFragmentSource(device, mShaderSrc)))
        {
            ERROR("Failed to compile custom material shader source. [class='%1']", mName);
            return nullptr;
        }
        DEBUG("Compiled custom shader source. [name='%1']", mName);
    }
    else
    {
        shader->SetName(mShaderUri);
        const auto& buffer = gfx::LoadResource(mShaderUri);
        if (!buffer)
        {
            ERROR("Failed to load custom material shader source file. [class='%1', uri='%1']", mName, mShaderUri);
            return nullptr;
        }

        const char* beg = (const char*)buffer->GetData();
        const char* end = beg + buffer->GetSize();
        if (!shader->CompileSource(state.shader_pass->ModifyFragmentSource(device, std::string(beg, end))))
        {
            ERROR("Failed to compile custom material shader source. [name='%1', uri='%2']", mName, mShaderUri);
            return nullptr;
        }
        DEBUG("Compiled shader source file. [name='%1', uri='%2']", mName, mShaderUri);
    }
    return shader;
}

bool MaterialClass::ApplyCustomDynamicState(const State& state, Device& device, Program& program) const noexcept
{
    for (const auto& uniform : mUniforms)
    {
        const char* name = uniform.first.c_str();
        if (const auto* ptr = std::get_if<float>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<int>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec2>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec3>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec4>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<Color4f>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<std::string>(&uniform.second)) {
            // ignored right now, no use for this type. we're (ab)using the uniforms
            // to change the active texture map in some other material types.
        } else BUG("Unhandled uniform type.");
    }

    unsigned texture_unit = 0;

    for (const auto& map : mTextureMaps)
    {
        TextureMap::BindingState ts;
        ts.dynamic_content = true; // todo: need static flag. for now use dynamic (which is slower) but always correct
        ts.current_time    = state.material_time;
        ts.group_tag       = mClassId;
        TextureMap::BoundState binds;
        if (!map->BindTextures(ts, device, binds))
            return false;
        for (unsigned i=0; i<2; ++i)
        {
            auto* texture = binds.textures[i];
            if (texture == nullptr)
                continue;

            texture->SetFilter(mTextureMinFilter);
            texture->SetFilter(mTextureMagFilter);
            texture->SetWrapX(mTextureWrapX);
            texture->SetWrapY(mTextureWrapY);
            texture->SetGroup(mClassId);

            const auto& rect = binds.rects[i];
            if (!binds.sampler_names[i].empty())
                program.SetTexture(binds.sampler_names[i].c_str(), texture_unit, *texture);
            if (!binds.rect_names[i].empty())
                program.SetUniform(binds.rect_names[i].c_str(),
                                   rect.GetX(), rect.GetY(),
                                   rect.GetWidth(), rect.GetHeight());
            texture_unit++;
        }
    }
    program.SetUniform("kTime", (float)state.material_time);
    program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
    program.SetTextureCount(texture_unit);
    return true;
}

bool MaterialClassInst::ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const
{
    if (env.shader_pass->GetType() == ShaderPass::Type::Stencil)
        return true;

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.shader_pass;
    state.material_time = mRuntime;
    state.uniforms      = &mUniforms;
    state.first_render  = mFirstRender;

    mFirstRender = false;

    if (!mClass->ApplyDynamicState(state, device, program))
        return false;

    const auto surface = mClass->GetSurfaceType();
    if (surface == MaterialClass::SurfaceType::Opaque)
        raster.blending = RasterState::Blending::None;
    else if (surface == MaterialClass::SurfaceType::Transparent)
        raster.blending = RasterState::Blending::Transparent;
    else if (surface == MaterialClass::SurfaceType::Emissive)
        raster.blending = RasterState::Blending::Additive;

    raster.premultiplied_alpha = mClass->PremultipliedAlpha();
    return true;
}

void MaterialClassInst::ApplyStaticState(const Environment& env, Device& device, Program& program) const
{
    if (env.shader_pass->GetType() == ShaderPass::Type::Stencil)
        return;

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.shader_pass;
    return mClass->ApplyStaticState(state, device, program);
}

std::string MaterialClassInst::GetProgramId(const Environment& env) const
{
    if (env.shader_pass->GetType() == ShaderPass::Type::Stencil)
        return "stencil-shader";

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.shader_pass;
    return mClass->GetProgramId(state);
}

Shader* MaterialClassInst::GetShader(const Environment& env, Device& device) const
{
    // two ways to replace a shader when doing a stencil or depth pass.
    // a) replace the shader source completely and ignore the "real" shader source.
    // b) combine the shader source from "real" shader and "pass" shader.
    //
    // Currently we're doing a) here but this means that stencil and depth
    // passes are not fine grained but consider only the fragments that are
    // being rasterized. For example if we're rendering a cut-out sprite using
    // a rectangle geometry a stencil pass using option a) will write stencil
    // values to the stencil buffer for the whole rectangle. But with option b)
    // the stencil pass shader could inspect the incoming vec4 value and then
    // decide to write or discard the shader.
    //
    // However doing option b) means that the shader pass ID must contribute
    // to the shader object GPU ID so that 'base shader S + pass0' map to a
    // different GPU shader object than 'base shader S + pass1'.
    // Moving to option b) would also let the caller (shader pass) to control
    // which type of behaviour to do. Downside would be more shader objects!

    if (env.shader_pass->GetType() == ShaderPass::Type::Stencil)
    {
        auto* shader = device.FindShader("simple-stencil-shader");
        if (shader)
            return shader;

        constexpr const auto* simple_stencil_source = R"(
#version 100
precision mediump float;
void main() {
   gl_FragColor = vec4(1.0);
}
)";
        shader = device.MakeShader("simple-stencil-shader");
        shader->SetName("SimpleStencilShader");
        shader->CompileSource(simple_stencil_source);
        return shader;
    }
    else if (env.shader_pass->GetType() == ShaderPass::Type::DepthTexture)
    {
        // this shader maps the interpolated fragment depth (the .z component)
        // to a color value linearly. (important to keep this in mind when using
        // the output values, if rendering to a texture if the sRGB encoding happens
        // then the depth values are no longer linear!)
        //
        // Remember that in the OpenGL pipeline by default the NDC values (-1.0 to 1.0 on all axis)
        // are mapped to depth values so that -1.0 is least depth and 1.0 is maximum depth.
        // (OpenGL and ES3 have glDepthRange for modifying this mapping.)
        constexpr const auto* depth_to_color = R"(
#version 100
precision highp float;

void main() {
   gl_FragColor.rgb = vec3(gl_FragCoord.z);
   gl_FragColor.a = 1.0;
}
)";

        auto* shader = device.FindShader("simple-depth-texture-shader");
        if (shader)
            return shader;
        shader = device.MakeShader("simple-depth-texture-shader");
        shader->SetName("SimpleDepthTextureShader");
        shader->CompileSource(depth_to_color);
        return shader;
    }

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.shader_pass;
    state.material_time = mRuntime;
    state.uniforms      = &mUniforms;
    return mClass->GetShader(state, device);
}

TextMaterial::TextMaterial(const TextBuffer& text)  : mText(text)
{}
TextMaterial::TextMaterial(TextBuffer&& text)
  : mText(std::move(text))
{}
bool TextMaterial::ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const
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
            texture->Upload(bitmap->GetDataPtr(), width, height, gfx::Texture::Format::Grayscale, mips);
        }
        else if (format == TextBuffer::RasterFormat::Texture)
        {
            texture = mText.RasterizeTexture(gpu_id, "TextMaterialTexture", device);
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
    }
    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kColor", mColor);
    return true;
}
void TextMaterial::ApplyStaticState(const Environment& env, Device& device, gfx::Program& program) const
{}
Shader* TextMaterial::GetShader(const Environment& env, Device& device) const
{
constexpr auto* text_shader_bitmap = R"(
#version 100
precision highp float;
uniform sampler2D kTexture;
uniform vec4 kColor;
uniform float kTime;
varying vec2 vTexCoord;
vec4 ShaderPass(vec4);
void main() {
   float alpha = texture2D(kTexture, vTexCoord).a;
   vec4 color = vec4(kColor.r, kColor.g, kColor.b, kColor.a * alpha);
   gl_FragColor = ShaderPass(color);
}
        )";
constexpr auto* text_shader_texture = R"(
#version 100
precision highp float;
uniform sampler2D kTexture;
varying vec2 vTexCoord;
vec4 ShaderPass(vec4);
void main() {
    mat3 flip = mat3(vec3(1.0,  0.0, 0.0),
                     vec3(0.0, -1.0, 0.0),
                     vec3(0.0,  1.0, 0.0));
    vec3 tex = flip * vec3(vTexCoord.xy, 1.0);
    vec4 color = texture2D(kTexture, tex.xy);
    gl_FragColor = ShaderPass(color);
}
    )";
    const auto* pass = env.shader_pass;

    const auto format = mText.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
    {
        auto* shader = device.MakeShader(GetProgramId(env));
        shader->SetName("BitmapTextShader");
        shader->CompileSource(pass->ModifyFragmentSource(device, text_shader_bitmap));
        return shader;
    }
    else if (format == TextBuffer::RasterFormat::Texture)
    {
        auto* shader = device.MakeShader(GetProgramId(env));
        shader->SetName("TextureTextShader");
        shader->CompileSource(pass->ModifyFragmentSource(device, text_shader_texture));
        return shader;
    } else if (format == TextBuffer::RasterFormat::None)
        return nullptr;
    else BUG("Unhandled texture raster format.");
    return nullptr;
}
std::string TextMaterial::GetProgramId(const Environment& env) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, env.shader_pass->GetHash());

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
std::string TextMaterial::GetClassId() const
{ return {}; }
void TextMaterial::Update(float dt)
{}
void TextMaterial::SetRuntime(double runtime)
{}
void TextMaterial::SetUniform(const std::string& name, const Uniform& value)
{}
void TextMaterial::SetUniform(const std::string& name, Uniform&& value)
{}
void TextMaterial::ResetUniforms()
{}
void TextMaterial::SetUniforms(const UniformMap& uniforms)
{}


GradientClass CreateMaterialClassFromColor(const Color4f& top_left,
                                           const Color4f& top_right,
                                           const Color4f& bottom_left,
                                           const Color4f& bottom_right)
{
    MaterialClass material(MaterialClass::Type::Gradient, std::string(""));
    material.SetColor(top_left, GradientClass::ColorIndex::TopLeft);
    material.SetColor(top_right, GradientClass::ColorIndex::TopRight);
    material.SetColor(bottom_left, GradientClass::ColorIndex::BottomLeft);
    material.SetColor(bottom_right, GradientClass::ColorIndex::BottomRight);
    return material;
}

ColorClass CreateMaterialClassFromColor(const Color4f& color)
{
    const auto alpha = color.Alpha();

    MaterialClass material(MaterialClass::Type::Color, std::string(""));
    material.SetBaseColor(color);
    material.SetSurfaceType(alpha == 1.0f
        ? MaterialClass::SurfaceType::Opaque
        : MaterialClass::SurfaceType::Transparent);
    return material;
}

TextureMap2DClass CreateMaterialClassFromImage(const std::string& uri)
{
    auto map = std::make_unique<TextureMap>();
    map->SetName("Sprite");
    map->SetNumTextures(1);
    map->SetTextureSource(0, LoadTextureFromFile(uri));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Opaque);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::initializer_list<std::string>& uris)
{
    const std::vector<std::string> tmp(uris);

    auto map = std::make_unique<TextureMap>();
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uris.size());
    for (size_t i=0; i<uris.size(); ++i)
        map->SetTextureSource(i, LoadTextureFromFile(tmp[i]));

    MaterialClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::vector<std::string>& uris)
{
    auto map = std::make_unique<TextureMap>();
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uris.size());
    for (size_t i=0; i<uris.size(); ++i)
        map->SetTextureSource(i, LoadTextureFromFile(uris[i]));

    SpriteClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames)
{
    auto map = std::make_unique<TextureMap>();
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uri.size());
    for (size_t i=0; i<frames.size(); ++i)
    {
        map->SetTextureSource(i, LoadTextureFromFile(uri));
        map->SetTextureRect(i, frames[i]);
    }

    MaterialClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

TextureMap2DClass CreateMaterialClassFromText(const TextBuffer& text)
{
    auto map = std::make_unique<TextureMap>();
    map->SetType(TextureMap::Type::Texture2D);
    map->SetName("Text");
    map->SetNumTextures(1);
    map->SetTextureSource(0, CreateTextureFromText(text));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass)
{
    return std::make_unique<MaterialClassInst>(klass);
}

std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass)
{
    return std::make_unique<MaterialClassInst>(klass);
}

std::unique_ptr<TextMaterial> CreateMaterialInstance(const TextBuffer& text)
{
    return std::make_unique<TextMaterial>(text);
}
std::unique_ptr<TextMaterial> CreateMaterialInstance(TextBuffer&& text)
{
    return std::make_unique<TextMaterial>(std::move(text));
}
std::unique_ptr<Material> CreateMaterialInstance(const gfx::Color4f& color)
{
    return CreateMaterialInstance(CreateMaterialClassFromColor(color));
}

} // namespace
