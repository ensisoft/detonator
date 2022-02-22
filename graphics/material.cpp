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

std::shared_ptr<IBitmap> detail::TextureFileSource::GetData() const
{
    try
    {
        DEBUG("Loading texture file. [file='%1']", mFile);
        // in case of an image load exception, catch and log and returnnull.
        // Todo: image class should probably provide a non-throw load
        // but then it also needs some mechanism for getting the error
        // message why it failed.
        Image file(mFile);
        if (file.GetDepthBits() == 8)
            return std::make_shared<GrayscaleBitmap>(file.AsBitmap<Grayscale>());
        else if (file.GetDepthBits() == 24)
            return std::make_shared<RgbBitmap>(file.AsBitmap<RGB>());
        else if (file.GetDepthBits() == 32)
            return std::make_shared<RgbaBitmap>(file.AsBitmap<RGBA>());
        else WARN("Unexpected texture bit depth.", mFile);

        ERROR("Failed to load texture. [file='%1']", mFile);
        return nullptr;
    }
    catch (const std::exception& e)
    {
        ERROR("Failed to load texture. [file='%1', error='%2']", mFile, e.what());
    }
    return nullptr;
}
void detail::TextureFileSource::IntoJson(data::Writer& data) const
{
    data.Write("id",   mId);
    data.Write("file", mFile);
    data.Write("name", mName);
    data.Write("flags", mFlags);
}
bool detail::TextureFileSource::FromJson(const data::Reader& data)
{
    return data.Read("id",   &mId) &&
           data.Read("file", &mFile) &&
           data.Read("name", &mName) &&
           data.Read("flags", &mFlags);
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
    unsigned width = 0;
    unsigned height = 0;
    unsigned depth = 0;
    std::string base64;
    if (!data.Read("id",     &mId) ||
        !data.Read("name",   &mName) ||
        !data.Read("width",  &width) ||
        !data.Read("height", &height) ||
        !data.Read("depth",  &depth) ||
        !data.Read("data",   &base64))
        return false;
    const auto& bits = base64::Decode(base64);
    if (depth == 1)
        mBitmap = std::make_shared<GrayscaleBitmap>((const Grayscale*) &bits[0], width, height);
    else if (depth == 3)
        mBitmap = std::make_shared<RgbBitmap>((const RGB*) &bits[0], width, height);
    else if (depth == 4)
        mBitmap = std::make_shared<RgbaBitmap>((const RGBA*) &bits[0], width, height);
    else return false;
    return true;
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
    if (!data.Read("id", &mId) ||
        !data.Read("name", &mName) ||
        !data.Read("function", &function))
        return false;
    if (function == IBitmapGenerator::Function::Noise)
        mGenerator = std::make_unique<NoiseBitmapGenerator>();
    const auto& chunk = data.GetReadChunk("generator");
    if (!chunk || !mGenerator->FromJson(*chunk))
        return false;
    return true;
}


