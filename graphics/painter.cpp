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
#include <unordered_set>

#include "base/format.h"
#include "base/logging.h"
#include "graphics/drawcmd.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"
#include "graphics/shader_program.h"
#include "graphics/shadersource.h"

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

void Painter::Prime(DrawCommand& draw) const
{
    static const glm::mat4 Identity(1.0f);

    Drawable::Environment drawable_env;
    drawable_env.editing_mode   = mEditingMode;
    drawable_env.pixel_ratio    = mPixelRatio;
    drawable_env.instanced_draw = draw.instanced_draw.has_value();
    drawable_env.view_matrix    = draw.view       ? draw.view       : &mViewMatrix;
    drawable_env.proj_matrix    = draw.projection ? draw.projection : &mProjMatrix;
    drawable_env.model_matrix   = draw.model      ? draw.model      : &Identity;
    draw.geometry_gpu_ptr = GetGpuGeometry(*draw.drawable, drawable_env);
    if (draw.instanced_draw.has_value())
        draw.instance_draw_ptr = GetGpuInstancedDraw(draw.instanced_draw.value(), *draw.drawable, drawable_env);
}

void Painter::Draw(const DrawList& list, const ShaderProgram& program) const
{
    static const glm::mat4 Identity(1.0f);

    Device::State device_state;
    device_state.viewport = MapToDevice(mViewport);
    device_state.scissor  = MapToDevice(mScissor);

    std::unordered_set<std::string> used_programs;

    for (const auto& draw : list)
    {
        // Low level draw filtering.
        if (!program.FilterDraw(draw.user))
            continue;

        Drawable::Environment drawable_env;
        drawable_env.editing_mode   = mEditingMode;
        drawable_env.pixel_ratio    = mPixelRatio;
        drawable_env.instanced_draw = draw.instanced_draw.has_value();
        drawable_env.view_matrix    = draw.view       ? draw.view       : &mViewMatrix;
        drawable_env.proj_matrix    = draw.projection ? draw.projection : &mProjMatrix;
        drawable_env.model_matrix   = draw.model      ? draw.model      : &Identity;

        auto geometry = draw.geometry_gpu_ptr;
        if (geometry == nullptr)
            geometry = GetGpuGeometry(*draw.drawable, drawable_env);

        if (!geometry)
            continue;

        InstancedDrawPtr instanced_draw;
        if (draw.instanced_draw.has_value())
        {
            instanced_draw = draw.instance_draw_ptr;
            if (instanced_draw == nullptr)
                instanced_draw = GetGpuInstancedDraw(draw.instanced_draw.value(), *draw.drawable, drawable_env);
        }

        Material::Environment material_env;
        material_env.editing_mode   = mEditingMode;
        material_env.draw_primitive = draw.drawable->GetDrawPrimitive();
        material_env.draw_category  = draw.drawable->GetDrawCategory();
        material_env.renderpass     = program.GetRenderPass();
        ProgramPtr gpu_program = GetProgram(program, *draw.drawable, *draw.material, drawable_env, material_env);
        if (gpu_program == nullptr)
            continue;

        ProgramState gpu_program_state;

        Material::RasterState material_raster_state;
        if (!draw.material->ApplyDynamicState(material_env, *mDevice, gpu_program_state, material_raster_state))
            continue;

        Drawable::RasterState drawable_raster_state;
        drawable_raster_state.culling    = draw.state.culling;
        drawable_raster_state.line_width = draw.state.line_width;
        draw.drawable->ApplyDynamicState(drawable_env, gpu_program_state, drawable_raster_state);

        device_state.blending      = material_raster_state.blending;
        device_state.premulalpha   = material_raster_state.premultiplied_alpha;
        device_state.line_width    = drawable_raster_state.line_width;
        device_state.culling       = drawable_raster_state.culling;
        device_state.stencil_func  = draw.state.stencil_func;
        device_state.stencil_dpass = draw.state.stencil_dpass;
        device_state.stencil_dfail = draw.state.stencil_dfail;
        device_state.stencil_fail  = draw.state.stencil_fail;
        device_state.stencil_mask  = draw.state.stencil_mask;
        device_state.stencil_ref   = draw.state.stencil_ref;
        device_state.bWriteColor   = draw.state.write_color;
        device_state.depth_test    = draw.state.depth_test;
        device_state.winding_order = draw.state.winding;

        // apply shader program state dynamically once on the GPU program object
        // if the GPU program object changes.
        if (!base::Contains(used_programs, gpu_program->GetId()))
        {
            program.ApplyDynamicState(*mDevice, gpu_program_state);
        }

        program.ApplyDynamicState(*mDevice, gpu_program_state, device_state, draw.user);

        // The drawable provides the draw command which identifies the
        // sequence of more primitive draw commands set on the geometry
        // in order to render only a part of the geometry. i.e. a sub-mesh.
        const auto& cmd_params = draw.drawable->GetDrawCmd();

        mDevice->Draw(*gpu_program,
                      gpu_program_state,
                      GeometryDrawCommand(*geometry,
                                          cmd_params.draw_cmd_start,
                                          cmd_params.draw_cmd_count,
                                          instanced_draw),
                      device_state, mFrameBuffer);
    }
}

