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
#include "graphics/shaderpass.h"
#include "graphics/utility.h"
#include "graphics/algo.h"

namespace {
class BloomShader : public gfx::ShaderProgram
{
public:
    BloomShader(const gfx::Color4f& color, float threshold)
      : mColor(color)
      , mThreshold(threshold)
    {}
    virtual bool FilterDraw(void* user) const override
    {
        const auto* packet = static_cast<const engine::DrawPacket*>(user);
        return packet->flags.test(engine::DrawPacket::Flags::PP_Bloom);
    }
    virtual std::string GetShaderId(const gfx::Material& material, const gfx::Material::Environment& env) const override
    {
        auto ret = material.GetShaderId(env);
        ret += "+bloom";
        return ret;
    }
    virtual std::string GetShader(const gfx::Material& material, const gfx::Material::Environment& env, const gfx::Device& device) const override
    {
        std::string source(R"(
#version 100
precision highp float;

struct FS_OUT {
   vec4 color;
} fs_out;

uniform float kBloomThreshold;
uniform vec4  kBloomColor;

vec4 Bloom(vec4 color) {
    float brightness = dot(color.rgb, kBloomColor.rgb); //vec3(0.2126, 0.7252, 0.0722));
    if (brightness > kBloomThreshold)
       return color;
    return vec4(0.0, 0.0, 0.0, 0.0);
}

void FragmentShaderMain();

void main() {
    FragmentShaderMain();

    vec4 color = Bloom(fs_out.color);

    gl_FragColor = color;

})");
        source.append(material.GetShader(env, device));
        return source;
    }
    virtual std::string GetName() const override
    { return "BloomShader"; }
    virtual void ApplyDynamicState(const gfx::Device& device, gfx::Program& program, gfx::Device::State& state) const override
    {
        program.SetUniform("kBloomThreshold", mThreshold);
        program.SetUniform("kBloomColor",     mColor);
        state.blending = gfx::Device::State::BlendOp::None;
    }
private:
    const gfx::Color4f mColor;
    const float mThreshold = 0.0f;
};

} // namespace

