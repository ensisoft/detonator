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
            std::string name;
            std::string source;
            // This is a debug feature to let the program know of
            // expected uniform blocks that need to be bound in the
            // program state when the program is used to draw.
            // If these are not bound there will likely be a
            // GL_INVALID_OPERATION from a some draw call.
            std::vector<UniformInfo> uniform_info;
            bool debug = false;
        };

        virtual ~Shader() = default;
        // Returns true if the shader has been compiled successfully.
        virtual bool IsValid() const = 0;
        // Get the (human-readable) name for the shader object.
        // Used for improved debug/log messages.
        virtual std::string GetName() const { return ""; }
        // Get the (human-readable) shader (compile) error string if any.
        virtual std::string GetCompileInfo() const { return ""; };

    private:
    };

    using ShaderPtr = std::shared_ptr<const Shader>;

} // namespace
