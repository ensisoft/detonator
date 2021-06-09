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
} // namespace

namespace gfx
{

std::shared_ptr<IBitmap> detail::TextureFileSource::GetData() const
{
    try
    {
        // in case of an image load exception, catch and log and return null.
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

        ERROR("Failed to load texture. '%1'", mFile);
        return nullptr;
    }
    catch (const std::exception& e)
    {
        ERROR(e.what());
        ERROR("Failed to load texture. '%1'", mFile);
    }
    return nullptr;
}
void detail::TextureFileSource::IntoJson(data::Writer& data) const
{
    data.Write("id",   mId);
    data.Write("file", mFile);
    data.Write("name", mName);
}
bool detail::TextureFileSource::FromJson(const data::Reader& data)
{
    return data.Read("id",   &mId) &&
           data.Read("file", &mFile) &&
           data.Read("name", &mName);
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


MaterialClass::MaterialClass(const MaterialClass& other)
{
    mId              = other.mId;
    mShaderFile      = other.mShaderFile;
    mBaseColor       = other.mBaseColor;
    mSurfaceType     = other.mSurfaceType;
    mType            = other.mType;
    mGamma           = other.mGamma;
    mFps             = other.mFps;
    mBlendFrames     = other.mBlendFrames;
    mStatic          = other.mStatic;
    mMinFilter       = other.mMinFilter;
    mMagFilter       = other.mMagFilter;
    mWrapX           = other.mWrapX;
    mWrapY           = other.mWrapY;
    mTextureScale    = other.mTextureScale;
    mTextureVelocity = other.mTextureVelocity;
    mColorMap[0]     = other.mColorMap[0];
    mColorMap[1]     = other.mColorMap[1];
    mColorMap[2]     = other.mColorMap[2];
    mColorMap[3]     = other.mColorMap[3];
    mParticleAction  = other.mParticleAction;

    // copy texture samplers.
    for (const auto& sampler : other.mTextures)
    {
        TextureSampler copy;
        copy.box       = sampler.box;
        copy.enable_gc = sampler.enable_gc;
        copy.source    = sampler.source->Copy();
        mTextures.push_back(std::move(copy));
    }
}

Shader* MaterialClass::GetShader(Device& device) const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, GetShaderFile());
    hash = base::hash_combine(hash, mStatic ? GetProgramId() : "");
    const auto& name = std::to_string(hash);
    Shader* shader = device.FindShader(name);
    if (shader)
        return shader;

    const auto& buffer = gfx::LoadResource(GetShaderFile());
    if (!buffer)
    {
        ERROR("Failed to load shader file: '%1'", GetShaderFile());
        return nullptr;
    }
    // todo: get rid of the copy into stringstream below
    const char* beg = (const char*)buffer->GetData();
    const char* end = beg + buffer->GetSize();
    std::string code;
    std::string line;
    std::stringstream ss(std::string(beg, end));
    while (std::getline(ss, line))
    {
        if (mStatic && base::Contains(line, "uniform"))
        {
            const auto original = line;
            if (base::Contains(line, "kGamma"))
                line = base::FormatString("const float kGamma = %1;", ToConst(mGamma));
            else if (base::Contains(line, "kBaseColor"))
                line = base::FormatString("const vec4 kBaseColor = %1;", ToConst(mBaseColor));
            else if (base::Contains(line, "kTextureScale"))
                line = base::FormatString("const vec2 kTextureScale = %1;", ToConst(mTextureScale));
            else if (base::Contains(line, "kTextureVelocityXY"))
                line = base::FormatString("const vec2 kTextureVelocityXY = %1;", ToConst(glm::vec2(mTextureVelocity)));
            else if (base::Contains(line, "kTextureVelocityZ"))
                line = base::FormatString("const float kTextureVelocityZ = %1;", ToConst(mTextureVelocity.z));
            else if (base::Contains(line, "kColor0"))
                line = base::FormatString("const vec4 kColor0 = %1;", ToConst(mColorMap[0]));
            else if (base::Contains(line, "kColor1"))
                line = base::FormatString("const vec4 kColor1 = %1;", ToConst(mColorMap[1]));
            else if (base::Contains(line, "kColor2"))
                line = base::FormatString("const vec4 kColor2 = %1;", ToConst(mColorMap[2]));
            else if (base::Contains(line, "kColor3"))
                line = base::FormatString("const vec4 kColor3 = %1;", ToConst(mColorMap[3]));
            if (original != line)
                DEBUG("'%1' => '%2", original, line);
        }
        code.append(line);
        code.append("\n");
    }
    shader = device.MakeShader(name);
    shader->CompileSource(code);
    return shader;
}

