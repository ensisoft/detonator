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

#include "base/bitflag.h"
#include "base/utility.h"
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
            Sampler2D,
            // todo: should refactor this away, using it here for convenience
            // when dealing with preprocessor strings
            PreprocessorString
        };
        using AttributeType = ShaderDataType;
        using UniformType   = ShaderDataType;
        using VaryingType   = ShaderDataType;
        using ConstantType  = ShaderDataType;

        enum class ShaderDataDeclarationType {
            Attribute, Uniform, Varying, Constant,
            // technically not part of the GLSL data types itself
            // since it's preprocessor #define BLAH 1
            // but combining it in the same enum for convenience.
            PreprocessorDefine
        };

        using ShaderDataDeclarationValue = std::variant<
                int,  float,
                Color4f,
                glm::vec2, glm::vec3, glm::vec4,
                glm::ivec2, glm::ivec3, glm::ivec4,
                glm::mat2, glm::mat3, glm::mat4, std::string>;

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
            std::string comment;
            std::optional<ShaderDataDeclarationValue> constant_value;
        };

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
        inline void AddData(ShaderDataDeclaration&& data)
        { mData.push_back(std::move(data)); }
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
        inline void SetStub(std::string stub) noexcept
        { mStubFunction = std::move(stub); }
        inline void SetShaderUniformAPIVersion(unsigned version)
        { mShaderUniformAPIVersion = version; }
        inline void SetShaderSourceUri(std::string uri) noexcept
        { mShaderSourceUri = std::move(uri); }

        inline Type GetType() const noexcept
        { return mType; }
        inline Precision GetPrecision() const noexcept
        { return mPrecision; }
        inline Version GetVersion() const noexcept
        { return mVersion; }

        const ShaderDataDeclaration* FindDataDeclaration(const std::string& key) const
        {
            for (const auto& data : mData)
            {
                if (data.name == key)
                    return &data;
            }
            return nullptr;
        }
        void AddPreprocessorDefinition(std::string name, int value, std::string  comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type      = ShaderDataDeclarationType::PreprocessorDefine;
            decl.data_type      = ShaderDataType::Int;
            decl.name           = std::move(name);
            decl.comment        = std::move(comment);
            decl.constant_value = value;
            AddData(std::move(decl));
        }
        void AddPreprocessorDefinition(std::string name, float value, std::string  comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type      = ShaderDataDeclarationType::PreprocessorDefine;
            decl.data_type      = ShaderDataType::Float;
            decl.name           = std::move(name);
            decl.comment        = std::move(comment);
            decl.constant_value = value;
            AddData(std::move(decl));
        }

        void AddAttribute(std::string name, AttributeType type, std::string comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type = ShaderDataDeclarationType::Attribute;
            decl.data_type = type;
            decl.name      = std::move(name);
            decl.comment   = std::move(comment);
            AddData(std::move(decl));
        }
        void AddUniform(std::string name, UniformType type, std::string comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type = ShaderDataDeclarationType::Uniform;
            decl.data_type = type;
            decl.name      = std::move(name);
            decl.comment   = std::move(comment);
            AddData(std::move(decl));
        }
        void AddConstant(std::string name, ShaderDataDeclarationValue value, std::string comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type = ShaderDataDeclarationType::Constant;
            decl.data_type = DataTypeFromValue(value);
            decl.constant_value = value;
            decl.name      = std::move(name);
            decl.comment   = std::move(comment);
            AddData(std::move(decl));
        }
        void AddVarying(std::string name, VaryingType type, std::string comment = "")
        {
            ShaderDataDeclaration decl;
            decl.decl_type = ShaderDataDeclarationType::Varying;
            decl.data_type = type;
            decl.name      = std::move(name);
            decl.comment   = std::move(comment);
            AddData(std::move(decl));
        }

        bool HasDataDeclaration(const std::string& name, ShaderDataDeclarationType type) const
        {
            for (const auto& data : mData)
            {
                if (data.decl_type == type && data.name == name)
                    return true;
            }
            return false;
        }
        inline bool HasUniform(const std::string& name) const
        { return HasDataDeclaration(name, ShaderDataDeclarationType::Uniform); }
        inline bool HasVarying(const std::string& name) const
        { return HasDataDeclaration(name, ShaderDataDeclarationType::Varying); }

        void FoldUniform(const std::string& name, ShaderDataDeclarationValue value);

        void SetComment(const std::string& name, std::string comment);

        enum class SourceVariant {
            Production, ShaderStub
        };

        // Get the actual shader source string by combining
        // the shader source object's contents (i.e. data
        // declarations and source code snippets) together.
        std::string GetSource(SourceVariant variant = SourceVariant::Production) const;

        // Merge the contents of the other shader source with this
        // shader source. The other shader source object must
        // be compatible with this shader source.
        void Merge(const ShaderSource& other);

        // Check whether this shader source object is compatible
        // with the other shader source, i.e. the shader type,
        // version and precision qualifiers match.
        bool IsCompatible(const ShaderSource& other) const noexcept;

        static ShaderSource FromRawSource(const std::string& source, Type type);

        static ShaderDataType DataTypeFromValue(ShaderDataDeclarationValue value) noexcept;

    private:
        unsigned mShaderUniformAPIVersion = 1;
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
        std::string mStubFunction;
        // for debugging help, we can embed the shader source
        // URI int he shader source as a comment so when it borks
        // in production the user can easily see which shader it is.
        std::string mShaderSourceUri;
    };
} // namespace
