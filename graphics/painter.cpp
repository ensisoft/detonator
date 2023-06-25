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
#  include <glm/gtc/type_ptr.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <string>
#include <map>

#include "base/logging.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/shaderpass.h"
#include "graphics/types.h"

namespace gfx
{

class StandardPainter : public Painter
{
public:
    StandardPainter(std::shared_ptr<Device> device)
      : mDeviceInst(device)
      , mDevice(device.get())
    {}
    StandardPainter(Device* device)
      : mDevice(device)
    {}
    virtual Device* GetDevice() override
    {
        return mDevice;
    }
    virtual void SetEditingMode(bool on_off) override
    {
        mEditingMode = on_off;
    }
    virtual void SetPixelRatio(const glm::vec2& ratio) override
    {
        mPixelRatio = ratio;
    }
    virtual void SetSurfaceSize(const USize& size) override
    {
        mSurfaceSize = size;
    }
    virtual void SetViewport(const IRect& viewport) override
    {
        mViewport = viewport;
    }
    virtual void SetScissor(const IRect& scissor) override
    {
        mScissor = scissor;
    }
    virtual void ClearScissor() override
    {
        mScissor = IRect(0, 0, 0, 0);
    }
    virtual void SetProjectionMatrix(const glm::mat4& proj) override
    {
        mProjection = proj;
    }
    virtual void SetViewMatrix(const glm::mat4& view) override
    {
        mViewMatrix = view;
    }
    virtual const glm::mat4& GetViewMatrix() const override
    {
        return mViewMatrix;
    }
    virtual const glm::mat4& GetProjMatrix() const override
    {
        return mProjection;
    }
    virtual USize GetSurfaceSize() const override
    {
        return mSurfaceSize;
    }
    virtual void ClearColor(const Color4f& color) override
    {
        mDevice->ClearColor(color);
    }

    virtual void ClearStencil(const StencilClearValue& stencil) override
    {
        mDevice->ClearStencil(stencil.value);
    }

    virtual void Draw(const std::vector<DrawShape>& shapes, const RenderPassState& state, const ShaderPass& pass) override
    {
        Device::State device_state;
        device_state.viewport      = MapToDevice(mViewport);
        device_state.scissor       = MapToDevice(mScissor);
        device_state.stencil_func  = state.stencil_func;
        device_state.stencil_dpass = state.stencil_dpass;
        device_state.stencil_dfail = state.stencil_dfail;
        device_state.stencil_fail  = state.stencil_fail;
        device_state.stencil_mask  = state.stencil_mask;
        device_state.stencil_ref   = state.stencil_ref;
        device_state.bWriteColor   = state.write_color;
        device_state.depth_test    = state.depth_test;

        for (const auto& shape : shapes)
        {
            // Low level draw filtering.
            if (!pass.FilterDraw(shape.user))
                continue;

            Drawable::Environment drawable_env;
            drawable_env.editing_mode = mEditingMode;
            drawable_env.pixel_ratio  = mPixelRatio;
            drawable_env.view_matrix  = &mViewMatrix;
            drawable_env.proj_matrix  = &mProjection;
            drawable_env.model_matrix = shape.transform;
            drawable_env.shader_pass  = &pass;
            Geometry* geometry = shape.drawable->Upload(drawable_env, *mDevice);
            if (geometry == nullptr)
                continue;

            Material::Environment material_env;
            material_env.editing_mode  = mEditingMode;
            material_env.render_points = shape.drawable->GetStyle() == Drawable::Style::Points;
            material_env.shader_pass   = &pass;
            Program* program = GetProgram(*shape.drawable, *shape.material, drawable_env, material_env);
            if (program == nullptr)
                continue;

            Material::RasterState material_raster_state;
            if (!shape.material->ApplyDynamicState(material_env, *mDevice, *program, material_raster_state))
                continue;
            device_state.blending    = material_raster_state.blending;
            device_state.premulalpha = material_raster_state.premultiplied_alpha;

            Drawable::RasterState drawable_raster_state;
            shape.drawable->ApplyDynamicState(drawable_env, *program, drawable_raster_state);
            device_state.line_width = drawable_raster_state.line_width;
            device_state.culling    = drawable_raster_state.culling;

            // Do final state setting here. The shader pass can then ultimately decide
            // on the best program and device state for this draw.
            pass.ApplyDynamicState(*program, device_state);

            mDevice->Draw(*program, *geometry, device_state);
        }
    }
private:
    Program* GetProgram(const Drawable& drawable,
                        const Material& material,
                        const Drawable::Environment& drawable_environment,
                        const Material::Environment& material_environment)
    {
        const auto& id = drawable.GetProgramId(drawable_environment) + "/" +
                         material.GetProgramId(material_environment);
        Program* program = mDevice->FindProgram(id);
        if (!program)
        {
            Shader* drawable_shader = drawable.GetShader(drawable_environment, *mDevice);
            if (!drawable_shader || !drawable_shader->IsValid())
                return nullptr;
            Shader* material_shader = material.GetShader(material_environment, *mDevice);
            if (!material_shader || !material_shader->IsValid())
                return nullptr;

            const auto& name = drawable_shader->GetName() + "/" +
                               material_shader->GetName();

            std::vector<const Shader*> shaders;
            shaders.push_back(drawable_shader);
            shaders.push_back(material_shader);
            program = mDevice->MakeProgram(id);
            program->SetName(name);
            program->Build(shaders);
            if (program->IsValid())
            {
                material.ApplyStaticState(material_environment, *mDevice, *program);
            }
        }
        if (program->IsValid())
            return program;
        return nullptr;
    }