std::shared_ptr<IBitmap> detail::TextureTextBufferSource::GetData() const
{
    try
    {
        // in case of font rasterization fails catch and log the error.
        // Todo: perhaps the TextBuffer should provide a non-throw rasterization
        // method, but then it'll need some other way to report the failure.
        return mTextBuffer.Rasterize();
    }
    catch (const std::exception& e)
    {
        ERROR(e.what());
        ERROR("Failed to rasterize text buffer.");
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
    if (!data.Read("name", &mName) ||
        !data.Read("id", &mId))
        return false;
    const auto& chunk = data.GetReadChunk("buffer");
    if (!chunk)
        return false;
    auto ret = TextBuffer::FromJson(*chunk);
    if (!ret.has_value())
        return false;
    mTextBuffer = std::move(ret.value());
    return true;
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
    for (unsigned i=0; i<2; ++i)
    {
        const auto& sprite  = mSprites[frame_index[i]];
        const auto& source  = sprite.source;
        const auto& name    = source->GetGpuId();
        auto* texture = device.FindTexture(name);

        bool needs_upload = false;
        size_t content_hash = 0;

        // check for changes.
        if (texture && state.dynamic_content) {
            content_hash = source->GetContentHash();
            needs_upload = content_hash != texture->GetContentHash();
        }
        if (!texture || needs_upload)
        {
            if (!texture)
                texture = device.MakeTexture(name);

            auto bitmap = source->GetData();
            if (!bitmap)
                return false;
            const auto width  = bitmap->GetWidth();
            const auto height = bitmap->GetHeight();
            const auto format = Texture::DepthToFormat(bitmap->GetDepthBits());
            texture->SetName(source->GetName());
            texture->Upload(bitmap->GetDataPtr(), width, height, format);
            if (!content_hash)
                content_hash = source->GetContentHash();
            texture->SetContentHash(content_hash);
        }
        result.textures[i]      = texture;
        result.rects[i]         = sprite.rect;
        result.sampler_names[i] = mSamplerName[i];
        result.rect_names[i]    = mRectUniformName[i];
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
    data.Read("fps", &mFps);
    data.Read("sampler_name0", &mSamplerName[0]);
    data.Read("sampler_name1", &mSamplerName[1]);
    data.Read("rect_name0", &mRectUniformName[0]);
    data.Read("rect_name1", &mRectUniformName[1]);
    data.Read("looping", &mLooping);

    for (unsigned i=0; i<data.GetNumChunks("textures"); ++i)
    {
        const auto& chunk = data.GetReadChunk("textures", i);
        TextureSource::Source type;
        Sprite sprite;
        if (!chunk->Read("type", &type) ||
            !chunk->Read("rect", &sprite.rect))
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
        else BUG("???");

        if (!source->FromJson(*chunk))
            return false;

        sprite.source = std::move(source);
        mSprites.push_back(std::move(sprite));
    }
    return true;
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

    const auto& source  = mSource;
    const auto& name    = mSource->GetGpuId();
    auto* texture = device.FindTexture(name);

    bool needs_upload = false;
    size_t content_hash = 0;

    // check for changes.
    if (texture && state.dynamic_content) {
        content_hash = source->GetContentHash();
        needs_upload = content_hash != texture->GetContentHash();
    }
    // upload if doesn't exist already or the content has changed.
    if (!texture || needs_upload)
    {
        if (!texture)
            texture = device.MakeTexture(name);

        auto bitmap = source->GetData();
        if (!bitmap)
            return false;
        const auto width  = bitmap->GetWidth();
        const auto height = bitmap->GetHeight();
        const auto format = Texture::DepthToFormat(bitmap->GetDepthBits());
        texture->SetName(mSource->GetName());
        texture->Upload(bitmap->GetDataPtr(), width, height, format);
        if (!content_hash)
            content_hash = source->GetContentHash();
        texture->SetContentHash(content_hash);
    }
    result.textures[0] = texture;
    result.rects[0]    = mRect;
    result.blend_coefficient = 0;
    result.sampler_names[0]  = mSamplerName;
    result.rect_names[0]     = mRectUniformName;
    return true;
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
    data.Read("rect", &mRect);
    data.Read("sampler_name", &mSamplerName);
    data.Read("rect_name", &mRectUniformName);

    const auto& texture = data.GetReadChunk("texture");
    if (!texture)
        return true;
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
    else BUG("???");

    if (!source->FromJson(*texture))
        return false;
    mSource = std::move(source);
    return true;
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
std::unique_ptr<MaterialClass> MaterialClass::FromJson(const data::Reader& data)
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
    else BUG("???");
    if (!klass->FromJson2(data))
        return nullptr;
    return klass;
}

Shader* ColorClass::GetShader(Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId()))
        return shader;

constexpr auto* src = R"(
#version 100
precision mediump float;

uniform vec4 kBaseColor;
uniform float kGamma;

// per vertex alpha.
varying float vAlpha;

void main()
{
  vec4 color = kBaseColor;
  color.a *= vAlpha;
  gl_FragColor = pow(color, vec4(kGamma));
}
)";
    auto* shader = device.MakeShader(GetProgramId());
    ShaderData data;
    data.gamma      = mGamma;
    data.base_color = mColor;
    shader->CompileSource(mStatic ? FoldUniforms(src, data) : src);
    return shader;
}
size_t ColorClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mColor);
    return hash;
}

