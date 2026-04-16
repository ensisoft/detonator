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
#include "base/trace.h"
#include "base/utility.h"
#include "graphics/drawcmd.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/painter.h"

#include "material_class.h"
#include "graphics/paint_log.h"
#include "graphics/shader_program.h"
#include "graphics/shader_programs.h"
#include "graphics/shader_source.h"

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

void Painter::Prime(DrawItemList& cmds) const
{
    static const glm::mat4 Identity(1.0f);

    if (cmds.mItems.empty())
        return;

    cmds.mHandles.resize(cmds.mItems.size());

    for (size_t i=0; i<cmds.mItems.size(); ++i)
    {
        const auto& cmd = cmds.mItems[i];

        Drawable::Environment drawable_env;
        drawable_env.editing_mode   = mEditingMode;
        drawable_env.pixel_ratio    = mPixelRatio;
        drawable_env.use_instancing = cmd.draw_call.IsInstanced();
        drawable_env.view_matrix    = cmd.view       ? cmd.view       : &mViewMatrix;
        drawable_env.proj_matrix    = cmd.projection ? cmd.projection : &mProjMatrix;
        drawable_env.model_matrix   = cmd.model      ? cmd.model      : &Identity;
        cmds.mHandles[i] = cmd.drawable->GetGeometry(drawable_env, *mDevice);
    }
}

bool Painter::Draw(const DrawItemList& list, const ShaderProgram& program, const RenderPassState& render_pass_state) const
{
    static const glm::mat4 Identity(1.0f);

    gfx::DeviceState ds(mDevice);

    gfx::Device::ViewportState vs;
    vs.viewport = MapToDevice(mViewport);
    vs.scissor  = MapToDevice(mScissor);

    mDevice->SetViewportState(vs);
    mDevice->SetColorDepthStencilState(render_pass_state.cds);

    program.InitializeResources(*mDevice);

    std::unordered_set<std::string> used_programs;

    bool success = false;

    for (size_t i=0; i<list.mItems.size(); ++i)
    {
        const auto& draw = list.mItems[i];

        // Low level draw filtering.
        if (!program.FilterDraw(draw.user))
            continue;

        Drawable::Environment drawable_env;
        drawable_env.editing_mode   = mEditingMode;
        drawable_env.pixel_ratio    = mPixelRatio;
        drawable_env.render_pass    = render_pass_state.render_pass;
        drawable_env.use_instancing = draw.draw_call.IsInstanced();
        drawable_env.view_matrix    = draw.view       ? draw.view       : &mViewMatrix;
        drawable_env.proj_matrix    = draw.projection ? draw.projection : &mProjMatrix;
        drawable_env.model_matrix   = draw.model      ? draw.model      : &Identity;

        auto geometry = list.GetGeometryPtr(i);
        if (geometry.IsNull())
            TRACE_CALL("GetGpuGeometry", geometry = draw.drawable->GetGeometry(drawable_env, *mDevice));

        if (!geometry.IsValid())
        {
            auto error_log = geometry.GetErrorLog();
            if (error_log.empty())
                error_log = base::FormatString("Failed to create '%1' geometry.", draw.drawable->GetName());
            GFX_PAINT_ERROR(error_log);
            continue;
        }

        Material::Environment material_env;
        material_env.editing_mode   = mEditingMode;
        material_env.draw_primitive = draw.drawable->GetDrawPrimitive();
        material_env.draw_category  = draw.drawable->GetDrawCategory();
        material_env.render_pass    = render_pass_state.render_pass;
        ProgramPtr gpu_program;

        TRACE_CALL("GetGpuProgram", gpu_program = GetProgram(program, *draw.drawable, *draw.material, drawable_env, material_env));
        if (gpu_program == nullptr)
            continue;

        ProgramState gpu_program_state;
        Device::RasterState device_state;

        bool state_ok = true;

        TRACE_BLOCK("ApplyState",
            Material::RasterState material_raster_state;
            state_ok &= draw.material->ApplyDynamicState(material_env, *mDevice, gpu_program_state, material_raster_state);

            Drawable::RasterState drawable_raster_state;
            drawable_raster_state.culling    = draw.culling;
            drawable_raster_state.line_width = draw.line_width;
            state_ok &= draw.drawable->ApplyDynamicState(drawable_env, draw.draw_call, geometry, *mDevice, gpu_program_state, drawable_raster_state);

            device_state.blending      = material_raster_state.blending;
            device_state.premulalpha   = material_raster_state.premultiplied_alpha;
            device_state.line_width    = drawable_raster_state.line_width;
            device_state.culling       = drawable_raster_state.culling;
            device_state.winding_order = draw.winding;

            // apply shader program state dynamically once on the GPU program object
            // if the GPU program object changes.
            if (!base::Contains(used_programs, gpu_program->GetId()))
            {
                program.ApplyDynamicState(*mDevice, gpu_program_state);
            }
            ShaderProgram::Environment program_env;
            program_env.render_pass  = render_pass_state.render_pass;
            program_env.proj_matrix  = draw.projection ? draw.projection : &mProjMatrix;
            program_env.view_matrix  = draw.view       ? draw.view       : &mViewMatrix;
            program_env.model_matrix = draw.model      ? draw.model      : &Identity;
            program.ApplyDynamicState(*mDevice, program_env, gpu_program_state, device_state, draw.user);
        );
        if (!state_ok)
            continue;

        // The drawable provides the draw command which identifies the
        // sequence of more primitive draw commands set on the geometry
        // in order to render only a part of the geometry. i.e. a sub-mesh.
        const auto& cmd_params = draw.draw_call.GetSubMesh();

        // modify the depth testing state since this is per draw right now.
        mDevice->ModifyState(draw.depth_test, Device::StateName::DepthTest);

        TRACE_CALL("DeviceDraw", mDevice->Draw(*gpu_program,
                      gpu_program_state,
                      GeometryDrawCommand(*geometry.GetGeometry(),
                          cmd_params.draw_cmd_start,
                          cmd_params.draw_cmd_count),
                      device_state, mFrameBuffer));

        // we supposedly rendered something, mark this as success.
        success = true;
    }
    return success;
}

