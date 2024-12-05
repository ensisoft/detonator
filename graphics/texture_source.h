// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#include <memory>

#include "base/bitflag.h"
#include "base/utility.h"
#include "data/fwd.h"

namespace gfx
{
    class Device;
    class ShaderSource;
    class Texture;
    class TexturePacker;
    class IBitmap;

    // Interface for acquiring texture data. Possible implementations
    // might load the data from a file or generate it on the fly.
    class TextureSource
    {
    public:
        // Enum to specify what is the underlying data source for the texture data.
        enum class Source {
            // Data comes from a file (such as a .png or a .jpg) in the filesystem.
            Filesystem,
            // Data comes as an in memory bitmap rasterized by the TextBuffer
            // based on the text/font content/parameters.
            TextBuffer,
            // Data comes from a bitmap buffer.
            BitmapBuffer,
            // Data comes from a bitmap generator algorithm.
            BitmapGenerator,
            // Data is already a texture on the device
            Texture
        };
        enum class ColorSpace {
            Linear, sRGB
        };

        enum class Effect {
            Blur,
            Edges,
        };
        struct Environment {
            bool dynamic_content = false;
        };

        virtual ~TextureSource() = default;
        // Get the color space of the texture source's texture content
        virtual ColorSpace GetColorSpace() const
        { return ColorSpace::Linear; }
        // Get the texture effect if any on the texture.
        virtual base::bitflag<Effect> GetEffects() const
        { return base::bitflag<Effect>(0); }
        // Get the type of the source of the texture data.
        virtual Source GetSourceType() const = 0;
        // Get the texture ID on the GPU, i.e. the ID that uniquely
        // identifies the texture object ont he GPU.
        virtual std::string GetGpuId() const = 0;
        // Get texture source class/resource id. This is *this*
        // object ID. Not to be confused with the GpuID
        virtual std::string GetId() const = 0;
        // Get the human-readable / and settable name.
        virtual std::string GetName() const = 0;
        // Get the texture source hash value based on the properties
        // of the texture source object itself *and* its content.
        virtual std::size_t GetHash() const = 0;
        // Set the texture source human-readable name.
        virtual void SetName(const std::string& name) = 0;
        // Set a texture effect on/off on the texture.
        virtual void SetEffect(Effect effect, bool on_off) {}
        // Generate or load the data as a bitmap. If there's a content
        // error this function should return empty shared pointer.
        // The returned bitmap can be potentially immutably shared.
        virtual std::shared_ptr<IBitmap> GetData() const = 0;
        // Create a texture out of the texture source on the device.
        // Returns a texture object on success of nullptr on error.
        virtual Texture* Upload(const Environment& env, Device& device) const = 0;
        // Serialize into JSON object.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Begin packing the texture source into the packer.
        virtual void BeginPacking(TexturePacker* packer) const {}
        // Finish packing the texture source into the packer.
        // Update the state with the details from the packer.
        virtual void FinishPacking(const TexturePacker* packer) {}

        // Create a similar clone of this texture source but with a new unique ID.
        inline std::unique_ptr<TextureSource> Clone()
        {
            return this->MakeCopy(base::RandomString(10));
        }
        // Create an exact bitwise copy of this texture source object.
        inline std::unique_ptr<TextureSource> Copy()
        {
            return this->MakeCopy(GetId());
        }
        inline bool TestEffect(Effect effect) const
        { return GetEffects().test(effect); }

    protected:
        virtual std::unique_ptr<TextureSource> MakeCopy(std::string copy_id) const = 0;
    private:
    };


} // namespace