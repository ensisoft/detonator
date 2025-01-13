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
#include "graphics/shader_source.h"

namespace {
    std::string ToConst(unsigned value)
    {
        return base::ToChars(value);
    }

    std::string ToConst(int value)
    {
        return base::ToChars(value);
    }

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
    std::string ToConst(T)
    {
        BUG("Shader data type has no const support.");
        return "";
    }


    std::string ToConst(const gfx::ShaderSource::ShaderDataDeclarationValue value)
    {
        std::string ret;
        std::visit([&ret](auto the_value) {
            ret = ToConst(the_value);
        }, value);
        return ret;
    }

    std::optional<gfx::ShaderSource::ShaderDataDeclarationType> DeclTypeFromString(const std::string& str, gfx::ShaderSource::Type type)
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
        else if (str == "in" && type == gfx::ShaderSource::Type::Vertex)
            return t::Attribute;
        else if (str == "out" && type == gfx::ShaderSource::Type::Vertex)
            return t::Varying;
        else if (str == "in" && type == gfx::ShaderSource::Type::Fragment)
            return t::Varying;

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

    std::string DataTypeToString(gfx::ShaderSource::ShaderDataType type)
    {
        using T = gfx::ShaderSource::ShaderDataType;

        if (type == T::Int)
            return "int";
        else if (type == T::Float)
            return "float";
        else if (type == T::Vec2f)
            return "vec2";
        else if (type == T::Vec3f)
            return "vec3";
        else if (type == T::Vec4f)
            return "vec4";
        else if (type == T::Vec2i)
            return "ivec2";
        else if (type == T::Vec3i)
            return "ivec3";
        else if (type == T::Vec4i)
            return "ivec4";
        else if (type == T::Mat2f)
            return "mat2";
        else if (type == T::Mat3f)
            return "mat3";
        else if (type == T::Mat4f)
            return "mat4";
        else if (type == T::Color4f)
            return "vec4";
        else if (type == T::Sampler2D)
            return "sampler2D";
       BUG("Bug on shader type string.");
    }

} // namespace

