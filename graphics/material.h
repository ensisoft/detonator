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
        virtual ~Material() = default;
        virtual Shader* GetShader(GraphicsDevice& device) const = 0;
        virtual void Apply(GraphicsDevice& device, Program& prog) const = 0;
        virtual bool IsTransparent() const
        { return false; }
        virtual bool IsPointSprite() const
        { return false; }
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
        virtual bool IsTransparent() const override
        { return mTransparency; }

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
        TextureFill(const std::string& texture, bool transparency = true)
          : mTexture(texture)
          , mTransparency(transparency)
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

        virtual bool IsTransparent() const override
        { return mTransparency; }
        virtual bool IsPointSprite() const override
        { return mRenderPoints; }

        TextureFill& SetBaseColor(const Color4f& color)
        {
            mColor = color;
            return *this;
        }

        TextureFill& EnableTransparency(bool on_off)
        {
            mTransparency = on_off;
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
        bool mTransparency    = false;
        bool mRenderPoints    = false;
        Color4f mColor = Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        float mGamma   = 1.0f;
    };

    class SlidingGlintEffect : public Material
    {
    public:
        virtual Shader* GetShader(GraphicsDevice& device) const override
        {
            Shader* s = device.FindShader("sliding_glint_effect.glsl");
            if (s == nullptr || !s->IsValid())
            {
                if (s == nullptr)
                    s = device.MakeShader("sliding_glint_effect.glsl");
                if (!s->CompileFile("sliding_glint_effect.glsl"))
                    return nullptr;
            }
            return s;
        }

        virtual void Apply(GraphicsDevice&, Program& prog) const override
        {
            prog.SetUniform("uRuntime", mRunTime);
        }
        virtual bool IsTransparent() const override
        { return true; }

        void SetAppRuntime(float r)
        { mRunTime = r; }
    private:
        float mRunTime = 0.0f;
    };

} // namespace
