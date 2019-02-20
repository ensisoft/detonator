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

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <string>
#include <map>
#include <stdexcept>
#include <fstream>

#include "base/assert.h"
#include "base/logging.h"
#include "device.h"
#include "shader.h"
#include "program.h"
#include "geometry.h"
#include "painter.h"
#include "drawable.h"
#include "material.h"
#include "types.h"

namespace invaders
{

class StandardPainter : public Painter
{
public:
    StandardPainter(std::shared_ptr<GraphicsDevice> device)
      : mDevice(device)
    {}

    virtual void SetViewport(int x, int y, unsigned width, unsigned height)
    {
        mViewW = width;
        mViewH = height;
        mViewX = x;
        mViewY = y;
    }

    virtual void Clear(const Color4f& color) override
    {
        mDevice->ClearColor(color);
    }

    virtual void Draw(const Drawable& shape, const Transform& transform, const Material& mat) override
    {
        Geometry* geom = shape.Upload(*mDevice);
        Program* prog = GetProgram(shape, mat);
        prog->SetUniform("kScalingFactor", transform.GetWidth(), transform.GetHeight());
        prog->SetUniform("kTranslationTerm", transform.GetXPosition(), transform.GetYPosition());
        prog->SetUniform("kViewport", mViewW, mViewH);
        prog->SetUniform("kRotation", transform.GetRotation());
        mat.Apply(*mDevice, *prog);

        GraphicsDevice::State state;
        state.viewport.x      = mViewX;
        state.viewport.y      = mViewY;
        state.viewport.width  = mViewW;
        state.viewport.height = mViewH;
        state.bEnableBlend    = mat.IsTransparent();
        mDevice->Draw(*prog, *geom, state);
    }

    virtual void DrawMasked(const Drawable& drawShape, const Transform& drawTransform,
                            const Drawable& maskShape, const Transform& maskTransform,
                            const Material& material) override
    {

        mDevice->ClearStencil(1);

        GraphicsDevice::State state;
        state.viewport.x      = mViewX;
        state.viewport.y      = mViewY;
        state.viewport.width  = mViewW;
        state.viewport.height = mViewH;
        state.stencil_func    = GraphicsDevice::State::StencilFunc::PassAlways;
        state.stencil_dpass   = GraphicsDevice::State::StencilOp::WriteRef;
        state.stencil_ref     = 0;
        state.bWriteColor     = false;

        Geometry* maskGeom = maskShape.Upload(*mDevice);
        Program* maskProg = GetProgram(maskShape, ColorFill());
        maskProg->SetUniform("kScalingFactor", maskTransform.GetWidth(), maskTransform.GetHeight());
        maskProg->SetUniform("kTranslationTerm", maskTransform.GetXPosition(), maskTransform.GetYPosition());
        maskProg->SetUniform("kViewport", mViewW, mViewH);
        maskProg->SetUniform("kRotation", maskTransform.GetRotation());
        material.Apply(*mDevice, *maskProg);
        mDevice->Draw(*maskProg, *maskGeom, state);

        state.stencil_func    = GraphicsDevice::State::StencilFunc::RefIsEqual;
        state.stencil_dpass   = GraphicsDevice::State::StencilOp::WriteRef;
        state.stencil_ref     = 1;
        state.bWriteColor     = true;
        state.bEnableBlend    = true;

        Geometry* drawGeom = drawShape.Upload(*mDevice);
        Program* drawProg = GetProgram(drawShape, material);
        drawProg->SetUniform("kScalingFactor", drawTransform.GetWidth(), drawTransform.GetHeight());
        drawProg->SetUniform("kTranslationTerm", drawTransform.GetXPosition(), drawTransform.GetYPosition());
        drawProg->SetUniform("kViewport", mViewW, mViewH);
        drawProg->SetUniform("kRotation", drawTransform.GetRotation());
        material.Apply(*mDevice, *drawProg);

        mDevice->Draw(*drawProg, *drawGeom, state);
    }

private:
    Program* GetProgram(const Drawable& drawable, const Material& material)
    {
        const std::string& name = typeid(drawable).name() + std::string("/") +
            typeid(material).name();
        Program* prog = mDevice->FindProgram(name);
        if (!prog)
        {
            Shader* drawable_shader = drawable.GetShader(*mDevice);
            if (!drawable_shader)
                return nullptr;
            Shader* material_shader = material.GetShader(*mDevice);
            if (!material_shader)
                return nullptr;

            std::vector<const Shader*> shaders;
            shaders.push_back(drawable_shader);
            shaders.push_back(material_shader);
            prog = mDevice->MakeProgram(name);
            if (!prog->Build(shaders))
                return nullptr;
        }
        return prog;
    }

private:
    std::shared_ptr<GraphicsDevice> mDevice;
private:
    float mViewW = 0.0f;
    float mViewH = 0.0f;
    float mViewX = 0.0f;
    float mViewY = 0.0f;

};

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<GraphicsDevice> device)
{
    return std::make_unique<StandardPainter>(device);
}

} // namespace
