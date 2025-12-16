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

#pragma once

#include "config.h"

#include <string>
#include <unordered_map>
#include <cstddef>

#include "device/graphics.h"
#include "graphics/enum.h"
#include "graphics/program.h"
#include "graphics/device_shader.h"

namespace gfx
{
    class DeviceShader;

    class DeviceProgram : public Program
    {
    public:
        explicit DeviceProgram(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}
       ~DeviceProgram() override;

        bool IsValid() const override
        { return mProgram.IsValid(); }
        std::string GetName() const override
        { return mName; }
        std::string GetId() const override
        { return mGpuId; }

        dev::GraphicsProgram GetProgram() const noexcept
        { return mProgram; }
        size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }

        void SetName(std::string name) noexcept
        { mName = std::move(name); }
        void SetId(std::string id) noexcept
        { mGpuId = std::move(id); }
        void SetFrameStamp(size_t frame_number) const noexcept
        { mFrameNumber = frame_number; }

        bool Build(const std::vector<ShaderPtr>& shaders);

        void ApplyTextureState(const ProgramState& state,
            TextureMinFilter device_default_min_filter, TextureMagFilter device_default_mag_filter) const;
        void ApplyUniformState(const ProgramState& state) const;
        void ValidateProgramState(const ProgramState& state) const;

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::GraphicsProgram mProgram;
        std::string mName;
        std::string mGpuId;
        struct UniformInfo {
            Shader::UniformType type;
            std::string name;
            std::uint8_t texture_unit = 0;
            bool has_binding = false;
        };
        // expected uniform blocks that the program user must bind
        // lest there be GL_INVALID_OPERATION from the draw call...
        mutable std::vector<UniformInfo> mUniformInfo;

        mutable std::unordered_map<std::string, dev::GraphicsBuffer> mUniformBlockBuffers;
        mutable std::size_t mFrameNumber = 0;
    };


} // namespace