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

namespace gfx
{
    class Shader
    {
    public:
        virtual ~Shader() = default;
        // Compile the shader from the given source string.
        // Returns true if compilation was successful, otherwise
        // returns false.
        // If the compilation was not successful the state of the
        // shader does not change, i.e. the shader will retain
        // any previously compiled state.
        virtual bool CompileSource(const std::string& source) = 0;
        // Returns true if the shader has been compiled successfully.
        virtual bool IsValid() const = 0;
        // Set a (human-readable) name for the shader object.
        // Used for improved debug/log messages.
        virtual void SetName(const std::string& name) {}
        // Get the (human-readable) name for the shader object.
        // Used for improved debug/log messages.
        virtual std::string GetName() const { return ""; }
    private:
    };

} // namespace