void MaterialClass::ApplyDynamicState(const Environment& env, const InstanceState& inst,
    Device& device, Program& prog, RasterState& state) const
{
    // set rasterizer state.
    if (mSurfaceType == SurfaceType::Opaque)
        state.blending = RasterState::Blending::None;
    else if (mSurfaceType == SurfaceType::Transparent)
        state.blending = RasterState::Blending::Transparent;
    else if (mSurfaceType == SurfaceType::Emissive)
        state.blending = RasterState::Blending::Additive;

    // mark whether textures are considered alpha masks. 1.0f = is alpha mask, 0.0f = not alpha mask
    float alpha_masks[2] = {0.0f, 0.0f};

    // currently we only bind two textures and set a blend
    // coefficient from the run time for the shader to do
    // blending between two animation frames.
    // Note different material instances map to the same program
    // object on the device. That means that if material m0 uses
    // textures t0 and t1 to render and material m1 *want's to use*
    // texture t2 and t3 to render but *forgets* to set them
    // then the rendering will use textures t0 and t1
    // or whichever textures happen to be bound to the texture units
    // since last rendering.
    if ((mType == Type::Texture || mType == Type::Sprite) && !mTextures.empty())
    {
        // if we're a sprite then we should probably animate if we have an FPS setting.
        const auto animating         = mType == Type::Sprite && mFps > 0.0f;
        const auto frame_interval    = animating ? 1.0f / mFps : 0.0f;
        const auto frame_fraction    = animating ? std::fmod(inst.runtime, frame_interval) : 0.0f;
        const auto frame_blend_coeff = animating ? frame_fraction/frame_interval : 0.0f;
        const auto first_frame_index = animating ? (unsigned)(inst.runtime/frame_interval) : 0u;

        const auto frame_index_count = mType == Type::Texture ? 1u : (unsigned)mTextures.size();
        const unsigned frame_index[2] = {
            (first_frame_index + 0) % frame_index_count,
            (first_frame_index + 1) % frame_index_count
        };
        const auto texture_count = mType == Type::Texture ? 1u : 2u;

        bool need_software_wrap = true;

        for (unsigned i=0; i<texture_count; ++i)
        {
            const auto& sampler = mTextures[frame_index[i]];
            const auto& source  = sampler.source;
            const auto& name    = std::to_string(source->GetContentHash());
            auto* texture = device.FindTexture(name);
            if (!texture)
            {
                texture = device.MakeTexture(name);
                auto bitmap = source->GetData();
                if (!bitmap)
                    continue;
                const auto width  = bitmap->GetWidth();
                const auto height = bitmap->GetHeight();
                const auto format = Texture::DepthToFormat(bitmap->GetDepthBits());
                texture->Upload(bitmap->GetDataPtr(), width, height, format);
                texture->EnableGarbageCollection(sampler.enable_gc);
            }
            const auto& box = sampler.box;
            const float x  = box.GetX();
            const float y  = box.GetY();
            const float sx = box.GetWidth();
            const float sy = box.GetHeight();
            const auto& kTexture = "kTexture" + std::to_string(i);
            const auto& kTextureBox = "kTextureBox" + std::to_string(i);
            if (texture->GetFormat() == Texture::Format::Grayscale)
                alpha_masks[i] = 1.0f;

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

            // set texture properties *before* setting it to the program.
            texture->SetFilter(mMinFilter);
            texture->SetFilter(mMagFilter);
            texture->SetWrapX(mWrapX);
            texture->SetWrapY(mWrapY);

            prog.SetTexture(kTexture.c_str(), i, *texture);
            prog.SetUniform(kTextureBox.c_str(), x, y, sx, sy);
            prog.SetUniform("kBlendCoeff", mBlendFrames ? frame_blend_coeff : 0.0f);
        }
        prog.SetTextureCount(texture_count);
        // set software wrap/clamp. 0 = disabled.
        if (need_software_wrap)
        {
            const auto wrap_x = mWrapX == TextureWrapping::Clamp ? 1 : 2;
            const auto wrap_y = mWrapY == TextureWrapping::Clamp ? 1 : 2;
            prog.SetUniform("kTextureWrap", wrap_x, wrap_y);
        }
        else
        {
            prog.SetUniform("kTextureWrap", 0, 0);
        }
    }
    prog.SetUniform("kApplyRandomParticleRotation",
        env.render_points && mParticleAction == ParticleAction::Rotate ? 1.0f : 0.0f);
    prog.SetUniform("kRenderPoints", env.render_points ? 1.0f : 0.0f);
    prog.SetUniform("kIsAlphaMask", alpha_masks[0], alpha_masks[1]);
    prog.SetUniform("kRuntime", inst.runtime);

    if (!mStatic)
    {
        // if not static then always make sure to apply full material state
        // in the shader program.
        prog.SetUniform("kBaseColor", inst.base_color);
        prog.SetUniform("kGamma", mGamma);
        prog.SetUniform("kTextureScale", mTextureScale.x, mTextureScale.y);
        prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
        prog.SetUniform("kTextureVelocityZ", mTextureVelocity.z);
        prog.SetUniform("kColor0", mColorMap[0]);
        prog.SetUniform("kColor1", mColorMap[1]);
        prog.SetUniform("kColor2", mColorMap[2]);
        prog.SetUniform("kColor3", mColorMap[3]);
    }
}

