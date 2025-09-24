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

#include "device/enum.h"

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

        using Format = dev::TextureFormat;
        using MinFilter = dev::TextureMinFilter;
        using MagFilter = dev::TextureMagFilter;
        using Wrapping = dev::TextureWrapping;

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
        virtual void Upload(const void* bytes, unsigned width, unsigned height, Format format) = 0;
        // Allocate texture storage based on the texture format and dimensions.
        // The contents of the texture are unspecified and any previous contents
        // are no longer valid/available. The primary use case for this method is
        // to be able to allocate texture storage for using the texture as a render
        // target when rendering to an FBO. Any mipmap generation must then be
        // performed later after the rendering has completed or then the filtering
        // mode must be set not to use mips either.
        virtual void Allocate(unsigned width, unsigned height, Format format) = 0;
        // Allocate an array of textures of specified width, height and format.
        // The contents of the texture are unspecified and any previous contents
        // are no longer valid/available. The primary use case for this method is
        // to allocate an array of shadow map textures.
        virtual void AllocateArray(unsigned width, unsigned height, unsigned array_size, Format format) = 0;
        // Get the texture width. Initially 0 until Upload is called
        // and new texture contents are uploaded.
        virtual unsigned GetWidth() const = 0;
        // Get the texture height. Initially 0 until Upload is called
        // and texture contents are uploaded.
        virtual unsigned GetHeight() const = 0;
        // Get the number of textures in a texture array allocated by a previous call
        // to  AllocateArray. If the texture is not an array then this will return 0.
        virtual unsigned GetArraySize() const = 0;
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
        // Get the (human-readable) name given for the texture object.
        virtual std::string GetName() const = 0;
        // Get the name of the texture group this texture belongs to (if any)
        virtual std::string GetGroup() const = 0;
        // Get the texture GPU resource ID used to create the texture.
        virtual std::string GetId() const = 0;

        // helpers.
        auto GetWidthF() const noexcept
        { return static_cast<float>(GetWidth()); }
        auto GetHeightF() const noexcept
        { return static_cast<float>(GetHeight()); }
        auto GetWidthI() const noexcept
        { return static_cast<int>(GetWidth()); }
        auto GetHeightI() const noexcept
        { return static_cast<int>(GetHeight()); }

        void SetTransient(bool on_off)
        { SetFlag(Flags::Transient, on_off); }
        void SetGarbageCollection(bool on_off)
        { SetFlag(Flags::GarbageCollect, on_off); }

        auto IsTransient() const noexcept
        { return TestFlag(Flags::Transient); }
        auto GarbageCollect() const noexcept
        { return TestFlag(Flags::GarbageCollect); }
        // Check whether the texture is an alpha mask (and should be used as one)
        // even if the underlying pixel format isn't
        auto IsAlphaMask() const noexcept
        {
            if (GetFormat() == Format::AlphaMask || TestFlag(Flags::AlphaMask))
                return true;
            return false;
        }
        auto HasSize() const noexcept
        {
            return GetWidth() && GetHeight();
        }


    protected:
    private:
    };


} // namespace