std::string ColorClass::GetProgramId() const
{
    size_t hash = base::hash_combine(0, "color");
    if (mStatic)
    {
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mColor);
    }
    return std::to_string(hash);
}

void ColorClass::ApplyDynamicState(State& state, Device& device, Program& program) const
{
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = State::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = State::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = State::Blending::Additive;

    if (!mStatic)
    {
        SetUniform("kGamma",     state.uniforms, mGamma, program);
        SetUniform("kBaseColor", state.uniforms, mColor, program);
    }
}
void ColorClass::ApplyStaticState(Device& device, Program& prog) const
{
    prog.SetUniform("kGamma", mGamma);
    prog.SetUniform("kBaseColor", mColor);
}
void ColorClass::IntoJson(data::Writer& data) const
{
    data.Write("type", Type::Color);
    data.Write("id",      mClassId);
    data.Write("surface", mSurfaceType);
    data.Write("gamma",   mGamma);
    data.Write("static",  mStatic);
    data.Write("color",   mColor);
}
bool ColorClass::FromJson2(const data::Reader& data)
{
    if (!data.Read("id",      &mClassId) ||
        !data.Read("surface", &mSurfaceType) ||
        !data.Read("gamma",   &mGamma) ||
        !data.Read("static",  &mStatic) ||
        !data.Read("color",   &mColor))
        return false;
    return true;
}

Shader* GradientClass::GetShader(Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId()))
        return shader;

constexpr auto* src = R"(
#version 100
precision highp float;

uniform vec4 kColor0;
uniform vec4 kColor1;
uniform vec4 kColor2;
uniform vec4 kColor3;
uniform vec2 kOffset;
uniform float kGamma;
uniform float kRenderPoints;

varying float vAlpha;
varying vec2 vTexCoord;

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
  color.a *= vAlpha;
  gl_FragColor = pow(color, vec4(kGamma));
}
)";

    auto* shader = device.MakeShader(GetProgramId());
    ShaderData data;
    data.gamma = mGamma;
    data.color_map[0] = mColorMap[0];
    data.color_map[1] = mColorMap[1];
    data.color_map[2] = mColorMap[2];
    data.color_map[3] = mColorMap[3];
    data.gradient_offset = mOffset;
    shader->CompileSource(mStatic ? FoldUniforms(src, data) : src);
    return shader;
}
size_t GradientClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mColorMap[0]);
    hash = base::hash_combine(hash, mColorMap[1]);
    hash = base::hash_combine(hash, mColorMap[2]);
    hash = base::hash_combine(hash, mColorMap[3]);
    hash = base::hash_combine(hash, mOffset);
    return hash;
}
std::string GradientClass::GetProgramId() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, "gradient");
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

void GradientClass::ApplyDynamicState(State& state, Device& device, Program& program) const
{
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = State::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = State::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = State::Blending::Additive;

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

void GradientClass::ApplyStaticState(Device& device, Program& program) const
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
    data.Write("type", Type::Gradient);
    data.Write("id",         mClassId);
    data.Write("surface",    mSurfaceType);
    data.Write("gamma",      mGamma);
    data.Write("static",     mStatic);
    data.Write("color_map0", mColorMap[0]);
    data.Write("color_map1", mColorMap[1]);
    data.Write("color_map2", mColorMap[2]);
    data.Write("color_map3", mColorMap[3]);
    data.Write("offset", mOffset);
}
bool GradientClass::FromJson2(const data::Reader& data)
{
    if (!data.Read("id",         &mClassId) ||
        !data.Read("surface",    &mSurfaceType) ||
        !data.Read("gamma",      &mGamma) ||
        !data.Read("static",     &mStatic) ||
        !data.Read("color_map0", &mColorMap[0]) ||
        !data.Read("color_map1", &mColorMap[1]) ||
        !data.Read("color_map2", &mColorMap[2]) ||
        !data.Read("color_map3", &mColorMap[3]) ||
        !data.Read("offset", &mOffset))
        return false;
    return true;
}

