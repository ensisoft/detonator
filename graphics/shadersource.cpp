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

} // namespace

namespace gfx
{

std::string ShaderSource::GetSource() const
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

    for (const auto& data : mData)
    {
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

    for (const auto& src : mSource)
    {
        ss << src;
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

} // namespace
