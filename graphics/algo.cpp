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

#include "warnpush.h"
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"


#include <algorithm>

#include "base/assert.h"
#include "graphics/algo.h"
#include "graphics/device.h"
#include "graphics/geometry.h"
#include "graphics/drawcmd.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/framebuffer.h"
#include "graphics/utility.h"

namespace gfx::algo {

void ExtractColor(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device,
                  const gfx::Color4f& color, float threshold)
{

    const auto src_width  = src->GetWidth();
    const auto src_height = src->GetHeight();
    const auto src_format = src->GetFormat();
    const auto dst_width  = dst->GetWidth();
    const auto dst_height = dst->GetHeight();
    const auto dst_format = dst->GetFormat();

    // currently no filtering allowed. should be okay though?
    ASSERT(src_width == dst_width && src_height == dst_height);

    ASSERT(src_format == Texture::Format::sRGBA ||
           src_format == Texture::Format::RGBA ||
           src_format == Texture::Format::RGB ||
           src_format == Texture::Format::sRGB);

    // render target RGBA texture only
    ASSERT(dst_format == Texture::Format::sRGBA ||
           dst_format == Texture::Format::RGBA);

    auto* fbo = device->FindFramebuffer("AlgoFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("AlgoFBO");
        Framebuffer::Config conf;
        conf.width  = 0;
        conf.height = 0;
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }

    dst->SetFilter(gfx::Texture::MinFilter::Linear);
    dst->SetFilter(gfx::Texture::MagFilter::Linear);
    dst->SetWrapX(gfx::Texture::Wrapping::Clamp);
    dst->SetWrapY(gfx::Texture::Wrapping::Clamp);
    fbo->SetColorTarget(dst);

    constexpr auto* vertex_src = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
   gl_Position = vec4(aPosition.xy, 0.0, 1.0);
   vTexCoord   = aTexCoord;
}
)";

    constexpr auto* fragment_src = R"(
#version 100
precision highp float;

varying vec2 vTexCoord;

uniform float     kThreshold;
uniform vec4      kColor;
uniform sampler2D kSourceTexture;

vec4 ExtractColor() {
    vec4 color = texture2D(kSourceTexture, vTexCoord);

    float brightness = dot(kColor.rgb, color.rgb);
    if (brightness > kThreshold)
        return color;

    return vec4(0.0, 0.0, 0.0, 0.0);
}

void main() {
   gl_FragColor = ExtractColor();
}
)";

    auto program = device->FindProgram("BloomColorProgram");
    if (!program)
        program = MakeProgram(vertex_src, fragment_src, "BloomColorProgram", *device);

    auto quad = MakeFullscreenQuad(*device);

    program->SetUniform("kColor", color);
    program->SetUniform("kThreshold", threshold);
    program->SetTextureCount(1);
    program->SetTexture("kSourceTexture", 0, *src);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.depth_test   = Device::State::DepthTest::Disabled;
    state.stencil_func = Device::State::StencilFunc::Disabled;
    state.culling      = Device::State::Culling::None;
    state.blending     = Device::State::BlendOp::None;
    state.viewport     = IRect(0, 0, dst->GetWidthI(), dst->GetHeightI());

    device->Draw(*program, *quad, state, fbo);

    fbo->SetColorTarget(nullptr);
}


void ExtractColor(const gfx::Texture* src, gfx::Texture** dst, gfx::Device* device,
                  const gfx::Color4f& color, float threshold)
{
    const auto& src_gpu_id = src->GetId();

    auto* texture = device->FindTexture(src_gpu_id + "/ColorExtract");
    if (!texture)
    {
        const auto& src_name   = src->GetName();
        const auto src_width   = src->GetWidth();
        const auto src_height  = src->GetHeight();
        const auto src_format  = src->GetFormat();

        texture = device->MakeTexture(src_gpu_id + "/ColorExtract");

        if (src_format == Texture::Format::RGB ||
            src_format == Texture::Format::RGBA)
            texture->Allocate(src_width, src_height, Texture::Format::RGBA);
        else if (src_format == Texture::Format::sRGB ||
                 src_format == Texture::Format::sRGBA)
            texture->Allocate(src_width, src_height, Texture::Format::sRGBA);
        else BUG("Incorrect texture format for color extract.");

        texture->SetName(src_name + "/ColorExtract");
    }
    *dst = texture;
    ExtractColor(src, *dst, device, color, threshold);
}


