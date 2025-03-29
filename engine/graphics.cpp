// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "game/enum.h"
#include "engine/graphics.h"
#include "graphics/framebuffer.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/program.h"
#include "graphics/renderpass.h"
#include "graphics/shader_program.h"
#include "graphics/shader_programs.h"
#include "graphics/shader_source.h"
#include "graphics/utility.h"
#include "graphics/algo.h"
#include "graphics/drawcmd.h"
#include "graphics/generic_shader_program.h"
#include "graphics/particle_engine.h"

namespace engine
{

LowLevelRenderer::LowLevelRenderer(const std::string* name, gfx::Device& device)
  : mRendererName(name)
  , mDevice(device)
{}

void LowLevelRenderer::DrawPackets(DrawPacketList& packets, LightList& lights) const
{
    if (mSettings.enable_bloom)
    {
        DrawFramebuffer(packets, lights);
    }
    else
    {
        DrawDefault(packets, lights);
    }
}

void LowLevelRenderer::DrawDefault(DrawPacketList& packets, LightList& lights) const
{
    // draw using the default frame buffer

    mDevice.ClearColorDepth(mSettings.camera.clear_color, 1.0f);

    if (mRenderHook)
    {
        LowLevelRendererHook::GPUResources res;
        res.device      = &mDevice;
        res.framebuffer = nullptr;
        res.main_image  = nullptr;
        mRenderHook->BeginDraw(mSettings, res);
    }

    gfx::GenericShaderProgram program;
    program.SetCameraCenter(0.0f, 0.0f, 10000.0f);
    program.EnableFeature(gfx::GenericShaderProgram::OutputFeatures::WriteBloomTarget, false);
    program.EnableFeature(gfx::GenericShaderProgram::ShadingFeatures::BasicLight, mSettings.enable_lights);

    Draw(packets, lights, nullptr, program);

    if (mRenderHook)
    {
        LowLevelRendererHook::GPUResources res;
        res.device      = &mDevice;
        res.framebuffer = nullptr;
        res.main_image  = nullptr;
        mRenderHook->EndDraw(mSettings, res);
    }

}
void LowLevelRenderer::DrawFramebuffer(DrawPacketList& packets, LightList& lights) const
{
    // draw using our own frame buffer with a texture color target
    // for post processing or when using bloom
    mMainFBO = CreateFrameBuffer("MainFBO");
    mMainImage = CreateTextureTarget("MainImage");
    mBloomImage = CreateTextureTarget("BloomImage");

    mMainFBO->SetColorTarget(mMainImage, gfx::Framebuffer::ColorAttachment::Attachment0);
    mMainFBO->SetColorTarget(mBloomImage, gfx::Framebuffer::ColorAttachment::Attachment1);

    mDevice.ClearColor(mSettings.camera.clear_color, mMainFBO, gfx::Device::ColorAttachment::Attachment0);
    mDevice.ClearColor(gfx::Color::Transparent, mMainFBO, gfx::Device::ColorAttachment::Attachment1);
    mDevice.ClearDepth(1.0f, mMainFBO);

    if (mRenderHook)
    {
        LowLevelRendererHook::GPUResources res;
        res.device      = &mDevice;
        res.framebuffer = mMainFBO;
        res.main_image  = nullptr;
        mRenderHook->BeginDraw(mSettings, res);
    }

    const auto& bloom = mSettings.bloom;
    gfx::GenericShaderProgram program;
    program.SetBloomColor(gfx::Color4f(bloom.red, bloom.green, bloom.blue, 1.0f));
    program.SetBloomThreshold(bloom.threshold);
    program.SetCameraCenter(0.0f, 0.0f, 10000.0f);
    program.EnableFeature(gfx::GenericShaderProgram::OutputFeatures::WriteBloomTarget, true);
    program.EnableFeature(gfx::GenericShaderProgram::ShadingFeatures::BasicLight, mSettings.enable_lights);

    Draw(packets, lights, mMainFBO, program);

    // after this we have the rendering result in the main image
    // texture FBO color attachment
    mMainFBO->Resolve(nullptr, gfx::Framebuffer::ColorAttachment::Attachment0);
    mMainFBO->Resolve(nullptr, gfx::Framebuffer::ColorAttachment::Attachment1);

    if (mRenderHook)
    {
        LowLevelRendererHook::GPUResources res;
        res.device      = &mDevice;
        res.framebuffer = mMainFBO;
        res.main_image  = mMainImage;
        mRenderHook->EndDraw(mSettings, res);
    }
}

void LowLevelRenderer::Draw(DrawPacketList& packets, LightList& lights,
                            gfx::Framebuffer* fbo, gfx::GenericShaderProgram& program) const
{
    const auto& camera = mSettings.camera;

    const auto surface_width = (float)GetSurfaceWidth();
    const auto surface_height = (float)GetSurfaceHeight();
    const auto window_size = glm::vec2{surface_width, surface_height};
    const auto logical_viewport_width = camera.viewport.GetWidth();
    const auto logical_viewport_height = camera.viewport.GetHeight();

    const auto& model_view_camera = CreateModelViewMatrix(GameView::AxisAligned, glm::vec2{0.0f, 0.0f}, camera.scale, camera.rotation);
    const auto& model_view_scene  = CreateModelViewMatrix(GameView::AxisAligned, camera.position, camera.scale, camera.rotation);
    const auto& orthographic = CreateProjectionMatrix(Projection::Orthographic, camera.viewport);
    const auto& perspective  = CreateProjectionMatrix(camera.viewport, camera.ppa);
    const auto& pixel_ratio = window_size / glm::vec2{logical_viewport_width, logical_viewport_height} * camera.scale;

    // draw editing mode tilemap data packets first.
    // this is only used to visualize map data in the editor.
    if (mSettings.editing_mode)
    {
        // Setup painter to draw in whatever is the map perspective.
        gfx::Painter map_painter(&mDevice);
        map_painter.SetProjectionMatrix(orthographic);
        map_painter.SetViewMatrix(CreateModelViewMatrix(camera.map_perspective, camera.position, camera.scale, camera.rotation));
        map_painter.SetPixelRatio({1.0f, 1.0f});
        map_painter.SetViewport(mSettings.surface.viewport);
        map_painter.SetSurfaceSize(mSettings.surface.size);
        map_painter.SetFramebuffer(fbo);

        for (auto& packet : packets)
        {
            if (packet.domain != DrawPacket::Domain::Editor)
                continue;
            else if (packet.source != DrawPacket::Source::Map)
                continue;

            map_painter.Draw(*packet.drawable, packet.transform, *packet.material);

            if (mRenderHook)
            {
                LowLevelRendererHook::GPUResources resources;
                resources.device      = &mDevice;
                resources.framebuffer = fbo;
                resources.main_image  = nullptr;
                mRenderHook->EndDrawPacket(mSettings, resources, packet, map_painter);
            }
        }
    }

    gfx::Painter scene_painter(&mDevice);
    scene_painter.SetProjectionMatrix(orthographic);
    scene_painter.SetViewMatrix(model_view_scene);
    scene_painter.SetViewport(mSettings.surface.viewport);
    scene_painter.SetSurfaceSize(mSettings.surface.size);
    scene_painter.SetEditingMode(mSettings.editing_mode);
    scene_painter.SetPixelRatio(pixel_ratio);
    scene_painter.SetFramebuffer(fbo);

    // Each entity in the scene is assigned to a scene/entity layer and each
    // entity node within an entity is assigned to an entity layer.
    // Thus, to have the right ordering both indices of each
    // render packet must be considered!
    std::vector<std::vector<RenderLayer>> layers;

    // assign lights to render layers
    TRACE_ENTER(LightLayers);
    for (auto& light : lights)
    {
        // transform the light to view space
        light.light->position = model_view_scene * light.transform * glm::vec4{0.0f, 0.0f, 0.0f, 1.0};
        //light.light->direction = light.transform * glm::vec4{light.light->direction, 0.0f};

        const auto render_layer_index = light.render_layer;
        ASSERT(render_layer_index >= 0);
        if (render_layer_index >= layers.size())
            layers.resize(render_layer_index + 1);

        auto& render_layer = layers[render_layer_index];

        const auto packet_index = light.packet_index;
        ASSERT(packet_index >= 0);
        if (packet_index >= render_layer.size())
            render_layer.resize(packet_index + 1);

        RenderLayer& layer = render_layer[packet_index];
        layer.layer_lights.push_back(&light);
    }
    TRACE_LEAVE(LightLayers);


    TRACE_ENTER(CreateDrawCmd);
    for (auto& packet : packets)
    {
        if (!packet.material || !packet.drawable)
            continue;
        else if (packet.domain != DrawPacket::Domain::Scene)
            continue;

        const glm::mat4* projection = nullptr;
        if (packet.projection == DrawPacket::Projection::Orthographic)
            projection = &orthographic;
        else if (packet.projection == DrawPacket::Projection::Perspective)
            projection = &perspective;
        else BUG("Bug on draw packet projection.");

        const glm::mat4* view = nullptr;
        if (packet.coordinate_space == DrawPacket::CoordinateSpace::Scene)
            view = &model_view_scene;
        else if (packet.coordinate_space == DrawPacket::CoordinateSpace::Camera)
            view = &model_view_camera;
        else BUG("Bug on draw packet coordinate space.");

        if (packet.coordinate_space == DrawPacket::CoordinateSpace::Scene)
        {
            if (CullDrawPacket(packet, *projection, model_view_scene))
            {
                packet.flags.set(DrawPacket::Flags::CullPacket, true);
                //DEBUG("culled packet %1", packet.name);
            }
        }

        if (mPacketFilter && !mPacketFilter->InspectPacket(packet))
            continue;

        if (packet.flags.test(DrawPacket::Flags::CullPacket))
            continue;

        gfx::Painter::DrawCommand draw;
        draw.user               = (void*)&packet;
        draw.model              = &packet.transform;
        draw.drawable           = packet.drawable.get();
        draw.material           = packet.material.get();
        draw.state.culling      = packet.culling;
        draw.state.line_width   = packet.line_width;
        draw.state.depth_test   = packet.depth_test;
        draw.state.write_color  = true;
        draw.state.stencil_func = gfx::Painter::StencilFunc ::Disabled;
        draw.view               = view;
        draw.projection         = projection;
        scene_painter.Prime(draw);

        const auto render_layer_index = packet.render_layer;
        ASSERT(render_layer_index >= 0);
        if (render_layer_index >= layers.size())
            layers.resize(render_layer_index + 1);

        auto& render_layer = layers[render_layer_index];

        const auto packet_index = packet.packet_index;
        ASSERT(packet_index >= 0);
        if (packet_index >= render_layer.size())
            render_layer.resize(packet_index + 1);

        RenderLayer& layer = render_layer[packet_index];
        if (packet.pass == DrawPacket::RenderPass::DrawColor)
            layer.draw_color_list.push_back(draw);
        else if (packet.pass == DrawPacket::RenderPass::MaskCover)
            layer.mask_cover_list.push_back(draw);
        else if (packet.pass == DrawPacket::RenderPass::MaskExpose)
            layer.mask_expose_list.push_back(draw);
        else BUG("Missing packet render pass mapping.");
    }
    TRACE_LEAVE(CreateDrawCmd);

    // set the state for each draw packet.
    TRACE_ENTER(ArrangeLayers);
    for (auto& scene_layer : layers)
    {
        for (auto& entity_layer : scene_layer)
        {
            const auto needs_stencil = !entity_layer.mask_cover_list.empty() ||
                                       !entity_layer.mask_expose_list.empty();
            if (needs_stencil)
            {
                for (auto& draw : entity_layer.mask_expose_list)
                {
                    draw.state.write_color   = false;
                    draw.state.stencil_ref   = 1;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
                }
                for (auto& draw : entity_layer.mask_cover_list)
                {
                    draw.state.write_color   = false;
                    draw.state.stencil_ref   = 0;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
                }
                for (auto& draw : entity_layer.draw_color_list)
                {
                    draw.state.write_color   = true;
                    draw.state.stencil_ref   = 1;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::RefIsEqual;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::DontModify;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::DontModify;
                }
            }
        }
    }
    TRACE_LEAVE(ArrangeLayers);

    for (const auto& scene_layer : layers)
    {
        for (const auto& entity_layer : scene_layer)
        {
            program.ClearLights();
            for (auto* light : entity_layer.layer_lights)
                program.AddLight(light->light);

            if (!entity_layer.mask_cover_list.empty() && !entity_layer.mask_expose_list.empty())
            {
                gfx::StencilShaderProgram stencil_program;

                scene_painter.ClearStencil(gfx::StencilClearValue(1));
                scene_painter.Draw(entity_layer.mask_cover_list, stencil_program);
                scene_painter.Draw(entity_layer.mask_expose_list, stencil_program);
                scene_painter.Draw(entity_layer.draw_color_list, program);
            }
            else if (!entity_layer.mask_cover_list.empty())
            {
                gfx::StencilShaderProgram stencil_program;

                scene_painter.ClearStencil(gfx::StencilClearValue(1));
                scene_painter.Draw(entity_layer.mask_cover_list, stencil_program);
                scene_painter.Draw(entity_layer.draw_color_list, program);
            }
            else if (!entity_layer.mask_expose_list.empty())
            {
                gfx::StencilShaderProgram stencil_program;

                scene_painter.ClearStencil(gfx::StencilClearValue(0));
                scene_painter.Draw(entity_layer.mask_expose_list, stencil_program);
                scene_painter.Draw(entity_layer.draw_color_list, program);
            }
            else if (!entity_layer.draw_color_list.empty())
            {
                scene_painter.Draw(entity_layer.draw_color_list, program);
            }

            if (mRenderHook)
            {
                LowLevelRendererHook::GPUResources resources;
                resources.device      = &mDevice;
                resources.framebuffer = fbo;
                resources.main_image  = nullptr;
                for (const auto& draw_cmd : entity_layer.draw_color_list)
                {
                    const auto* draw_packet = static_cast<const DrawPacket*>(draw_cmd.user);
                    mRenderHook->EndDrawPacket(mSettings, resources, *draw_packet, scene_painter);
                }
            }
        }
    }

    // draw editor packets
    if (mSettings.editing_mode)
    {
        for (auto& packet : packets)
        {
            // we're only drawing editor packets that are used for extra
            // visualization
            if (packet.domain != DrawPacket::Domain::Editor)
                continue;

            // currently editor + map = data layer visualization which
            // is drawn before anything else so that ends up below
            // the rendering layer stuff.
            if (packet.source == DrawPacket::Source::Map)
                continue;

            scene_painter.Draw(*packet.drawable, packet.transform, *packet.material);

            if (mRenderHook)
            {
                LowLevelRendererHook::GPUResources resources;
                resources.device      = &mDevice;
                resources.framebuffer = fbo;
                resources.main_image  = nullptr;
                mRenderHook->EndDrawPacket(mSettings, resources, packet, scene_painter);
            }
        }
    }
}

void LowLevelRenderer::BlitImage() const
{
    if (!mSettings.enable_bloom)
        return;

    const auto surface_width  = (int)GetSurfaceWidth();
    const auto surface_height = (int)GetSurfaceHeight();
    constexpr auto* vertex_source = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
  vTexCoord   = aTexCoord;
}
    )";

    constexpr auto* fragment_source = R"(