SpriteClass::SpriteClass(const SpriteClass& other, bool copy)
   : mSprite(other.mSprite, copy)
{
    mClassId         = copy ? other.mClassId : base::RandomString(10);
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
}


Shader* SpriteClass::GetShader(Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId()))
        return shader;

    // todo: maybe pack some of shader uniforms
constexpr auto* src = R"(
#version 100
precision highp float;

uniform sampler2D kTexture0;
uniform sampler2D kTexture1;
uniform vec4 kTextureBox0;
uniform vec4 kTextureBox1;
uniform vec4 kBaseColor;
uniform float kRenderPoints;
uniform float kGamma;
uniform float kRuntime;
uniform float kBlendCoeff;
uniform float kApplyRandomParticleRotation;
uniform vec2 kTextureScale;
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
uniform float kTextureRotation;
uniform ivec2 kTextureWrap;
uniform vec2 kAlphaMask;

varying vec2 vTexCoord;
varying float vRandomValue;
varying float vAlpha;

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
    float random_angle = mix(0.0, vRandomValue, kApplyRandomParticleRotation);
    float angle = kTextureRotation + kTextureVelocityZ * kRuntime + random_angle * 3.1415926;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

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

    coords += kTextureVelocityXY * kRuntime;
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
    color.a *= vAlpha;

    // apply gamma (in)correction.
    gl_FragColor = pow(color, vec4(kGamma));
}
)";
    auto* shader = device.MakeShader(GetProgramId());
    ShaderData data;
    data.gamma            = mGamma;
    data.texture_scale    = mTextureScale;
    data.texture_velocity = mTextureVelocity;
    data.texture_rotation = mTextureRotation;
    shader->CompileSource(mStatic ? FoldUniforms(src, data) : src);
    return shader;
}

std::size_t SpriteClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
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
    hash = base::hash_combine(hash, mSprite.GetHash());
    return hash;
}