namespace engine
{

BloomPass::BloomPass(const std::string& name, const gfx::Color4f& color, float threshold, const gfx::Painter& painter)
    : mName(name)
    , mColor(color)
    , mThreshold(threshold)
    , mPainter(painter)
{ }

void BloomPass::Draw(const SceneRenderLayerList& layers) const
{
    auto* device  = mPainter.GetDevice();
    const auto& surface_size  = mPainter.GetSurfaceSize();
    const auto surface_width  = surface_size.GetWidth();
    const auto surface_height = surface_size.GetHeight();
    constexpr auto no_mips = false;

    auto* bloom_tex = device->FindTexture(mName + "/BloomRenderTexture");
    if (!bloom_tex)
    {
        bloom_tex = device->MakeTexture(mName + "/BloomRenderTexture");
        bloom_tex->SetName("BloomRenderTexture");
        bloom_tex->SetFilter(gfx::Texture::MagFilter::Linear);
        bloom_tex->SetFilter(gfx::Texture::MinFilter::Linear);
        bloom_tex->SetWrapY(gfx::Texture::Wrapping::Clamp);
        bloom_tex->SetWrapX(gfx::Texture::Wrapping::Clamp);
        bloom_tex->Upload(nullptr, surface_width, surface_height, gfx::Texture::Format::RGBA, no_mips);
    }
    else
    {
        const auto texture_width  = bloom_tex->GetWidth();
        const auto texture_height = bloom_tex->GetHeight();
        if (texture_width != surface_width || texture_height != surface_height)
            bloom_tex->Upload(nullptr, surface_width, surface_height, gfx::Texture::Format::RGBA, no_mips);
    }

    auto* bloom_fbo = device->FindFramebuffer("BloomFBO");
    if (!bloom_fbo)
    {
        gfx::Framebuffer::Config conf;
        conf.format = gfx::Framebuffer::Format::ColorRGBA8;
        conf.width  = surface_width;
        conf.height = surface_height;
        bloom_fbo = device->MakeFramebuffer("BloomFBO");
        bloom_fbo->SetColorTarget(bloom_tex);
    }
    bloom_fbo->SetColorTarget(bloom_tex);

    mPainter.SetFramebuffer(bloom_fbo);
    mPainter.ClearColor(gfx::Color::Transparent);

    const BloomShader bloom_shader(mColor, mThreshold);

    for (const auto& scene_layer : layers)
    {
        for (const auto& entity_layer : scene_layer)
        {
            if (!entity_layer.mask_cover_list.empty() && !entity_layer.mask_expose_list.empty())
            {
                const gfx::StencilMaskPass stencil_cover(gfx::StencilClearValue(1),
                                                         gfx::StencilWriteValue(0), mPainter);
                const gfx::StencilMaskPass stencil_expose(gfx::StencilWriteValue(1), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);

                stencil_cover.Draw(entity_layer.mask_cover_list);
                stencil_expose.Draw(entity_layer.mask_expose_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.mask_cover_list.empty())
            {
                const gfx::StencilMaskPass stencil_cover(gfx::StencilClearValue(1),
                                                         gfx::StencilWriteValue(0), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);
                stencil_cover.Draw(entity_layer.mask_cover_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.mask_expose_list.empty())
            {
                const gfx::StencilMaskPass stencil_expose(gfx::StencilClearValue(0),
                                                          gfx::StencilWriteValue(1), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);
                stencil_expose.Draw(entity_layer.mask_expose_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.draw_color_list.empty())
            {
                const gfx::GenericRenderPass pass(mPainter);
                pass.Draw(entity_layer.draw_color_list, bloom_shader);
            }
        }
    }
    gfx::algo::ApplyBlur(mName + "/BloomRenderTexture", bloom_tex, device, 8);

    mBloomTexture = bloom_tex;
}

const gfx::Texture* BloomPass::GetBloomTexture() const
{
    return mBloomTexture;
}

void MainRenderPass::Draw(const SceneRenderLayerList& layers) const
{
    for (const auto& scene_layer : layers)
    {
        for (const auto& entity_layer : scene_layer)
        {
            if (!entity_layer.mask_cover_list.empty() && !entity_layer.mask_expose_list.empty())
            {
                const gfx::StencilMaskPass stencil_cover(gfx::StencilClearValue(1),
                                                         gfx::StencilWriteValue(0), mPainter);
                const gfx::StencilMaskPass stencil_expose(gfx::StencilWriteValue(1), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);

                stencil_cover.Draw(entity_layer.mask_cover_list);
                stencil_expose.Draw(entity_layer.mask_expose_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.mask_cover_list.empty())
            {
                const gfx::StencilMaskPass stencil_cover(gfx::StencilClearValue(1),
                                                         gfx::StencilWriteValue(0), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);
                stencil_cover.Draw(entity_layer.mask_cover_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.mask_expose_list.empty())
            {
                const gfx::StencilMaskPass stencil_expose(gfx::StencilClearValue(0),
                                                          gfx::StencilWriteValue(1), mPainter);
                const gfx::StencilTestColorWritePass cover(gfx::StencilPassValue(1), mPainter);
                stencil_expose.Draw(entity_layer.mask_expose_list);
                cover.Draw(entity_layer.draw_color_list);
            }
            else if (!entity_layer.draw_color_list.empty())
            {
                const gfx::GenericRenderPass pass(mPainter);
                pass.Draw(entity_layer.draw_color_list);
            }
        }
    }
}

void MainRenderPass::Composite(const BloomPass* bloom) const
{
    if (!bloom || !bloom->GetBloomTexture())
        return;

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
uniform sampler2D kBloomTexture;
void main() {
  vec4 sample = texture2D(kBloomTexture, vTexCoord.xy);
  gl_FragColor = sample; //vec4(sample.rgb*sample.a, 1.0);
}
        )";

    const auto& surface = mPainter.GetSurfaceSize();

    auto* bloom_texture = bloom->GetBloomTexture();

    auto* device = mPainter.GetDevice();
    auto* program = device->FindProgram("MainCompositor");
    if (!program)
        program = gfx::MakeProgram(vertex_source, fragment_source, "MainCompositor", *device);

    program->SetTextureCount(1);
    program->SetTexture("kBloomTexture", 0, *bloom_texture);

    auto* quad = gfx::MakeFullscreenQuad(*device);

    gfx::Device::State state;
    state.depth_test   = gfx::Device::State::DepthTest::Disabled;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.culling      = gfx::Device::State::Culling::None;
    state.blending     = gfx::Device::State::BlendOp::Additive;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.viewport     = gfx::IRect(0, 0, surface.GetWidth(), surface.GetHeight());
    device->Draw(*program, *quad, state);
}

} // namespace
