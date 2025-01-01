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
#include <unordered_map>

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
            Sampler2D
        };
        using AttributeType = ShaderDataType;
        using UniformType   = ShaderDataType;
        using VaryingType   = ShaderDataType;
        using ConstantType  = ShaderDataType;

        enum class ShaderBlockType {
            // Attribute, Uniform, Varying, Constant,
            ShaderDataDeclaration,
            // technically not part of the GLSL data types itself
            // since it's preprocessor #define BLAH 1
            // but combining it in the same enum for convenience.
            PreprocessorDefine,
            // #ifdef, #else, #endif, #define
            PreprocessorToken,
            // this is a comment
            Comment,
            StructDeclaration,
            ShaderCode
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

        enum class ShaderDataDeclarationType {
            Attribute, Uniform, Varying, Constant,
        };

        struct ShaderDataDeclaration {
            // attribute, uniform, constant et.
            ShaderDataDeclarationType decl_type;
            // int, float, vec2, etc.
            ShaderDataType data_type;
            // name of the data variable, uniform for example kBaseColor
            std::string name;
            // constant value (if any), only used when the decl_type is Constant
            std::optional<ShaderDataDeclarationValue> constant_value;
        };

        struct ShaderBlock {
            ShaderBlockType type;
            std::string data;
            std::optional<ShaderDataDeclaration> data_decl;
        };

        ShaderSource() = default;

        inline void SetType(Type type) noexcept
        { mType = type; }
        inline void SetPrecision(Precision precision) noexcept
        { mPrecision = precision; }
        inline void SetVersion(Version version) noexcept
        { mVersion = version; }
        inline bool IsEmpty() const noexcept
        { return mShaderBlocks.empty(); }
        inline void Clear() noexcept
        { mShaderBlocks.clear(); }
        inline Type GetType() const noexcept
        { return mType; }
        inline Precision GetPrecision() const noexcept
        { return mPrecision; }
        inline Version GetVersion() const noexcept
        { return mVersion; }

        struct DebugInfo {
            std::string key;
            std::string val;
        };

        inline void AddDebugInfo(std::string key, std::string val) noexcept
        { mDebugInfos.push_back( { std::move(key), std::move(val) } ); }
        inline void AddDebugInfo(DebugInfo info) noexcept
        { mDebugInfos.push_back(std::move(info)); }
        inline const auto& GetDebugInfo(size_t index) const noexcept
        { return mDebugInfos[index]; }
        inline auto GetDebugInfoCount() const noexcept
        { return mDebugInfos.size(); }

        inline void AddShaderName(std::string name) noexcept
        { AddDebugInfo("Name", std::move(name)); }
        inline void AddShaderSourceUri(std::string uri) noexcept
        { AddDebugInfo("Source", std::move(uri)); }

        const ShaderDataDeclaration* FindDataDeclaration(const std::string& key) const;
        const ShaderBlock* FindShaderBlock(const std::string& key) const;

        void AddSource(std::string source);
        void AddPreprocessorDefinition(std::string name);
        void AddPreprocessorDefinition(std::string name, int value);
        void AddPreprocessorDefinition(std::string name, float value);
        void AddPreprocessorDefinition(std::string name, std::string value);
        void AddAttribute(std::string name, AttributeType type);
        void AddUniform(std::string name, UniformType type);
        void AddConstant(std::string name, ShaderDataDeclarationValue value);
        void AddVarying(std::string name, VaryingType type);

        bool HasShaderBlock(const std::string& key, ShaderBlockType type) const;
        bool HasDataDeclaration(const std::string& name, ShaderDataDeclarationType type) const;

        void FoldUniform(const std::string& name, ShaderDataDeclarationValue value);

        inline bool HasUniform(const std::string& name) const
        { return HasDataDeclaration(name, ShaderDataDeclarationType::Uniform); }
        inline bool HasVarying(const std::string& name) const
        { return HasDataDeclaration(name, ShaderDataDeclarationType::Varying); }

        enum class SourceVariant {
            Production, Development
        };

        std::string GetShaderName() const;

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

        bool LoadRawSource(const std::string& source);

        static ShaderSource FromRawSource(const std::string& source, Type type);

        static ShaderDataType DataTypeFromValue(ShaderDataDeclarationValue value) noexcept;

    private:
        Type mType = Type::NotSet;
        Version mVersion = Version::NotSet;
        Precision mPrecision = Precision::NotSet;

        std::unordered_map<std::string,
           std::vector<ShaderBlock>> mShaderBlocks;

        std::vector<DebugInfo> mDebugInfos;
    };
} // namespace
