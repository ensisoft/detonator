// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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

namespace gfx
{
    class Texture;

    class Framebuffer
    {
    public:
        virtual ~Framebuffer() = default;
        // Each Format specifies the logical buffers and their bitwise representations.
        enum class Format {
            // RGBA color buffer with 8bits (unsigned) per channel.
            ColorRGBA8,
            // RGBA color buffer with 8bits (unsigned) per channel with 16bit depth buffer.
            ColorRGBA8_Depth16,
            // RGBA color buffer with 8bits (unsigned) per channel with 24bit depth buffer and 8bit stencil buffer.
            ColorRGBA8_Depth24_Stencil8
        };

        // Framebuffer configuration.
        struct Config {
            Format format = Format::ColorRGBA8;
            // The width of the fbo in pixels.
            unsigned width = 0;
            // The height of the fbo in pixels.
            unsigned height = 0;
        };
        // Set the framebuffer configuration that will be used when drawing.
        virtual void SetConfig(const Config& conf) = 0;
        // Set the color buffer texture target. If this is not set when the
        // FBO is used to render one is created for you based on the width/height
        // set in the FBO config.
        // The texture format must match the FBO config, i.e. the dimension
        // of any other buffers and the configured color format.
        virtual void SetColorTarget(Texture* texture) = 0;
        // Set the MSAA resolve texture target. If this is not set when the
        // FBO is used to render one is created for you based on the width/height
        // set in the FBO config.
        virtual void SetResolveTarget(Texture* texture) = 0;
        // Resolve the framebuffer color buffer contents into a texture that can be
        // used to sample the rendered image.
        virtual void Resolve(Texture** color) const = 0;
        // Get the framebuffer width in pixels.
        virtual unsigned GetWidth() const = 0;
        // Get the framebuffer height in pixels.
        virtual unsigned GetHeight() const = 0;
        // Get the framebuffer format.
        virtual Format GetFormat() const = 0;
    private:
    };
} // namespace
