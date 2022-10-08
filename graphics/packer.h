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

#include "graphics/types.h"

namespace gfx
{
    // Collect and combine texture resources  into a single "package".
    // What the package means or how it's implemented is left for the
    // implementation. It is expected that the packing is a two-step process.
    // First the graphics objects are visited in some order/manner
    // and each object that supports/needs packing will call the appropriate
    // packing function for the type of resource they need.
    // Once all the objects have been visited the resource packer will
    // perform whatever packing functions it wants, possibly combining
    // data by for example building texture atlasses and/or copying
    // files from one location to another.
    // After this processing is done the graphics objects are visited
    // again and this time they will request back the information that
    // is then used to identify their resources in the packed form.
    class TexturePacker
    {
    public:
        // opaque handle type for identifying and mapping objects
        // to their resources.
        // The handle doesn't exist for the packer to gain any insight
        // into the objects that are performing packing but rather just
        // to let the objects identify their new resource (file) handles
        // after the packing has been completed.
        using ObjectHandle = const void*;
        // Pack the texture resource identified by file. Box is the actual
        // sub-rectangle within the texture object.
        virtual void PackTexture(ObjectHandle instance, const std::string& file) = 0;
        virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) = 0;

        enum class TextureFlags {
            // texture can be combined with other textures into a larger texture file (atlas)
            CanCombine,
            // Texture flags allow resizing.
            AllowedToResize,
            // Texture flags allow packing/combining.
            AllowedToPack
        };
        // Set the texture flags that impact how the texture can be packed
        virtual void SetTextureFlag(ObjectHandle instance, TextureFlags flag, bool on_off) = 0;
        // The resource packer may assign new URIs to the
        // resources that are packed. These APIs are used to fetch
        // the new identifiers that will be used to identify the
        // resources after packing.
        virtual std::string GetPackedTextureId(ObjectHandle instance) const = 0;
        virtual gfx::FRect  GetPackedTextureBox(ObjectHandle instance) const = 0;
    protected:
        ~TexturePacker() = default;
    };

} // namespace
