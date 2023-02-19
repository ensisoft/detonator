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

#include <algorithm>

#include "base/assert.h"
#include "graphics/algo.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/texture.h"
#include "graphics/framebuffer.h"
#include "graphics/help.h"

namespace gfx {
namespace algo {
void ApplyBlur(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device,
    unsigned iterations, BlurDirection direction)
{
    const auto has_mips   = texture->HasMips();
    const auto min_filter = texture->GetMinFilter();
    const auto tex_format = texture->GetFormat();

    // Currently, this is the only supported format due to limitations on the
    // GL ES2 FBO color buffer target.
    ASSERT(tex_format == gfx::Texture::Format::RGBA);

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
        tmp->SetGarbageCollection(texture->GarbageCollect());
        tmp->SetTransient(texture->IsTransient());
    }

    const auto src_width  = texture->GetWidth();
    const auto src_height = texture->GetHeight();
    const auto tmp_width  = tmp->GetWidth();
    const auto tmp_height = tmp->GetHeight();
    if (tmp_width != src_width || tmp_height != src_height)
        tmp->Allocate(src_width, src_height, gfx::Texture::Format::RGBA);

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
    constexpr auto* fragment_src = R"(
#version 100
precision highp float;
varying vec2 vTexCoord;
uniform vec2 kTextureSize;
uniform sampler2D kTexture;

uniform int kDirection;

void main() {
  vec2 texel_size = vec2(1.0, 1.0) / vec2(256.0);

  // The total sum of weights w=weights[0] + 2*(weights[1] + weights[2] + ... weights[n])
  // W should then be approximately 1.0
  float weights[5];
  weights[0] = 0.227027;
  weights[1] = 0.1945946;
  weights[2] = 0.1216216;
  weights[3] = 0.054054;
  weights[4] = 0.016216;

  vec4 color = texture2D(kTexture, vTexCoord) * weights[0];

  if (kDirection == 0)
  {
    for (int i=1; i<5; ++i)
    {
      float left  = vTexCoord.x - float(i) * texel_size.x;
      float right = vTexCoord.x + float(i) * texel_size.x;
      float y = vTexCoord.y;
      vec4 left_sample  = texture2D(kTexture, vec2(left, y)) * weights[i];
      vec4 right_sample = texture2D(kTexture, vec2(right, y)) * weights[i];
      color += left_sample;
      color += right_sample;
    }
  }
  else if (kDirection == 1)
  {
    for (int i=1; i<5; ++i)
    {
      float below = vTexCoord.y - float(i) * texel_size.y;
      float above = vTexCoord.y + float(i) * texel_size.y;
      float x = vTexCoord.x;
      vec4 below_sample = texture2D(kTexture, vec2(x, below)) * weights[i];
      vec4 above_sample = texture2D(kTexture, vec2(x, above)) * weights[i];
      color += below_sample;
      color += above_sample;
    }
  }
  gl_FragColor = color;
}

)";
    auto* program = device->FindProgram("BlurProgram");
    if (!program)
        program = MakeProgram(vertex_src, fragment_src, "BlurProgram", *device);

    auto* quad = gfx::MakeFullscreenQuad(*device);

    AutoFBO change(*device);

    fbo->SetColorTarget(tmp);
    device->SetFramebuffer(fbo);

    gfx::Device::State state;
    state.bWriteColor  = true;
    state.premulalpha  = false;
    state.depth_test   = gfx::Device::State::DepthTest::Disabled;
    state.stencil_func = gfx::Device::State::StencilFunc::Disabled;
    state.culling      = gfx::Device::State::Culling::None;
    state.blending     = gfx::Device::State::BlendOp::None;
    state.viewport     = gfx::IRect(0, 0, src_width, src_height);

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
        device->Draw(*program, *quad, state);

        std::swap(textures[0], textures[1]);
    }

    fbo->SetColorTarget(nullptr);
}
} // namespace
} // namespace