void ColorTextureFromAlpha(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device)
{
    ASSERT(texture->GetFormat() == Texture::Format::AlphaMask);

    const auto width  = texture->GetWidth();
    const auto height = texture->GetHeight();

    // Create a new temp texture, copy the alpha texture over
    // then respecify the incoming texture to be RGBA and copy
    // the data over from the temp.

    auto* tmp = device->FindTexture(gpu_id + "/tmp-color");
    if (!tmp)
    {
        tmp = device->MakeTexture(gpu_id + "/tmp-color");
        tmp->Allocate(texture->GetWidth(), texture->GetHeight(), gfx::Texture::Format::RGBA);
        tmp->SetName("AlphaColorHelperTexture");
        tmp->SetFilter(gfx::Texture::MinFilter::Nearest);
        tmp->SetFilter(gfx::Texture::MagFilter::Nearest);
        tmp->SetWrapX(gfx::Texture::Wrapping::Clamp);
        tmp->SetWrapY(gfx::Texture::Wrapping::Clamp);
        tmp->SetGarbageCollection(true);
        tmp->SetTransient(true);
    }

    // copy from alpha into temp
    CopyTexture(texture, tmp, device);

    // respecify the alpha texture. Format RGBA should be okay
    // since alpha is linear and we don't have rea RGB data.
    texture->Allocate(width, height, Texture::Format::RGBA);

    // copy temp back to alpha
    CopyTexture(tmp, texture, device);

    // logical alpha only.
    texture->SetFlag(Texture::Flags::AlphaMask, true);
}

void ApplyBlur(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device,
    unsigned iterations, BlurDirection direction)
{
    const auto has_mips   = texture->HasMips();
    const auto min_filter = texture->GetMinFilter();
    const auto tex_format = texture->GetFormat();

    // Currently, this is the only supported format due to limitations on the
    // GL ES2 FBO color buffer target.
    ASSERT(tex_format == gfx::Texture::Format::RGBA ||
           tex_format == gfx::Texture::Format::sRGBA);

    // Since we're both sampling from and rendering to the input texture and
    // *not* generating any mips during the process the sampling must
    // use non-mipmap sampling filtering mode. Likely use case anyway is to
    // first create the source texture, upload level 0, apply blur and then
    // generate mips and proceed to use the texture in normal rendering.
    ASSERT(min_filter == gfx::Texture::MinFilter::Linear ||
           min_filter == gfx::Texture::MinFilter::Nearest);

    auto* fbo = device->FindFramebuffer("BlurFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("BlurFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }
    auto* tmp = device->FindTexture(gpu_id + "/tmp-color");
    if (!tmp)
    {
        tmp = device->MakeTexture(gpu_id + "/tmp-color");
        tmp->SetName("BlurHelperTexture");
        tmp->SetFilter(gfx::Texture::MinFilter::Linear);
        tmp->SetFilter(gfx::Texture::MagFilter::Linear);
        tmp->SetWrapX(gfx::Texture::Wrapping::Clamp);
        tmp->SetWrapY(gfx::Texture::Wrapping::Clamp);
        tmp->SetGarbageCollection(true);
        tmp->SetTransient(true);
    }

    const auto src_width  = texture->GetWidth();
    const auto src_height = texture->GetHeight();
    const auto tmp_width  = tmp->GetWidth();
    const auto tmp_height = tmp->GetHeight();
    if (tmp_width != src_width || tmp_height != src_height)
        tmp->Allocate(src_width, src_height, gfx::Texture::Format::sRGBA);

    constexpr auto* vertex_src = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
  vTexCoord   = aTexCoord;
}
)";

    // we can control the sampling dispersion by adjusting the normalized
    // texel size that is used to advance the sampling position from the
    // current fragment. one method that I've seen is to simply move from
    // texel to texel, i.e. texel_size = vec2(1.0, 1.0) / kTextureSize;
    // but the problem with this is that the blurring results vary depending
    // on the size of input texture and a small texture will blur much more
    // on fewer iterations than a large texture.
    static const char* fragment_src = {
#include "shaders/fragment_blur_kernel.glsl"
    };
    auto program = device->FindProgram("BlurProgram");
    if (!program)
        program = MakeProgram(vertex_src, fragment_src, "BlurProgram", *device);

    auto quad = gfx::MakeFullscreenQuad(*device);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.depth_test   = gfx::Device::State::DepthTest::Disabled;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.culling      = gfx::Device::State::Culling::None;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.viewport     = gfx::IRect(0, 0, (int)src_width, (int)src_height);

    gfx::Texture* textures[] = {tmp, texture};
    for (unsigned i=0; i<iterations; ++i)
    {
        int dir_value = 0;
        if (direction == BlurDirection::Horizontal)
            dir_value = 0;
        else if (direction == BlurDirection::Vertical)
            dir_value = 1;
        else dir_value = int(i & 1);
        fbo->SetColorTarget(textures[0]);
        program->SetUniform("kDirection", dir_value);
        program->SetUniform("kTextureSize", (float)src_width, (float)src_height);
        program->SetTextureCount(1);
        program->SetTexture("kTexture", 0, *textures[1]);
        device->Draw(*program, *quad, state, fbo);

        std::swap(textures[0], textures[1]);
    }

    fbo->SetColorTarget(nullptr);
}

