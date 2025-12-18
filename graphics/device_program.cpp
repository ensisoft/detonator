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
#include "base/utility.h"
#include "device/graphics.h"
#include "graphics/program.h"
#include "graphics/device_program.h"
#include "graphics/device_shader.h"
#include "graphics/device_texture.h"

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
        ERROR("Program build error. [name='%1']",  mName);
        const auto& error_lines = base::SplitString(build_info, '\n');
        for (const auto& error_line : error_lines)
        {
            if (error_line.empty() || error_line == "\n")
                continue;
            ERROR("Program error: %1", error_line);
        }

        for (const auto& shader : shaders)
        {
            const auto* ptr = static_cast<const gfx::DeviceShader*>(shader.get());
            ptr->DumpSource();
        }
        return false;
    }

    DEBUG("Program was built successfully. [name='%1']", mName);

    const auto& info_lines = base::SplitString(build_info, '\n');
    for (const auto& info_line : info_lines)
    {
        if (info_line.empty() || info_line == "'\n")
            continue;;
        INFO("Program info: %1", info_line);
    }

    for (auto& shader : shaders)
    {
        const auto* ptr = static_cast<const gfx::DeviceShader*>(shader.get());
        ptr->ClearSource();
    }
    mProgram = program;
    return true;
}

void DeviceProgram::ApplyTextureState(const ProgramState &state,
    TextureMinFilter device_default_min_filter, TextureMagFilter device_default_mag_filter) const
{
    // set program texture bindings
    const auto num_textures = state.GetSamplerCount();

    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& sampler = state.GetSamplerSetting(i);

        const auto* texture = static_cast<const DeviceTexture*>(sampler.texture);
        // if the program sampler/texture setting is using a discontinuous set of
        // texture units we might end up with "holes" in the program texture state
        // and then the texture object is actually a nullptr.
        if (texture == nullptr)
            continue;

        texture->SetFrameStamp(mFrameNumber);

        auto texture_min_filter = texture->GetMinFilter();
        auto texture_mag_filter = texture->GetMagFilter();
        if (texture_min_filter == dev::TextureMinFilter::Default)
            texture_min_filter = static_cast<dev::TextureMinFilter>(device_default_min_filter);
        if (texture_mag_filter == dev::TextureMagFilter::Default)
            texture_mag_filter = static_cast<dev::TextureMagFilter>(device_default_mag_filter);

        const auto texture_wrap_x = texture->GetWrapX();
        const auto texture_wrap_y = texture->GetWrapY();
        const auto texture_name = texture->GetName();
        const auto texture_unit = i;

        dev::GraphicsDevice::BindWarnings warnings;

        mDevice->BindTexture2D(texture->GetTexture(), mProgram, sampler.name,
                               texture_unit, texture_wrap_x, texture_wrap_y,
                               texture_min_filter, texture_mag_filter, &warnings);

        if (!texture->IsTransient() && texture->WarnOnce())
        {
            if (warnings.force_min_linear)
                WARN("Forcing GL_LINEAR on texture without mip maps. [texture='%1']", texture_name);
            if (warnings.force_clamp_x)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
            if (warnings.force_clamp_y)
                WARN("Forcing GL_CLAMP_TO_EDGE on NPOT texture. [texture='%1']", texture_name);
        }
    }
}

void DeviceProgram::ApplyUniformState(const ProgramState& state) const
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