    IRect MapToDevice(const IRect& rect) const
    {
        if (rect.IsEmpty())
            return rect;
        const int surface_width  = mSurfaceSize.GetWidth();
        const int surface_height = mSurfaceSize.GetHeight();
        // map from window coordinates (top left origin) to device
        // coords (bottom left origin)
        const auto bottom = rect.GetY() + rect.GetHeight();
        const auto x = rect.GetX();
        const auto y = surface_height - bottom;
        return IRect(x, y, rect.GetWidth(), rect.GetHeight());
    }
private:
    std::shared_ptr<Device> mDeviceInst;
    Device* mDevice = nullptr;
private:
    bool mEditingMode = false;
    // Expected Size of the rendering surface.
    USize mSurfaceSize;
    // the viewport setting for mapping the NDC coordinates
    // into some region of the rendering surface.
    IRect mViewport;
    // the current scissor setting to be applied
    // on the rendering surface.
    IRect mScissor;
   // the ratio of rendering surface pixels to game units.
    glm::vec2 mPixelRatio = {1.0f, 1.0f};
    // current (orthographic) projection matrix.
    glm::mat4 mProjection;
    // current additional view matrix that gets multiplied
    // with the draw transforms. convenient for cases when
    // everything that is to be drawn needs to get transformed
    // in some additional way.
    glm::mat4 mViewMatrix {1.0f};
};

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<Device> device)
{
    return std::make_unique<StandardPainter>(device);
}
// static
std::unique_ptr<Painter> Painter::Create(Device* device)
{
    return std::make_unique<StandardPainter>(device);
}

void Painter::SetViewport(int x, int y, unsigned int width, unsigned int height)
{
    SetViewport(IRect(x, y, width, height));
}
void Painter::SetScissor(int x, int y, unsigned int width, unsigned int height)
{
    SetScissor(IRect(x, y, width, height));
}
void Painter::SetSurfaceSize(unsigned width, unsigned height)
{
    SetSurfaceSize(USize(width, height));
}

void Painter::Draw(const Drawable& shape,
                   const glm::mat4& model,
                   const Material& material,
                   const RenderPassState& renderp,
                   const ShaderPass& shaderp)
{
    std::vector<DrawShape> shapes;
    shapes.resize(1);
    shapes[0].drawable  = &shape;
    shapes[0].material  = &material;
    shapes[0].transform = &model;
    Draw(shapes, renderp, shaderp);
}

void Painter::Draw(const Drawable& drawable, const glm::mat4& model, const Material& material)
{
    RenderPassState state;
    state.write_color  = true;
    state.stencil_func = StencilFunc::Disabled;
    state.depth_test   = DepthTest::Disabled;
    detail::GenericShaderPass pass;
    Draw(drawable, model, material, state, pass);
}

} // namespace
