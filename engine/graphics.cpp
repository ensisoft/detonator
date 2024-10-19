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
#include "engine/graphics.h"
#include "graphics/framebuffer.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/program.h"
#include "graphics/renderpass.h"
#include "graphics/shaderprogram.h"
#include "graphics/shadersource.h"
#include "graphics/utility.h"
#include "graphics/algo.h"
#include "graphics/drawcmd.h"

namespace {
class MainShaderProgram : public gfx::ShaderProgram
{
public:
    MainShaderProgram(const gfx::Color4f& bloom_color, float bloom_threshold)
      : mBloomColor(bloom_color)
      , mBloomThreshold(bloom_threshold)
    {}
    virtual std::string GetShaderId(const gfx::Material& material, const gfx::Material::Environment& env) const override
    {
        auto ret = material.GetShaderId(env);
        ret += "+main";
        return ret;
    }
    virtual gfx::ShaderSource GetShader(const gfx::Material& material, const gfx::Material::Environment& env, const gfx::Device& device) const override
    {
        gfx::ShaderSource source;
        source.SetType(gfx::ShaderSource::Type::Fragment);
        source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
        source.SetPrecision(gfx::ShaderSource::Precision::High);

        source.AddSource(R"(
struct FS_OUT {
   vec4 color;
} fs_out;

uniform float kBloomThreshold;
uniform vec4  kBloomColor;

layout (location=0) out vec4 fragOutColor0;
layout (location=1) out vec4 fragOutColor1;

vec4 Bloom(vec4 color) {
    float brightness = dot(color.rgb, kBloomColor.rgb); //vec3(0.2126, 0.7252, 0.0722));
    if (brightness > kBloomThreshold)
       return color;
    return vec4(0.0, 0.0, 0.0, 0.0);
}

float sRGB_encode(float value)
{
   return value <= 0.0031308
       ? value * 12.92
       : pow(value, 1.0/2.4) * 1.055 - 0.055;
}
vec4 sRGB_encode(vec4 color)
{
   vec4 ret;
   ret.r = sRGB_encode(color.r);
   ret.g = sRGB_encode(color.g);
   ret.b = sRGB_encode(color.b);
   ret.a = color.a; // alpha is always linear
   return ret;
}

void FragmentShaderMain();

void main() {
    FragmentShaderMain();

    vec4 bloom = Bloom(fs_out.color);

    fragOutColor0 = sRGB_encode(fs_out.color);
    fragOutColor1 = sRGB_encode(bloom);

})");
        source.Merge(material.GetShader(env, device));
        return source;
    }
    virtual std::string GetName() const override
    { return "MainShader"; }
    virtual void ApplyDynamicState(const gfx::Device& device, gfx::ProgramState& program, gfx::Device::State& state, void* user) const override
    {
        const auto* packet = static_cast<const engine::DrawPacket*>(user);

        const auto bloom = packet->flags.test(engine::DrawPacket::Flags::PP_Bloom);

        program.SetUniform("kBloomThreshold", bloom ? mBloomThreshold : 1.0f);
        program.SetUniform("kBloomColor",     mBloomColor);
        //state.blending = gfx::Device::State::BlendOp::None;
    }
private:
    const gfx::Color4f mBloomColor;
    const float mBloomThreshold = 0.0f;
};

} // namespace

namespace engine
{

LowLevelRenderer::LowLevelRenderer(const std::string* name, gfx::Device& device)
  : mRendererName(name)
  , mDevice(device)
{}

void LowLevelRenderer::Draw(const SceneRenderLayerList& layers) const
{
    mMainFBO = CreateFrameBuffer("MainFBO");
    mMainImage = CreateTextureTarget("MainImage");
    mBloomImage = CreateTextureTarget("BloomImage");

    mMainFBO->SetColorTarget(mMainImage, gfx::Framebuffer::ColorAttachment::Attachment0);
    mMainFBO->SetColorTarget(mBloomImage, gfx::Framebuffer::ColorAttachment::Attachment1);

    mDevice.ClearColor(mSettings.clear_color, mMainFBO, gfx::Device::ColorAttachment::Attachment0);
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

    gfx::Painter painter(&mDevice);
    painter.SetViewport(mSettings.viewport);
    painter.SetSurfaceSize(mSettings.surface_size);
    painter.SetEditingMode(mSettings.editing_mode);
    painter.SetPixelRatio(mSettings.pixel_ratio);
    painter.SetFramebuffer(mMainFBO);

    MainShaderProgram main_program(mSettings.bloom_color, mSettings.bloom_threshold);

    for (const auto& scene_layer : layers)
    {
        for (const auto& entity_layer : scene_layer)
        {
            if (!entity_layer.mask_cover_list.empty() && !entity_layer.mask_expose_list.empty())
            {
                gfx::detail::StencilShaderProgram stencil_program;

                painter.ClearStencil(gfx::StencilClearValue(1));
                painter.Draw(entity_layer.mask_cover_list, stencil_program);
                painter.Draw(entity_layer.mask_expose_list, stencil_program);
                painter.Draw(entity_layer.draw_color_list, main_program);
            }
            else if (!entity_layer.mask_cover_list.empty())
            {
                gfx::detail::StencilShaderProgram stencil_program;

                painter.ClearStencil(gfx::StencilClearValue(1));
                painter.Draw(entity_layer.mask_cover_list, stencil_program);
                painter.Draw(entity_layer.draw_color_list, main_program);
            }
            else if (!entity_layer.mask_expose_list.empty())
            {
                gfx::detail::StencilShaderProgram stencil_program;

                painter.ClearStencil(gfx::StencilClearValue(0));
                painter.Draw(entity_layer.mask_expose_list, stencil_program);
                painter.Draw(entity_layer.draw_color_list, main_program);
            }
            else if (!entity_layer.draw_color_list.empty())
            {
                painter.Draw(entity_layer.draw_color_list, main_program);
            }
        }
    }

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

void LowLevelRenderer::Blit() const
{
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
        program->SetTextureCount(1);
        program->SetTexture("kTexture", 0, *mMainImage);

        gfx::Device::State state;
        state.depth_test   = gfx::Device::State::DepthTest::Disabled;
        state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
        state.culling      = gfx::Device::State::Culling::None;
        state.blending     = gfx::Device::State::BlendOp::None;
        state.bWriteColor  = true;
        state.premulalpha  = false;
        state.viewport     = gfx::IRect(0, 0, surface_width, surface_height);
        mDevice.Draw(*program, *quad, state, nullptr /*framebuffer*/);
    }

    // blend in the bloom image if any
    if (mSettings.enable_bloom)
    {
        gfx::algo::ApplyBlur(*mRendererName + "BloomImage", mBloomImage, &mDevice, 8);

        program->SetTextureCount(1);
        program->SetTexture("kTexture", 0, *mBloomImage);

        gfx::Device::State state;
        state.depth_test   = gfx::Device::State::DepthTest::Disabled;
        state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
        state.culling      = gfx::Device::State::Culling::None;
        state.blending     = gfx::Device::State::BlendOp::Additive;
        state.bWriteColor  = true;
        state.premulalpha  = false;
        state.viewport     = gfx::IRect(0, 0, surface_width, surface_height);
        mDevice.Draw(*program, *quad, state, nullptr /*framebuffer*/);
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

} // namespace
