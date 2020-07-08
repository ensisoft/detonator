// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "types.h"

#include <string>

namespace gfx
{
    // ResourceMap maps resources to alternative locations and resources
    // for example mapping paths with special prefixes to actual file
    // system paths or in fact to completely different files.
    // The interesting resources currently are:
    // Texture files (.png, .jpg etc) files that are loaded by material.
    // Font files (.ttf, .otf) that are loaded by TextBuffer.
    // Shader (.glsl) files that are loaded by material/graphics device.
    // There can also be some additional data for example for textures
    // we might want to map a resource to some subregion of some other
    // texture (in case of so called texture atlas)
    class ResourceMap
    {
    public:
        enum class ResourceType {
            Texture,
            Shader,
            Font
        };
        // Map a filename to some actual resource.
        // The resource in question can be any of the resources mentioned
        // previously (texture, font, shader)
        virtual std::string MapFilePath(ResourceType type, const std::string& file) const = 0;

        // Map the given (normalized) texture box to a texture box in the actual
        // resource (texture file) returned by MapFilePath.
        // The resource is identified by the name "unique_file" which is the the
        // original name of the file as is known before mapping.
        virtual FRect MapTextureBox(const std::string& mapped_file,
                                    const std::string& unique_file,
                                    const FRect& box) const = 0;

        // todo: we could conceivably enhance this interface with some actual
        // methods for loading the data as well. This would allow for more
        // complicated resource packing schemes.
    private:
    };

    // Set the global resource map object.
    // If nothing is ever set the mapping will be effectively
    // disabled and every resourcde handle maps to itself.
    void SetResourceMap(ResourceMap* map);

    // Get the current resource map.
    ResourceMap* GetResourceMap();

    // Shortcut for mapping a file path through the resource map if any is set.
    // If resource map is not set then returns the original file name.
    inline std::string MapFilePath(ResourceMap::ResourceType type, const std::string& file)
    {
        if (ResourceMap* map = GetResourceMap())
            return map->MapFilePath(type, file);
        return file;
    }

    // Shortcut for mapping a texture box through the resource map if any is set.
    // If resource map is not set then returns the original texture box.
    inline FRect MapTextureBox(const std::string& mapped_file,
                               const std::string& unique_file,
                               const FRect& box)
    {
        if (ResourceMap* map = GetResourceMap())
            return map->MapTextureBox(mapped_file, unique_file, box);
        return box;
    }

} // namespace
