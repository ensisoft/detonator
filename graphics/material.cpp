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
        content_hash = mTextBuffer.GetHash();
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
            texture->SetContentHash(content_hash);
            texture->Upload(mask->GetDataPtr(), mask->GetWidth(), mask->GetHeight(), Texture::Format::Grayscale);
        }
    }
    else if (format == TextBuffer::RasterFormat::Texture)
    {
        texture = mTextBuffer.RasterizeTexture(mId, device);
        if (texture)
        {
            texture->SetContentHash(content_hash);
            texture->GenerateMips();
        }
        return texture;
    }
    return nullptr;
}

void detail::TextureTextBufferSource::IntoJson(data::Writer& data) const
{
    auto chunk = data.NewWriteChunk();
    mTextBuffer.IntoJson(*chunk);
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("buffer", std::move(chunk));
}
bool detail::TextureTextBufferSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);

    const auto& chunk = data.GetReadChunk("buffer");
    if (!chunk)
        return false;

    ok &= mTextBuffer.FromJson(*chunk);
    return ok;
}

SpriteMap* TextureMap::AsSpriteMap()
{ return dynamic_cast<SpriteMap*>(this); }
TextureMap2D* TextureMap::AsTextureMap2D()
{ return dynamic_cast<TextureMap2D*>(this); }
const SpriteMap* TextureMap::AsSpriteMap() const
{ return dynamic_cast<const SpriteMap*>(this); }
const TextureMap2D* TextureMap::AsTextureMap2D() const
{ return dynamic_cast<const TextureMap2D*>(this); }

SpriteMap::SpriteMap(const SpriteMap& other, bool copy)
{
    mFps = other.mFps;
    mRectUniformName[0] = other.mRectUniformName[0];
    mRectUniformName[1] = other.mRectUniformName[1];
    mSamplerName[0] = other.mSamplerName[0];
    mSamplerName[1] = other.mSamplerName[1];
    mLooping = other.mLooping;
    for (const auto& sprite : other.mSprites)
    {
        Sprite s;
        if (sprite.source && copy)
            s.source = sprite.source->Copy();
        else if (sprite.source)
            s.source = sprite.source->Clone();
        s.rect = sprite.rect;
        mSprites.push_back(std::move(s));
    }
}

void SpriteMap::DeleteTextureById(const std::string& id)
{
    for (auto it = mSprites.begin(); it != mSprites.end(); ++it)
    {
        if (it->source && it->source->GetId() == id)
        {
            mSprites.erase(it);
            return;
        }
    }
}
const TextureSource* SpriteMap::FindTextureSourceById(const std::string& id) const
{
    for (const auto& it : mSprites)
    {
        if (it.source->GetId() == id)
            return it.source.get();
    }
    return nullptr;
}
const TextureSource* SpriteMap::FindTextureSourceByName(const std::string& name) const
{
    for (const auto& it : mSprites)
    {
        if (it.source->GetName() == name)
            return it.source.get();
    }
    return nullptr;
}

TextureSource* SpriteMap::FindTextureSourceById(const std::string& id)
{
    for (const auto& it : mSprites)
    {
        if (it.source->GetId() == id)
            return it.source.get();
    }
    return nullptr;
}
TextureSource* SpriteMap::FindTextureSourceByName(const std::string& name)
{
    for (const auto& it : mSprites)
    {
        if (it.source->GetName() == name)
            return it.source.get();
    }
    return nullptr;
}

bool SpriteMap::FindTextureRect(const TextureSource* source, FRect* rect) const
{
    for (const auto& it : mSprites)
    {
        if (it.source.get() == source)
        {
            *rect = it.rect;
            return true;
        }
    }
    return false;
}

bool SpriteMap::SetTextureRect(const TextureSource* source, const FRect& rect)
{
    for (auto& it : mSprites)
    {
        if (it.source.get() == source)
        {
            it.rect = rect;
            return true;
        }
    }
    return false;
}

bool SpriteMap::DeleteTexture(const TextureSource* source)
{
    for (auto it = mSprites.begin(); it != mSprites.end();)
    {
        if (it->source.get() == source)
        {
            mSprites.erase(it);
            return true;
        } else ++it;
    }
    return false;
}

std::size_t SpriteMap::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFps);
    hash = base::hash_combine(hash, mSamplerName[0]);
    hash = base::hash_combine(hash, mSamplerName[1]);
    hash = base::hash_combine(hash, mRectUniformName[0]);
    hash = base::hash_combine(hash, mRectUniformName[1]);
    hash = base::hash_combine(hash, mLooping);
    for (const auto& sprite : mSprites)
    {
        const auto* source = sprite.source.get();
        hash = base::hash_combine(hash, source ? source->GetHash() : 0);
        hash = base::hash_combine(hash, sprite.rect);
    }
    return hash;
}

bool SpriteMap::BindTextures(const BindingState& state, Device& device, BoundState& result) const
{
    if (mSprites.empty())
        return false;
    const auto frame_interval = 1.0f / std::max(mFps, 0.001f);
    const auto frame_fraction = std::fmod(state.current_time, frame_interval);
    const auto blend_coeff = frame_fraction/frame_interval;
    const auto first_index = (unsigned)(state.current_time/frame_interval);
    const auto frame_count = (unsigned)mSprites.size();
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

    for (unsigned i=0; i<2; ++i)
    {
        const auto& sprite  = mSprites[frame_index[i]];
        const auto& source  = sprite.source;
        if (auto* texture = source->Upload(texture_source_env, device))
        {
            result.textures[i]      = texture;
            result.rects[i]         = sprite.rect;
            result.sampler_names[i] = mSamplerName[i];
            result.rect_names[i]    = mRectUniformName[i];
        } else return false;
    }
    result.blend_coefficient = blend_coeff;
    return true;
}

void SpriteMap::IntoJson(data::Writer& data) const
{
    data.Write("fps", mFps);
    data.Write("sampler_name0", mSamplerName[0]);
    data.Write("sampler_name1", mSamplerName[1]);
    data.Write("rect_name0", mRectUniformName[0]);
    data.Write("rect_name1", mRectUniformName[1]);
    data.Write("looping", mLooping);

    for (const auto& sprite : mSprites)
    {
        auto chunk = data.NewWriteChunk();
        if (sprite.source)
            sprite.source->IntoJson(*chunk);
        ASSERT(!chunk->HasValue("type"));
        ASSERT(!chunk->HasValue("box"));
        chunk->Write("type", sprite.source->GetSourceType());
        chunk->Write("rect",  sprite.rect);
        data.AppendChunk("textures", std::move(chunk));
    }
}
bool SpriteMap::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("fps",           &mFps);
    ok &= data.Read("sampler_name0", &mSamplerName[0]);
    ok &= data.Read("sampler_name1", &mSamplerName[1]);
    ok &= data.Read("rect_name0",    &mRectUniformName[0]);
    ok &= data.Read("rect_name1",    &mRectUniformName[1]);
    ok &= data.Read("looping",       &mLooping);

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

            Sprite sprite;
            ok &= source->FromJson(*chunk);
            ok &= chunk->Read("rect", &sprite.rect);
            sprite.source = std::move(source);
            mSprites.push_back(std::move(sprite));
        }
    }
    return ok;
}