bool Painter::Draw(const Drawable& shape,
                   const Matrix4x4& model,
                   const Material& material,
                   const DrawState& state,
                   const ShaderProgram& program,
                   DrawCall draw_call) const
{
    std::vector<DrawItem> list;
    list.resize(1);
    list[0].draw_call  = std::move(draw_call);
    list[0].drawable   = &shape;
    list[0].material   = &material;
    list[0].model      = &model;
    list[0].depth_test = state.depth_test;
    list[0].culling    = state.culling;
    list[0].winding    = state.winding;
    list[0].line_width = state.line_width;

    RenderPassState render_pass_state;
    render_pass_state.render_pass       = state.render_pass;
    render_pass_state.cds.depth_test    = state.depth_test;
    render_pass_state.cds.stencil_func  = state.stencil_func;
    render_pass_state.cds.stencil_fail  = state.stencil_fail;
    render_pass_state.cds.stencil_dpass = state.stencil_dpass;
    render_pass_state.cds.stencil_dfail = state.stencil_dfail;
    render_pass_state.cds.stencil_mask  = state.stencil_mask;
    render_pass_state.cds.stencil_ref   = state.stencil_ref;
    render_pass_state.cds.bWriteColor   = state.write_color;

    return Draw(list, program, render_pass_state);
}

bool Painter::Draw(const Drawable& drawable,
                   const Matrix4x4& model,
                   const Material& material,
                   const MinimalDrawState& state,
                   DrawCall draw_call) const
{
    DrawState full_state;
    full_state.render_pass          = RenderPass::ColorPass;
    full_state.write_color          = true;
    full_state.premultiply_alpha    = false;
    full_state.stencil_func         = StencilFunc::Disabled;
    full_state.depth_test           = DepthTest::Disabled;
    full_state.winding              = WindigOrder::CounterClockWise;
    full_state.culling              = state.culling;
    full_state.line_width           = state.line_width;

    FlatShadedColorProgram program;
    return Draw(drawable, model, material, full_state, program, std::move(draw_call));
}