std::string SpriteClass::GetProgramId() const
{
    size_t hash = 0;
    base::hash_combine(hash, "sprite");
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

void SpriteClass::ApplyDynamicState(State& state, Device& device, Program& program) const
{
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = State::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = State::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = State::Blending::Additive;

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !mStatic;
    ts.current_time    = state.material_time;

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
    program.SetTextureCount(2);
    program.SetUniform("kBlendCoeff", mBlendFrames ? binds.blend_coefficient : 0.0f);
    program.SetUniform("kRuntime", (float)state.material_time);
    program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
    program.SetUniform("kAlphaMask", alpha_mask);
    program.SetUniform("kApplyRandomParticleRotation",
                    state.render_points && mParticleAction == ParticleAction::Rotate ? 1.0f : 0.0f);

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

void SpriteClass::ApplyStaticState(Device& device, Program& prog) const
{
    prog.SetUniform("kBaseColor", mBaseColor);
    prog.SetUniform("kGamma", mGamma);
    prog.SetUniform("kTextureScale", mTextureScale.x, mTextureScale.y);
    prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
    prog.SetUniform("kTextureVelocityZ", mTextureVelocity.z);
    prog.SetUniform("kTextureRotation", mTextureRotation);
}

void SpriteClass::IntoJson(data::Writer& data) const
{
    data.Write("type", Type::Sprite);
    data.Write("id", mClassId);
    data.Write("surface", mSurfaceType);
    data.Write("gamma", mGamma);
    data.Write("static", mStatic);
    data.Write("blending", mBlendFrames);
    data.Write("color", mBaseColor);
    data.Write("texture_min_filter", mMinFilter);
    data.Write("texture_mag_filter", mMagFilter);
    data.Write("texture_wrap_x", mWrapX);
    data.Write("texture_wrap_y", mWrapY);
    data.Write("texture_scale",  mTextureScale);
    data.Write("texture_velocity", mTextureVelocity);
    data.Write("texture_rotation", mTextureRotation);
    data.Write("particle_action", mParticleAction);
    mSprite.IntoJson(data);
}

bool SpriteClass::FromJson2(const data::Reader& data)
{
    data.Read("id", &mClassId);
    data.Read("surface", &mSurfaceType);
    data.Read("gamma", &mGamma);
    data.Read("static", &mStatic);
    data.Read("blending", &mBlendFrames);
    data.Read("color", &mBaseColor);
    data.Read("texture_min_filter", &mMinFilter);
    data.Read("texture_mag_filter", &mMagFilter);
    data.Read("texture_wrap_x", &mWrapX);
    data.Read("texture_wrap_y", &mWrapY);
    data.Read("texture_scale",  &mTextureScale);
    data.Read("texture_velocity", &mTextureVelocity);
    data.Read("texture_rotation", &mTextureRotation);
    data.Read("particle_action", &mParticleAction);
    mSprite.FromJson(data);
    return true;
}

void SpriteClass::BeginPacking(Packer* packer) const
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
        const Packer::ObjectHandle handle = source;
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
        packer->SetTextureFlag(handle, Packer::TextureFlags::CanCombine, can_combine);
    }
}
void SpriteClass::FinishPacking(const Packer* packer)
{
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        auto* source = mSprite.GetTextureSource(i);
        source->FinishPacking(packer);
    }
    for (size_t i=0; i<mSprite.GetNumTextures(); ++i)
    {
        auto* source = mSprite.GetTextureSource(i);
        const Packer::ObjectHandle handle = source;
        mSprite.SetTextureRect(i, packer->GetPackedTextureBox(handle));
    }
}

SpriteClass& SpriteClass::operator=(const SpriteClass& other)
{
    if (this == &other)
        return *this;
    SpriteClass tmp(other, true);
    std::swap(mClassId        , tmp.mClassId);
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
    return *this;
}


TextureMap2DClass::TextureMap2DClass(const TextureMap2DClass& other, bool copy)
    : mTexture(other.mTexture, copy)
{
    mClassId         = copy ? other.mClassId : base::RandomString(10);
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
}

Shader* TextureMap2DClass::GetShader(Device& device) const
{
    if (auto* shader = device.FindShader(GetProgramId()))
        return shader;

// todo: pack some of the uniforms ?
constexpr auto* src = R"(
#version 100
precision highp float;

uniform sampler2D kTexture;
uniform vec4 kTextureBox;
uniform float kAlphaMask;
uniform float kRenderPoints;
uniform float kGamma;
uniform float kApplyRandomParticleRotation;
uniform float kRuntime;
uniform vec2 kTextureScale;
uniform vec2 kTextureVelocityXY;
uniform float kTextureVelocityZ;
uniform float kTextureRotation;
uniform vec4 kBaseColor;
// 0 disabled, 1 clamp, 2 wrap
uniform ivec2 kTextureWrap;

varying vec2 vTexCoord;
varying float vRandomValue;
varying float vAlpha;

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
    float random_angle = mix(0.0, vRandomValue, kApplyRandomParticleRotation);
    float angle = kTextureRotation + kTextureVelocityZ * kRuntime + random_angle * 3.1415926;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

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
    coords += kTextureVelocityXY * kRuntime;
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
    vec4 col = mix(kBaseColor * texel, vec4(kBaseColor.rgb, kBaseColor.a * texel.a), kAlphaMask);
    col.a *= vAlpha;

    // apply gamma (in)correction.
    gl_FragColor = pow(col, vec4(kGamma));
}
)";
    auto* shader = device.MakeShader(GetProgramId());
    ShaderData data;
    data.gamma            = mGamma;
    data.base_color       = mBaseColor;
    data.texture_scale    = mTextureScale;
    data.texture_velocity = mTextureVelocity;
    data.texture_rotation = mTextureRotation;
    shader->CompileSource(mStatic ? FoldUniforms(src, data) : src);
    return shader;
}
std::size_t TextureMap2DClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
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
    hash = base::hash_combine(hash, mTexture.GetHash());
    return hash;
}
std::string TextureMap2DClass::GetProgramId() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, "texture");
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

