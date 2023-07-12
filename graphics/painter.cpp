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
#include "warnpop.h"

#include <string>

#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/shaderpass.h"

namespace gfx
{

void Painter::ClearColor(const Color4f& color)
{
    mDevice->ClearColor(color, mFrameBuffer);
}

void Painter::ClearStencil(const StencilClearValue& stencil)
{
    mDevice->ClearStencil(stencil.value, mFrameBuffer);
}

void Painter::ClearDepth(float depth)
{
    mDevice->ClearDepth(depth, mFrameBuffer);
}

void Painter::Draw(const std::vector<DrawShape>& shapes,
                   const RenderPassState& state,
                   const ShaderPass& pass)
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
        drawable_env.proj_matrix  = &mProjMatrix;
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

        mDevice->Draw(*program, *geometry, device_state, mFrameBuffer);
    }
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

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<Device> device)
{
    return std::make_unique<Painter>(device);
}
// static
std::unique_ptr<Painter> Painter::Create(Device* device)
{
    return std::make_unique<Painter>(device);
}

Program* Painter::GetProgram(const Drawable& drawable,
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

IRect Painter::MapToDevice(const IRect& rect) const noexcept
{
    if (rect.IsEmpty())
        return rect;
    const int surface_width  = mSize.GetWidth();
    const int surface_height = mSize.GetHeight();
    // map from window coordinates (top left origin) to device
    // coordinates (bottom left origin)
    const auto bottom = rect.GetY() + rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = surface_height - bottom;
    return IRect(x, y, rect.GetWidth(), rect.GetHeight());
}

} // namespace
