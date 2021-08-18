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

#include <memory>

#include "graphics/resource.h"

namespace gfx
{
    using ResourceHandle = std::shared_ptr<const Resource>;
    using ResourceType   = Resource::Type;

    // Loader is the interface for accessing actual gfx resources
    // such as textures (.png, .jpg), fonts (.ttf and .otf) and shader
    // (.glsl) files.
    class Loader
    {
    public:
        using ResourceHandle = gfx::ResourceHandle;
        virtual ~Loader() = default;
        // Load the contents of the given resource and return a pointer to the actual
        // contents of the resource. If the load fails a nullptr is returned.
        virtual ResourceHandle LoadResource(const std::string& URI) = 0;
    protected:
    private:
    };

    // Set the global graphics resource loader.
    // If nothing is ever set the default built loader will be used.
    // This will expect the resource URIs to be file paths usable as-is.
    void SetResourceLoader(Loader* loader);
    // Get the current resource loader if any.
    Loader* GetResourceLoader();
    // Shortcut for loading the contents of a file through a the resource loader if any is set.
    // If resource loader is not set then performs a default file loader operation.
    // Returns nullptr on any error.
    ResourceHandle LoadResource(const std::string& URI);

} // namespace
