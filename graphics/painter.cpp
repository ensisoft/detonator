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
#  include <glm/gtc/type_ptr.hpp>
#  include <glm/gtc/matrix_transform.hpp>
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
#include "transform.h"
#include "types.h"

namespace gfx
{

class StandardPainter : public Painter
{
public:
    StandardPainter(std::shared_ptr<Device> device)
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
        if (!prog)
            return;

        // create simple orthographic projection matrix.
        // 0,0 is the window top left, x grows left and y grows down
        const auto& kProjectionMatrix = glm::ortho(0.0f, mViewW, mViewH, 0.0f);
        const auto& kViewMatrix = transform.GetAsMatrix();

        const auto draw_type = geom->GetDrawType();

        Material::RasterState raster;
        Material::Environment env;
        env.render_points = draw_type == Geometry::DrawType::Points;

        prog->SetUniform("kProjectionMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kProjectionMatrix));
        prog->SetUniform("kViewMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kViewMatrix));
        mat.Apply(env, *mDevice, *prog, raster);

        Device::State state;
        state.viewport     = IRect(mViewX, mViewY, mViewW, mViewH);
        state.bWriteColor  = true;
        state.stencil_func = Device::State::StencilFunc::Disabled;
        state.blending     = raster.blending;
        mDevice->Draw(*prog, *geom, state);
    }

    virtual void DrawMasked(const Drawable& drawShape, const Transform& drawTransform,
                            const Drawable& maskShape, const Transform& maskTransform,
                            const Material& material) override
    {

        // todo: clean this up and split into render passes
        // i.e. masking pass and then drawing pass.

        mDevice->ClearStencil(1);

        Geometry* maskGeom = maskShape.Upload(*mDevice);
        Program* maskProg = GetProgram(maskShape, SolidColor(gfx::Color::White));
        if (!maskProg)
            return;

        // create simple orthographic projection matrix.
        // 0,0 is the window top left, x grows left and y grows down
        const auto& kProjectionMatrix = glm::ortho(0.0f, mViewW, mViewH, 0.0f);
        const auto& kViewMatrixDrawShape = drawTransform.GetAsMatrix();
        const auto& kViewMatrixMaskShape = maskTransform.GetAsMatrix();

        Device::State state;
        state.viewport         = IRect(mViewX, mViewY, mViewW, mViewH);
        state.stencil_func     = Device::State::StencilFunc::PassAlways;
        state.stencil_dpass    = Device::State::StencilOp::WriteRef;
        state.stencil_ref      = 0;
        state.bWriteColor      = false;

        maskProg->SetUniform("kProjectionMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kProjectionMatrix));
        maskProg->SetUniform("kViewMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kViewMatrixMaskShape));

        Material::RasterState raster;
        Material::Environment env;
        env.render_points = maskGeom->GetDrawType() == Geometry::DrawType::Points;

        material.Apply(env, *mDevice, *maskProg, raster);
        mDevice->Draw(*maskProg, *maskGeom, state);

        Geometry* drawGeom = drawShape.Upload(*mDevice);
        Program* drawProg = GetProgram(drawShape, material);
        if (!drawProg)
            return;

        env.render_points = drawGeom->GetDrawType() == Geometry::DrawType::Points;

        drawProg->SetUniform("kProjectionMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kProjectionMatrix));
        drawProg->SetUniform("kViewMatrix", *(const Program::Matrix4x4*)glm::value_ptr(kViewMatrixDrawShape));
        material.Apply(env, *mDevice, *drawProg, raster);

        state.stencil_func       = Device::State::StencilFunc::RefIsEqual;
        state.stencil_dpass      = Device::State::StencilOp::WriteRef;
        state.stencil_ref        = 1;
        state.bWriteColor        = true;
        state.blending           = raster.blending;
        mDevice->Draw(*drawProg, *drawGeom, state);
    }

private:
    Program* GetProgram(const Drawable& drawable, const Material& material)
    {
        const std::string& name = typeid(drawable).name() + std::string("/") +
            material.GetShaderName();
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
    std::shared_ptr<Device> mDevice;
private:
    float mViewW = 0.0f;
    float mViewH = 0.0f;
    float mViewX = 0.0f;
    float mViewY = 0.0f;

};

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<Device> device)
{
    return std::make_unique<StandardPainter>(device);
}

} // namespace
