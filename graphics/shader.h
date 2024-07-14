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

namespace gfx
{
    class Shader
    {
    public:
        struct CreateArgs {
            std::string name;
            std::string source;
        };

        virtual ~Shader() = default;
        // Returns true if the shader has been compiled successfully.
        virtual bool IsValid() const = 0;
        // Get the (human-readable) name for the shader object.
        // Used for improved debug/log messages.
        virtual std::string GetName() const { return ""; }
        // Get the (human-readable) shader (compile) error string if any.
        virtual std::string GetError() const { return ""; };
    private:
    };

    using ShaderPtr = std::shared_ptr<const Shader>;

} // namespace
