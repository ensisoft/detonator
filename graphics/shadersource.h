// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <variant>

#include "graphics/color4f.h"

namespace gfx
{
    class ShaderSource
    {
    public:
        enum class Type {
            NotSet, Vertex, Fragment
        };

        enum class Version {
            NotSet,
            GLSL_100,
            GLSL_300
        };
        enum class Precision {
            NotSet, Low, Medium, High
        };
        explicit ShaderSource(std::string source)
        {
            mSnippets.push_back(std::move(source));
        }
        ShaderSource() = default;

        inline void SetType(Type type) noexcept
        { mType = type; }
        inline void SetPrecision(Precision precision) noexcept
        { mPrecision = precision; }
        inline void SetVersion(Version version) noexcept
        { mVersion = version; }
        inline void AddSource(std::string source)
        { mSnippets.push_back(std::move(source)); }
        inline bool IsEmpty() const noexcept
        { return mSnippets.empty(); }
        inline size_t GetSnippetCount() const noexcept
        { return mSnippets.size(); }
        inline std::string GetSnippet(size_t index) const noexcept
        { return base::SafeIndex(mSnippets, index); }
        inline void ClearSnippets() noexcept
        { mSnippets.clear(); }

        // Get the actual shader source string by combining
        // the shader source object's contents (i.e. data
        // declarations and source code snippets) together.
        std::string GetSource() const;

        // Merge the contents of the other shader source with this
        // shader source. The other shader source object must
        // be compatible with this shader source.
        void Merge(const ShaderSource& other);

        // Check whether this shader source object is compatible
        // with the other shader source, i.e. the shader type,
        // version and precision qualifiers match.
        bool IsCompatible(const ShaderSource& other) const noexcept;
    private:
        Type mType = Type::NotSet;
        Version mVersion = Version::NotSet;
        Precision mPrecision = Precision::NotSet;
        std::vector<std::string> mSnippets;
    };
} // namespace