void Painter::Draw(const Drawable& shape,
                   const glm::mat4& model,
                   const Material& material,
                   const DrawState& state,
                   const ShaderProgram& program) const
{
    std::vector<DrawCommand> list;
    list.resize(1);
    list[0].drawable = &shape;
    list[0].material = &material;
    list[0].model    = &model;
    list[0].state    = state;
    Draw(list, program);
}

void Painter::Draw(const Drawable& shape,
                   const glm::mat4& model,
                   const Material& material,
                   const DrawState& state,
                   const ShaderProgram& program,
                   const LegacyDrawState& legacy_draw_state) const
{
    std::vector<DrawCommand> list;
    list.resize(1);
    list[0].drawable = &shape;
    list[0].material = &material;
    list[0].model    = &model;
    list[0].state    = state;
    list[0].state.line_width = legacy_draw_state.line_width;
    list[0].state.culling    = legacy_draw_state.culling;
    Draw(list, program);
}

void Painter::Draw(const Drawable& drawable,
                   const glm::mat4& model,
                   const Material& material,
                   const LegacyDrawState& draw_state) const
{
    DrawState state;
    state.write_color  = true;
    state.stencil_func = StencilFunc::Disabled;
    state.depth_test   = DepthTest::Disabled;

    detail::GenericShaderProgram program;
    Draw(drawable, model, material, state, program, draw_state);
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

ProgramPtr Painter::GetProgram(const ShaderProgram& program,
                               const Drawable& drawable,
                               const Material& material,
                               const Drawable::Environment& drawable_environment,
                               const Material::Environment& material_environment) const
{
    const auto& material_gpu_id = program.GetShaderId(material, material_environment);
    const auto& drawable_gpu_id = program.GetShaderId(drawable, drawable_environment);
    const auto& program_gpu_id = drawable_gpu_id + "/" + material_gpu_id;

    ProgramPtr gpu_program = mDevice->FindProgram(program_gpu_id);
    if (!gpu_program)
    {
        ShaderPtr material_shader = mDevice->FindShader(material_gpu_id);
        if (material_shader == nullptr)
        {
            const auto& material_shader_source = program.GetShader(material, material_environment, *mDevice);
            if (material_shader_source.IsEmpty())
                return nullptr;

            DEBUG("Compile shader: %1", material_shader_source.GetShaderName());
            DEBUG(" GPU ID     = %1", material_gpu_id);
            for (size_t i=0; i<material_shader_source.GetDebugInfoCount(); ++i)
            {
                const auto& info = material_shader_source.GetDebugInfo(i);
                DEBUG(" %1 = %2", base::fmt::FixedString { info.key, 10 }, info.val);
            }

            Shader::CreateArgs args;
            args.name   = material_shader_source.GetShaderName();
            args.source = material_shader_source.GetSource();
            args.debug  = mDebugMode;
            material_shader = mDevice->CreateShader(material_gpu_id, args);
        }
        if (!material_shader->IsValid())
        {
            auto error = material_shader->GetError();
            if (!error.empty())
                mErrors.push_back(std::move(error));
            return nullptr;
        }

        ShaderPtr drawable_shader = mDevice->FindShader(drawable_gpu_id);
        if (drawable_shader == nullptr)
        {
            const auto& drawable_shader_source = program.GetShader(drawable, drawable_environment, *mDevice);
            if (drawable_shader_source.IsEmpty())
                return nullptr;

            DEBUG("Compile shader: %1", drawable_shader_source.GetShaderName());
            DEBUG(" GPU ID     = %1", drawable_gpu_id);
            for (size_t i=0; i<drawable_shader_source.GetDebugInfoCount(); ++i)
            {
                const auto& info = drawable_shader_source.GetDebugInfo(i);
                DEBUG(" %1 = %2", base::fmt::FixedString { info.key, 10 }, info.val);
            }

            Shader::CreateArgs args;
            args.name   = drawable_shader_source.GetShaderName();
            args.source = drawable_shader_source.GetSource();
            args.debug  = mDebugMode;
            drawable_shader = mDevice->CreateShader(drawable_gpu_id, args);
        }
        if (!drawable_shader->IsValid())
        {
            auto error = drawable_shader->GetError();
            if (!error.empty())
                mErrors.push_back(std::move(error));
            return nullptr;
        }

        DEBUG("Build program: %1", program.GetName());
        DEBUG(" FS GPU ID = %1", material_gpu_id);
        DEBUG(" VS GPU ID = %1", drawable_gpu_id);

        Program::CreateArgs args;
        args.fragment_shader = material_shader;
        args.vertex_shader   = drawable_shader;
        args.name = base::FormatString("%1(%2, %3)",
                                       program.GetName(),
                                       drawable_shader->GetName(),
                                       material_shader->GetName());

        material.ApplyStaticState(material_environment, *mDevice, args.state);

        program.ApplyStaticState(*mDevice, args.state);

        gpu_program = mDevice->CreateProgram(program_gpu_id, args);
        if (!gpu_program->IsValid())
            return nullptr;
    }
    if (!gpu_program->IsValid())
        return nullptr;

    return gpu_program;
}

GeometryPtr Painter::GetGpuGeometry(const Drawable& drawable, const Drawable::Environment& env) const
{
    const auto& id = drawable.GetGeometryId(env);

    const auto usage = drawable.GetGeometryUsage();
    if (usage == Drawable::Usage::Stream)
    {
        Geometry::CreateArgs args;
        if (!drawable.Construct(env, args))
            return nullptr;

        return mDevice->CreateGeometry(id, std::move(args));
    }
    else if (usage == Drawable::Usage::Dynamic)
    {
        auto geom = mDevice->FindGeometry(id);
        if (geom == nullptr)
        {
            Geometry::CreateArgs args;
            if (!drawable.Construct(env, args))
                return nullptr;

            return mDevice->CreateGeometry(id, std::move(args));
        }
        if (geom->GetContentHash() == drawable.GetGeometryHash())
            return geom;

        Geometry::CreateArgs args;
        if (!drawable.Construct(env, args))
            return nullptr;

        return mDevice->CreateGeometry(id, std::move(args));
    }
    else if (usage == Drawable::Usage::Static)
    {
        auto geom = mDevice->FindGeometry(id);
        if (geom == nullptr)
        {
            Geometry::CreateArgs args;
            if (!drawable.Construct(env, args))
                return nullptr;

            return mDevice->CreateGeometry(id, std::move(args));
        }
        if (!mEditingMode)
            return geom;

        if (geom->GetContentHash() == drawable.GetGeometryHash())
            return geom;

        Geometry::CreateArgs args;
        if (!drawable.Construct(env, args))
            return nullptr;

        return mDevice->CreateGeometry(id, std::move(args));

    } else BUG("Missing geometry usage handling.");
}

InstancedDrawPtr Painter::GetGpuInstancedDraw(const InstancedDraw& draw, const gfx::Drawable& drawable, const Drawable::Environment& env) const
{
    const auto& id = drawable.GetInstanceId(env, draw);
    const auto usage = drawable.GetInstanceUsage(draw);

    if (usage == BufferUsage::Stream)
    {
        gfx::InstancedDraw::CreateArgs args;
        if (!drawable.Construct(env, draw, args))
            return nullptr;

        return mDevice->CreateInstancedDraw(id, std::move(args));
    }
    else if (usage == BufferUsage::Dynamic)
    {
        auto foo = mDevice->FindInstancedDraw(id);
        if (foo == nullptr)
        {
            gfx::InstancedDraw::CreateArgs args;
            if (!drawable.Construct(env, draw, args))
                return nullptr;

            return mDevice->CreateInstancedDraw(draw.gpu_id, std::move(args));
        }
        if (foo->GetContentHash() == drawable.GetInstanceHash(draw))
            return foo;

        gfx::InstancedDraw::CreateArgs args;
        if (!drawable.Construct(env, draw, args))
            return nullptr;

        return mDevice->CreateInstancedDraw(id, std::move(args));
    }
    else if (usage == BufferUsage::Static)
    {
        auto foo = mDevice->FindInstancedDraw(id);
        if (foo == nullptr)
        {
            gfx::InstancedDraw::CreateArgs args;
            if (!drawable.Construct(env, draw, args))
                return nullptr;

            return mDevice->CreateInstancedDraw(id, std::move(args));
        }

        if (!mEditingMode)
            return foo;

        if (foo->GetContentHash() == drawable.GetInstanceHash(draw))
            return foo;

        gfx::InstancedDraw::CreateArgs args;
        if (!drawable.Construct(env, draw, args))
            return nullptr;

        return mDevice->CreateInstancedDraw(id, std::move(args));
    }


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
