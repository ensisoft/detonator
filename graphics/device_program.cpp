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

#include "base/assert.h"
#include "base/logging.h"
#include "device/graphics.h"
#include "graphics/program.h"
#include "graphics/device_program.h"
#include "graphics/device_shader.h"

namespace gfx
{

DeviceProgram::~DeviceProgram()
{
    if (mProgram.IsValid())
    {
        mDevice->DeleteProgram(mProgram);
        DEBUG("Deleted program object. [name='%1']", mName);
    }

    for (auto pair : mUniformBlockBuffers)
    {
        const auto& buffer = pair.second;
        if (buffer.IsValid())
        {
            mDevice->FreeBuffer(buffer);
        }
    }
}

bool DeviceProgram::Build(const std::vector<gfx::ShaderPtr>& shaders)
{
    std::vector<dev::GraphicsShader> shader_handles;
    for (const auto& shader : shaders)
    {
        const auto* ptr = static_cast<const gfx::DeviceShader*>(shader.get());
        shader_handles.push_back(ptr->GetShader());
    }

    std::string build_info;
    auto program = mDevice->BuildProgram(shader_handles, &build_info);
    if (!program.IsValid())
    {
        ERROR("Program build error. [error='%1']", build_info);

        for (const auto& shader : shaders)
        {
            const auto* ptr = static_cast<const gfx::DeviceShader*>(shader.get());
            ptr->DumpSource();
        }
        return false;
    }

    DEBUG("Program was built successfully. [name='%1', info='%2']", mName, build_info);
    for (auto& shader : shaders)
    {
        const auto* ptr = static_cast<const gfx::DeviceShader*>(shader.get());
        ptr->ClearSource();
    }
    mProgram = program;
    return true;
}

void DeviceProgram::ApplyUniformState(const gfx::ProgramState& state) const
{
    dev::ProgramState ps;
    for (size_t i=0; i<state.GetUniformCount(); ++i)
    {
        const auto& uniform = state.GetUniformSetting(i);
        ps.uniforms.push_back(&uniform);
    }
    mDevice->SetProgramState(mProgram, ps);

    for (size_t i=0; i<state.GetUniformBlockCount(); ++i)
    {
        const auto& block = state.GetUniformBlock(i);

        auto gpu_buffer = mUniformBlockBuffers[block.GetName()];

        const auto& cpu_block_buffer = block.GetBuffer();
        const auto block_buffer_size = cpu_block_buffer.size();

        if (gpu_buffer.buffer_bytes < block_buffer_size)
        {
            gpu_buffer = mDevice->AllocateBuffer(block_buffer_size, dev::BufferUsage::Stream, dev::BufferType::UniformBuffer);
        }
        mDevice->UploadBuffer(gpu_buffer, cpu_block_buffer.data(), block_buffer_size);
        mDevice->BindProgramBuffer(mProgram, gpu_buffer, block.GetName(), i); // todo: binding index ??
        mUniformBlockBuffers[block.GetName()] = gpu_buffer;
    }
}


} // namespace