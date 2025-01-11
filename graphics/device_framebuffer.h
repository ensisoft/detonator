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

#include "device/device.h"
#include "graphics/framebuffer.h"

namespace gfx
{

    class DeviceFramebuffer : public gfx::Framebuffer
    {
    public:
        DeviceFramebuffer(dev::GraphicsDevice* device, std::string name) noexcept
          : mName(std::move(name))
          , mDevice(device)
        {}
       ~DeviceFramebuffer() override;

        void SetConfig(const Config& conf) override;
        void SetColorTarget(gfx::Texture* texture, ColorAttachment attachment) override;
        void Resolve(gfx::Texture** color, ColorAttachment attachment) const override;

        unsigned GetWidth() const override;
        unsigned GetHeight() const override;

        Format GetFormat() const override
        { return mConfig.format; }

        inline size_t GetFrameStamp() const noexcept
        { return mFrameNumber; }
        inline bool IsReady() const noexcept
        { return mFramebuffer.IsValid(); }
        inline bool IsMultisampled() const noexcept
        { return mConfig.msaa == MSAA::Enabled; }

        inline unsigned GetClientTextureCount() const noexcept
        { return mConfig.color_target_count; }
        inline dev::Framebuffer GetFramebuffer() const noexcept
        { return mFramebuffer; }

        inline const gfx::DeviceTexture* GetClientTexture(unsigned index) const noexcept
        { return mClientTextures[index]; }
        gfx::DeviceTexture* GetColorBufferTexture(unsigned index) const noexcept;

        bool Complete();
        bool Create();
        void SetFrameStamp(size_t stamp);
        void CreateColorBufferTextures();

    private:
        const std::string mName;

        dev::GraphicsDevice* mDevice = nullptr;
        dev::Framebuffer mFramebuffer;

        // Texture target that we allocate when the use hasn't
        // provided a client texture. In case of a single sampled
        // FBO this is used directly as the color attachment
        // in case of multiple sampled this will be used as the
        // resolve target.
        std::vector<std::unique_ptr<gfx::DeviceTexture>> mTextures;

        // Client provided texture(s) that will ultimately contain
        // the rendered result.
        std::vector<gfx::DeviceTexture*> mClientTextures;
        std::size_t mFrameNumber = 0;

        Config mConfig;
    };


} // namespace