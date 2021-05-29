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

#pragma once

#include "config.h"
#include "base/assert.h"

namespace gfx
{
    class Texture
    {
    public:
        virtual ~Texture() = default;

        enum class Format {
            RGB, RGBA, Grayscale
        };
        // Texture minifying filter is used whenever the
        // pixel being textured maps to an area greater than
        // one texture element.
        enum class MinFilter {
            // Use the default filtering set for the device.
            Default,
            // Use the texture element nearest to the
            // center of the pixel (Manhattan distance)
            Nearest,
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear,
            // Use mips (precomputed) minified textures.
            // Use the nearest texture element from the nearest
            // mipmap level
            Mipmap,
            // Use mips (precomputed) minified textures).
            // Use the weighted average of the four texture
            // elements that are sampled from the closest mipmap level.
            Bilinear,
            // Use mips (precomputed minified textures).
            // Use the weighted average of the four texture
            // elements that are sampled from the two nearest mipmap levels.
            Trilinear
        };

        // Texture magnifying filter is used whenever the
        // pixel being textured maps to to an area less than
        // one texture element.
        enum class MagFilter {
            // Use the default filtering set for the device.
            Default,
            // Use the texture element nearest to the center
            // of the pixel. (Manhattan distance).
            Nearest,
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear
        };

        // Texture wrapping options for how to deal with
        // texture coords outside of [0,1] range,
        enum class Wrapping {
            // Clamp the texture coordinate to the boundary.
            Clamp,
            // Wrap the coordinate by ignoring the integer part.
            Repeat
        };

        // Identify texture format based on the bit depth
        static Format DepthToFormat(unsigned bit_depth)
        {
            if (bit_depth == 8)
                return Format::Grayscale;
            else if (bit_depth == 24)
                return Format::RGB;
            else if (bit_depth == 32)
                return Format::RGBA;
            // this function is only valid for the above bit depths.
            // everything else is considered a bug.
            // when reading data from external sources validation
            // of expected formats needs to be done elsewhere.
            BUG("Unexpected bit depth.");
        }

        // Set texture minification filter.
        virtual void SetFilter(MinFilter filter) = 0;
        // Set texture magnification filter.
        virtual void SetFilter(MagFilter filter) = 0;
        // Get current texture minification filter.
        virtual MinFilter GetMinFilter() const = 0;
        // Get current texture magnification filter.
        virtual MagFilter GetMagFilter() const = 0;
        // Set texture coordinate wrapping behaviour on X axis.
        virtual void SetWrapX(Wrapping w) = 0;
        // Set texture coordinate wrapping behaviour on Y axis.
        virtual void SetWrapY(Wrapping w) = 0;
        // Get current texture coordinate wrapping behaviour on X axis.
        virtual Wrapping GetWrapX() const = 0;
        // Get current texture coordinate wrapping behaviour on Y axis.
        virtual Wrapping GetWrapY() const = 0;
        // Upload the texture contents from the given buffer.
        // Will overwrite any previous contents and reshape the texture dimensions.
        virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) = 0;
        // Get the texture width. Initially 0 until Upload is called
        // and new texture contents are uploaded.
        virtual unsigned GetWidth() const = 0;
        // Get the texture height. Initially 0 until Upload is called
        // and texture contents are uploaded.
        virtual unsigned GetHeight() const = 0;
        // Get the texture format.
        virtual Format GetFormat() const = 0;
        // Enable or disable this texture from being garbage collected.
        // When this is enabled the texture may be deleted if it goes
        // unused long enough.
        // The default state is false. I.e. no garbage collection.
        virtual void EnableGarbageCollection(bool gc) = 0;
    protected:
    private:
    };


} // namespace