void TextureMap2DClass::ApplyDynamicState(State& state, Device& device, Program& program) const
{
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = State::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = State::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = State::Blending::Additive;

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
    program.SetUniform("kRuntime", (float)state.material_time);
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

void TextureMap2DClass::ApplyStaticState(Device& device, Program& prog) const
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
    data.Write("type", Type::Texture);
    data.Write("id", mClassId);
    data.Write("surface", mSurfaceType);
    data.Write("gamma", mGamma);
    data.Write("static", mStatic);
    data.Write("color", mBaseColor);
    data.Write("texture_min_filter", mMinFilter);
    data.Write("texture_mag_filter", mMagFilter);
    data.Write("texture_wrap_x", mWrapX);
    data.Write("texture_wrap_y", mWrapY);
    data.Write("texture_scale",  mTextureScale);
    data.Write("texture_velocity", mTextureVelocity);
    data.Write("texture_rotation", mTextureRotation);
    data.Write("particle_action", mParticleAction);
    mTexture.IntoJson(data);
}

bool TextureMap2DClass::FromJson2(const data::Reader& data)
{
    data.Read("id", &mClassId);
    data.Read("surface", &mSurfaceType);
    data.Read("gamma", &mGamma);
    data.Read("static", &mStatic);
    data.Read("color", &mBaseColor);
    data.Read("texture_min_filter", &mMinFilter);
    data.Read("texture_mag_filter", &mMagFilter);
    data.Read("texture_wrap_x", &mWrapX);
    data.Read("texture_wrap_y", &mWrapY);
    data.Read("texture_scale",  &mTextureScale);
    data.Read("texture_velocity", &mTextureVelocity);
    data.Read("texture_rotation", &mTextureRotation);
    data.Read("particle_action", &mParticleAction);
    mTexture.FromJson(data);
    return true;
}

void TextureMap2DClass::BeginPacking(Packer* packer) const
{
    auto* source = mTexture.GetTextureSource();
    if (!source) return;

    source->BeginPacking(packer);

    const Packer::ObjectHandle handle = source;
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
    packer->SetTextureFlag(handle, Packer::TextureFlags::CanCombine, can_combine);

}
void TextureMap2DClass::FinishPacking(const Packer* packer)
{
    auto* source = mTexture.GetTextureSource();
    if (!source) return;

    const Packer::ObjectHandle handle = source;
    source->FinishPacking(packer);
    mTexture.SetTextureRect(packer->GetPackedTextureBox(handle));
}

TextureMap2DClass& TextureMap2DClass::operator=(const TextureMap2DClass& other)
{
    if (this == &other)
        return *this;

    TextureMap2DClass tmp(other, true);
    std::swap(mClassId        , tmp.mClassId);
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
    std::swap(mTexture, tmp.mTexture);
    return *this;
}