void DetectSpriteEdges(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device,
                       const gfx::Color4f& edge_color)
{
    auto* fbo = device->FindFramebuffer("EdgeFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("EdgeFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        conf.msaa   = Framebuffer::MSAA::Enabled;
        fbo->SetConfig(conf);
    }
    dst->SetFilter(gfx::Texture::MinFilter::Linear);
    dst->SetFilter(gfx::Texture::MagFilter::Linear);
    dst->SetWrapX(gfx::Texture::Wrapping::Clamp);
    dst->SetWrapY(gfx::Texture::Wrapping::Clamp);
    fbo->SetColorTarget(dst);

    constexpr auto* vertex_src = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;

void main() {
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
  vTexCoord   = aTexCoord;
}
)";

    static const char* fragment_src = {
#include "shaders/fragment_edge_kernel.glsl"
    };
    auto program = device->FindProgram("EdgeProgram");
    if (!program)
        program = MakeProgram(vertex_src, fragment_src, "EdgeProgram", *device);

    const float src_width  = src->GetWidthF();
    const float src_height = src->GetHeightF();
    program->SetTextureCount(1);
    program->SetTexture("kSrcTexture", 0, *src);
    program->SetUniform("kTextureSize", src_width, src_height);
    program->SetUniform("kEdgeColor", edge_color);

    auto quad = MakeFullscreenQuad(*device);
    const auto dst_width  = dst->GetWidthI();
    const auto dst_height = dst->GetHeightI();

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.depth_test   = gfx::Device::State::DepthTest::Disabled;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.culling      = gfx::Device::State::Culling::None;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.viewport     = gfx::IRect(0, 0, dst_width, dst_height);
    device->Draw(*program, *quad, state, fbo);

    fbo->Resolve(nullptr);
    fbo->SetColorTarget(nullptr);
}

void DetectSpriteEdges(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device, const gfx::Color4f& edge_color)
{
    auto* edges = device->FindTexture(gpu_id + "/edges");
    if (!edges)
    {
        edges = device->MakeTexture(gpu_id + "/edges");

        edges->Allocate(texture->GetWidth(),
                      texture->GetHeight(),
                      gfx::Texture::Format::sRGBA);

        edges->SetName("EdgeDetectionHelperTexture");
        edges->SetFilter(gfx::Texture::MinFilter::Linear);
        edges->SetFilter(gfx::Texture::MagFilter::Linear);
        edges->SetWrapX(gfx::Texture::Wrapping::Clamp);
        edges->SetWrapY(gfx::Texture::Wrapping::Clamp);
        edges->SetGarbageCollection(true);
        edges->SetTransient(true);
    }
    DetectSpriteEdges(texture, edges, device, edge_color);
    CopyTexture(edges, texture, device);
}

void CopyTexture(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device, const glm::mat3& matrix)
{
    ASSERT(dst->GetFormat() == Texture::Format::RGBA ||
           dst->GetFormat() == Texture::Format::sRGBA);

    auto* fbo = device->FindFramebuffer("AlgoFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("AlgoFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }
    dst->SetFilter(gfx::Texture::MinFilter::Linear);
    dst->SetFilter(gfx::Texture::MagFilter::Linear);
    dst->SetWrapX(gfx::Texture::Wrapping::Clamp);
    dst->SetWrapY(gfx::Texture::Wrapping::Clamp);
    fbo->SetColorTarget(dst);

    constexpr auto* vertex_src = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;
uniform mat3 kTextureMatrix;
varying vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.xy, 0.0, 1.0);
  vTexCoord   = (kTextureMatrix * vec3(aTexCoord.xy, 1.0)).xy;
}
)";
    constexpr auto* fragment_src = R"(
