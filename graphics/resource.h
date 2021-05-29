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
#include <memory>

#include "graphics/types.h"

namespace gfx
{
    // Resource is the interface for accessing the actual
    // content/bytes of some graphics resource such as a
    // texture, font file etc.
    // The resource abstraction allows the resource to be
    // provided through several possible means, such as
    // loading it from the file system through file reads
    // or by for example memory mapping it.
    class Resource
    {
    public:
        virtual ~Resource() = default;
        // Get the immutable data content pointer.
        virtual const void* GetData() const = 0;
        // Get the size of the resource in bytes.
        virtual size_t GetSize() const = 0;
    private:
    };

    // ResourceLoader is the interface for accessing actual resources
    // such as textures (.png, .jpg), fonts (.ttf and .otf) and shader
    // (.glsl) files.
    // Currently it only provides functionality for mapping arbitrary
    // resource identifiers to actual locations on the file system.
    class ResourceLoader
    {
    public:
        // Expected Type of the resource to be loaded.
        enum class ResourceType {
            // Image file with the purpose of being used as a texture.
            Texture,
            // A glsl shader file (text)
            Shader,
            // Font (.otf) file.
            Font,
            // Compressed image such as .png or .jpeg
            Image
        };

        // Resolve the given resource URI to an actual filename on the file system.
        // Whenever the graphics system needs some resource such as a font file
        // or a texture file the resource is identified by an URI. Before the data
        // be loaded the URI needs to be mapped to an actual filename.
        // Resource type is the expected type of the resource in question.
        // The return value should be a path of the file in the file system.
        virtual std::string ResolveURI(ResourceType type, const std::string& URI) const = 0;

        // Load the contents of the given resource and return a pointer to the actual
        // contents of the resource. If the load fails a nullptr is returned.
        virtual std::shared_ptr<const Resource> LoadResource(ResourceType type, const std::string& URI);
    protected:
        ~ResourceLoader() = default;
    private:
    };

    // Collect and combine resources (such as textures, fonts
    // and shaders) into a single "package". What the package
    // means or how it's implemented is left for the implementation.
    // It is expected that the packing is a two step process.
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
    class ResourcePacker
    {
    public:
        // opaque handle type for identifying and mapping objects
        // to their resources.
        // The handle doesn't exist for the packer to gain any insight
        // into the objects that are performing packing but rather just
        // to let the objects identify their new resource (file) handles
        // after the packing has been completed.
        using ObjectHandle = const void*;
        // Pack the shader resource identified by file.
        virtual void PackShader(ObjectHandle instance, const std::string& file) = 0;
        // Pack the texture resource identified by file. Box is the actual
        // sub-rectangle within the texture object.
        virtual void PackTexture(ObjectHandle instance, const std::string& file) = 0;
        virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) = 0;

        enum class TextureFlags {
            // texture can be combined with other textures into a larger
            // texture file (atlas)
            CanCombine
        };
        // Set the texture flags that impact how the texture can be packed
        virtual void SetTextureFlag(ObjectHandle instance, TextureFlags flag, bool on_off) = 0;
        // Pack the font resource identified by file.
        virtual void PackFont(ObjectHandle instance, const std::string& file) = 0;

        // The resource packer may assign new URIs to the
        // resources that are packed. These API's are used to fetch
        // the new identifiers that will be used to identify the
        // resources after packing.
        virtual std::string GetPackedShaderId(ObjectHandle instance) const = 0;
        virtual std::string GetPackedTextureId(ObjectHandle instance) const = 0;
        virtual gfx::FRect  GetPackedTextureBox(ObjectHandle instance) const = 0;
        virtual std::string GetPackedFontId(ObjectHandle instance) const = 0;
    protected:
        ~ResourcePacker() = default;
    };

    // Set the global resource map object.
    // If nothing is ever set the mapping will be effectively
    // disabled and every resource handle maps to itself.
    void SetResourceLoader(ResourceLoader* map);

    // Get the current resource map.
    ResourceLoader* GetResourceLoader();

    // Shortcut for resolving a graphics resource URI to a filename on the
    // filesystem through the resource loader if any is set.
    // If resource loader is not set then returns the original URI.
    std::string ResolveURI(ResourceLoader::ResourceType type, const std::string& URI);

    // Shortcut for loading the contents of a file through a the resource loader if any is set.
    // If resource loader is not set then performs a default file loader operation.
    // Returns nullptr on any error.
    std::shared_ptr<const Resource> LoadResource(ResourceLoader::ResourceType type, const std::string& URI);
} // namespace
