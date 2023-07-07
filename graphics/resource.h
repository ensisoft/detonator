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
    // Resource is the interface for accessing the actual
    // bits and bytes  of some graphics resource such as a
    // texture, font file etc.
    // The resource abstraction allows the resource to be
    // provided through several possible means, such as
    // loading it from the file system through file reads
    // or by for example memory mapping it.
    class Resource
    {
    public:
        // Expected Type of the resource to be loaded.
        enum class Type {
            // Image file with the purpose of being used as a texture.
            Texture,
            // A glsl shader file (text)
            Shader,
            // Font (.otf) file.
            Font,
            // Compressed image such as .png or .jpeg
            Image
        };
        virtual ~Resource() = default;
        // Get the immutable data content pointer.
        virtual const void* GetData() const = 0;
        // Get the size of the resource in bytes.
        virtual std::size_t GetByteSize() const = 0;
        virtual std::string GetSourceName() const = 0;
    private:
    };


} // namespace
