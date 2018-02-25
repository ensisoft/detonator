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
        mDevice->Clear(color);
    }

    virtual void Draw(const Rect& rc, const Transform& t, const Fill& fill) override
    {
        Geometry* geom = ToDeviceGeometry(rc);
        Program* prog = GetProgram("FillRect", "vertex_array.glsl", "fill_color.glsl");
        prog->SetUniform("kScalingFactor", t.Width(), t.Height());
        prog->SetUniform("kTranslationTerm", t.X(), t.Y());
        prog->SetUniform("kViewport", mViewW, mViewH);

        GraphicsDevice::State state;
        state.viewport.x      = mViewX;
        state.viewport.y      = mViewY;
        state.viewport.width  = mViewW;
        state.viewport.height = mViewH;
        mDevice->Draw(*prog, *geom, state);
    }

private:
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
    template<typename Shape>
    Geometry* ToDeviceGeometry(const Shape& shape)
    {
        const auto& name = typeid(Shape).name();
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