SpriteMap& SpriteMap::operator=(const SpriteMap& other)
{
    if (this == &other)
        return *this;
    SpriteMap tmp(other, true);
    std::swap(mFps,     tmp.mFps);
    std::swap(mSprites, tmp.mSprites);
    std::swap(mSamplerName[0], tmp.mSamplerName[0]);
    std::swap(mSamplerName[1], tmp.mSamplerName[1]);
    std::swap(mRectUniformName[0], tmp.mRectUniformName[0]);
    std::swap(mRectUniformName[1], tmp.mRectUniformName[1]);
    std::swap(mLooping, tmp.mLooping);
    return *this;
}

TextureMap2D::TextureMap2D(const TextureMap2D& other, bool copy)
{
    if (other.mSource)
        mSource = copy ? other.mSource->Copy() : other.mSource->Clone();
    mRect = other.mRect;
    mSamplerName = other.mSamplerName;
    mRectUniformName = other.mRectUniformName;
}

std::size_t TextureMap2D::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mRect);
    hash = base::hash_combine(hash, mSamplerName);
    hash = base::hash_combine(hash, mRectUniformName);
    hash = base::hash_combine(hash, mSource ? mSource->GetHash() : 0);
    return hash;
}
bool TextureMap2D::BindTextures(const BindingState& state, Device& device, BoundState& result) const
{
    if (!mSource)
        return false;

    TextureSource::Environment texture_source_env;
    texture_source_env.dynamic_content = state.dynamic_content;

    if (auto* texture = mSource->Upload(texture_source_env, device))
    {
        result.textures[0]       = texture;
        result.rects[0]          = mRect;
        result.sampler_names[0]  = mSamplerName;
        result.rect_names[0]     = mRectUniformName;
        result.blend_coefficient = 0;
        return true;
    }
    return false;
}
void TextureMap2D::IntoJson(data::Writer& data) const
{
    data.Write("rect", mRect);
    data.Write("sampler_name", mSamplerName);
    data.Write("rect_name", mRectUniformName);
    if  (mSource)
    {
        auto chunk = data.NewWriteChunk();
        mSource->IntoJson(*chunk);
        ASSERT(!chunk->HasValue("type"));
        chunk->Write("type", mSource->GetSourceType());
        data.Write("texture", std::move(chunk));
    }
}
bool TextureMap2D::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("rect",         &mRect);
    ok &= data.Read("sampler_name", &mSamplerName);
    ok &= data.Read("rect_name",    &mRectUniformName);

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
    mSource = std::move(source);
    return ok;
}

TextureSource* TextureMap2D::FindTextureSourceById(const std::string& id)
{
    if (mSource && mSource->GetId() == id)
        return mSource.get();
    return nullptr;
}
TextureSource* TextureMap2D::FindTextureSourceByName(const std::string& name)
{
    if (mSource && mSource->GetName() == name)
        return mSource.get();
    return nullptr;
}
const TextureSource* TextureMap2D::FindTextureSourceById(const std::string& id) const
{
    if (mSource && mSource->GetId() == id)
        return mSource.get();
    return nullptr;
}
const TextureSource* TextureMap2D::FindTextureSourceByName(const std::string& name) const
{
    if (mSource && mSource->GetName() == name)
        return mSource.get();
    return nullptr;
}

bool TextureMap2D::FindTextureRect(const TextureSource* source, FRect* rect) const
{
    if (mSource.get() != source)
        return false;
    *rect = mRect;
    return true;
}

bool TextureMap2D::SetTextureRect(const TextureSource* source, const FRect& rect)
{
    if (mSource.get() != source)
        return false;
    mRect = rect;
    return true;
}
bool TextureMap2D::DeleteTexture(const TextureSource* source)
{
    if (mSource.get() != source)
        return false;
    mSource.reset();
    return true;
}

TextureMap2D& TextureMap2D::operator=(const TextureMap2D& other)
{
    if (this == &other)
        return *this;
    TextureMap2D tmp(other, true);
    std::swap(mSource, tmp.mSource);
    std::swap(mRect,   tmp.mRect);
    std::swap(mRectUniformName, tmp.mRectUniformName);
    std::swap(mSamplerName, tmp.mSamplerName);
    return *this;
}

// static
std::unique_ptr<MaterialClass> MaterialClass::ClassFromJson(const data::Reader& data)
{
    Type type;
    if (!data.Read("type", &type))
        return nullptr;

    std::unique_ptr<MaterialClass> klass;
    if (type == Type::Color)
        klass.reset(new ColorClass);
    else if (type == Type::Gradient)
        klass.reset(new GradientClass);
    else if (type == Type::Sprite)
        klass.reset(new SpriteClass);
    else if (type == Type::Texture)
        klass.reset(new TextureMap2DClass);
    else if (type == Type::Custom)
        klass.reset(new CustomMaterialClass);
    else BUG("Unhandled material class type.");

    // todo: change the API so that we can also return the OK value.
    bool ok = klass->FromJson(data);
    return klass;
}

Shader* ColorClass::GetShader(const State& state, Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId(state)))
        return shader;

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

    if (mStatic)
    {
        ShaderData data;
        data.gamma      = mGamma;
        data.base_color = mColor;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, std::move(source));

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(mStatic ? mName : "ColorShader");
    shader->CompileSource(source);
    return shader;
}
size_t ColorClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mColor);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

std::string ColorClass::GetProgramId(const State& state) const
{
    size_t hash = base::hash_combine(0, "color");
    hash = base::hash_combine(hash, state.shader_pass->GetHash());
    if (mStatic)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mColor);
    }
    return std::to_string(hash);
}

