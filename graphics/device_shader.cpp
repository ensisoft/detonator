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

#include "base/assert.h"
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
        ERROR("Shader object compile error. [name='%1', info='%2']", mName, mCompileInfo);
        DumpSource(source);
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

void DeviceShader::DumpSource(const std::string& source) const
{
    DEBUG("Shader source: [name='%1']", mName);
    std::stringstream ss(source);
    std::string line;
    std::size_t counter = 1;
    while (std::getline(ss, line))
    {
        DEBUG("L:%1 %2", counter, line);
        ++counter;
    }
}
void DeviceShader::ClearSource() const
{
    mSource.clear();
}

} // namespace