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
#include "graphics/shadersource.h"

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

    for (const auto& snip : mSnippets)
    {
        ss << snip;
    }

    return ss.str();
}

void ShaderSource::Merge(const ShaderSource& other)
{
    ASSERT(IsCompatible(other));

    for (const auto& other_snip : other.mSnippets)
    {
        mSnippets.push_back(other_snip);
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
