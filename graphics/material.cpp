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

#include "base/logging.h"
#include "graphics/material.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "graphics/program.h"

namespace gfx
{

std::shared_ptr<IBitmap> detail::TextureFileSource::GetData() const
{
    try
    {
        // in case of an image load exception, catch and log and return
        // null.
        // Todo: image class should probably provide a non-throw load
        // but then it also needs some mechanism for getting the error
        // message why it failed.
        Image file(mFile, ResourceLoader::ResourceType::Texture);
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

MaterialClass::MaterialClass(const MaterialClass& other)
{
    mId          = other.mId;
    mShaderFile  = other.mShaderFile;
    mBaseColor   = other.mBaseColor;
    mSurfaceType = other.mSurfaceType;
    mType        = other.mType;
    mGamma       = other.mGamma;
    mFps         = other.mFps;
    mBlendFrames = other.mBlendFrames;
    mStatic      = other.mStatic;
    mMinFilter   = other.mMinFilter;
    mMagFilter   = other.mMagFilter;
    mWrapX       = other.mWrapX;
    mWrapY       = other.mWrapY;
    mTextureScale = other.mTextureScale;
    mTextureVelocity = other.mTextureVelocity;
    mColorMap[0] = other.mColorMap[0];
    mColorMap[1] = other.mColorMap[1];
    mColorMap[2] = other.mColorMap[2];
    mColorMap[3] = other.mColorMap[3];
    mParticleAction = other.mParticleAction;

    // copy texture samplers.
    for (const auto& sampler : other.mTextures)
    {
        TextureSampler copy;
        copy.box = sampler.box;
        copy.enable_gc = sampler.enable_gc;
        copy.source = sampler.source->Clone();
        mTextures.push_back(std::move(copy));
    }
}

Shader* MaterialClass::GetShader(Device& device) const
{
    const std::string& file = GetShaderFile();
    ASSERT(!file.empty());

    Shader* shader = device.FindShader(file);
    if (shader == nullptr)
    {
        shader = device.MakeShader(file);
        shader->CompileFile(file);
    }
    return shader;
}

void MaterialClass::ApplyDynamicState(const Environment& env, const MaterialInstance& inst,
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

        const auto frame_count = mType == Type::Texture ? 1u : (unsigned)mTextures.size();
        const unsigned frame_index[2] = {
            (first_frame_index + 0) % frame_count,
            (first_frame_index + 1) % frame_count
        };

        bool need_software_wrap = true;

        for (unsigned i=0; i<std::min(frame_count, 2u); ++i)
        {
            const auto& sampler = mTextures[frame_index[i]];
            const auto& source  = sampler.source;
            const auto& name    = std::to_string(source->GetHash());
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
        ApplyStaticState(device, prog);
    }

    // set the instance alpha. todo: maybe this should just be a float
    gfx::Color4f color = mBaseColor;
    color.SetAlpha(inst.alpha);
    prog.SetUniform("kBaseColor", color);
}

void MaterialClass::ApplyStaticState(Device& device, Program& prog) const
{
    // color is now set always because of instance alpha.
    //prog.SetUniform("kBaseColor", mBaseColor);
    prog.SetUniform("kGamma", mGamma);
    prog.SetUniform("kTextureScale", mTextureScale.x, mTextureScale.y);
    prog.SetUniform("kTextureVelocityXY", mTextureVelocity.x, mTextureVelocity.y);
    prog.SetUniform("kTextureVelocityZ", mTextureVelocity.z);
    prog.SetUniform("kColor0", mColorMap[0]);
    prog.SetUniform("kColor1", mColorMap[1]);
    prog.SetUniform("kColor2", mColorMap[2]);
    prog.SetUniform("kColor3", mColorMap[3]);
}

std::string MaterialClass::GetProgramId() const
{
    // if the static flag is set the material id is
    // derived from the current state, thus  mapping material objects with
    // different parameters to unique shader programs (even when they're
    // the same type of shader program)
    if (mStatic)
        return std::to_string(GetHash());

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
    hash = base::hash_combine(hash, mBaseColor.GetHash());
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
    hash = base::hash_combine(hash, mColorMap[0].GetHash());
    hash = base::hash_combine(hash, mColorMap[1].GetHash());
    hash = base::hash_combine(hash, mColorMap[2].GetHash());
    hash = base::hash_combine(hash, mColorMap[3].GetHash());
    hash = base::hash_combine(hash, mParticleAction);
    for (const auto& sampler : mTextures)
    {
        hash = base::hash_combine(hash, sampler.source->GetId());
        hash = base::hash_combine(hash, sampler.enable_gc);
        hash = base::hash_combine(hash, sampler.box.GetHash());
    }
    return hash;
}

nlohmann::json MaterialClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mId);
    base::JsonWrite(json, "shader_file", mShaderFile);
    base::JsonWrite(json, "type", mType);
    base::JsonWrite(json, "color", mBaseColor);
    base::JsonWrite(json, "surface", mSurfaceType);
    base::JsonWrite(json, "gamma", mGamma);
    base::JsonWrite(json, "fps", mFps);
    base::JsonWrite(json, "blending", mBlendFrames);
    base::JsonWrite(json, "static", mStatic);
    base::JsonWrite(json, "texture_min_filter", mMinFilter);
    base::JsonWrite(json, "texture_mag_filter", mMagFilter);
    base::JsonWrite(json, "texture_wrap_x", mWrapX);
    base::JsonWrite(json, "texture_wrap_y", mWrapY);
    base::JsonWrite(json, "texture_scale", mTextureScale);
    base::JsonWrite(json, "texture_velocity", mTextureVelocity);
    base::JsonWrite(json, "color_map0", mColorMap[0]);
    base::JsonWrite(json, "color_map1", mColorMap[1]);
    base::JsonWrite(json, "color_map2", mColorMap[2]);
    base::JsonWrite(json, "color_map3", mColorMap[3]);
    base::JsonWrite(json, "particle_action", mParticleAction);

    for (const auto& sampler : mTextures)
    {
        nlohmann::json js;
        base::JsonWrite(js, "box", sampler.box);
        base::JsonWrite(js, "type", sampler.source->GetSourceType());
        base::JsonWrite(js, "source", *sampler.source);
        base::JsonWrite(js, "enable_gc", sampler.enable_gc);
        json["samplers"].push_back(js);
    }
    return json;
}
// static
std::optional<MaterialClass> MaterialClass::FromJson(const nlohmann::json& object)
{
    MaterialClass mat;
    if (!base::JsonReadSafe(object, "id", &mat.mId) ||
        !base::JsonReadSafe(object, "shader_file", &mat.mShaderFile) ||
        !base::JsonReadSafe(object, "color", &mat.mBaseColor) ||
        !base::JsonReadSafe(object, "type", &mat.mType) ||
        !base::JsonReadSafe(object, "surface", &mat.mSurfaceType) ||
        !base::JsonReadSafe(object, "gamma", &mat.mGamma) ||
        !base::JsonReadSafe(object, "fps", &mat.mFps) ||
        !base::JsonReadSafe(object, "blending", &mat.mBlendFrames) ||
        !base::JsonReadSafe(object, "static", &mat.mStatic) ||
        !base::JsonReadSafe(object, "texture_min_filter", &mat.mMinFilter) ||
        !base::JsonReadSafe(object, "texture_mag_filter", &mat.mMagFilter) ||
        !base::JsonReadSafe(object, "texture_wrap_x", &mat.mWrapX) ||
        !base::JsonReadSafe(object, "texture_wrap_y", &mat.mWrapY) ||
        !base::JsonReadSafe(object, "texture_scale", &mat.mTextureScale) ||
        !base::JsonReadSafe(object, "texture_velocity", &mat.mTextureVelocity) ||
        !base::JsonReadSafe(object, "color_map0", &mat.mColorMap[0]) ||
        !base::JsonReadSafe(object, "color_map1", &mat.mColorMap[1]) ||
        !base::JsonReadSafe(object, "color_map2", &mat.mColorMap[2]) ||
        !base::JsonReadSafe(object, "color_map3", &mat.mColorMap[3]) ||
        !base::JsonReadSafe(object, "particle_action", &mat.mParticleAction))
        return std::nullopt;

    if (!object.contains("samplers"))
        return mat;

    for (const auto& json_sampler : object["samplers"].items())
    {
        const auto& obj = json_sampler.value();
        TextureSource::Source type;
        if (!base::JsonReadSafe(obj, "type", &type))
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
        else return std::nullopt;

        if (!source->FromJson(obj["source"]))
            return std::nullopt;

        TextureSampler s;
        if (!base::JsonReadSafe(obj, "box", &s.box) ||
            !base::JsonReadSafe(obj, "enable_gc", &s.enable_gc))
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