CustomMaterialClass::CustomMaterialClass(const CustomMaterialClass& other, bool copy)
{
    mClassId     = copy ? other.mClassId : base::RandomString(10);
    mShaderUri   = other.mShaderUri;
    mShaderSrc   = other.mShaderSrc;
    mUniforms    = other.mUniforms;
    mSurfaceType = other.mSurfaceType;
    mMinFilter   = other.mMinFilter;
    mMagFilter   = other.mMagFilter;
    mWrapX       = other.mWrapX;
    mWrapY       = other.mWrapY;
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

gfx::Shader* CustomMaterialClass::GetShader(Device& device) const
{
    if (auto* shader = device.FindShader(mClassId))
        return shader;
    auto* shader = device.MakeShader(mClassId);
    if (!mShaderSrc.empty())
        shader->CompileSource(mShaderSrc);
    else shader->CompileFile(mShaderUri);
    return shader;
}
std::size_t CustomMaterialClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mShaderUri);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mMinFilter);
    hash = base::hash_combine(hash, mMagFilter);
    hash = base::hash_combine(hash, mWrapX);
    hash = base::hash_combine(hash, mWrapY);

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
std::string CustomMaterialClass::GetProgramId() const
{
    return mClassId;
}
std::unique_ptr<MaterialClass> CustomMaterialClass::Copy() const
{ return std::make_unique<CustomMaterialClass>(*this); }