#version 100
precision highp float;

varying vec2 vTexCoord;
uniform sampler2D kTexture;

void main() {
  gl_FragColor = texture2D(kTexture, vTexCoord.xy);
}
        )";

    auto program = mDevice.FindProgram("MainCompositor");
    if (!program)
        program = gfx::MakeProgram(vertex_source, fragment_source, "MainCompositor", mDevice);

    auto quad = gfx::MakeFullscreenQuad(mDevice);

    // transfer the main image to the default frame buffer as-is
    {
        gfx::DeviceState ds(mDevice);

        gfx::ProgramState ps;
        ps.SetTextureCount(1);
        ps.SetTexture("kTexture", 0, *mMainImage);

        gfx::Device::State state;
        state.culling      = gfx::Device::State::Culling::None;
        state.blending     = gfx::Device::State::BlendOp::None;
        state.premulalpha  = false;

        gfx::Device::ColorDepthStencilState dss;
        dss.bWriteColor  = true;
        dss.depth_test   = gfx::Device::ColorDepthStencilState::DepthTest::Disabled;
        dss.stencil_func = gfx::Device::ColorDepthStencilState::StencilFunc::Disabled;

        gfx::Device::ViewportState vs;
        vs.viewport = gfx::IRect(0, 0, surface_width, surface_height);

        mDevice.SetColorDepthStencilState(dss);
        mDevice.SetViewportState(vs);
        mDevice.Draw(*program, ps, *quad, state, nullptr /*framebuffer*/);
    }

    // blend in the bloom image if any
    if (mSettings.enable_bloom)
    {
        gfx::algo::ApplyBlur(*mRendererName + "BloomImage", mBloomImage, &mDevice, 8);

        gfx::DeviceState ds(mDevice);

        gfx::ProgramState ps;
        ps.SetTextureCount(1);
        ps.SetTexture("kTexture", 0, *mBloomImage);

        gfx::Device::State state;
        state.culling      = gfx::Device::State::Culling::None;
        state.blending     = gfx::Device::State::BlendOp::Additive;
        state.premulalpha  = false;

        gfx::Device::ColorDepthStencilState dss;
        dss.depth_test   = gfx::Device::ColorDepthStencilState::DepthTest::Disabled;
        dss.stencil_func = gfx::Device::ColorDepthStencilState::StencilFunc::Disabled;
        dss.bWriteColor  = true;

        gfx::Device::ViewportState vs;
        vs.viewport = gfx::IRect(0, 0, surface_width, surface_height);

        mDevice.SetViewportState(vs);
        mDevice.SetColorDepthStencilState(dss);
        mDevice.Draw(*program, ps, *quad, state, nullptr /*framebuffer*/);
    }
}

