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

        // Resolve the given (pseudo) filename to an actual filename on the file system.
        // Whenever the graphics system needs some resource such as a font file
        // or a texture file the file path can be some arbitrary string that doesn't
        // need to be an actual file system path. It can be a abstract identifier
        // or a pseudo path or some URI type encoded file/resource locator.
        // When the resource actually is needed to be loaded this method will be invoked
        // in order to map the given file to an actual file path accessible
        // in the file system.
        // Resource type is the expected type of the resource in question.
        virtual std::string ResolveFile(ResourceType type, const std::string& file) const = 0;

        // todo: we could conceivably enhance this interface with some actual
        // methods for loading the data as well. This would allow for more
        // complicated resource packing schemes.
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
        // opaque handle type for idetifying and mapping objects
        // to their resources.
        // The handle doesn't exist for the packer to gain any insight
        // into the objects that are performing packing but rather just
        // to let the objects identify their new resource (file) handles
        // after the packing has been completed.
        using ObjectHandle = const void*;
        // Pack the shader resource identified by file.
        virtual void PackShader(ObjectHandle instance, const std::string& file) = 0;
        // Pack the texture resource identified by file. Box is the actual
        // subrectangle within the texture object.
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

        // The resource pack my assign new (file) identifiers to the
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
    // disabled and every resourcde handle maps to itself.
    void SetResourceLoader(ResourceLoader* map);

    // Get the current resource map.
    ResourceLoader* GetResourceLoader();

    // Shortcut for mapping a file path through the resource map if any is set.
    // If resource map is not set then returns the original file name.
    inline std::string ResolveFile(ResourceLoader::ResourceType type, const std::string& file)
    {
        if (ResourceLoader* loader = GetResourceLoader())
            return loader->ResolveFile(type, file);
        return file;
    }

} // namespace
