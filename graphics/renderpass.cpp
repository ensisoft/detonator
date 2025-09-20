// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <glm/mat4x4.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <cmath>

#include "graphics/device.h"
#include "graphics/framebuffer.h"
#include "graphics/renderpass.h"
#include "graphics/texture.h"
#include "graphics/shader_program.h"
#include "graphics/utility.h"

namespace gfx
{

void ShadowMapRenderPass::InitState() const
{
    auto* fbo = CreateFramebuffer();

    for (size_t i=0; i<mProgram.GetLightCount(); ++i)
    {
        auto* depth_texture = GetDepthTexture(i);
        fbo->SetDepthTarget(depth_texture);
        mDevice->ClearDepth(1.0f, fbo);
    }
}

bool ShadowMapRenderPass::Draw(const DrawCommandList& draw_cmd_list) const
{
    bool ok = true;

    auto* fbo = CreateFramebuffer();

    Painter::RenderPassState render_pass_state;
    render_pass_state.render_pass      = RenderPass::ShadowMapPass;
    render_pass_state.cds.bWriteColor  = false;
    render_pass_state.cds.depth_test   = Painter::DepthTest::LessOrEQual;
    render_pass_state.cds.stencil_func = Painter::StencilFunc::Disabled;

    const auto shadow_map_width = mProgram.GetShadowMapWidth();
    const auto shadow_map_height = mProgram.GetShadowMapHeight();

    Painter shadow_painter;
    shadow_painter.SetViewport(0, 0, shadow_map_width, shadow_map_height);
    shadow_painter.SetSurfaceSize(shadow_map_width, shadow_map_height);
    shadow_painter.SetDevice(mDevice);
    shadow_painter.SetFramebuffer(fbo);

    // for each light, create a  depth render target.
    // render each object from the lights perspective into the
    // render buffer depth target
    for (size_t i=0; i<mProgram.GetLightCount(); ++i)
    {
        const auto& light = mProgram.GetLight(i);
        if (light.type == BasicLightType::Ambient)
            continue;

        auto* depth_texture = GetDepthTexture(i);
        fbo->SetDepthTarget(depth_texture);

        const auto& world_to_light = GetLightViewMatrix(i);
        const auto& light_projection = GetLightProjectionMatrix(i);

        shadow_painter.SetViewMatrix(world_to_light);
        shadow_painter.SetProjectionMatrix(light_projection);

        ok &= shadow_painter.Draw(draw_cmd_list, mProgram, render_pass_state);
    }
    return ok;
}

Framebuffer* ShadowMapRenderPass::CreateFramebuffer() const
{
    if (auto* fbo = mDevice->FindFramebuffer(mRendererName + "/ShadowMapFBO"))
        return fbo;

    gfx::Framebuffer::Config config;
    config.height = mProgram.GetShadowMapHeight();
    config.width  = mProgram.GetShadowMapWidth();
    config.msaa   = gfx::Framebuffer::MSAA::Disabled;
    config.format = gfx::Framebuffer::Format::DepthTexture32f;
    config.color_target_count = 0;

    auto* fbo = mDevice->MakeFramebuffer(mRendererName + "/ShadowMapFBO");
    fbo->SetConfig(config);
    return fbo;
}
Texture* ShadowMapRenderPass::GetDepthTexture(unsigned light_index) const
{
    const auto shadow_map_width = mProgram.GetShadowMapWidth();
    const auto shadow_map_height = mProgram.GetShadowMapHeight();

    auto* texture = mDevice->FindTexture(mRendererName + "/ShadowMap" + std::to_string(light_index));
    if (!texture)
    {
        texture = mDevice->MakeTexture(mRendererName + "/ShadowMap" + std::to_string(light_index));
        texture->SetName(mRendererName + "/ShadowMap" + std::to_string(light_index));
        texture->SetFilter(gfx::Texture::MagFilter::Nearest);
        texture->SetFilter(gfx::Texture::MinFilter::Nearest);
        texture->SetWrapY(gfx::Texture::Wrapping::Clamp);
        texture->SetWrapX(gfx::Texture::Wrapping::Clamp);
        texture->Allocate(shadow_map_width, shadow_map_height, Texture::Format::DepthComponent32f);
    }
    else
    {
        const auto texture_width  = texture->GetWidth();
        const auto texture_height = texture->GetHeight();
        if (texture_width != shadow_map_width || texture_height != shadow_map_height)
            texture->Allocate(shadow_map_width, shadow_map_height, Texture::Format::DepthComponent32f);
    }
    return texture;
}

glm::mat4 ShadowMapRenderPass::GetLightViewMatrix(unsigned light_index) const
{
    return mProgram.GetLightViewMatrix(light_index);
}
glm::mat4 ShadowMapRenderPass::GetLightProjectionMatrix(unsigned light_index) const
{
    return mProgram.GetLightProjectionMatrix(light_index);
}
float ShadowMapRenderPass::GetLightProjectionNearPlane(unsigned light_index) const
{
    return mProgram.GetLightProjectionNearPlane(light_index);
}
float ShadowMapRenderPass::GetLightProjectionFarPlane(unsigned light_index) const
{
    return mProgram.GetLightProjectionFarPlane(light_index);
}

ShadowMapRenderPass::LightProjectionType ShadowMapRenderPass::GetLightProjectionType(unsigned light_index) const
{
    return mProgram.GetLightProjectionType(light_index);
}


} // namespace