std::unique_ptr<MaterialClass> CustomMaterialClass::Clone() const
{
    auto ret = std::make_unique<CustomMaterialClass>(*this);
    ret->mClassId = base::RandomString(10);
    return ret;
}
void CustomMaterialClass::ApplyDynamicState(State& state, Device& device, Program& program) const
{
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = State::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = State::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = State::Blending::Additive;

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
    program.SetUniform("kRuntime", (float)state.material_time);
    program.SetUniform("kRenderPoints", state.render_points ? 1.0f : 0.0f);
    program.SetTextureCount(texture_unit);
}
void CustomMaterialClass::ApplyStaticState(Device& device, Program& prog) const
{
    // nothing to do here, static state should be in the shader
    // already, either by the shader programmer or by the shader
    // source generator.
}
void CustomMaterialClass::IntoJson(data::Writer& data) const
{
    data.Write("type",  Type::Custom);
    data.Write("id",          mClassId);
    data.Write("shader_uri",  mShaderUri);
    data.Write("shader_src",  mShaderSrc);
    data.Write("surface",     mSurfaceType);
    data.Write("min_filter",  mMinFilter);
    data.Write("mag_filter",  mMagFilter);
    data.Write("wrap_x",      mWrapX);
    data.Write("wrap_y",      mWrapY);

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

bool CustomMaterialClass::FromJson2(const data::Reader& data)
{
    data.Read("id",         &mClassId);
    data.Read("shader_uri", &mShaderUri);
    data.Read("shader_src", &mShaderSrc);
    data.Read("surface",    &mSurfaceType);
    data.Read("min_filter", &mMinFilter);
    data.Read("mag_filter", &mMagFilter);
    data.Read("wrap_x",     &mWrapX);
    data.Read("wrap_y",     &mWrapY);
    for (unsigned i=0; i<data.GetNumChunks("uniforms"); ++i)
    {
        Uniform uniform;
        const auto& chunk = data.GetReadChunk("uniforms", i);
        std::string name;
        if (!chunk->Read("name", &name))
            return false;
        if (!chunk->Read("value", &uniform))
            return false;
        mUniforms[std::move(name)] = std::move(uniform);
    }
    for (unsigned i=0; i<data.GetNumChunks("texture_maps"); ++i)
    {
        const auto& chunk = data.GetReadChunk("texture_maps", i);
        std::string name;
        TextureMap::Type type;
        if (!chunk->Read("name", &name) ||
            !chunk->Read("type", &type))
            return false;
        std::unique_ptr<TextureMap> map;
        if (type == TextureMap::Type::Texture2D)
            map.reset(new TextureMap2D);
        else if (type == TextureMap::Type::Sprite)
            map.reset(new SpriteMap);
        else BUG("wot");
        if (!map->FromJson(*chunk))
            return false;
        mTextureMaps[name] = std::move(map);
    }
    return true;
}
void CustomMaterialClass::BeginPacking(Packer* packer) const
{
    packer->PackShader(this, mShaderUri);

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
                const Packer::ObjectHandle handle = source;
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
void CustomMaterialClass::FinishPacking(const Packer* packer)
{
    mShaderUri = packer->GetPackedShaderId(this);
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
    std::swap(mClassId, tmp.mClassId);
    std::swap(mShaderUri, tmp.mShaderUri);
    std::swap(mShaderSrc, tmp.mShaderSrc);
    std::swap(mUniforms, tmp.mUniforms);
    std::swap(mSurfaceType, tmp.mSurfaceType);
    std::swap(mTextureMaps, tmp.mTextureMaps);
    std::swap(mMinFilter, tmp.mMinFilter);
    std::swap(mMagFilter, tmp.mMagFilter);
    std::swap(mWrapX, tmp.mWrapX);
    std::swap(mWrapY, tmp.mWrapY);
    return *this;
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
    const auto& name = std::to_string(hash);
    auto* texture = device.FindTexture(name);
    if (!texture)
    {
        // create the texture object first. The if check above
        // will then act as a throttle and prevent superfluous
        // attempts to rasterize when the contents of the text
        // buffer have not changed.
        texture = device.MakeTexture(name);

        auto bitmap = mText.TryRasterize();
        if (!bitmap)
            return;
        const auto width  = bitmap->GetWidth();
        const auto height = bitmap->GetHeight();

        // current text rendering use cases for this TextMaterial
        // are such that we expect the rendered geometry to match
        // the underlying rasterized text texture size almost exactly.
        // this means that we can skip the mipmap generation and use
        // a simple fast nearest/linear texture filter without mips.
        const bool mips = false;
        texture->SetName("TextMaterial");
        texture->SetTransient(true);
        texture->Upload(bitmap->GetDataPtr(), width, height, gfx::Texture::Format::Grayscale, mips);
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
void TextMaterial::ApplyStaticState(gfx::Device& device, gfx::Program& program) const
{}
Shader* TextMaterial::GetShader(gfx::Device& device) const
{
constexpr auto* src = R"(
#version 100
precision highp float;
uniform sampler2D kTexture;
uniform vec4 kColor;
uniform float kTime;
varying vec2 vTexCoord;
void main() {
   float alpha = texture2D(kTexture, vTexCoord).a;
   gl_FragColor = vec4(kColor.r, kColor.g, kColor.b, kColor.a * alpha);
}
        )";
    auto* shader = device.MakeShader("text-shader");
    shader->CompileSource(src);
    return shader;
}
std::string TextMaterial::GetProgramId() const
{ return "text-shader"; }
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

ColorClass CreateMaterialClassFromColor(const Color4f& color)
{
    ColorClass material;
    material.SetBaseColor(color);
    return material;
}

TextureMap2DClass CreateMaterialClassFromTexture(const std::string& uri)
{
    TextureMap2DClass material;
    material.SetTexture(LoadTextureFromFile(uri));
    material.SetSurfaceType(MaterialClass::SurfaceType::Opaque);
    return material;
}

SpriteClass CreateMaterialClassFromSprite(const std::initializer_list<std::string>& textures)
{
    SpriteClass material;
    for (const auto& texture : textures)
        material.AddTexture(LoadTextureFromFile(texture));

    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    return material;
}

SpriteClass CreateMaterialClassFromSprite(const std::vector<std::string>& textures)
{
    SpriteClass material;
    for (const auto& texture : textures)
        material.AddTexture(LoadTextureFromFile(texture));

    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    return material;
}

SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& texture, const std::vector<FRect>& frames)
{
    SpriteClass material;
    for (size_t i=0; i<frames.size(); ++i)
    {
        material.AddTexture(LoadTextureFromFile(texture));
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
