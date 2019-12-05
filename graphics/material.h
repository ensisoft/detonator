// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#pragma once

#include "config.h"

#include <vector>
#include <string>
#include <cmath>
#include <cassert>

#include "device.h"
#include "shader.h"
#include "program.h"
#include "texture.h"
#include "color4f.h"
#include "text.h"

namespace gfx
{
    class Material
    {
    public:
        enum class SurfaceType {
            // Surface is opaque and no blending is done.
            Opaque,
            // Surface is transparent and is blended with the destination
            // to create a interpolated mix of the colors.
            Transparent,
            // Surface gives off color (light)
            Emissive
        };

        virtual ~Material() = default;

        // Create the shader for this material on the given device.
        // Returns the new shader object or nullptr if the shader
        // failed to compile.
        virtual Shader* GetShader(Device& device) const = 0;

        // Apply the material properties in the given program object.
        virtual void Apply(Device& device, Program& prog) const = 0;

        // Get the material surface type.
        virtual SurfaceType GetSurfaceType() const = 0;

        // Get whether the material is used as material for point sprites.
        virtual bool IsPointSprite() const = 0;
    protected:
    private:
    };

    class ColorFill : public Material
    {
    public:
        ColorFill()
        {}

        ColorFill(const Color4f& color) : mColor(color)
        {}
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* s = device.FindShader("fill_color.glsl");
            if (s == nullptr || !s->IsValid())
            {
                if (s == nullptr)
                    s = device.MakeShader("fill_color.glsl");
                if (!s->CompileFile("fill_color.glsl"))
                    return nullptr;
            }
            return s;
        }

        virtual void Apply(Device&, Program& prog) const override
        {
            prog.SetUniform("kFillColor",
                mColor.Red(), mColor.Green(), mColor.Blue(),
                mColor.Alpha());
        }
        virtual SurfaceType GetSurfaceType() const override
        {
            if (mTransparency)
                return SurfaceType::Transparent;
            return SurfaceType::Opaque;
        }
        virtual bool IsPointSprite() const override
        { return false; }

        void SetColor(const Color4f color)
        { mColor = color; }