void ColorClass::ApplyDynamicState(const State& state, Device& device, Program& program) const
{
    if (!mStatic)
    {
        SetUniform("kGamma",     state.uniforms, mGamma, program);
        SetUniform("kBaseColor", state.uniforms, mColor, program);
    }
}
void ColorClass::ApplyStaticState(const State&, Device& device, Program& prog) const
{
    prog.SetUniform("kGamma", mGamma);
    prog.SetUniform("kBaseColor", mColor);
}
void ColorClass::IntoJson(data::Writer& data) const
{
    data.Write("type",    Type::Color);
    data.Write("id",      mClassId);
    data.Write("name",    mName);
    data.Write("surface", mSurfaceType);
    data.Write("gamma",   mGamma);
    data.Write("static",  mStatic);
    data.Write("color",   mColor);
    data.Write("flags",   mFlags);
}
bool ColorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",      &mClassId);
    ok &= data.Read("name",    &mName);
    ok &= data.Read("surface", &mSurfaceType);
    ok &= data.Read("gamma",   &mGamma);
    ok &= data.Read("static",  &mStatic);
    ok &= data.Read("color",   &mColor);
    ok &= data.Read("flags",   &mFlags);
    return ok;
}

Shader* GradientClass::GetShader(const State& state, Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId(state)))
        return shader;

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
    if (mStatic)
    {
        ShaderData data;
        data.gamma = mGamma;
        data.color_map[0] = mColorMap[0];
        data.color_map[1] = mColorMap[1];
        data.color_map[2] = mColorMap[2];
        data.color_map[3] = mColorMap[3];
        data.gradient_offset = mOffset;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, source);

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(mStatic ? mName : "GradientShader");
    shader->CompileSource(source);
    return shader;
}
size_t GradientClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mColorMap[0]);
    hash = base::hash_combine(hash, mColorMap[1]);
    hash = base::hash_combine(hash, mColorMap[2]);
    hash = base::hash_combine(hash, mColorMap[3]);
    hash = base::hash_combine(hash, mOffset);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}
std::string GradientClass::GetProgramId(const State& state) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, "gradient");
    hash = base::hash_combine(hash, state.shader_pass->GetHash());
    if (mStatic)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mColorMap[0]);
        hash = base::hash_combine(hash, mColorMap[1]);
        hash = base::hash_combine(hash, mColorMap[2]);
        hash = base::hash_combine(hash, mColorMap[3]);
        hash = base::hash_combine(hash, mOffset);
    }
    return std::to_string(hash);
}

void GradientClass::ApplyDynamicState(const State& state, Device& device, Program& program) const
{
    program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
    if (!mStatic)
    {
        SetUniform("kGamma",  state.uniforms, mGamma, program);
        SetUniform("kColor0", state.uniforms, mColorMap[0], program);
        SetUniform("kColor1", state.uniforms, mColorMap[1], program);
        SetUniform("kColor2", state.uniforms, mColorMap[2], program);
        SetUniform("kColor3", state.uniforms, mColorMap[3], program);
        SetUniform("kOffset", state.uniforms, mOffset, program);
    }
}

void GradientClass::ApplyStaticState(const State& state, Device& device, Program& program) const
{
    program.SetUniform("kGamma",  mGamma);
    program.SetUniform("kColor0", mColorMap[0]);
    program.SetUniform("kColor1", mColorMap[1]);
    program.SetUniform("kColor2", mColorMap[2]);
    program.SetUniform("kColor3", mColorMap[3]);
    program.SetUniform("kOffset", mOffset);
}

void GradientClass::IntoJson(data::Writer& data) const
{
    data.Write("type",       Type::Gradient);
    data.Write("id",         mClassId);
    data.Write("name",       mName);
    data.Write("surface",    mSurfaceType);
    data.Write("gamma",      mGamma);
    data.Write("static",     mStatic);
    data.Write("color_map0", mColorMap[0]);
    data.Write("color_map1", mColorMap[1]);
    data.Write("color_map2", mColorMap[2]);
    data.Write("color_map3", mColorMap[3]);
    data.Write("offset",     mOffset);
    data.Write("flags",      mFlags);
}
bool GradientClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",         &mClassId);
    ok &= data.Read("name",       &mName);
    ok &= data.Read("surface",    &mSurfaceType);
    ok &= data.Read("gamma",      &mGamma);
    ok &= data.Read("static",     &mStatic);
    ok &= data.Read("color_map0", &mColorMap[0]);
    ok &= data.Read("color_map1", &mColorMap[1]);
    ok &= data.Read("color_map2", &mColorMap[2]);
    ok &= data.Read("color_map3", &mColorMap[3]);
    ok &= data.Read("offset",     &mOffset);
    ok &= data.Read("flags",      &mFlags);
    return ok;
}

SpriteClass::SpriteClass(const SpriteClass& other, bool copy)
   : mSprite(other.mSprite, copy)
{
    mClassId         = copy ? other.mClassId : base::RandomString(10);
    mName            = other.mName;
    mSurfaceType     = other.mSurfaceType;
    mGamma           = other.mGamma;
    mStatic          = other.mStatic;
    mBlendFrames     = other.mBlendFrames;
    mBaseColor       = other.mBaseColor;
    mTextureScale    = other.mTextureScale;
    mTextureVelocity = other.mTextureVelocity;
    mTextureRotation = other.mTextureRotation;
    mMinFilter       = other.mMinFilter;
    mMagFilter       = other.mMagFilter;
    mWrapX           = other.mWrapX;
    mWrapY           = other.mWrapY;
    mParticleAction  = other.mParticleAction;
    mFlags           = other.mFlags;
}

Shader* SpriteClass::GetShader(const State& state, Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId(state)))
        return shader;

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
    if (mStatic)
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
    shader->SetName(mStatic ? mName : "SpriteShader");
    shader->CompileSource(source);
    return shader;
}

std::size_t SpriteClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mBlendFrames);
    hash = base::hash_combine(hash, mBaseColor);
    hash = base::hash_combine(hash, mTextureScale);
    hash = base::hash_combine(hash, mTextureVelocity);
    hash = base::hash_combine(hash, mTextureRotation);
    hash = base::hash_combine(hash, mMinFilter);
    hash = base::hash_combine(hash, mMagFilter);
    hash = base::hash_combine(hash, mWrapX);
    hash = base::hash_combine(hash, mWrapY);
    hash = base::hash_combine(hash, mParticleAction);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mSprite.GetHash());
    return hash;
}

std::string SpriteClass::GetProgramId(const State& state) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, "sprite");
    hash = base::hash_combine(hash, state.shader_pass->GetHash());
    if (mStatic)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mBaseColor);
        hash = base::hash_combine(hash, mTextureScale);
        hash = base::hash_combine(hash, mTextureVelocity);
        hash = base::hash_combine(hash, mTextureRotation);
    }
    return std::to_string(hash);
}

std::unique_ptr<MaterialClass> SpriteClass::Copy() const
{ return std::make_unique<SpriteClass>(*this, true); }

std::unique_ptr<MaterialClass> SpriteClass::Clone() const
{ return std::make_unique<SpriteClass>(*this, false); }