#version 100
precision highp float;
varying vec2 vTexCoord;
uniform sampler2D kTexture;
void main() {
   gl_FragColor = texture2D(kTexture, vTexCoord);
}
)";

    auto program = device->FindProgram("CopyProgram");
    if (!program)
        program = MakeProgram(vertex_src, fragment_src, "CopyProgram", *device);

    program->SetUniform("kTextureMatrix", matrix);
    program->SetTexture("kTexture", 0, *src);
    program->SetTextureCount(1);

    auto quad = MakeFullscreenQuad(*device);
    const auto dst_width  = dst->GetWidthI();
    const auto dst_height = dst->GetHeightI();

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.depth_test   = gfx::Device::State::DepthTest::Disabled;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.culling      = gfx::Device::State::Culling::None;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.viewport     = gfx::IRect(0, 0, dst_width, dst_height);
    device->Draw(*program, *quad, state, fbo);

    fbo->SetColorTarget(nullptr);
}

void FlipTexture(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device, FlipDirection direction)
{
    const auto format = texture->GetFormat();

    // Currently, this is the only supported format due to limitations on the
    // GL ES2 FBO color buffer target.
    ASSERT(format == gfx::Texture::Format::RGBA ||
           format == gfx::Texture::Format::sRGBA);

    auto* fbo = device->FindFramebuffer("AlgoFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("AlgoFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }
    auto* tmp = device->FindTexture(gpu_id + "/tmp-color");
    if (!tmp)
    {
        tmp = device->MakeTexture(gpu_id + "/tmp-color");
        tmp->SetName("FlipTextureHelper");
        tmp->SetFilter(gfx::Texture::MinFilter::Linear);
        tmp->SetFilter(gfx::Texture::MagFilter::Linear);
        tmp->SetWrapX(gfx::Texture::Wrapping::Clamp);
        tmp->SetWrapY(gfx::Texture::Wrapping::Clamp);
        tmp->SetGarbageCollection(true);
        tmp->SetTransient(true);
    }

    const auto src_width  = texture->GetWidth();
    const auto src_height = texture->GetHeight();
    const auto tmp_width  = tmp->GetWidth();
    const auto tmp_height = tmp->GetHeight();
    if (tmp_width != src_width || tmp_height != src_height)
        tmp->Allocate(src_width, src_height, gfx::Texture::Format::sRGBA);

    // copy the contents from the source texture into the temp texture.
    CopyTexture(texture, tmp, device);

    if (direction == FlipDirection::Horizontal)
    {
        // copy the contents from the temp texture back into source texture
        // but with flipped texture coordinates.
        const auto mat = glm::mat3(glm::vec3(1.0f,  0.0f, 0.0f),
                                   glm::vec3(0.0f, -1.0f, 0.0f),
                                   glm::vec3(0.0f,  1.0f, 0.0f));
        CopyTexture(tmp, texture, device, mat);
    }
    else if (direction == FlipDirection::Vertical)
    {
        const auto mat = glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f),
                                   glm::vec3(0.0f,  1.0f, 0.0f),
                                   glm::vec3(1.0f,  0.0f, 0.0f));
        CopyTexture(tmp, texture, device, mat);
    }
}

std::unique_ptr<IBitmap> ReadTexture(const gfx::Texture* texture, gfx::Device* device)
{
    const auto format = texture->GetFormat();
    const auto width  = texture->GetWidth();
    const auto height = texture->GetHeight();

    // Currently, this is the only supported format due to limitations on the
    // GL ES2 FBO color buffer target.
    ASSERT(format == gfx::Texture::Format::RGBA ||
           format == gfx::Texture::Format::sRGBA);

    auto* fbo = device->FindFramebuffer("AlgoFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("AlgoFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }
    fbo->SetColorTarget(const_cast<gfx::Texture*>(texture));

    auto bmp = device->ReadColorBuffer(width, height, fbo);

    fbo->SetColorTarget(nullptr);

    return std::make_unique<RgbaBitmap>(std::move(bmp));
}

void ClearTexture(gfx::Texture* texture, gfx::Device* device, const gfx::Color4f& clear_color)
{
    const auto format = texture->GetFormat();
    const auto width  = texture->GetWidth();
    const auto height = texture->GetHeight();

    // Currently, this is the only supported format due to limitations on the
    // GL ES2 FBO color buffer target.
    ASSERT(format == gfx::Texture::Format::RGBA ||
           format == gfx::Texture::Format::sRGBA);

    auto* fbo = device->FindFramebuffer("AlgoFBO");
    if (!fbo)
    {
        fbo = device->MakeFramebuffer("AlgoFBO");
        Framebuffer::Config conf;
        conf.width  = 0; // irrelevant since using texture target
        conf.height = 0; // irrelevant since using texture target.
        conf.format = Framebuffer::Format::ColorRGBA8;
        fbo->SetConfig(conf);
    }
    fbo->SetColorTarget(texture);

    device->ClearColor(clear_color, fbo);

    fbo->SetColorTarget(nullptr);
}

} // namespace
