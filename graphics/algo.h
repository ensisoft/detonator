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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/mat3x3.hpp>
#include "warnpop.h"

#include <string>
#include <memory>

#include "graphics/bitmap.h"

namespace gfx
{
class Device;
class Texture;
namespace algo {
enum class BlurDirection {
    BiDirectional,
    Horizontal,
    Vertical
};


// Extract source color values that exceed the threshold magnitude on the color axis.
// This is the basis of bloom effect. Extract the color map, then blur it it and blend
// with the source image.
void ExtractColor(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device,
                  const gfx::Color4f& color, float threshold);

void ExtractColor(const gfx::Texture* src, gfx::Texture** dst, gfx::Device* device,
                  const gfx::Color4f& color, float threshold);

// Create a color texture from alpha texture. In other words expand a texture which originally
// only has alpha channel into a RGBA texture while keeping it as a "logical alpha" mask
void ColorTextureFromAlpha(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device);

// Apply a blur kernel on the texture. The input texture is used both as a source
// and as destination for rendering while doing multiple blur passes (defined by iterations).
// Important requirements for the texture.
// * The texture must have RGBA format.
// * The texture must use filtering mode that doesn't require mips.
// Iterations must be an even number, otherwise the result of the last blurring pass
// will not be in the input texture.
void ApplyBlur(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device,
    unsigned iterations = 4, BlurDirection direction = BlurDirection::BiDirectional);

void DetectSpriteEdges(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device,
                       const gfx::Color4f& edge_color = gfx::Color::White);
void DetectSpriteEdges(const std::string& gpu_id,  gfx::Texture* texture, gfx::Device* device,
                       const gfx::Color4f& edge_color = gfx::Color::White);

void CopyTexture(const gfx::Texture* src, gfx::Texture* dst, gfx::Device* device, const glm::mat3& matrix = glm::mat3(1.0f));

enum class FlipDirection {
    Horizontal,
    Vertical
};
void FlipTexture(const std::string& gpu_id, gfx::Texture* texture, gfx::Device* device, FlipDirection direction);

std::unique_ptr<IBitmap> ReadTexture(const gfx::Texture* texture, gfx::Device* device);

void ClearTexture(gfx::Texture* texture, gfx::Device* device, const gfx::Color4f& clear_color);


} // namespace
} // namespace
