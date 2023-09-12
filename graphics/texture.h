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

#include <string>

namespace gfx
{
    class Texture
    {
    public:
        virtual ~Texture() = default;

        // Flags controlling texture usage and lifetime.
        enum class Flags {
            // Transient textures are used temporarily for a short period
            // of time to display for example rasterized text. Default is false.
            Transient,
            // Flag to control whether the (non-transient) texture can be
            // ever be garbage collected or not. Default is true.
            GarbageCollect,
            // Logical alpha mask flag to indicate that the texture should only
            // be used as an alpha mask even though it has RGA format.
            AlphaMask
        };

        enum class Format {
            // non-linear sRGB(A) encoded RGB data.
            sRGB,
            sRGBA,
            // linear RGB(A) data.
            RGB,
            RGBA,
            // 8bit linear alpha mask
            AlphaMask
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
        // pixel being textured maps to an area less than
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
        // texture coordinates outside of [0,1] range,
        enum class Wrapping {
            // Clamp the texture coordinate to the boundary.
            Clamp,
            // Wrap the coordinate by ignoring the integer part.
            Repeat
        };

        // Identify texture format based on the bit depth
        static Format DepthToFormat(unsigned bit_depth, bool srgb)
        {
            if (bit_depth == 8)
                return Format::AlphaMask;
            else if (bit_depth == 24 && srgb)
                return Format::sRGB;
            else if (bit_depth == 24)
                return Format::RGB;
            else if (bit_depth == 32 && srgb)
                return Format::sRGBA;
            else if (bit_depth == 32)
                return Format::RGBA;
            // this function is only valid for the above bit depths.
            // everything else is considered a bug.
            // when reading data from external sources validation
            // of expected formats needs to be done elsewhere.
            BUG("Unexpected bit depth.");
        }

        // Set a texture flag to control texture behaviour.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
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
        // Upload the texture contents from the given CPU side buffer.
        // This will overwrite any previous contents and reshape the texture dimensions.
        // If mips is false (no mipmap generation) the texture minification filter
        // must be set not to use any mips either.
        virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format, bool mips=true) = 0;
        // Get the texture width. Initially 0 until Upload is called
        // and new texture contents are uploaded.
        virtual unsigned GetWidth() const = 0;
        // Get the texture height. Initially 0 until Upload is called
        // and texture contents are uploaded.
        virtual unsigned GetHeight() const = 0;
        // Get the texture format.
        virtual Format GetFormat() const = 0;
        // Set the hash value that identifies the data.
        virtual void SetContentHash(size_t hash) = 0;
        // Get the hash value that was used in the latest data upload.
        virtual size_t GetContentHash() const = 0;
        // Set a (human-readable) name for the texture object.
        // Used for improved debug/log messages.
        virtual void SetName(const std::string& name) = 0;
        // Set the group id used to identify a set of textures that
        // conceptually belong together, for example to a sprite batch.
        virtual void SetGroup(const std::string& name) = 0;
        // Test whether a flag is on or off.
        virtual bool TestFlag(Flags flag) const = 0;
        // Try to generate texture mip maps based on a previously provided texture data.
        // This is mostly useful for generating mips after using the texture as render target.
        // Several constraints on the implementation might prohibit the mip map generation.
        // In such case the function returns false and the texture will not have any mips.
        // The caller needs to make sure to deal with the situation, i.e. using a texture
        // filtering mode that requires no mips.
        virtual bool GenerateMips() = 0;
        // Check whether the texture has mip maps or not.
        virtual bool HasMips() const = 0;

        // helpers.
        inline void SetTransient(bool on_off)
        { SetFlag(Flags::Transient, on_off); }
        inline void SetGarbageCollection(bool on_off)
        { SetFlag(Flags::GarbageCollect, on_off); }

        inline bool IsTransient() const noexcept
        { return TestFlag(Flags::Transient); }
        inline bool GarbageCollect() const noexcept
        { return TestFlag(Flags::GarbageCollect); }
        // Check whether the texture is an alpha mask (and should be used as one)
        // even if the underlying pixel format isn't
        inline bool IsAlphaMask() const noexcept
        {
            if (GetFormat() == Format::AlphaMask || TestFlag(Flags::AlphaMask))
                return true;
            return false;
        }

        // Allocate texture storage based on the texture format and dimensions.
        // The contents of the texture are unspecified and any previous contents
        // are no longer valid/available. The primary use case for this method is
        // to be able to allocate texture storage for using the texture as a render
        // target when rendering to an FBO. Any mipmap generation must then be
        // performed later after the rendering has completed or then the filtering
        // mode must be set not to use mips either.
        void Allocate(unsigned width, unsigned height, Format format)
        { Upload(nullptr, width, height, format, false); }

    protected:
    private:
    };


} // namespace
