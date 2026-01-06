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
#include <vector>

namespace gfx
{
    class Shader
    {
    public:
        enum class UniformType {
            UniformBlock,
            Sampler2D,
            Sampler2DArray
        };

        struct UniformInfo {
            UniformType type;
            std::string name;
        };

        struct CreateArgs {
            // HUman-readable name for the shader. Useful for debugging and
            // error diagnostics.
            std::string name;
            // The GLSL source for the shader.
            std::string source;
            // This is a debug feature to let the program know of
            // expected uniform blocks that need to be bound in the
            // program state when the program is used to draw.
            // If these are not bound there will likely be a
            // GL_INVALID_OPERATION from a some draw call.
            std::vector<UniformInfo> uniform_info;
            // Flag to control whether the shader related functions
            // should produce debug logs or not. This flag also
            // controls dumping the source to debug log on compile.
            bool debug = false;
            // Flag that indicates the shader is a fallback shader
            // that replaces a user defined shader when the user defined
            // shader source has failed to load.
            // A fallback shader is not really meant to be used, it
            // only exists as a valid shader object to indicate the
            // failure of some user defined shader.
            bool fallback = false;
            // Human-readable information related to why this shader is
            // flagged as a fallback shader. This is optional and could
            // be an empty string.
            std::string fallback_info;
        };

        virtual ~Shader() = default;
        // Returns true if the shader has been compiled successfully.
        virtual bool IsValid() const = 0;
        // Returns true if the shader is a fallback shader for some user
        // defined shader that has failed to load.
        virtual bool IsFallback() const { return false; }
        // Get the (human-readable) name for the shader object.
        // Used for improved debug/log messages.
        virtual std::string GetName() const { return ""; }
        // Get the (human-readable) shader (compile) error string if any.
        virtual std::string GetCompileInfo() const { return ""; };
        // Get the (human-readable) shader fallback info (error) if any.
        // The fallback info is passed in the CreateArgs during shader construction
        // and is used to store the information about why some user defined
        // shader could not be used.
        virtual std::string GetFallbackInfo() const { return ""; }
    private:
    };

    using ShaderPtr = std::shared_ptr<const Shader>;

} // namespace
