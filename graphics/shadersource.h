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
#  include <glm/mat2x2.hpp>
#  include <glm/mat3x3.hpp>
#  include <glm/mat4x4.hpp>
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
        enum class ShaderDataType {
            Int,
            Float,
            Vec2f, Vec3f, Vec4f,
            Vec2i, Vec3i, Vec4i,
            Mat2f, Mat3f, Mat4f,
            Color4f,
            Sampler2D
        };
        using AttributeType = ShaderDataType;
        using UniformType   = ShaderDataType;
        using VaryingType   = ShaderDataType;
        using ConstantType  = ShaderDataType;

        enum class ShaderDataDeclarationType {
            Attribute, Uniform, Varying, Constant
        };

        using ShaderDataDeclarationValue = std::variant<
                int,  float,
                Color4f,
                glm::vec2, glm::vec3, glm::vec4,
                glm::ivec2, glm::ivec3, glm::ivec4,
                glm::mat2, glm::mat3, glm::mat4>;

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

        struct ShaderDataDeclaration {
            ShaderDataDeclarationType decl_type = ShaderDataDeclarationType::Attribute;
            ShaderDataType data_type = ShaderDataType::Float;
            std::string name;
            std::optional<ShaderDataDeclarationValue> constant_value;
        };

        explicit ShaderSource(std::string source)
        {
            mSource.push_back(std::move(source));
        }
        ShaderSource() = default;

        inline void SetType(Type type) noexcept
        { mType = type; }
        inline void SetPrecision(Precision precision) noexcept
        { mPrecision = precision; }
        inline void SetVersion(Version version) noexcept
        { mVersion = version; }
        inline void AddSource(std::string source)
        { mSource.push_back(std::move(source)); }
        inline void AddData(const ShaderDataDeclaration& data)
        { mData.push_back(data); }
        inline bool IsEmpty() const noexcept
        { return mSource.empty(); }
        inline size_t GetSourceCount() const noexcept
        { return mSource.size(); }
        inline std::string GetSource(size_t index) const noexcept
        { return base::SafeIndex(mSource, index); }
        inline void ClearSource() noexcept
        { mSource.clear(); }
        inline void ClearData() noexcept
        { mData.clear(); }

        void AddAttribute(std::string name, AttributeType type)
        {
            AddData({ShaderDataDeclarationType::Attribute, type, std::move(name)});
        }
        void AddUniform(std::string name, UniformType type)
        {
            AddData({ShaderDataDeclarationType::Uniform, type, std::move(name)});
        }
        void AddConstant(std::string name, ConstantType type)
        {
            AddData({ShaderDataDeclarationType::Constant, type, std::move(name)});
        }
        void AddVarying(std::string name, VaryingType type)
        {
            AddData({ShaderDataDeclarationType::Varying, type, std::move(name)});
        }

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
        // Data are the shader data declarations such as uniforms,
        // constants, varyings and (vertex) attributes.
        // These are the shader program's data interface
        // the interface mechanism for data flow from vertex
        // shader to the fragment shader.
        std::vector<ShaderDataDeclaration> mData;
        // Source are the actual GLSL shader code functions etc.
        // Currently basically everything else other than the
        // data declarations.
        std::vector<std::string> mSource;
    };
} // namespace