        void EnableTransparency(bool on_off)
        { mTransparency = on_off; }
    private:
        Color4f mColor;
        bool mTransparency = false;
    };

    class TextureFill : public Material
    {
    public:
        TextureFill() = default;
        TextureFill(const std::string& texture) : mTexture(texture)
        {}

        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("fill_texture.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (!shader)
                    shader = device.MakeShader("fill_texture.glsl");
                if (!shader->CompileFile("fill_texture.glsl"))
                    return nullptr;
            }
            return shader;
        }
        virtual void Apply(Device& device, Program& prog) const override
        {
            Texture* texture = device.FindTexture(mTexture);
            if (texture == nullptr)
            {
                texture = device.MakeTexture(mTexture);
                texture->UploadFromFile(mTexture);
            }
            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kRenderPoints", mRenderPoints ? 1.0f : 0.0f);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
            prog.SetUniform("kGamma", mGamma);
        }

        virtual SurfaceType GetSurfaceType() const override
        { return mSurfaceType; }

        virtual bool IsPointSprite() const override
        { return mRenderPoints; }

        TextureFill& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
        TextureFill& SetSurfaceType(SurfaceType type)
        {
            mSurfaceType = type;
            return *this;
        }


        // set this to true if rendering a textured point
        TextureFill& SetRenderPoints(bool value)
        {
            mRenderPoints = value;
            return *this;
        }

        TextureFill& SetTexture(const std::string& texfile)
        {
            mTexture = texfile;
            return *this;
        }
        TextureFill& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }
    private:
        std::string mTexture;
        SurfaceType mSurfaceType = SurfaceType::Opaque;
        bool mRenderPoints    = false;
        Color4f mColor = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        float mGamma   = 1.0f;
    };

    // SpriteSet is a material which renders a simple animation
    // by cycling through a set of textures at some fps.
    // The assumption is that cycling through the textures
    // renders a coherent animation.
    class SpriteSet : public Material
    {
    public:
        // Create a new sprite with an initializer list of texture names.
        SpriteSet(const std::initializer_list<std::string>& textures)
          : mTextures(textures)
        {}
        // Create a new sprite with a vector of texture names.
        SpriteSet(const std::vector<std::string>& textures)
          : mTextures(textures)
        {}
        // Create an empty sprite.
        // You must then manually call AddTexture before using
        // the sprite to render something.
        SpriteSet()
        {}

        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("sprite_set.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader("sprite_set.glsl");
                if (!shader->CompileFile("sprite_set.glsl"))
                    return nullptr;
            }
            return shader;
        }
        virtual void Apply(Device& device, Program& prog) const override
        {
            assert(!mTextures.empty() &&
                "The sprite has no textures and cannot be used to draw.");

            const auto num_textures = (unsigned)mTextures.size();
            const auto frame_interval = 1.0f / mFps;
            const unsigned frame_index[2] = {
                (unsigned(mRuntime / frame_interval) + 0) % num_textures,
                (unsigned(mRuntime / frame_interval) + 1) % num_textures
            };
            for (unsigned i=0; i<2; ++i)
            {
                const auto index = frame_index[i];
                Texture* texture = device.FindTexture(mTextures[index]);
                if (!texture)
                {
                    texture = device.MakeTexture(mTextures[index]);
                    texture->UploadFromFile(mTextures[index]);
                }
                const auto sampler = "kTexture" + std::to_string(i);
                prog.SetTexture(sampler.c_str(), i, *texture);
            }
            // blend factor between the two textures.
            const auto coeff = std::fmod(mRuntime, frame_interval) / frame_interval;
            prog.SetUniform("kBlendCoeff", coeff);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }
        virtual bool IsPointSprite() const override
        { return false; }

        // Set the desired frame rate per second.
        SpriteSet& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        // Set the current runtime. This is used to compute
        // the current texture to be rendered.
        SpriteSet& SetAppRuntime(float time)
        {
            mRuntime = time;
            return *this;
        }
        // Add a new texture name to the sprite.
        SpriteSet& AddTexture(const std::string& texture)
        {
            mTextures.push_back(texture);
            return *this;
        }
        SpriteSet& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
    private:
        std::vector<std::string> mTextures;
        float mRuntime = 0;
        float mFps = 1.0f;
        Color4f mColor = Color::White;
    };


    // SpriteMap is a material which renders a simple animation
    // by cycling through different subregions of a single sprite
    // texture at some fps.
    // The assumption is that cycling through the regions
    // renders a coherent animation.
    class SpriteMap : public Material
    {
    public:
        // Frame identifies a sub-region in the given sprite map
        // texture that is used as a single frame in the animation.
        struct Frame {
            // x position (distance from the texture's left edge)
            // of the top left corner of the frame within the texture
            // in units of texels.
            unsigned x = 0;
            // y position (distance from the texture's top left corner)
            // of the top left corner of the frame within the texture
            // in units of texels.
            unsigned y = 0;
            // width of the texture sub-region used as the frame
            // in units of texels.
            unsigned w = 0;
            // height of the texture sub-region used as the frame
            // in units of texels.
            unsigned h = 0;
        };

        // Create a new sprite with texture name and a init list of frames.
        SpriteMap(const std::string& texture, const std::initializer_list<Frame>& frames)
          : mTexture(texture)
          , mFrames(frames)
        {}
        // Create a new sprite with texture name and a vector of frames.
        SpriteMap(const std::string& texture, const std::vector<Frame> frames)
          : mTexture(texture)
          , mFrames(frames)
        {}
        // Create an empty sprite map. You must then manually
        // call AddFrame and SetTexture before the sprite
        // can be used to draw.
        SpriteMap()
        {}

        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("sprite_map.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader("sprite_map.glsl");
                if (!shader->CompileFile("sprite_map.glsl"))
                    return nullptr;
            }
            return shader;
        }
        virtual void Apply(Device& device, Program& prog) const override
        {
            assert(!mTexture.empty() &&
                "The sprite has no texture and cannot be used to draw.");
            assert(!mFrames.empty() &&
                "The sprite has no frames and cannot be used to draw.");

            const auto num_frames = (unsigned)mFrames.size();
            const auto frame_interval = 1.0f / mFps;
            const unsigned frame_index[2] = {
                (unsigned(mRuntime / frame_interval) + 0) % num_frames,
                (unsigned(mRuntime / frame_interval) + 1) % num_frames
            };
            Texture* texture = device.FindTexture(mTexture);
            if (!texture)
            {
                texture = device.MakeTexture(mTexture);
                texture->UploadFromFile(mTexture);
            }
            const float width  = texture->GetWidth();
            const float height = texture->GetHeight();

            for (unsigned i=0; i<2; ++i)
            {
                const auto index = frame_index[i];
                const auto& frame = mFrames[index];
                const auto& uniform_name = "kTextureFrame" + std::to_string(i);
                const auto x = frame.x / width;
                const auto y = 1.0 - ((frame.y + frame.h) / height);
                const auto w = frame.w / width;
                const auto h = frame.h / height;
                prog.SetUniform(uniform_name.c_str(), x, y, w, h);
            }
            const auto coeff = std::fmod(mRuntime, frame_interval) / frame_interval;
            prog.SetUniform("kBlendCoeff", coeff);
            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(),
                mColor.Blue(), mColor.Alpha());
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }
        virtual bool IsPointSprite() const override
        { return false; }

        SpriteMap& SetTexture(const std::string& texture)
        {
            mTexture = texture;
            return *this;
        }
        SpriteMap& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }
        SpriteMap& SetAppRuntime(float time)
        {
            mRuntime = time;
            return *this;
        }
        SpriteMap& AddFrame(const Frame& frame)
        {
            mFrames.push_back(frame);
            return *this;
        }
        SpriteMap& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
    private:
        std::string mTexture;
        std::vector<Frame> mFrames;
        float mRuntime = 0.0f;
        float mFps = 1.0f;
        Color4f mColor = Color::White;
    };

    class BitmapText : public Material
    {
    public:
        BitmapText(const TextBuffer& text) : mText(text)
        {}
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader("texture_alpha_mask.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (!shader)
                    shader = device.MakeShader("texture_alpha_mask.glsl");
                if (!shader->CompileFile("texture_alpha_mask.glsl"));
                    return nullptr;
            }
            return shader;
        }
        virtual void Apply(Device& device, Program& prog) const override
        {
            auto bmp = mText.GetBitmap();
            Texture* texture = device.FindTexture("text-bitmap");
            if (!texture)
                texture = device.MakeTexture("text-bitmap");

            bmp.FlipHorizontally();
            texture->Upload(bmp.GetData(), bmp.GetWidth(), bmp.GetHeight(), 
                Texture::Format::Grayscale);
            
            prog.SetTexture("kTexture", 0, *texture);
            prog.SetUniform("kRenderPoints", 0.0f);
            prog.SetUniform("kBaseColor", mColor.Red(), mColor.Green(), 
                mColor.Blue(), mColor.Alpha());
            prog.SetUniform("kGamma", mGamma);
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }
        virtual bool IsPointSprite() const override
        { return false; }

        BitmapText& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }
        BitmapText& SetGamma(float gamma)
        {
            mGamma = gamma;
            return *this;
        }

    private:
        const TextBuffer& mText;
        Color4f mColor = Color::White;
        float mGamma   = 1.0f;
    };


    // base class for material effects.
    class MaterialEffect : public Material
    {
    public:
        virtual Shader* GetShader(Device& device) const override
        {
            Shader* shader = device.FindShader(mShader);
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader(mShader);
                if (!shader->CompileFile(mShader))
                    return nullptr;
            }
            return shader;
        }

        virtual void Apply(Device&, Program& prog) const override
        {
            prog.SetUniform("uRuntime", mRuntime);
            prog.SetUniform("kBaseColor", mBaseColor.Red(), mBaseColor.Green(),
                mBaseColor.Blue(), mBaseColor.Alpha());
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        virtual bool IsPointSprite() const override
        { return false; }

        MaterialEffect& SetBaseColor(const Color4f& color)
        {
            mBaseColor = color;
            return *this;
        }

    protected:
        MaterialEffect(const std::string& shader, float runtime)
          : mShader(shader)
          , mRuntime(runtime)
        {}

    private:
        const std::string mShader;
        const float mRuntime = 0.0f;
        Color4f mBaseColor = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    };

    class SlidingGlintEffect : public MaterialEffect
    {
    public:
        SlidingGlintEffect(float runtime)
          : MaterialEffect("sliding_glint_effect.glsl", runtime)
        {}
    private:
    };

    class ConcentricRingsEffect : public MaterialEffect
    {
    public:
        ConcentricRingsEffect(float runtime)
          : MaterialEffect("concentric_rings_effect.glsl", runtime)
        {}
    private:
    };

} // namespace