void SpriteClass::ApplyDynamicState(const State& state, Device& device, Program& program) const
{
    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !mStatic;
    ts.current_time    = state.material_time;
    ts.group_tag       = mClassId;

    TextureMap::BoundState binds;
    if (!mSprite.BindTextures(ts, device,  binds))
        return;

    glm::vec2 alpha_mask;

    bool need_software_wrap = true;
    for (unsigned i=0; i<2; ++i)
    {
        auto* texture = binds.textures[i];
        // set texture properties *before* setting it to the program.
        texture->SetFilter(mMinFilter);
        texture->SetFilter(mMagFilter);
        texture->SetWrapX(mWrapX);
        texture->SetWrapY(mWrapY);
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
    const float kBlendCoeff       = mBlendFrames ? binds.blend_coefficient : 0.0f;
    program.SetTextureCount(2);
    program.SetUniform("kBlendCoeff",                  kBlendCoeff);
    program.SetUniform("kTime",                        kMaterialTime);
    program.SetUniform("kRenderPoints",                kRenderPoints);
    program.SetUniform("kAlphaMask",                   alpha_mask);
    program.SetUniform("kApplyRandomParticleRotation", kParticleRotation);

    // set software wrap/clamp. 0 = disabled.
    if (need_software_wrap)
    {
        const auto wrap_x = mWrapX == TextureWrapping::Clamp ? 1 : 2;
        const auto wrap_y = mWrapY == TextureWrapping::Clamp ? 1 : 2;
        program.SetUniform("kTextureWrap", wrap_x, wrap_y);
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
    }
    if (!mStatic)
    {
        SetUniform("kBaseColor",         state.uniforms, mBaseColor, program);
        SetUniform("kGamma",             state.uniforms, mGamma, program);
        SetUniform("kTextureScale",      state.uniforms, glm::vec2(mTextureScale.x, mTextureScale.y), program);
        SetUniform("kTextureVelocityXY", state.uniforms, glm::vec2(mTextureVelocity.x, mTextureVelocity.y), program);
        SetUniform("kTextureVelocityZ",  state.uniforms, mTextureVelocity.z, program);
        SetUniform("kTextureRotation",   state.uniforms, mTextureRotation, program);
    }
}

void SpriteClass::ApplyStaticState(const State& state, Device& device, Program& prog) const
{
    prog.SetUniform("kBaseColor",         mBaseColor);
    prog.SetUniform("kGamma",             mGamma);
    prog.SetUniform("kTextureScale",      mTextureScale.x, mTextureScale.y);
    prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
    prog.SetUniform("kTextureVelocityZ",  mTextureVelocity.z);
    prog.SetUniform("kTextureRotation",   mTextureRotation);
}

void SpriteClass::IntoJson(data::Writer& data) const
{
    data.Write("type",               Type::Sprite);
    data.Write("id",                 mClassId);
    data.Write("name",               mName);
    data.Write("surface",            mSurfaceType);
    data.Write("gamma",              mGamma);
    data.Write("static",             mStatic);
    data.Write("blending",           mBlendFrames);
    data.Write("color",              mBaseColor);
    data.Write("texture_min_filter", mMinFilter);
    data.Write("texture_mag_filter", mMagFilter);
    data.Write("texture_wrap_x",     mWrapX);
    data.Write("texture_wrap_y",     mWrapY);
    data.Write("texture_scale",      mTextureScale);
    data.Write("texture_velocity",   mTextureVelocity);
    data.Write("texture_rotation",   mTextureRotation);
    data.Write("particle_action",    mParticleAction);
    data.Write("flags",              mFlags);
    mSprite.IntoJson(data);
}

bool SpriteClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                 &mClassId);
    ok &= data.Read("name",               &mName);
    ok &= data.Read("surface",            &mSurfaceType);
    ok &= data.Read("gamma",              &mGamma);
    ok &= data.Read("static",             &mStatic);
    ok &= data.Read("blending",           &mBlendFrames);
    ok &= data.Read("color",              &mBaseColor);
    ok &= data.Read("texture_min_filter", &mMinFilter);
    ok &= data.Read("texture_mag_filter", &mMagFilter);
    ok &= data.Read("texture_wrap_x",     &mWrapX);
    ok &= data.Read("texture_wrap_y",     &mWrapY);
    ok &= data.Read("texture_scale",      &mTextureScale);
    ok &= data.Read("texture_velocity",   &mTextureVelocity);
    ok &= data.Read("texture_rotation",   &mTextureRotation);
    ok &= data.Read("particle_action",    &mParticleAction);
    ok &= data.Read("flags",              &mFlags);
    ok &= mSprite.FromJson(data);
    return ok;
}

void SpriteClass::BeginPacking(TexturePacker* packer) const
{
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        auto* source = mSprite.GetTextureSource(i);
        source->BeginPacking(packer);
    }
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        const auto& rect = mSprite.GetTextureRect(i);
        const auto* source = mSprite.GetTextureSource(i);
        const TexturePacker::ObjectHandle handle = source;
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
            if (has_x_velocity && mWrapX == TextureWrapping::Repeat)
                can_combine = false;
            else if (has_y_velocity && mWrapY == TextureWrapping::Repeat)
                can_combine = false;

            // scale check
            if (mTextureScale.x > 1.0f && mWrapX == TextureWrapping::Repeat)
                can_combine = false;
            else if (mTextureScale.y > 1.0f && mWrapY == TextureWrapping::Repeat)
                can_combine = false;
        }
        packer->SetTextureFlag(handle, TexturePacker::TextureFlags::CanCombine, can_combine);
    }
}
void SpriteClass::FinishPacking(const TexturePacker* packer)
{
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        auto* source = mSprite.GetTextureSource(i);
        source->FinishPacking(packer);
    }
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        auto* source = mSprite.GetTextureSource(i);
        const TexturePacker::ObjectHandle handle = source;
        mSprite.SetTextureRect(i, packer->GetPackedTextureBox(handle));
    }
}

SpriteClass& SpriteClass::operator=(const SpriteClass& other)
{
    if (this == &other)
        return *this;
    SpriteClass tmp(other, true);
    std::swap(mClassId        , tmp.mClassId);
    std::swap(mName           , tmp.mName);
    std::swap(mSurfaceType    , tmp.mSurfaceType);
    std::swap(mGamma          , tmp.mGamma);
    std::swap(mStatic         , tmp.mStatic);
    std::swap(mBlendFrames    , tmp.mBlendFrames);
    std::swap(mBaseColor      , tmp.mBaseColor);
    std::swap(mTextureScale   , tmp.mTextureScale);
    std::swap(mTextureVelocity, tmp.mTextureVelocity);
    std::swap(mTextureRotation, tmp.mTextureRotation);
    std::swap(mMinFilter      , tmp.mMinFilter);
    std::swap(mMagFilter      , tmp.mMagFilter);
    std::swap(mWrapX          , tmp.mWrapX);
    std::swap(mWrapY          , tmp.mWrapY);
    std::swap(mParticleAction , tmp.mParticleAction);
    std::swap(mSprite         , tmp.mSprite);
    std::swap(mFlags          , tmp.mFlags);
    return *this;
}