void MaterialClass::ApplyStaticState(Device& device, Program& prog) const
{
    if (mStatic)
    {
        prog.SetUniform("kBaseColor", mBaseColor);
        prog.SetUniform("kGamma", mGamma);
        prog.SetUniform("kTextureScale", mTextureScale.x, mTextureScale.y);
        prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
        prog.SetUniform("kTextureVelocityZ", mTextureVelocity.z);
        prog.SetUniform("kColor0", mColorMap[0]);
        prog.SetUniform("kColor1", mColorMap[1]);
        prog.SetUniform("kColor2", mColorMap[2]);
        prog.SetUniform("kColor3", mColorMap[3]);
    }
}

std::string MaterialClass::GetProgramId() const
{
    // if the static flag is set the material id is
    // derived from the current state that is marked static,
    // thus mapping material objects with
    // different parameters to unique shader programs (even when they
    // share the same underlying shader program)
    if (mStatic)
    {
        std::size_t hash = 0;
        hash = base::hash_combine(hash, mBaseColor);
        hash = base::hash_combine(hash, mGamma);
        hash = base::hash_combine(hash, mTextureScale);
        hash = base::hash_combine(hash, mTextureVelocity);
        hash = base::hash_combine(hash, mColorMap[0]);
        hash = base::hash_combine(hash, mColorMap[1]);
        hash = base::hash_combine(hash, mColorMap[2]);
        hash = base::hash_combine(hash, mColorMap[3]);
        return std::to_string(hash);
    }

    // get the material id based on the type of the material. in this
    // case when then material state must be set each time into the
    // shader program before drawing.
    // For example if there are two materials m1 and m2 and both are of
    // type of Color. They'll both internally use the same shader program
    // but both material instances can have their own properties (i.e.
    // different color etc material properties) and thus all these
    // properties must be set on the shader program on each draw when
    // the material is being used.
    const std::size_t hash = std::hash<std::string>()(GetShaderFile());
    return std::to_string(hash);
}

std::string MaterialClass::GetShaderFile() const
{
    // check if have a user defined specific shader or not.
    if (!mShaderFile.empty())
        return mShaderFile;

    if (mType == Type::Color)
        return "shaders/es2/solid_color.glsl";
    else if (mType == Type::Gradient)
        return "shaders/es2/gradient.glsl";
    else if (mType == Type::Texture)
        return "shaders/es2/texture_map.glsl";
    else if (mType == Type::Sprite)
        return "shaders/es2/texture_map.glsl";
    BUG("???");
    return "";
}

size_t MaterialClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mShaderFile);
    hash = base::hash_combine(hash, mBaseColor);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mGamma);
    hash = base::hash_combine(hash, mFps);
    hash = base::hash_combine(hash, mBlendFrames);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mMinFilter);
    hash = base::hash_combine(hash, mMagFilter);
    hash = base::hash_combine(hash, mWrapX);
    hash = base::hash_combine(hash, mWrapY);
    hash = base::hash_combine(hash, mTextureScale);
    hash = base::hash_combine(hash, mTextureVelocity);
    hash = base::hash_combine(hash, mColorMap[0]);
    hash = base::hash_combine(hash, mColorMap[1]);
    hash = base::hash_combine(hash, mColorMap[2]);
    hash = base::hash_combine(hash, mColorMap[3]);
    hash = base::hash_combine(hash, mParticleAction);
    for (const auto& sampler : mTextures)
    {
        hash = base::hash_combine(hash, sampler.source->GetHash());
        hash = base::hash_combine(hash, sampler.enable_gc);
        hash = base::hash_combine(hash, sampler.box);
    }
    return hash;
}

void MaterialClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("shader_file", mShaderFile);
    data.Write("type", mType);
    data.Write("color", mBaseColor);
    data.Write("surface", mSurfaceType);
    data.Write("gamma", mGamma);
    data.Write("fps", mFps);
    data.Write("blending", mBlendFrames);
    data.Write("static", mStatic);
    data.Write("texture_min_filter", mMinFilter);
    data.Write("texture_mag_filter", mMagFilter);
    data.Write("texture_wrap_x", mWrapX);
    data.Write("texture_wrap_y", mWrapY);
    data.Write("texture_scale", mTextureScale);
    data.Write("texture_velocity", mTextureVelocity);
    data.Write("color_map0", mColorMap[0]);
    data.Write("color_map1", mColorMap[1]);
    data.Write("color_map2", mColorMap[2]);
    data.Write("color_map3", mColorMap[3]);
    data.Write("particle_action", mParticleAction);

    for (const auto& sampler : mTextures)
    {
        auto meta = data.NewWriteChunk();
        auto source = data.NewWriteChunk();
        sampler.source->IntoJson(*source);
        meta->Write("box", sampler.box);
        meta->Write("type", sampler.source->GetSourceType());
        meta->Write("enable_gc", sampler.enable_gc);
        meta->Write("source", std::move(source));
        data.AppendChunk("samplers", std::move(meta));
    }
}
// static
std::optional<MaterialClass> MaterialClass::FromJson(const data::Reader& data)
{
    MaterialClass mat;
    if (!data.Read("id", &mat.mId) ||
        !data.Read("shader_file", &mat.mShaderFile) ||
        !data.Read("color", &mat.mBaseColor) ||
        !data.Read("type", &mat.mType) ||
        !data.Read("surface", &mat.mSurfaceType) ||
        !data.Read("gamma", &mat.mGamma) ||
        !data.Read("fps", &mat.mFps) ||
        !data.Read("blending", &mat.mBlendFrames) ||
        !data.Read("static", &mat.mStatic) ||
        !data.Read("texture_min_filter", &mat.mMinFilter) ||
        !data.Read("texture_mag_filter", &mat.mMagFilter) ||
        !data.Read("texture_wrap_x", &mat.mWrapX) ||
        !data.Read("texture_wrap_y", &mat.mWrapY) ||
        !data.Read("texture_scale", &mat.mTextureScale) ||
        !data.Read("texture_velocity", &mat.mTextureVelocity) ||
        !data.Read("color_map0", &mat.mColorMap[0]) ||
        !data.Read("color_map1", &mat.mColorMap[1]) ||
        !data.Read("color_map2", &mat.mColorMap[2]) ||
        !data.Read("color_map3", &mat.mColorMap[3]) ||
        !data.Read("particle_action", &mat.mParticleAction))
        return std::nullopt;

    for (unsigned i=0; i<data.GetNumChunks("samplers"); ++i)
    {
        const auto& meta = data.GetReadChunk("samplers", i);
        TextureSource::Source type;
        TextureSampler s;
        if (!meta->Read("type", &type) ||
            !meta->Read("box", &s.box) ||
            !meta->Read("enable_gc", &s.enable_gc))
            return std::nullopt;
        std::shared_ptr<TextureSource> source;
        if (type == TextureSource::Source::Filesystem)
            source = std::make_shared<detail::TextureFileSource>();
        else if (type == TextureSource::Source::TextBuffer)
            source = std::make_shared<detail::TextureTextBufferSource>();
        else if (type == TextureSource::Source::BitmapBuffer)
            source = std::make_shared<detail::TextureBitmapBufferSource>();
        else if (type == TextureSource::Source::BitmapGenerator)
            source = std::make_shared<detail::TextureBitmapGeneratorSource>();
        else BUG("???");

        const auto& chunk = meta->GetReadChunk("source");
        if (!chunk || !source->FromJson(*chunk))
            return std::nullopt;

        s.source = std::move(source);
        mat.mTextures.push_back(std::move(s));
    }
    return mat;
}