gfx::Texture* LowLevelRenderer::CreateTextureTarget(const std::string& name) const
{
    const auto surface_width  = GetSurfaceWidth();
    const auto surface_height = GetSurfaceHeight();

    auto* texture = mDevice.FindTexture(*mRendererName + name);
    if (!texture)
    {
        texture = mDevice.MakeTexture(*mRendererName + name);
        texture->SetName(*mRendererName + name);
        texture->SetFilter(gfx::Texture::MagFilter::Linear);
        texture->SetFilter(gfx::Texture::MinFilter::Linear);
        texture->SetWrapY(gfx::Texture::Wrapping::Clamp);
        texture->SetWrapX(gfx::Texture::Wrapping::Clamp);
        texture->Allocate(surface_width, surface_height, gfx::Texture::Format::sRGBA);
    }
    else
    {
        const auto texture_width  = texture->GetWidth();
        const auto texture_height = texture->GetHeight();
        if (texture_width != surface_width || texture_height != surface_height)
            texture->Allocate(surface_width, surface_height, gfx::Texture::Format::sRGBA);
    }
    return texture;
}

gfx::Framebuffer* LowLevelRenderer::CreateFrameBuffer(const std::string& name) const
{
    const auto surface_width  = GetSurfaceWidth();
    const auto surface_height = GetSurfaceHeight();

    auto* fbo = mDevice.FindFramebuffer(*mRendererName + name);
    if (fbo)
    {
        const auto fbo_width = fbo->GetWidth();
        const auto fbo_height = fbo->GetHeight();
        if (fbo_width != surface_width || fbo_height != surface_height)
        {
            DEBUG("Recreate frame buffer object for new surface size. [fbo='%1', size=%2x%3].",
                  *mRendererName + name, surface_width, surface_height);
            mDevice.DeleteFramebuffer(*mRendererName + name);
            fbo = nullptr;
        }
    }

    if (!fbo)
    {
        gfx::Framebuffer::Config conf;
        conf.format = gfx::Framebuffer::Format::ColorRGBA8_Depth24_Stencil8;
        conf.msaa   = gfx::Framebuffer::MSAA::Enabled;
        conf.width  = surface_width;
        conf.height = surface_height;
        conf.color_target_count = 2;
        fbo = mDevice.MakeFramebuffer(*mRendererName + name);
        fbo->SetConfig(conf);
    }
    return fbo;
}