TextureMap2DClass::TextureMap2DClass(const TextureMap2DClass& other, bool copy)
    : mTexture(other.mTexture, copy)
{
    mClassId         = copy ? other.mClassId : base::RandomString(10);
    mName            = other.mName;
    mSurfaceType     = other.mSurfaceType;
    mGamma           = other.mGamma;
    mStatic          = other.mStatic;
    mBaseColor       = other.mBaseColor;
    mTextureScale    = other.mTextureScale;
    mTextureVelocity = other.mTextureVelocity;
    mTextureRotation = other.mTextureRotation;
    mMinFilter       = other.mMinFilter;
    mMagFilter       = other.mMagFilter;
    mWrapX           = other.mWrapX;
    mWrapY           = other.mWrapY;
    mParticleAction  = other.mParticleAction;
    mFlags           = other.mFlags;
}

Shader* TextureMap2DClass::GetShader(const State& state, Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId(state)))
        return shader;

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
    if (mStatic)
    {
        ShaderData data;
        data.gamma            = mGamma;
        data.base_color       = mBaseColor;
        data.texture_scale    = mTextureScale;
        data.texture_velocity = mTextureVelocity;
        data.texture_rotation = mTextureRotation;
        source = FoldUniforms(source, data);
    }
    source = state.shader_pass->ModifyFragmentSource(device, std::move(source));

    auto* shader = device.MakeShader(GetProgramId(state));
    shader->SetName(mStatic ? mName : "Texture2DShader");
    shader->CompileSource(source);
    return shader;
}
std::size_t TextureMap2DClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mBaseColor);
    hash = base::hash_combine(hash, mTextureScale);
    hash = base::hash_combine(hash, mTextureVelocity);
    hash = base::hash_combine(hash, mTextureRotation);
    hash = base::hash_combine(hash, mMinFilter);
    hash = base::hash_combine(hash, mMagFilter);
    hash = base::hash_combine(hash, mWrapX);
    hash = base::hash_combine(hash, mWrapY);
    hash = base::hash_combine(hash, mParticleAction);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mTexture.GetHash());
    return hash;
}
std::string TextureMap2DClass::GetProgramId(const State& state) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, "texture");
    hash = base::hash_combine(hash, state.shader_pass->GetHash());
    if (mStatic)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mBaseColor);
        hash = base::hash_combine(hash, mTextureScale);
        hash = base::hash_combine(hash, mTextureVelocity);
        hash = base::hash_combine(hash, mTextureRotation);
    }
    return std::to_string(hash);
}

std::unique_ptr<MaterialClass> TextureMap2DClass::Copy() const
{ return std::make_unique<TextureMap2DClass>(*this, true); }

std::unique_ptr<MaterialClass> TextureMap2DClass::Clone() const
{ return std::make_unique<TextureMap2DClass>(*this, false); }

void TextureMap2DClass::ApplyDynamicState(const State& state, Device& device, Program& program) const
{
    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !mStatic;
    ts.current_time    = 0.0;

    TextureMap::BoundState binds;
    if (!mTexture.BindTextures(ts, device, binds))
        return;

    auto* texture = binds.textures[0];
    texture->SetFilter(mMinFilter);
    texture->SetFilter(mMagFilter);
    texture->SetWrapX(mWrapX);
    texture->SetWrapY(mWrapY);

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
        const auto wrap_x = mWrapX == TextureWrapping::Clamp ? 1 : 2;
        const auto wrap_y = mWrapY == TextureWrapping::Clamp ? 1 : 2;
        program.SetUniform("kTextureWrap", wrap_x, wrap_y);
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
    }
    if (!mStatic)
    {
        SetUniform("kGamma",             state.uniforms, mGamma, program);
        SetUniform("kBaseColor",         state.uniforms, mBaseColor, program);
        SetUniform("kTextureScale",      state.uniforms, glm::vec2(mTextureScale.x, mTextureScale.y), program);
        SetUniform("kTextureVelocityXY", state.uniforms, glm::vec2(mTextureVelocity.x, mTextureVelocity.y), program);
        SetUniform("kTextureVelocityZ",  state.uniforms, mTextureVelocity.z, program);
        SetUniform("kTextureRotation",   state.uniforms, mTextureRotation, program);
    }
}

void TextureMap2DClass::ApplyStaticState(const State& state, Device& device, Program& prog) const
{
    prog.SetUniform("kGamma",             mGamma);
    prog.SetUniform("kBaseColor",         mBaseColor);
    prog.SetUniform("kTextureScale",      mTextureScale.x, mTextureScale.y);
    prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
    prog.SetUniform("kTextureVelocityZ",  mTextureVelocity.z);
    prog.SetUniform("kTextureRotation",   mTextureRotation);
}

void TextureMap2DClass::IntoJson(data::Writer& data) const
{
    data.Write("type",               Type::Texture);
    data.Write("id",                 mClassId);
    data.Write("name",               mName);
    data.Write("surface",            mSurfaceType);
    data.Write("gamma",              mGamma);
    data.Write("static",             mStatic);
    data.Write("color",              mBaseColor);
    data.Write("texture_min_filter", mMinFilter);
    data.Write("texture_mag_filter", mMagFilter);
    data.Write("texture_wrap_x",     mWrapX);
    data.Write("texture_wrap_y",     mWrapY);
    data.Write("texture_scale",      mTextureScale);
    data.Write("texture_velocity",   mTextureVelocity);
    data.Write("texture_rotation",   mTextureRotation);
    data.Write("particle_action",    mParticleAction);
    data.Write("flags",              mFlags);
    mTexture.IntoJson(data);
}

bool TextureMap2DClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                 &mClassId);
    ok &= data.Read("name",               &mName);
    ok &= data.Read("surface",            &mSurfaceType);
    ok &= data.Read("gamma",              &mGamma);
    ok &= data.Read("static",             &mStatic);
    ok &= data.Read("color",              &mBaseColor);
    ok &= data.Read("texture_min_filter", &mMinFilter);
    ok &= data.Read("texture_mag_filter", &mMagFilter);
    ok &= data.Read("texture_wrap_x",     &mWrapX);
    ok &= data.Read("texture_wrap_y",     &mWrapY);
    ok &= data.Read("texture_scale",      &mTextureScale);
    ok &= data.Read("texture_velocity",   &mTextureVelocity);
    ok &= data.Read("texture_rotation",   &mTextureRotation);
    ok &= data.Read("particle_action",    &mParticleAction);
    ok &= data.Read("flags",              &mFlags);
    ok &= mTexture.FromJson(data);
    return ok;
}