// static
std::unique_ptr<Painter> Painter::Create(std::shared_ptr<Device> device)
{
    return std::make_unique<Painter>(std::move(device));
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
            std::string fallback_info;
            auto error = ShaderProgram::ShaderSourceError::Nada;
            auto material_shader_source = program.GetShader(material, material_environment, *mDevice, &error);
            if (material_shader_source.IsEmpty())
            {
                ERROR("Failed to create fragment shader. Using a placeholder that will not render.");
                material_shader_source.AddDebugInfo("Fallback", "yes");
                material_shader_source.SetVersion(ShaderSource::Version::GLSL_300);
                material_shader_source.SetPrecision(ShaderSource::Precision::High);
                material_shader_source.SetType(ShaderSource::Type::Fragment);
                material_shader_source.LoadRawSource("layout (location=0) out vec4 fragOutColor0;\n"
                                                     "void main() {}");
                if (error == ShaderProgram::ShaderSourceError::ShaderType)
                    fallback_info = "Incorrect shader type. Expected 'fragment' shader.";
                else if (error == ShaderProgram::ShaderSourceError::ShaderVersion)
                    fallback_info = "Incorrect shader version. Version must be '300 es'.";
            }

            DEBUG("Compile shader");
            DEBUG(" GPU ID     = %1", material_gpu_id);
            for (size_t i=0; i<material_shader_source.GetDebugInfoCount(); ++i)
            {
                const auto& info = material_shader_source.GetDebugInfo(i);
                DEBUG(" %1 = %2", base::fmt::FixedString { info.key, 10 }, info.val);
            }

            Shader::CreateArgs args;
            args.name     = material_shader_source.GetShaderName();
            args.source   = material_shader_source.GetSource();
            args.debug    = mDebugMode;
            args.fallback = error != ShaderProgram::ShaderSourceError::Nada;
            args.fallback_info = std::move(fallback_info);

            const auto& uniform_blocks = material_shader_source.ListUniformBlocks();
            for (const auto& uniform_block : uniform_blocks)
            {
                Shader::UniformInfo info;
                info.name = uniform_block.data_decl->name;
                if (uniform_block.data_decl->decl_type == ShaderSource::ShaderDataDeclarationType::UniformBlock)
                {
                    info.type = Shader::UniformType::UniformBlock;
                }
                else if (uniform_block.data_decl->decl_type == ShaderSource::ShaderDataDeclarationType::Uniform)
                {
                    if (uniform_block.data_decl->data_type == ShaderSource::ShaderDataType::Sampler2D)
                        info.type = Shader::UniformType::Sampler2D;
                    else if (uniform_block.data_decl->data_type == ShaderSource::ShaderDataType::Sampler2DArray)
                        info.type = Shader::UniformType::Sampler2DArray;
                } else BUG("Missing shader data type.");

                args.uniform_info.push_back(std::move(info));
            }
            material_shader = mDevice->CreateShader(material_gpu_id, args);
        }
        if (!material_shader->IsValid())
        {
            const auto* material_class = material.GetClass();
            GFX_PAINT_ERROR("Material shader compile error. [name='%1'].",
                material_class ? material_class->GetName() : "");
            const auto& info = material_shader->GetCompileInfo();
            const auto& info_lines = base::SplitString(info, '\n');
            for (const auto& info_line : info_lines)
                GFX_PAINT_ERROR(info_line);

            return nullptr;
        }
        if (material_shader->IsFallback())
        {
            const auto* material_class = material.GetClass();
            const auto& info = material_shader->GetFallbackInfo();
            GFX_PAINT_ERROR("Material shader error. [name='%1'].",
                material_class ? material_class->GetName() : "");
            GFX_PAINT_ERROR(info);
            return nullptr;
        }

        ShaderPtr drawable_shader = mDevice->FindShader(drawable_gpu_id);
        if (drawable_shader == nullptr)
        {
            std::string fallback_info;
            auto error = ShaderProgram::ShaderSourceError::Nada;
            auto drawable_shader_source = program.GetShader(drawable, drawable_environment, *mDevice, &error);
            if (drawable_shader_source.IsEmpty())
            {
                ERROR("Failed to create vertex shader. Using a placeholder that will not render.");
                drawable_shader_source.AddDebugInfo("Fallback", "yes");
                drawable_shader_source.SetVersion(ShaderSource::Version::GLSL_300);
                drawable_shader_source.SetPrecision(ShaderSource::Precision::NotSet);
                drawable_shader_source.SetType(ShaderSource::Type::Vertex);
                drawable_shader_source.LoadRawSource("void main() { gl_Position = vec4(1.0); }");
                if (error == ShaderProgram::ShaderSourceError::ShaderType)
                    fallback_info = "Incorrect shader type. Expected 'vertex' shader.";
                else if (error == ShaderProgram::ShaderSourceError::ShaderVersion)
                    fallback_info = "Incorrect shader version. Version must be '300 es'.";
            }

            DEBUG("Compile shader");
            DEBUG(" GPU ID     = %1", drawable_gpu_id);
            for (size_t i=0; i<drawable_shader_source.GetDebugInfoCount(); ++i)
            {
                const auto& info = drawable_shader_source.GetDebugInfo(i);
                DEBUG(" %1 = %2", base::fmt::FixedString { info.key, 10 }, info.val);
            }

            Shader::CreateArgs args;
            args.name     = drawable_shader_source.GetShaderName();
            args.source   = drawable_shader_source.GetSource();
            args.debug    = mDebugMode;
            args.fallback = error != ShaderProgram::ShaderSourceError::Nada;
            args.fallback_info = std::move(fallback_info);

            const auto& uniform_blocks = drawable_shader_source.ListUniformBlocks();
            for (const auto& uniform_block : uniform_blocks)
            {
                Shader::UniformInfo info;
                info.name = uniform_block.data_decl->name;
                if (uniform_block.data_decl->decl_type == ShaderSource::ShaderDataDeclarationType::UniformBlock)
                {
                    info.type = Shader::UniformType::UniformBlock;
                }
                else if (uniform_block.data_decl->decl_type == ShaderSource::ShaderDataDeclarationType::Uniform)
                {
                    if (uniform_block.data_decl->data_type == ShaderSource::ShaderDataType::Sampler2D)
                        info.type = Shader::UniformType::Sampler2D;
                    else if (uniform_block.data_decl->data_type == ShaderSource::ShaderDataType::Sampler2DArray)
                        info.type = Shader::UniformType::Sampler2DArray;
                } else BUG("Missing shader data type.");

                args.uniform_info.push_back(std::move(info));
            }
            drawable_shader = mDevice->CreateShader(drawable_gpu_id, args);
        }
        if (!drawable_shader->IsValid())
        {
            const auto* drawable_class = drawable.GetClass();
            GFX_PAINT_ERROR("Drawable shader compile error [name='%1'].",
                drawable_class ? drawable_class->GetName() : "");
            const auto& info = drawable_shader->GetCompileInfo();
            const auto& info_lines = base::SplitString(info, '\n');
            for (const auto& info_line : info_lines)
                GFX_PAINT_ERROR(info_line);

            return nullptr;
        }
        if (drawable_shader->IsFallback())
        {
            const auto* drawable_class = drawable.GetClass();
            const auto& info = drawable_shader->GetFallbackInfo();
            GFX_PAINT_ERROR("Drawable shader error. [name='%1']",
                drawable_class ? drawable_class->GetName() : "");
            GFX_PAINT_ERROR(info);
            return nullptr;
        }

        DEBUG("Build program: %1", program.GetName());
        DEBUG(" FS GPU ID = %1", material_gpu_id);
        DEBUG(" VS GPU ID = %1", drawable_gpu_id);
        DEBUG(" FS Name   = %1", material_shader->GetName());
        DEBUG(" VS Name   = %1", drawable_shader->GetName());

        Program::CreateArgs args;
        args.fragment_shader = material_shader;
        args.vertex_shader   = drawable_shader;
        args.name = program.GetName();

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
