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
        Geometry* geom = ToDeviceGeometry(shape);
        Program* prog = GetProgram(shape, mat);
        prog->SetUniform("kScalingFactor", transform.GetWidth(), transform.GetHeight());
        prog->SetUniform("kTranslationTerm", transform.GetXPosition(), transform.GetYPosition());
        prog->SetUniform("kViewport", mViewW, mViewH);
        prog->SetUniform("kRotation", transform.GetRotation());

        GraphicsDevice::State state;
        state.viewport.x      = mViewX;
        state.viewport.y      = mViewY;
        state.viewport.width  = mViewW;
        state.viewport.height = mViewH;
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

        Geometry* maskGeom = ToDeviceGeometry(maskShape);
        Program* maskProg = GetProgram(maskShape, Fill());
        maskProg->SetUniform("kScalingFactor", maskTransform.GetWidth(), maskTransform.GetHeight());
        maskProg->SetUniform("kTranslationTerm", maskTransform.GetXPosition(), maskTransform.GetYPosition());
        maskProg->SetUniform("kViewport", mViewW, mViewH);
        maskProg->SetUniform("kRotation", maskTransform.GetRotation());
        material.Apply(*maskProg);
        mDevice->Draw(*maskProg, *maskGeom, state);

        state.stencil_func    = GraphicsDevice::State::StencilFunc::RefIsEqual;
        state.stencil_dpass   = GraphicsDevice::State::StencilOp::WriteRef;
        state.stencil_ref     = 1;
        state.bWriteColor     = true;
        state.bEnableBlend    = true;

        Geometry* drawGeom = ToDeviceGeometry(drawShape);
        Program* drawProg = GetProgram(drawShape, material);
        drawProg->SetUniform("kScalingFactor", drawTransform.GetWidth(), drawTransform.GetHeight());
        drawProg->SetUniform("kTranslationTerm", drawTransform.GetXPosition(), drawTransform.GetYPosition());
        drawProg->SetUniform("kViewport", mViewW, mViewH);
        drawProg->SetUniform("kRotation", drawTransform.GetRotation());
        material.Apply(*drawProg);

        mDevice->Draw(*drawProg, *drawGeom, state);
    }

private:
    Program* GetProgram(const Drawable& shape, const Material& mat)
    {
        std::string drawable_shader = "vertex_array.glsl";
        std::string material_shader;
        // map material to the shader we use to implement it.
        if (dynamic_cast<const Fill*>(&mat))
            material_shader = "fill_color.glsl";
        else if (dynamic_cast<const SlidingGlintEffect*>(&mat))
            material_shader = "sliding_glint_effect.glsl";

        ASSERT(!material_shader.empty());

        // program is a combination of the vertex shader + fragment shader.
        return GetProgram(drawable_shader + "/" + material_shader,
            drawable_shader, material_shader);
    }

    Program* GetProgram(const std::string& name,
        const std::string& vshader_file, const std::string& fshader_file)
    {
        auto it = mPrograms.find(name);
        if (it == std::end(mPrograms))
        {
            Shader* vs = GetShader(vshader_file);
            Shader* fs = GetShader(fshader_file);
            if (vs == nullptr || fs == nullptr)
                return nullptr;

            std::vector<const Shader*> shaders;
            shaders.push_back(vs);
            shaders.push_back(fs);

            std::unique_ptr<Program> program = mDevice->NewProgram();
            if (!program->Build(shaders))
                return nullptr;

            it = mPrograms.insert(std::make_pair(name, std::move(program))).first;
        }
        return it->second.get();
    }
    Shader* GetShader(const std::string& file)
    {
        auto it = mShaders.find(file);
        if (it == std::end(mShaders))
        {
            // todo: maybe add some abstraction
            std::ifstream stream;
            stream.open("shaders/es2/" + file);
            if (!stream.is_open())
                throw std::runtime_error("failed to open file: " + file);

            const std::string source(std::istreambuf_iterator<char>(stream), {});

            std::unique_ptr<Shader> shader = mDevice->NewShader();
            if (!shader->CompileSource(source))
                return nullptr;

            it = mShaders.insert(std::make_pair(file, std::move(shader))).first;
        }
        return it->second.get();
    }
    Geometry* ToDeviceGeometry(const Drawable& shape)
    {
        const auto& name = typeid(shape).name();
        auto it = mGeoms.find(name);
        if (it == std::end(mGeoms))
        {
            std::unique_ptr<Geometry> geom = mDevice->NewGeometry();
            shape.Upload(*geom);
            it = mGeoms.insert(std::make_pair(name, std::move(geom))).first;
        }
        return it->second.get();
    }
    std::map<std::string, std::unique_ptr<Geometry>> mGeoms;
    std::map<std::string, std::unique_ptr<Shader>> mShaders;
    std::map<std::string, std::unique_ptr<Program>> mPrograms;

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