void TextureMap2DClass::BeginPacking(TexturePacker* packer) const
{
    auto* source = mTexture.GetTextureSource();
    if (!source) return;

    source->BeginPacking(packer);

    const TexturePacker::ObjectHandle handle = source;
    packer->SetTextureBox(handle, mTexture.GetTextureRect());

    // when texture rects are used to address a sub rect within the
    // texture wrapping on texture coordinates must be done "manually"
    // since the HW sampler coords are outside the sub rectangle coords.
    // for example if the wrapping is set to wrap on x and our box is
    // 0.25 units the HW sampler would not help us here to wrap when the
    // X coordinate is 0.26. Instead we need to do the wrap manually.
    // However this can cause rendering artifacts when texture sampling
    // is done depending on the current filter being used.
    bool can_combine = true;

    const auto& rect = mTexture.GetTextureRect();
    const auto x = rect.GetX();
    const auto y = rect.GetY();
    const auto w = rect.GetWidth();
    const auto h = rect.GetHeight();
    const float eps = 0.001;
    // if the texture uses sub rect then we still have this problem and packing
    // won't make it any worse. in other words if the box is the normal 0.f - 1.0f
    // box then combining can make it worse and should be disabled.
    if (math::equals(0.0f, x, eps) &&
        math::equals(0.0f, y, eps) &&
        math::equals(1.0f, w, eps) &&
        math::equals(1.0f, h, eps))
    {
        // is it possible for a texture to go beyond its range ?
        // the only case we know here is when texture velocity is non-zero
        // or texture scaling is used.
        // note that it's still possible for the game to programmatically
        // change the coordinates but that's their problem then.

        // velocity check
        const bool has_x_velocity = !math::equals(0.0f, mTextureVelocity.x, eps);
        const bool has_y_velocity = !math::equals(0.0f, mTextureVelocity.y, eps);
        if (has_x_velocity && mWrapX == TextureWrapping::Repeat)
            can_combine = false;
        else if (has_y_velocity && mWrapY == TextureWrapping::Repeat)
            can_combine = false;

        // scale check
        if (mTextureScale.x > 1.0f && mWrapX == TextureWrapping::Repeat)
            can_combine = false;
        else if (mTextureScale.y > 1.0f && mWrapY == TextureWrapping::Repeat)
            can_combine = false;
    }
    packer->SetTextureFlag(handle, TexturePacker::TextureFlags::CanCombine, can_combine);

}
void TextureMap2DClass::FinishPacking(const TexturePacker* packer)
{
    auto* source = mTexture.GetTextureSource();
    if (!source) return;

    const TexturePacker::ObjectHandle handle = source;
    source->FinishPacking(packer);
    mTexture.SetTextureRect(packer->GetPackedTextureBox(handle));
}

TextureMap2DClass& TextureMap2DClass::operator=(const TextureMap2DClass& other)
{
    if (this == &other)
        return *this;

    TextureMap2DClass tmp(other, true);
    std::swap(mClassId        , tmp.mClassId);
    std::swap(mName           , tmp.mName);
    std::swap(mSurfaceType    , tmp.mSurfaceType);
    std::swap(mGamma          , tmp.mGamma);
    std::swap(mStatic         , tmp.mStatic);
    std::swap(mBaseColor      , tmp.mBaseColor);
    std::swap(mTextureScale   , tmp.mTextureScale);
    std::swap(mTextureVelocity, tmp.mTextureVelocity);
    std::swap(mTextureRotation, tmp.mTextureRotation);
    std::swap(mMinFilter      , tmp.mMinFilter);
    std::swap(mMagFilter      , tmp.mMagFilter);
    std::swap(mWrapX          , tmp.mWrapX);
    std::swap(mWrapY          , tmp.mWrapY);
    std::swap(mParticleAction , tmp.mParticleAction);
    std::swap(mTexture        , tmp.mTexture);
    std::swap(mFlags         ,  tmp.mFlags);
    return *this;
}

CustomMaterialClass::CustomMaterialClass(const CustomMaterialClass& other, bool copy)
{
    mClassId     = copy ? other.mClassId : base::RandomString(10);
    mName        = other.mName;
    mShaderUri   = other.mShaderUri;
    mShaderSrc   = other.mShaderSrc;
    mUniforms    = other.mUniforms;
    mSurfaceType = other.mSurfaceType;
    mMinFilter   = other.mMinFilter;
    mMagFilter   = other.mMagFilter;
    mWrapX       = other.mWrapX;
    mWrapY       = other.mWrapY;
    mFlags       = other.mFlags;
    for (const auto& map : other.mTextureMaps)
    {
        mTextureMaps[map.first] = copy ? map.second->Copy() : map.second->Clone();
    }
}

TextureSource* CustomMaterialClass::FindTextureSourceById(const std::string& id)
{
    for (const auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (auto* source = map->FindTextureSourceById(id))
            return source;
    }
    return nullptr;
}
TextureSource* CustomMaterialClass::FindTextureSourceByName(const std::string& name)
{
    for (const auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (auto* source = map->FindTextureSourceByName(name))
            return source;
    }
    return nullptr;
}
const TextureSource* CustomMaterialClass::FindTextureSourceById(const std::string& id) const
{
    for (const auto& pair : mTextureMaps)
    {
        const auto& map = pair.second;
        if (const auto* source = map->FindTextureSourceById(id))
            return source;
    }
    return nullptr;
}
const TextureSource* CustomMaterialClass::FindTextureSourceByName(const std::string& name) const
{
    for (const auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (auto* source = map->FindTextureSourceByName(name))
            return source;
    }
    return nullptr;
}

gfx::FRect CustomMaterialClass::FindTextureSourceRect(const TextureSource* source) const
{
    FRect rect;
    for (const auto& pair : mTextureMaps)
    {
        const auto& map = pair.second;
        if (map->FindTextureRect(source, &rect))
            return rect;
    }
    return rect;
}

void CustomMaterialClass::SetTextureSourceRect(const TextureSource* source, const FRect& rect)
{
    for (auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (map->SetTextureRect(source, rect))
            return;
    }
}

void CustomMaterialClass::DeleteTextureSource(const TextureSource* source)
{
    for (auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (map->DeleteTexture(source))
            return;
    }
}