namespace gfx
{

const ShaderSource::ShaderDataDeclaration* ShaderSource::FindDataDeclaration(const std::string& key) const
{
    for (const auto& pair : mShaderBlocks)
    {
        const auto& blocks = pair.second;
        for (const auto& block : blocks)
        {
            if (block.type != ShaderBlockType::ShaderDataDeclaration)
                continue;
            auto& data = block.data_decl.value();
            if (data.name == key)
                return &data;
        }
    }
    return nullptr;
}

const ShaderSource::ShaderBlock* ShaderSource::FindShaderBlock(const std::string& key) const
{
    for (const auto& pair : mShaderBlocks)
    {
        const auto& blocks = pair.second;
        for (const auto& block : blocks)
        {
            if (base::Contains(block.data, key))
                return &block;
        }
    }
    return nullptr;
}

void ShaderSource::AddSource(std::string source)
{
    std::string line;
    std::stringstream ss(source);
    while (std::getline(ss, line))
    {
        ShaderBlock block;
        block.type = ShaderBlockType::ShaderCode;
        block.data = line;
        mShaderBlocks["code"].push_back(std::move(block));
    }
}

void ShaderSource::AddPreprocessorDefinition(std::string name)
{
    ShaderBlock block;
    block.type = ShaderBlockType::PreprocessorDefine;
    block.data = base::FormatString("#define %1", name);
    mShaderBlocks["preprocessor"].push_back(std::move(block));
}

void ShaderSource::AddPreprocessorDefinition(std::string name, unsigned value)
{
    ShaderBlock block;
    block.type = ShaderBlockType::PreprocessorDefine;
    block.data = base::FormatString("#define %1 uint(%2)", name, ToConst(value));
    mShaderBlocks["preprocessor"].push_back(std::move(block));
}

void ShaderSource::AddPreprocessorDefinition(std::string name, int value)
{
    ShaderBlock block;
    block.type = ShaderBlockType::PreprocessorDefine;
    block.data = base::FormatString("#define %1 %2", name, ToConst(value));
    mShaderBlocks["preprocessor"].push_back(std::move(block));
}

void ShaderSource::AddPreprocessorDefinition(std::string name, float value)
{
    ShaderBlock block;
    block.type = ShaderBlockType::PreprocessorDefine;
    block.data = base::FormatString("#define %1 %2", name, ToConst(value));
    mShaderBlocks["preprocessor"].push_back(std::move(block));
}

void ShaderSource::AddPreprocessorDefinition(std::string name, std::string value)
{
    ShaderBlock block;
    block.type = ShaderBlockType::PreprocessorDefine;
    block.data = base::FormatString("#define %1 %2", name, value);
    mShaderBlocks["preprocessor"].push_back(std::move(block));
}

void ShaderSource::AddAttribute(std::string name, AttributeType type)
{
    std::string code;
    if (mVersion == Version::GLSL_100)
        code = base::FormatString("attribute %1 %2;", DataTypeToString(type), name);
    else if (mVersion == Version::GLSL_300)
        code = base::FormatString("in %1 %2;", DataTypeToString(type), name);
    else BUG("Bug on attribute formatting.");

    ShaderDataDeclaration decl;
    decl.decl_type = ShaderDataDeclarationType::Attribute;
    decl.data_type = type;
    decl.name      = name;

    ShaderBlock block;
    block.type = ShaderBlockType::ShaderDataDeclaration;
    block.data = std::move(code);
    block.data_decl = decl;
    mShaderBlocks["attributes"].push_back(std::move(block));
}
void ShaderSource::AddUniform(std::string name, UniformType type)
{
    ShaderDataDeclaration decl;
    decl.decl_type = ShaderDataDeclarationType::Uniform;
    decl.data_type = type;
    decl.name      = name;

    ShaderBlock block;
    block.type = ShaderBlockType::ShaderDataDeclaration;
    block.data = base::FormatString("uniform %1 %2;", DataTypeToString(type), name);
    block.data_decl = decl;
    mShaderBlocks["uniforms"].push_back(std::move(block));
}
void ShaderSource::AddConstant(std::string name, ShaderDataDeclarationValue value)
{
    const auto data_type  = DataTypeFromValue(value);

    ShaderDataDeclaration decl;
    decl.decl_type = ShaderDataDeclarationType::Constant;
    decl.data_type = data_type;
    decl.name      = name;
    decl.constant_value = value;

    ShaderBlock block;
    block.type = ShaderBlockType::ShaderDataDeclaration;
    block.data = base::FormatString("const %1 %2 = %3;", DataTypeToString(data_type), name, ToConst(value));
    block.data_decl = decl;
    mShaderBlocks["constants"].push_back(std::move(block));
}
void ShaderSource::AddVarying(std::string name, VaryingType type)
{
    std::string code;
    if (mVersion == Version::GLSL_100)
        code = base::FormatString("varying %1 %2;", DataTypeToString(type), name);
    else if (mVersion == Version::GLSL_300)
    {
        if (mType == Type::Fragment)
            code = base::FormatString("in %1 %2;", DataTypeToString(type), name);
        else if (mType == Type::Vertex)
            code = base::FormatString("out %1 %2;", DataTypeToString(type), name);
        else BUG("Bug on varying formatting.");
    } else BUG("Bug on varying formatting.");

    ShaderDataDeclaration decl;
    decl.decl_type = ShaderDataDeclarationType::Varying;
    decl.data_type = type;
    decl.name      = name;

    ShaderBlock block;
    block.type = ShaderBlockType::ShaderDataDeclaration;
    block.data = code;
    block.data_decl = decl;
    mShaderBlocks["varyings"].push_back(std::move(block));
}

bool ShaderSource::HasShaderBlock(const std::string& key, ShaderBlockType type) const
{
    for (const auto& pair : mShaderBlocks)
    {
        const auto& blocks = pair.second;
        for (const auto& block : blocks)
        {
            if (block.type != type)
                continue;
            if (base::Contains(block.data, key))
                return true;
        }
    }
    return false;
}

bool ShaderSource::HasDataDeclaration(const std::string& name, ShaderDataDeclarationType type) const
{
    for (const auto& pair : mShaderBlocks)
    {
        const auto& blocks = pair.second;
        for (const auto& block : blocks)
        {
            if (block.type != ShaderBlockType::ShaderDataDeclaration)
                continue;

            const auto& decl = block.data_decl.value();
            if (decl.name == name && decl.decl_type == type)
                return true;
        }
    }
    return false;
}


void ShaderSource::FoldUniform(const std::string& name, ShaderDataDeclarationValue value)
{
    auto& uniforms = mShaderBlocks["uniforms"];

    for (auto& block : uniforms)
    {
        if (block.type != ShaderBlockType::ShaderDataDeclaration)
            continue;

        auto& decl = block.data_decl.value();
        if (decl.name != name)
            continue;

        const auto data_type  = DataTypeFromValue(value);
        ASSERT(data_type == decl.data_type);
        decl.decl_type = ShaderDataDeclarationType::Constant;
        decl.constant_value = std::move(value);
        block.data =  base::FormatString("const %1 %2 = %3;", DataTypeToString(data_type), name, ToConst(value));
        break;
    }
}

std::string ShaderSource::GetShaderName() const
{
    for (const auto& info : mDebugInfos)
    {
        if (info.key == "Name")
            return info.val;
    }
    return "";
}

std::string ShaderSource::GetSource(SourceVariant variant) const
{
    std::stringstream ss;
    if (mVersion == Version::GLSL_100)
        ss << "#version 100";
    else if (mVersion == Version::GLSL_300)
        ss << "#version 300 es";
    else if (mVersion != Version::NotSet)
        BUG("Missing GLSL version handling.");
    ss << "\n\n";

    if (mType == Type::Fragment)
    {
        if (mPrecision == Precision::Low)
            ss << "precision lowp float;";
        else if (mPrecision == Precision::Medium)
            ss << "precision mediump float;";
        else if (mPrecision == Precision::High)
            ss << "precision highp float;";
        else if (mPrecision != Precision::NotSet)
            BUG("Missing GLSL fragment shader floating point precision handling.");
        ss << "\n\n";
    }

    // this could go to the beginning but I'm 100% sure it'll
    // bug out with shitty buggy drivers.
    for (const auto& debug : mDebugInfos)
    {
        ss << "// " << base::FormatString("%1 = %2", debug.key, debug.val);
        ss << "\n";
    }

    static const char* groups[] = {
        "preprocessor",
        "constants",
        "types",
        "attributes",
        "uniforms",
        "varyings",
        "out",
        "code"
    };
    for (const char* group_key : groups)
    {
        const auto* blocks = base::SafeFind(mShaderBlocks, std::string(group_key));
        if (blocks == nullptr)
            continue;
        for (const auto& block : *blocks)
        {
            if (block.type == ShaderBlockType::Comment)
            {
                if (variant == SourceVariant::Development)
                {
                    ss << block.data;
                    ss << "\n";
                }
                continue;
            }
            if (block.type == ShaderBlockType::PreprocessorToken &&
                base::StartsWith(base::TrimString(block.data), "#ifdef"))
            {
                ss << "\n";
            }

            ss << block.data;
            ss << "\n";
        }
        ss << "\n";
    }
    return ss.str();
}

void ShaderSource::Merge(const ShaderSource& other)
{
    for (const auto& pair : other.mShaderBlocks)
    {
        const auto& key = pair.first;
        const auto& blocks = pair.second;
        for (const auto& block : blocks)
        {
            mShaderBlocks[key].push_back(block);
        }
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
ShaderSource::ShaderDataType ShaderSource::DataTypeFromValue(gfx::ShaderSource::ShaderDataDeclarationValue value) noexcept
{
    if (std::holds_alternative<int>(value))
        return ShaderDataType::Int;
    else if (std::holds_alternative<float>(value))
        return ShaderDataType::Float;
    else if (std::holds_alternative<Color4f>(value))
        return ShaderDataType::Vec4f;
    else if (std::holds_alternative<glm::vec2>(value))
        return ShaderDataType::Vec2f;
    else if (std::holds_alternative<glm::vec3>(value))
        return ShaderDataType::Vec3f;
    else if (std::holds_alternative<glm::vec4>(value))
        return ShaderDataType::Vec4f;
    else if (std::holds_alternative<glm::ivec2>(value))
        return ShaderDataType::Vec2i;
    else if (std::holds_alternative<glm::ivec3>(value))
        return ShaderDataType::Vec3i;
    else if (std::holds_alternative<glm::ivec4>(value))
        return ShaderDataType::Vec4i;
    else if (std::holds_alternative<glm::mat2>(value))
        return ShaderDataType::Mat2f;
    else if (std::holds_alternative<glm::mat3>(value))
        return ShaderDataType::Mat3f;
    else if (std::holds_alternative<glm::mat4>(value))
        return ShaderDataType::Mat4f;
    else if (std::holds_alternative<std::string>(value))
        BUG("String is not a valid GLSL shader constant value type.");

    BUG("Missing shader source type.");
    return ShaderDataType::Int;
}

bool ShaderSource::LoadRawSource(const std::string& source)
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

    std::vector<std::string> line_buffer;
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line))
    {
        if (base::StartsWith(line, "R\"CPP_RAW_STRING(") ||
            base::StartsWith(line, ")CPP_RAW_STRING\""))
            continue;

        line_buffer.push_back(line);
    }

