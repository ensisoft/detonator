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

#include <string>

#include "device.h"
#include "shader.h"
#include "program.h"
#include "texture.h"
#include "types.h"

namespace invaders
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
        virtual Shader* GetShader(GraphicsDevice& device) const = 0;

        // Apply the material properties in the given program object.
        virtual void Apply(GraphicsDevice& device, Program& prog) const = 0;

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
        virtual Shader* GetShader(GraphicsDevice& device) const override
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

        virtual void Apply(GraphicsDevice&, Program& prog) const override
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

        virtual Shader* GetShader(GraphicsDevice& device) const override
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
        virtual void Apply(GraphicsDevice& device, Program& prog) const override
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

    // base class for material effects.
    class MaterialEffect : public Material
    {
    public:
        virtual Shader* GetShader(GraphicsDevice& device) const override
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

        virtual void Apply(GraphicsDevice&, Program& prog) const override
        {
            prog.SetUniform("uRuntime", mRuntime);
        }
        virtual SurfaceType GetSurfaceType() const override
        { return SurfaceType::Transparent; }

        virtual bool IsPointSprite() const override
        { return false; }
    protected:
        MaterialEffect(const std::string& shader, float runtime)
          : mShader(shader)
          , mRuntime(runtime)
        {}
    private:
        const std::string mShader;
        const float mRuntime = 0.0f;
    };

    class SlidingGlintEffect : public MaterialEffect
    {
    public:
        SlidingGlintEffect(float runtime)
          : MaterialEffect("sliding_glint_effect.glsl", runtime)
        {}
    private:
    };

} // namespace