std::unordered_set<std::string> CustomMaterialClass::GetTextureMapNames() const
{
    std::unordered_set<std::string> set;
    for (const auto& p : mTextureMaps)
        set.insert(p.first);
    return set;
}

Shader* CustomMaterialClass::GetShader(const State& state, Device& device) const
{
    const auto& id = GetProgramId(state);
    if (auto* shader = device.FindShader(id))
        return shader;

    auto* shader = device.MakeShader(id);
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
std::size_t CustomMaterialClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mShaderUri);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mMinFilter);
    hash = base::hash_combine(hash, mMagFilter);
    hash = base::hash_combine(hash, mWrapX);
    hash = base::hash_combine(hash, mWrapY);
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

    keys.clear();
    for (const auto& map : mTextureMaps)
        keys.insert(map.first);

    for (const auto& key : keys)
    {
        const auto* map = base::SafeFind(mTextureMaps, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, map->GetHash());
    }
    return hash;
}
std::string CustomMaterialClass::GetProgramId(const State&) const
{
    size_t hash = 0;
    if (!mShaderSrc.empty())
        hash = base::hash_combine(hash, mShaderSrc);
    if (!mShaderUri.empty())
        hash = base::hash_combine(hash, mShaderUri);
    return std::to_string(hash);
}
std::unique_ptr<MaterialClass> CustomMaterialClass::Copy() const
{ return std::make_unique<CustomMaterialClass>(*this); }

std::unique_ptr<MaterialClass> CustomMaterialClass::Clone() const
{
    auto ret = std::make_unique<CustomMaterialClass>(*this);
    ret->mClassId = base::RandomString(10);
    return ret;
}
void CustomMaterialClass::ApplyDynamicState(const State& state, Device& device, Program& program) const
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
        else BUG("Unhandled uniform type.");
    }

    unsigned texture_unit = 0;

    for (const auto& map : mTextureMaps)
    {
        TextureMap::BindingState ts;
        ts.dynamic_content = true; // todo: need static flag. for now use dynamic (which is slower) but always correct
        ts.current_time    = state.material_time;
        ts.group_tag       = mClassId;
        TextureMap::BoundState binds;
        if (!map.second->BindTextures(ts, device, binds))
            return;
        for (unsigned i=0; i<2; ++i)
        {
            auto* texture = binds.textures[i];
            if (texture == nullptr)
                continue;

            texture->SetFilter(mMinFilter);
            texture->SetFilter(mMagFilter);
            texture->SetWrapX(mWrapX);
            texture->SetWrapY(mWrapY);
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
}
void CustomMaterialClass::ApplyStaticState(const State& state, Device& device, Program& program) const
{
    // nothing to do here, static state should be in the shader
    // already, either by the shader programmer or by the shader
    // source generator.
}
void CustomMaterialClass::IntoJson(data::Writer& data) const
{
    data.Write("type",  Type::Custom);
    data.Write("id",          mClassId);
    data.Write("name",        mName);
    data.Write("shader_uri",  mShaderUri);
    data.Write("shader_src",  mShaderSrc);
    data.Write("surface",     mSurfaceType);
    data.Write("min_filter",  mMinFilter);
    data.Write("mag_filter",  mMagFilter);
    data.Write("wrap_x",      mWrapX);
    data.Write("wrap_y",      mWrapY);
    data.Write("flags",       mFlags);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> uniform_keys;
    for (const auto& uniform : mUniforms)
        uniform_keys.insert(uniform.first);

    std::set<std::string> texture_keys;
    for (const auto& map : mTextureMaps)
        texture_keys.insert(map.first);

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
    for (const auto& key : texture_keys)
    {
        const auto& map = SafeFind(mTextureMaps, key);
        auto chunk = data.NewWriteChunk();
        map->IntoJson(*chunk);
        ASSERT(chunk->HasValue("name") == false);
        ASSERT(chunk->HasValue("type") == false);
        chunk->Write("name", key);
        chunk->Write("type", map->GetType());
        data.AppendChunk("texture_maps", std::move(chunk));
    }
}

bool CustomMaterialClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",         &mClassId);
    ok &= data.Read("name",       &mName);
    ok &= data.Read("shader_uri", &mShaderUri);
    ok &= data.Read("shader_src", &mShaderSrc);
    ok &= data.Read("surface",    &mSurfaceType);
    ok &= data.Read("min_filter", &mMinFilter);
    ok &= data.Read("mag_filter", &mMagFilter);
    ok &= data.Read("wrap_x",     &mWrapX);
    ok &= data.Read("wrap_y",     &mWrapY);
    ok &= data.Read("flags",      &mFlags);

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
        if (chunk->Read("type", &type) &&
            chunk->Read("name", &name))
        {
            std::unique_ptr<TextureMap> map;
            if (type == TextureMap::Type::Texture2D)
                map.reset(new TextureMap2D);
            else if (type == TextureMap::Type::Sprite)
                map.reset(new SpriteMap);
            else BUG("Unhandled texture map type.");

            ok &= map->FromJson(*chunk);
            mTextureMaps[name] = std::move(map);
        } else ok = false;
    }
    return ok;
}
void CustomMaterialClass::BeginPacking(TexturePacker* packer) const
{
    // todo: rethink this packing stuff.

    for (const auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (auto* sprite = map->AsSpriteMap())
        {
            for (size_t i=0; i<sprite->GetNumTextures(); ++i)
            {
                const auto& rect   = sprite->GetTextureRect(i);
                const auto* source = sprite->GetTextureSource(i);
                const TexturePacker::ObjectHandle handle = source;
                source->BeginPacking(packer);
                packer->SetTextureBox(handle, rect);
            }
        }
        else if (auto* texture = map->AsTextureMap2D())
        {
            if (auto* source = texture->GetTextureSource())
            {
                source->BeginPacking(packer);
                packer->SetTextureBox(source, texture->GetTextureRect());
            }
        }
    }
}
void CustomMaterialClass::FinishPacking(const TexturePacker* packer)
{
    for (auto& pair : mTextureMaps)
    {
        auto& map = pair.second;
        if (auto* sprite = map->AsSpriteMap())
        {
            for (size_t i=0; i<sprite->GetNumTextures(); ++i)
            {
                auto* source = sprite->GetTextureSource(i);
                source->FinishPacking(packer);
                sprite->SetTextureRect(i, packer->GetPackedTextureBox(source));
            }
        }
        else if (auto* texture = map->AsTextureMap2D())
        {
            auto* source = texture->GetTextureSource();
            if (source)
            {
                source->FinishPacking(packer);
                texture->SetTextureRect(packer->GetPackedTextureBox(source));
            }
        }
    }
}

