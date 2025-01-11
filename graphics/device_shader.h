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

#include "device/graphics.h"
#include "graphics/shader.h"

namespace gfx {

    class DeviceShader : public gfx::Shader
    {
    public:
        explicit DeviceShader(dev::GraphicsDevice* device) noexcept
          : mDevice(device)
        {}

       ~DeviceShader() override;

        void CompileSource(const std::string& source, bool debug);
        void DumpSource() const;
        void DumpSource(const std::string& source) const;
        void ClearSource() const;

        bool IsValid() const override
        { return mShader.IsValid(); }
        std::string GetName() const override
        { return mName; }
        std::string GetError() const override
        { return mCompileInfo; }

        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }

        inline dev::GraphicsShader GetShader() const noexcept
        { return mShader; }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::GraphicsShader mShader;
        std::string mName;
        std::string mCompileInfo;
        mutable std::string mSource;
    };

} // namespace