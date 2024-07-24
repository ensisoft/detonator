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

#include <sstream>

#include "base/assert.h"
#include "base/utility.h"
#include "base/format.h"
#include "base/logging.h"
#include "graphics/shadersource.h"

namespace {
    std::string ToConst(float value)
    {
        return base::ToChars(value);
    }
    std::string ToConst(const gfx::Color4f& sRGB)
    {
        // convert to linear.
        const auto& color = sRGB_Decode(sRGB);

        return base::FormatString("vec4(%1,%2,%3,%4)",
                                  base::ToChars(color.Red()),
                                  base::ToChars(color.Green()),
                                  base::ToChars(color.Blue()),
                                  base::ToChars(color.Alpha()));
    }
    std::string ToConst(const glm::vec2& vec2)
    {
        return base::FormatString("vec2(%1,%2)",
                                  base::ToChars(vec2.x),
                                  base::ToChars(vec2.y));
    }
    std::string ToConst(const glm::vec3& vec3)
    {
        return base::FormatString("vec3(%1,%2,%3)",
                                  base::ToChars(vec3.x),
                                  base::ToChars(vec3.y),
                                  base::ToChars(vec3.z));
    }
    std::string ToConst(const glm::vec4& vec4)
    {
        return base::FormatString("vec4(%1,%2,%3,%4)",
                                  base::ToChars(vec4.x),
                                  base::ToChars(vec4.y),
                                  base::ToChars(vec4.z),
                                  base::ToChars(vec4.w));
    }
    template<typename T>
    std::string ToConst(const T& value)
    {
        BUG("Not implemented!");
        return "";
    }

    bool ValidateConstDataType(float, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Float;
    }
    bool ValidateConstDataType(int, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Int;
    }
    bool ValidateConstDataType(const glm::vec2&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec2f;
    }
    bool ValidateConstDataType(const glm::vec3&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec3f;
    }
    bool ValidateConstDataType(const glm::vec4&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec4f;
    }
    bool ValidateConstDataType(const glm::ivec2&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec2i;
    }
    bool ValidateConstDataType(const glm::ivec3&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec3i;
    }
    bool ValidateConstDataType(const glm::ivec4&, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Vec4i;
    }
    bool ValidateConstDataType(const gfx::Color4f, gfx::ShaderSource::ShaderDataType type)
    {
        return type == gfx::ShaderSource::ShaderDataType::Color4f;
    }

    template<typename T>
    bool ValidateConstDataType(T, gfx::ShaderSource::ShaderDataType)
    {
        BUG("Not implemented");
        return false;
    }

    std::optional<gfx::ShaderSource::ShaderDataDeclarationType> DeclTypeFromString(const std::string& str)
    {
        using t = gfx::ShaderSource::ShaderDataDeclarationType;

        if (str == "attribute")
            return t::Attribute;
        else if (str == "uniform")
            return t::Uniform;
        else if (str == "varying")
            return t::Varying;
        else if (str == "const")
            return t::Constant;

        return std::nullopt;
    }

    std::optional<gfx::ShaderSource::ShaderDataType> DataTypeFromString(const std::string& str)
    {
        using t = gfx::ShaderSource::ShaderDataType;

        if (str == "int")
            return t::Int;
        else if (str == "float")
            return t::Float;
        else if (str == "vec2")
            return t::Vec2f;
        else if (str == "vec3")
            return t::Vec3f;
        else if (str == "vec4")
            return t::Vec4f;
        else if (str == "ivec2")
            return t::Vec2i;
        else if (str == "ivec3")
            return t::Vec3i;
        else if (str == "ivec4")
            return t::Vec4i;
        else if (str == "mat2")
            return t::Mat2f;
        else if (str == "mat3")
            return t::Mat3f;
        else if (str == "mat4")
            return t::Mat4f;
        else if (str =="sampler2D")
            return t::Sampler2D;

        return std::nullopt;
    }

    std::optional<std::string> GetTokenName(const std::string& str)
    {
        std::string ret;
        for (size_t i=0; i<str.size(); ++i)
        {
            if (str[i] == ';')
                return base::TrimString(ret);
            ret += str[i];
        }
        return std::nullopt;
    }

} // namespace