    std::string group;

    // try to "parse" (lol) the GLSL in two segments.
    // first try to extract the in, out, varying, uniform
    // shader data declarations but with relative ordering
    // and preprocessor definitions intact.
    //
    // then assume the rest is code...
    size_t line_buffer_index = 0;
    for (; line_buffer_index < line_buffer.size(); line_buffer_index++)
    {
        auto line = line_buffer[line_buffer_index];
        if (line.empty())
            continue;

        const auto& trimmed = base::TrimString(line);

        if (base::StartsWith(trimmed, "// @"))
        {
            group = trimmed.substr(4);
            continue;
        }
        else if (base::StartsWith(trimmed, "//@"))
        {
            group = trimmed.substr(3);
            continue;
        }

        if (base::StartsWith(trimmed, "#version"))
        {
            if (base::Contains(trimmed, "100"))
                SetVersion(ShaderSource::Version::GLSL_100);
            else if (base::Contains(trimmed, "300 es"))
                SetVersion(ShaderSource::Version::GLSL_300);
            else
            {
                ERROR("Unsupported GLSL version '%1'.", trimmed);
                return false;
            }
        }
        else if (base::StartsWith(trimmed, "#define"))
        {
            ShaderBlock block;
            block.type = ShaderBlockType::PreprocessorDefine;
            block.data = line;
            mShaderBlocks[group.empty() ? "preprocessor" : group].push_back(std::move(block));
        }
        else if (base::StartsWith(trimmed, "#ifdef") ||
                 base::StartsWith(trimmed, "#ifndef") ||
                 base::StartsWith(trimmed, "#else") ||
                 base::StartsWith(trimmed, "#elif") ||
                 base::StartsWith(trimmed, "#endif") ||
                 base::StartsWith(trimmed, "#if "))
        {
            ShaderBlock block;
            block.type = ShaderBlockType::PreprocessorToken;
            block.data = line;
            if (group.empty())
            {
                WARN("Empty shader block group for preprocessor conditional.");
                WARN("Your shader will likely not work as expected.");
                WARN("Use '// @ group-name' to set the expected shader block group.");
            }
            mShaderBlocks[group].push_back(std::move(block));
        }
        else if (base::StartsWith(trimmed, "precision"))
        {
            if (base::Contains(trimmed, "lowp"))
                SetPrecision(ShaderSource::Precision::Low);
            else if (base::Contains(trimmed, "mediump"))
                SetPrecision(ShaderSource::Precision::Medium);
            else if (base::Contains(trimmed, "highp"))
                SetPrecision(ShaderSource::Precision::High);
            else WARN("Unsupported GLSL precision '%1'.]", trimmed);
        }
        else if (base::StartsWith(trimmed, "attribute") ||
                 base::StartsWith(trimmed, "uniform") ||
                 base::StartsWith(trimmed, "varying") ||
                 base::StartsWith(trimmed, "in ") || // SPACE HERE on purpose to distinguish from int
                 base::StartsWith(trimmed, "out"))
        {
            const auto& parts = base::SplitString(trimmed);
            const auto& decl_type = DeclTypeFromString(GetToken(parts, 0), mType);
            const auto& data_type = DataTypeFromString(GetToken(parts, 1));
            const auto& name = GetTokenName(GetToken(parts, 2));
            if (!decl_type.has_value() || !data_type.has_value() || !name.has_value())
                ERROR_RETURN(false, "Failed to parse GLSL declaration '%1'.", trimmed);

            ShaderDataDeclaration decl;
            decl.data_type = data_type.value();
            decl.decl_type = decl_type.value();
            decl.name      = name.value();

            ShaderBlock block;
            block.type = ShaderBlockType::ShaderDataDeclaration;
            block.data = line;
            block.data_decl = decl;
            if (base::StartsWith(trimmed, "attribute"))
                mShaderBlocks["attributes"].push_back(std::move(block));
            else if (base::StartsWith(trimmed, "uniform"))
                mShaderBlocks["uniforms"].push_back(std::move(block));
            else if (base::StartsWith(trimmed, "varying"))
                mShaderBlocks["varyings"].push_back(std::move(block));
            else if (base::StartsWith(trimmed, "in "))
            {
                if (mType == Type::Vertex)
                    mShaderBlocks["attributes"].push_back(std::move(block));
                else if (mType == Type::Fragment)
                    mShaderBlocks["varyings"].push_back(std::move(block));
                else ERROR_RETURN(false, "Failed to parse GLSL declaration '%1.", trimmed);
            }
            else if (base::StartsWith(trimmed, "out"))
            {
                if (mType == Type::Vertex)
                    mShaderBlocks["varyings"].push_back(std::move(block));
                else if (mType == Type::Fragment)
                    mShaderBlocks["out"].push_back(std::move(block));
                else ERROR_RETURN(false, "Failed to parse GLSL declaration '%1'.", trimmed);
            }
        }
        else if (base::StartsWith(trimmed, "const"))
        {
            // todo: for correct behaviour would need to parse
            // const int foo=123;
            // const int foo = 123;
            // const int foo= 123;
            // const int foo =123;
            ERROR("Unimplemented GLSL constant parsing.");
            return false;
        }
        else if (base::StartsWith(trimmed, "layout"))
        {
            if (base::Contains(trimmed, "uniform") && base::Contains(trimmed, "{"))
            {
                // todo: data declaration parsing.
                ShaderBlock block;
                block.type = ShaderBlockType::ShaderDataDeclaration;
                block.data = line;
                block.data += "\n";
                for (++line_buffer_index; line_buffer_index<line_buffer.size(); ++line_buffer_index)
                {
                    auto line = line_buffer[line_buffer_index];
                    auto trimmed = base::TrimString(line);
                    block.data += std::move(line);
                    block.data += "\n";
                    if (base::StartsWith(trimmed, "}") && base::EndsWith(trimmed, ";"))
                        break;
                }
                mShaderBlocks["uniforms"].push_back(std::move(block));
            }
            else if (base::Contains(trimmed, " out "))
            {
                // todo: data declaration parsing.
                ShaderBlock block;
                block.type = ShaderBlockType::ShaderDataDeclaration;
                block.data = line;
                mShaderBlocks["out"].push_back(std::move(block));
            }
            else ERROR_RETURN(false, "Failed to parse GLSL layout declaration '%1'.", trimmed);
        }
        else if (base::StartsWith(trimmed, "struct"))
        {
            // todo: parse properly until }
            ShaderBlock block;
            block.type = ShaderBlockType::StructDeclaration;
            block.data = line + "\n";

            for (++line_buffer_index; line_buffer_index<line_buffer.size(); ++line_buffer_index)
            {
                auto line = line_buffer[line_buffer_index];
                auto trimmed = base::TrimString(line);
                block.data += line;
                block.data += "\n";
                if (base::StartsWith(trimmed, "}") && base::EndsWith(trimmed, ";"))
                    break;
            }
            mShaderBlocks["types"].push_back(std::move(block));
        }
        else if (base::StartsWith(trimmed, "//"))
        {
            ShaderBlock block;
            block.type = ShaderBlockType::Comment;
            block.data = line;
            mShaderBlocks[group].push_back(std::move(block));
        }
        else if (base::StartsWith(trimmed, "/*"))
        {
            ERROR("Unimplemented GLSL block comment parsing.");
            return false;
        }
        else
        {
            // start code ?
            break;
        }
    }

    for (; line_buffer_index < line_buffer.size(); line_buffer_index++)
    {
        auto line = line_buffer[line_buffer_index];
        auto trimmed = base::TrimString(line);
        if (base::StartsWith(trimmed, "//"))
        {
            ShaderBlock block;
            block.type = ShaderBlockType::Comment;
            block.data = std::move(line);
            mShaderBlocks["code"].push_back(std::move(block));
        }
        else if (base::StartsWith(trimmed, "/*"))
        {
            ERROR("Unimplemented GLSL block comment parsing.");
            return false;
        }
        else
        {
            ShaderBlock block;
            block.type = ShaderBlockType::ShaderCode;
            block.data = std::move(line);
            mShaderBlocks["code"].push_back(std::move(block));
        }
    }
    return true;
}

// static
ShaderSource ShaderSource::FromRawSource(const std::string& raw_source, Type type)
{
    ShaderSource source;
    source.SetType(type);
    source.LoadRawSource(raw_source);
    return source;
}

} // namespace
