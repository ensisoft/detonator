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

#include "base/format.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/shaderpass.h"

namespace gfx
{

void Painter::ClearColor(const Color4f& color) const
{
    mDevice->ClearColor(color, mFrameBuffer);
}

void Painter::ClearStencil(const StencilClearValue& stencil) const
{
    mDevice->ClearStencil(stencil.value, mFrameBuffer);
}

void Painter::ClearDepth(float depth) const
{
    mDevice->ClearDepth(depth, mFrameBuffer);
}

void Painter::Draw(const std::vector<DrawShape>& shapes,
                   const DrawState& state,
                   const ShaderProgram& program) const
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
        if (!program.FilterDraw(shape.user))
            continue;

        Drawable::Environment drawable_env;
        drawable_env.editing_mode = mEditingMode;
        drawable_env.pixel_ratio  = mPixelRatio;
        drawable_env.view_matrix  = &mViewMatrix;
        drawable_env.proj_matrix  = &mProjMatrix;
        drawable_env.model_matrix = shape.transform;
        Geometry* geometry = shape.drawable->Upload(drawable_env, *mDevice);
        if (geometry == nullptr)
            continue;

        Material::Environment material_env;
        material_env.editing_mode  = mEditingMode;
        material_env.render_points = shape.drawable->GetStyle() == Drawable::Style::Points;
        Program* gpu_program = GetProgram(program, *shape.drawable, *shape.material, drawable_env, material_env);
        if (gpu_program == nullptr)
            continue;

        Material::RasterState material_raster_state;
        if (!shape.material->ApplyDynamicState(material_env, *mDevice, *gpu_program, material_raster_state))
            continue;
        device_state.blending    = material_raster_state.blending;
        device_state.premulalpha = material_raster_state.premultiplied_alpha;

        Drawable::RasterState drawable_raster_state;
        drawable_raster_state.culling    = shape.culling;
        drawable_raster_state.line_width = shape.line_width;

        shape.drawable->ApplyDynamicState(drawable_env, *gpu_program, drawable_raster_state);
        device_state.line_width = drawable_raster_state.line_width;
        device_state.culling    = drawable_raster_state.culling;

        program.ApplyDynamicState(*mDevice, *gpu_program, device_state);

        mDevice->Draw(*gpu_program, *geometry, device_state, mFrameBuffer);
    }
}

void Painter::Draw(const Drawable& shape,
                   const glm::mat4& model,
                   const Material& material,
                   const DrawState& state,
                   const ShaderProgram& program,
                   const LegacyDrawState& draw_state) const
{
    std::vector<DrawShape> shapes;
    shapes.resize(1);
    shapes[0].drawable   = &shape;
    shapes[0].material   = &material;
    shapes[0].transform  = &model;
    shapes[0].line_width = draw_state.line_width;
    shapes[0].culling    = draw_state.culling;
    Draw(shapes, state, program);
}

void Painter::Draw(const Drawable& drawable,
                   const glm::mat4& model,
                   const Material& material,
                   const LegacyDrawState& draw_state) const
{
    DrawState render_pass_state;
    render_pass_state.write_color  = true;
    render_pass_state.stencil_func = StencilFunc::Disabled;
    render_pass_state.depth_test   = DepthTest::Disabled;
    detail::GenericShaderProgram shader_pass;
    Draw(drawable, model, material, render_pass_state, shader_pass, draw_state);
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

Program* Painter::GetProgram(const ShaderProgram& program,
                             const Drawable& drawable,
                             const Material& material,
                             const Drawable::Environment& drawable_environment,
                             const Material::Environment& material_environment) const
{
    const auto& material_gpu_id = program.GetShaderId(material, material_environment);
    const auto& drawable_gpu_id = program.GetShaderId(drawable, drawable_environment);
    const auto& program_gpu_id = drawable_gpu_id + "/" + material_gpu_id;

    Program* gpu_program = mDevice->FindProgram(program_gpu_id);
    if (!gpu_program)
    {
        gfx::Shader* material_shader = mDevice->FindShader(material_gpu_id);
        if (material_shader == nullptr)
        {
            std::string material_shader_source = program.GetShader(material, material_environment, *mDevice);
            if (material_shader_source.empty())
                return nullptr;

            material_shader = mDevice->MakeShader(material_gpu_id);
            material_shader->SetName(material.GetShaderName(material_environment));
            material_shader->CompileSource(std::move(material_shader_source));
        }
        if (!material_shader->IsValid())
            return nullptr;

        gfx::Shader* drawable_shader = mDevice->FindShader(drawable_gpu_id);
        if (drawable_shader == nullptr)
        {
            std::string drawable_shader_source = program.GetShader(drawable, drawable_environment, *mDevice);
            if (drawable_shader_source.empty())
                return nullptr;

            drawable_shader = mDevice->MakeShader(drawable_gpu_id);
            drawable_shader->SetName(drawable.GetShaderName(drawable_environment));
            drawable_shader->CompileSource(std::move(drawable_shader_source));
        }
        if (!drawable_shader->IsValid())
            return nullptr;

        const auto& gpu_program_name = base::FormatString("%1(%2, %3)",
                                              program.GetName(),
                                              drawable_shader->GetName(),
                                              material_shader->GetName());
        std::vector<const Shader*> shaders;
        shaders.push_back(drawable_shader);
        shaders.push_back(material_shader);

        gpu_program = mDevice->MakeProgram(program_gpu_id);
        gpu_program->SetName(gpu_program_name);
        gpu_program->Build(shaders);
        if (!gpu_program->IsValid())
            return nullptr;

        material.ApplyStaticState(material_environment, *mDevice, *gpu_program);

        program.ApplyStaticState(*mDevice, *gpu_program);
    }
    if (!gpu_program->IsValid())
        return nullptr;

    return gpu_program;
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
