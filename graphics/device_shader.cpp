// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include <sstream>
#include <unordered_set>
#include <cstdio>

#include "base/assert.h"
#include "base/utility.h"
#include "base/logging.h"
#include "graphics/device_shader.h"

namespace gfx {

DeviceShader::~DeviceShader()
{
    if (mShader.IsValid())
    {
        mDevice->DeleteShader(mShader);
        DEBUG("Deleted graphics shader. [name='%1']", mName);
    }
}

void DeviceShader::CompileSource(const std::string& source, bool debug)
{
    dev::ShaderType type = dev::ShaderType::Invalid;
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line) && type == dev::ShaderType::Invalid)
    {
        if (line.find("gl_Position") != std::string::npos)
            type = dev::ShaderType::VertexShader;
        else if (line.find("gl_FragColor") != std::string::npos ||
                 line.find("fragOutColor") != std::string::npos)
            type = dev::ShaderType::FragmentShader;
    }
    if (type == dev::ShaderType::Invalid)
    {
        ERROR("Failed to identify shader type. [name='%1']", mName);
        DEBUG("In order for the automatic shader type identification to work your shader must have one of the following:");
        DEBUG("GLSL 100 (ES2) gl_Position => vertex shader");
        DEBUG("GLSL 300 (ES3) gl_Position => vertex shader");
        DEBUG("GLSL 100 (ES2) gl_FragColor => fragment shader");
        DEBUG("GLSL 300 (ES3) fragOutColor => fragment shader");
        return;
    }

    auto shader = mDevice->CompileShader(source, type, &mCompileInfo);
    if (!shader.IsValid())
    {
        const auto& info_lines = base::SplitString(mCompileInfo, '\n');

        std::unordered_set<unsigned> error_lines;

        ERROR("Shader object compile error. [name='%1']", mName);
        for (const auto& info_line : info_lines)
        {
            if (info_line.empty() || info_line == "\n" || info_line == "\r\n")
                continue;

            ERROR("%1", info_line);

            int tmp, line_number;
            // likely not universal format but specific to NVIDIA driver
            if (std::sscanf(info_line.c_str(), "%d(%d)", &tmp, &line_number) == 2)
                error_lines.insert(line_number);
        }

        DumpSource(source, &error_lines);
        return;
    }
    else
    {
        if (debug)
            DumpSource(source);

        DEBUG("Shader was built successfully. [name='%1', info='%2']", mName, mCompileInfo);
    }
    mShader = shader;
    mSource = source;
}

void DeviceShader::DumpSource() const
{
    DumpSource(mSource);
}

void DeviceShader::DumpSource(const std::string& source,
                              const std::unordered_set<unsigned>* error_lines) const
{
    DEBUG("Shader source: [name='%1']", mName);
    std::stringstream ss(source);
    std::string line;
    unsigned line_number = 1;
    while (std::getline(ss, line))
    {
        if (error_lines && base::Contains(*error_lines, line_number))
            ERROR("L:%1 %2", line_number, line);
        else DEBUG("L:%1 %2", line_number, line);

        ++line_number;
    }
}
void DeviceShader::ClearSource() const
{
    mSource.clear();
}

} // namespace