bool LowLevelRenderer::CullDrawPacket(const DrawPacket& packet, const glm::mat4& projection, const glm::mat4& modelview) const
{
    // the draw packets for the map are only generated for the visible part
    // of the map already. so culling check is not needed.
    if (packet.source == DrawPacket::Source::Map)
        return false;

    const auto& shape = packet.drawable;

    // don't cull global particle engines since the particles can be "where-ever"
    if (shape->GetType() == gfx::Drawable::Type::ParticleEngine)
    {
        const auto& particle = std::static_pointer_cast<const gfx::ParticleEngineInstance>(shape);
        const auto& params   = particle->GetParams();
        if (params.coordinate_space == gfx::ParticleEngineClass::CoordinateSpace::Global)
            return false;
    }

    // take the model view bounding box (which we should probably get from the drawable)
    // and project all the 6 corners on the rendering plane. cull the packet if it's
    // outside the NDC the X, Y axis.
    std::vector<glm::vec4> corners;

    if (Is3DShape(*shape))
    {
        corners.resize(8);
        corners[0] = glm::vec4 { -0.5f,  0.5f, 0.5f, 1.0f };
        corners[1] = glm::vec4 { -0.5f, -0.5f, 0.5f, 1.0f };
        corners[2] = glm::vec4 {  0.5f,  0.5f, 0.5f, 1.0f };
        corners[3] = glm::vec4 {  0.5f, -0.5f, 0.5f, 1.0f };
        corners[4] = glm::vec4 { -0.5f,  0.5f, -0.5f, 1.0f };
        corners[5] = glm::vec4 { -0.5f, -0.5f, -0.5f, 1.0f };
        corners[6] = glm::vec4 {  0.5f,  0.5f, -0.5f, 1.0f };
        corners[7] = glm::vec4 {  0.5f, -0.5f, -0.5f, 1.0f };
    }
    else
    {
        // regarding the Y value, remember the complication
        // in the 2D vertex shader. huhu.. should really fix
        // this one soon...
        corners.resize(4);
        corners[0] = glm::vec4 { 0.0f,  0.0f, 0.0f, 1.0f };
        corners[1] = glm::vec4 { 0.0f,  1.0f, 0.0f, 1.0f };
        corners[2] = glm::vec4 { 1.0f,  1.0f, 0.0f, 1.0f };
        corners[3] = glm::vec4 { 1.0f,  0.0f, 0.0f, 1.0f };
    }

    const auto& transform = projection * modelview;

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    for (const auto& p0 : corners)
    {
        auto p1 = transform * packet.transform * p0;
        p1 /= p1.w;
        min_x = std::min(min_x, p1.x);
        max_x = std::max(max_x, p1.x);
        min_y = std::min(min_y, p1.y);
        max_y = std::max(max_y, p1.y);
    }
    // completely above or below the NDC
    if (max_y < -1.0f || min_y > 1.0f)
        return true;

    // completely to the left of to the right of the NDC
    if (max_x < -1.0f || min_x > 1.0f)
        return true;

    return false;

}

} // namespace