CustomMaterialClass& CustomMaterialClass::operator=(const CustomMaterialClass& other)
{
    if (this == &other)
        return *this;

    CustomMaterialClass tmp(other, true);
    std::swap(mClassId,     tmp.mClassId);
    std::swap(mName,        tmp.mName);
    std::swap(mShaderUri,   tmp.mShaderUri);
    std::swap(mShaderSrc,   tmp.mShaderSrc);
    std::swap(mUniforms,    tmp.mUniforms);
    std::swap(mSurfaceType, tmp.mSurfaceType);
    std::swap(mTextureMaps, tmp.mTextureMaps);
    std::swap(mMinFilter,   tmp.mMinFilter);
    std::swap(mMagFilter,   tmp.mMagFilter);
    std::swap(mWrapX,       tmp.mWrapX);
    std::swap(mWrapY,       tmp.mWrapY);
    std::swap(mFlags,       tmp.mFlags);
    return *this;
}

void MaterialClassInst::ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const
{
    if (env.render_pass->GetType() == ShaderPass::Type::Stencil)
        return;

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.render_pass;
    state.material_time = mRuntime;
    state.uniforms      = &mUniforms;
    mClass->ApplyDynamicState(state, device, program);

    const auto surface = mClass->GetSurfaceType();
    if (surface == MaterialClass::SurfaceType::Opaque)
        raster.blending = RasterState::Blending::None;
    else if (surface == MaterialClass::SurfaceType::Transparent)
        raster.blending = RasterState::Blending::Transparent;
    else if (surface == MaterialClass::SurfaceType::Emissive)
        raster.blending = RasterState::Blending::Additive;

    raster.premultiplied_alpha = mClass->PremultipliedAlpha();
}

void MaterialClassInst::ApplyStaticState(const Environment& env, Device& device, Program& program) const
{
    if (env.render_pass->GetType() == ShaderPass::Type::Stencil)
        return;

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.render_pass;
    return mClass->ApplyStaticState(state, device, program);
}

std::string MaterialClassInst::GetProgramId(const Environment& env) const
{
    if (env.render_pass->GetType() == ShaderPass::Type::Stencil)
        return "stencil-shader";

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.render_pass;
    return mClass->GetProgramId(state);
}

Shader* MaterialClassInst::GetShader(const Environment& env, Device& device) const
{
    if (env.render_pass->GetType() == ShaderPass::Type::Stencil)
    {
        // if the render pass is a stenciling pass we can replace the material
        // shader by a simple fragment shader that doesn't do any expensive
        // operations. It could be desirable to use the texture alpha to write
        // to the stencil buffer (by discarding fragments) to create a way to
        // use the texture as a stencil mask. This would then need to be implemented
        // in the Sprite and TextureMap classes separately.
        size_t hash = 0;
        hash = base::hash_combine(hash, "stencil-shader");
        hash = base::hash_combine(hash, env.render_pass->GetHash());
        const auto id = std::to_string(hash);

        auto* shader = device.FindShader(id);
        if (shader)
            return shader;

        std::string source(R"(
#version 100
precision mediump float;

vec4 ShaderPass(vec4);

void main() {
   gl_FragColor = ShaderPass(vec4(1.0));
}
)");
        source = env.render_pass->ModifyFragmentSource(device, std::move(source));
        shader = device.MakeShader(id);
        shader->SetName("StencilShader");
        shader->CompileSource(source);
        return shader;
    }

    MaterialClass::State state;
    state.editing_mode  = env.editing_mode;
    state.render_points = env.render_points;
    state.shader_pass   = env.render_pass;
    state.material_time = mRuntime;
    state.uniforms      = &mUniforms;
    return mClass->GetShader(state, device);
}

TextMaterial::TextMaterial(const TextBuffer& text)  : mText(text)
{}
TextMaterial::TextMaterial(TextBuffer&& text)
  : mText(std::move(text))
{}
void TextMaterial::ApplyDynamicState(const Environment& env, Device& device, Program& program, RasterState& raster) const
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
            texture->SetName("FreeTypeText");

            auto bitmap = mText.RasterizeBitmap();
            if (!bitmap)
                return;
            const auto width = bitmap->GetWidth();
            const auto height = bitmap->GetHeight();
            texture->Upload(bitmap->GetDataPtr(), width, height, gfx::Texture::Format::Grayscale, mips);
        }
        else if (format == TextBuffer::RasterFormat::Texture)
        {
            texture = mText.RasterizeTexture(gpu_id, device);
            if (!texture)
                return;
            texture->SetTransient(true);
            texture->SetName("BitmapText");
            // texture->GenerateMips(); << this would be the place to generate mips if needed.
        } else if (format == TextBuffer::RasterFormat::None)
            return;
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
    const auto* pass = env.render_pass;

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
    hash = base::hash_combine(hash, env.render_pass->GetHash());

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
void TextMaterial::SetRuntime(float runtime)
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
    GradientClass material;
    material.SetColor(top_left, GradientClass::ColorIndex::TopLeft);
    material.SetColor(top_right, GradientClass::ColorIndex::TopRight);
    material.SetColor(bottom_left, GradientClass::ColorIndex::BottomLeft);
    material.SetColor(bottom_right, GradientClass::ColorIndex::BottomRight);
    return material;
}

ColorClass CreateMaterialClassFromColor(const Color4f& color)
{
    const auto alpha = color.Alpha();

    ColorClass material;
    material.SetBaseColor(color);
    material.SetSurfaceType(alpha == 1.0f
        ? MaterialClass::SurfaceType::Opaque
        : MaterialClass::SurfaceType::Transparent);
    return material;
}

TextureMap2DClass CreateMaterialClassFromImage(const std::string& uri)
{
    TextureMap2DClass material;
    material.SetTexture(LoadTextureFromFile(uri));
    material.SetSurfaceType(MaterialClass::SurfaceType::Opaque);
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::initializer_list<std::string>& uris)
{
    SpriteClass material;
    for (const auto& uri : uris)
        material.AddTexture(LoadTextureFromFile(uri));

    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::vector<std::string>& uris)
{
    SpriteClass material;
    for (const auto& uri : uris)
        material.AddTexture(LoadTextureFromFile(uri));

    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    return material;
}

SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames)
{
    SpriteClass material;
    for (size_t i=0; i<frames.size(); ++i)
    {
        material.AddTexture(LoadTextureFromFile(uri));
        material.SetTextureRect(i, frames[i]);
    }
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    return material;
}

TextureMap2DClass CreateMaterialClassFromText(const TextBuffer& text)
{
    TextureMap2DClass material;
    material.SetTexture(CreateTextureFromText(text));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
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

} // namespace