void MaterialClass::BeginPacking(ResourcePacker* packer) const
{
    packer->PackShader(this, GetShaderFile());
    for (const auto& sampler : mTextures)
    {
        sampler.source->BeginPacking(packer);
    }
    for (const auto& sampler : mTextures)
    {
        const ResourcePacker::ObjectHandle handle = sampler.source.get();
        packer->SetTextureBox(handle, sampler.box);

        // when texture rects are used to address a sub rect within the
        // texture wrapping on texture coordinates must be done "manually"
        // since the HW sampler coords are outside the sub rectangle coords.
        // for example if the wrapping is set to wrap on x and our box is
        // 0.25 units the HW sampler would not help us here to wrap when the
        // X coordinate is 0.26. Instead we need to do the wrap manually.
        // However this can cause rendering artifacts when texture sampling
        // is done depending on the current filter being used.
        bool can_combine = true;

        const auto& box = sampler.box;
        const auto x = box.GetX();
        const auto y = box.GetY();
        const auto w = box.GetWidth();
        const auto h = box.GetHeight();
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

        packer->SetTextureFlag(handle, ResourcePacker::TextureFlags::CanCombine, can_combine);
    }
}

void MaterialClass::FinishPacking(const ResourcePacker* packer)
{
    mShaderFile = packer->GetPackedShaderId(this);
    for (auto& sampler : mTextures)
    {
        sampler.source->FinishPacking(packer);
    }
    for (auto& sampler : mTextures)
    {
        const ResourcePacker::ObjectHandle handle = sampler.source.get();
        sampler.box = packer->GetPackedTextureBox(handle);
    }
}

MaterialClass MaterialClass::Clone() const
{
    MaterialClass ret;
    ret.mShaderFile      = mShaderFile;
    ret.mBaseColor       = mBaseColor;
    ret.mSurfaceType     = mSurfaceType;
    ret.mType            = mType;
    ret.mGamma           = mGamma;
    ret.mFps             = mFps;
    ret.mBlendFrames     = mBlendFrames;
    ret.mStatic          = mStatic;
    ret.mMinFilter       = mMinFilter;
    ret.mMagFilter       = mMagFilter;
    ret.mWrapX           = mWrapX;
    ret.mWrapY           = mWrapY;
    ret.mTextureScale    = mTextureScale;
    ret.mTextureVelocity = mTextureVelocity;
    ret.mColorMap[0]     = mColorMap[0];
    ret.mColorMap[1]     = mColorMap[1];
    ret.mColorMap[2]     = mColorMap[2];
    ret.mColorMap[3]     = mColorMap[3];
    ret.mParticleAction  = mParticleAction;

    // copy texture samplers.
    for (const auto& sampler : mTextures)
    {
        TextureSampler clone;
        clone.box       = sampler.box;
        clone.enable_gc = sampler.enable_gc;
        clone.source    = sampler.source->Clone();
        ret.mTextures.push_back(std::move(clone));
    }
    return ret;
}


MaterialClass& MaterialClass::operator=(const MaterialClass& other)
{
    if (this == &other)
        return *this;

    MaterialClass tmp(other);
    std::swap(mId, tmp.mId);
    std::swap(mShaderFile, tmp.mShaderFile);
    std::swap(mBaseColor, tmp.mBaseColor);
    std::swap(mSurfaceType, tmp.mSurfaceType);
    std::swap(mType, tmp.mType);
    std::swap(mGamma, tmp.mGamma);
    std::swap(mFps, tmp.mFps);
    std::swap(mBlendFrames, tmp.mBlendFrames);
    std::swap(mStatic, tmp.mStatic);
    std::swap(mMinFilter, tmp.mMinFilter);
    std::swap(mMagFilter, tmp.mMagFilter);
    std::swap(mWrapX, tmp.mWrapX);
    std::swap(mWrapY, tmp.mWrapY);
    std::swap(mTextureScale, tmp.mTextureScale);
    std::swap(mTextureVelocity, tmp.mTextureVelocity);
    std::swap(mTextures, tmp.mTextures);
    std::swap(mColorMap[0], tmp.mColorMap[0]);
    std::swap(mColorMap[1], tmp.mColorMap[1]);
    std::swap(mColorMap[2], tmp.mColorMap[2]);
    std::swap(mColorMap[3], tmp.mColorMap[3]);
    std::swap(mParticleAction, tmp.mParticleAction);
    return *this;
}

std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass)
{
    return std::make_unique<Material>(klass);
}

std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass)
{
    return std::make_unique<Material>(klass);
}
} // namespace