namespace gfx
{

void ShaderSource::FoldUniform(const std::string& name, ShaderDataDeclarationValue value)
{
    for (auto& decl : mData)
    {
        if (decl.decl_type != ShaderDataDeclarationType::Uniform)
            continue;
        if (decl.name != name)
            continue;

        std::visit([&decl](auto the_value)  {
            ASSERT(ValidateConstDataType(the_value, decl.data_type));
        }, value);

        decl.decl_type = ShaderDataDeclarationType::Constant;
        decl.constant_value = std::move(value);
        break;
    }
}

void ShaderSource::SetComment(const std::string& name, std::string comment)
{
    for (auto& decl : mData)
    {
        if (decl.name != name)
            continue;
        decl.comment = std::move(comment);
        break;
    }
}


std::string ShaderSource::GetSource(SourceVariant variant) const
{
    std::stringstream ss;
    if (mVersion == Version::GLSL_100)
        ss << "#version 100\n";
    else if (mVersion == Version::GLSL_300)
        ss << "#version 300 es\n";
    else if (mVersion != Version::NotSet)
        BUG("Missing GLSL version handling.");

    if (mType == Type::Fragment)
    {
        if (mPrecision == Precision::Low)
            ss << "precision lowp float;\n";
        else if (mPrecision == Precision::Medium)
            ss << "precision mediump float;\n";
        else if (mPrecision == Precision::High)
            ss << "precision highp float;\n";
        else if (mPrecision != Precision::NotSet)
            BUG("Missing GLSL fragment shader floating point precision handling.");
    }

    ss << "\n// Warning. Do not delete the below line.";
    ss << "\n// shader_uniform_api_version=" << mShaderUniformAPIVersion << "\n\n";

    for (const auto& data : mData)
    {
        if (variant == SourceVariant::ShaderStub)
        {
            const auto& lines = base::SplitString(data.comment, '\n');
            for (const auto& line: lines)
            {
                ss << "// " << line << "\n";
            }
        }

        if (data.decl_type == ShaderDataDeclarationType::Attribute)
            ss << "attribute ";
        else if (data.decl_type == ShaderDataDeclarationType::Uniform)
            ss << "uniform ";
        else if (data.decl_type == ShaderDataDeclarationType::Constant)
            ss << "const ";
        else if (data.decl_type == ShaderDataDeclarationType::Varying)
            ss << "varying ";
        else BUG("Missing GLSL data declaration type handling.");

        if (data.data_type == ShaderDataType::Int)
            ss << "int ";
        else if (data.data_type == ShaderDataType::Float)
            ss << "float ";
        else if (data.data_type == ShaderDataType::Vec2f)
            ss << "vec2 ";
        else if (data.data_type == ShaderDataType::Vec3f)
            ss << "vec3 ";
        else if (data.data_type == ShaderDataType::Vec4f)
            ss << "vec4 ";
        else if (data.data_type == ShaderDataType::Vec2i)
            ss << "ivec2 ";
        else if (data.data_type == ShaderDataType::Vec3i)
            ss << "ivec3 ";
        else if (data.data_type == ShaderDataType::Vec4i)
            ss << "ivec4 ";
        else if (data.data_type == ShaderDataType::Mat2f)
            ss << "mat2 ";
        else if (data.data_type == ShaderDataType::Mat3f)
            ss << "mat3 ";
        else if (data.data_type == ShaderDataType::Mat4f)
            ss << "mat4 ";
        else if (data.data_type == ShaderDataType::Color4f)
            ss << "vec4 ";
        else if (data.data_type == ShaderDataType::Sampler2D)
        {
            if (data.decl_type != ShaderDataDeclarationType::Uniform)
            {
                ERROR("Shader uses sampler2D data declaration that isn't a uniform.");
                return "";
            }
            ss << "sampler2D ";
        }
        else BUG("Missing GLSL data type handling.");


        if (data.decl_type == ShaderDataDeclarationType::Constant)
        {
            ASSERT(data.constant_value.has_value());

            ss << data.name;
            ss << " = ";
            std::visit([&ss, &data](auto the_value) {
                ASSERT(ValidateConstDataType(the_value, data.data_type));

                ss << ToConst(the_value);
            }, data.constant_value.value());
            ss << ";";
        }
        else
        {
            ss << data.name;
            ss << ";";
        }
        ss << "\n";
    }
    if (variant == SourceVariant::ShaderStub && !mStubFunction.empty())
    {
        ss << mStubFunction;
    }
    else
    {
        for (const auto& src: mSource)
        {
            ss << src;
        }
    }

    return ss.str();
}

void ShaderSource::Merge(const ShaderSource& other)
{
    ASSERT(IsCompatible(other));

    for (const auto& other_source : other.mSource)
    {
        mSource.push_back(other_source);
    }

    for (const auto& other_data : other.mData)
    {
        mData.push_back(other_data);
    }
}

bool ShaderSource::IsCompatible(const ShaderSource& other) const noexcept
{
    const auto check_type = (mType != Type::NotSet) && (other.mType != Type::NotSet);
    const auto check_version = (mVersion != Version::NotSet) && (other.mVersion != Version::NotSet);
    const auto check_precision = (mPrecision != Precision::NotSet) && (other.mPrecision != Precision::NotSet);
    if (check_type)
    {
        if (mType != other.mType)
            return false;
    }
    else if (check_version)
    {
        if (mVersion != other.mVersion)
            return false;
    }
    else if (check_precision)
    {
        if (mPrecision != other.mPrecision)
            return false;
    }
    return true;
}

// static
ShaderSource ShaderSource::FromRawSource(std::string raw_source)
{
    // Go over the raw GLSL source and try to extract higher
    // level information out of it so that more reasoning
    // can be done later on in terms of understanding the
    // shader uniforms/varyings etc.

    auto GetToken = [](const std::vector<std::string>& tokens, size_t index) {
        if (index >= tokens.size())
            return std::string("");
        return tokens[index];
    };

    ShaderSource source;

    std::string glsl_code;

    std::stringstream ss(raw_source);
    std::string line;
    while (std::getline(ss, line))
    {
        const auto& trimmed = base::TrimString(line);
        if (trimmed.empty())
            continue;
        if (base::StartsWith(trimmed, "#version"))
        {
            if (base::Contains(trimmed, "100"))
                source.SetVersion(ShaderSource::Version::GLSL_100);
            else if (base::Contains(trimmed, "300 es"))
                source.SetVersion(ShaderSource::Version::GLSL_300);
            else WARN("Unsupported GLSL version '%1'.", trimmed);
        }
        else if (base::StartsWith(trimmed, "precision"))
        {
            if (base::Contains(trimmed, "lowp"))
                source.SetPrecision(ShaderSource::Precision::Low);
            else if (base::Contains(trimmed, "mediump"))
                source.SetPrecision(ShaderSource::Precision::Medium);
            else if (base::Contains(trimmed, "highp"))
                source.SetPrecision(ShaderSource::Precision::High);
            else WARN("Unsupported GLSL precision '%1'.]", trimmed);
        }
        else if (base::StartsWith(trimmed, "attribute") ||
                 base::StartsWith(trimmed, "uniform") ||
                 base::StartsWith(trimmed, "varying"))
        {
            const auto& parts = base::SplitString(trimmed);
            const auto& decl_type = DeclTypeFromString(GetToken(parts, 0));
            const auto& data_type = DataTypeFromString(GetToken(parts, 1));
            const auto& name = GetTokenName(GetToken(parts, 2));
            if (!decl_type.has_value() || !data_type.has_value() || !name.has_value())
            {
                WARN("Failed to parse GLSL declaration '%1'.", trimmed);
                glsl_code.append(trimmed);
                continue;
            }
            ShaderDataDeclaration decl;
            decl.data_type = data_type.value();
            decl.decl_type = decl_type.value();
            decl.name      = name.value();
            source.AddData(std::move(decl));
        }
        else if (!base::StartsWith(trimmed, "//"))
        {
            glsl_code.append(line);
            glsl_code.append("\n");
        }
    }

    source.AddSource(glsl_code);
    return source;
}

} // namespace
