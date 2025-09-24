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

#include <memory>
#include <string>

#include "base/bitflag.h"
#include "device/enum.h"
#include "device/graphics.h"
#include "graphics/texture.h"

namespace gfx {

    class DeviceTexture : public gfx::Texture
    {
    public:
        DeviceTexture(dev::GraphicsDevice* device, std::string id) noexcept
           : mDevice(device)
           , mGpuId(std::move(id))
        {
            mFlags.set(Flags::Transient, false);
            mFlags.set(Flags::GarbageCollect, true);
        }
        ~DeviceTexture() override;

        void Upload(const void* bytes, unsigned width, unsigned height, Format format) override;
        bool GenerateMips() override;

        void SetFlag(Flags flag, bool on_off) override
        { mFlags.set(flag, on_off); }

        // refer actual state setting to the point when
        // the texture is actually used in a program's sampler
        void SetFilter(MinFilter filter) override
        { mMinFilter = filter; }
        void SetFilter(MagFilter filter) override
        { mMagFilter = filter; }
        void SetWrapX(Wrapping w) override
        { mWrapX = w; }
        void SetWrapY(Wrapping w) override
        { mWrapY = w; }

        Texture::MinFilter GetMinFilter() const override
        { return mMinFilter; }
        Texture::MagFilter GetMagFilter() const override
        { return mMagFilter; }
        Texture::Wrapping GetWrapX() const override
        { return mWrapX; }
        Texture::Wrapping GetWrapY() const override
        { return mWrapY; }
        unsigned GetWidth() const override
        { return mWidth; }
        unsigned GetHeight() const override
        { return mHeight; }
        Texture::Format GetFormat() const override
        { return mFormat; }
        void SetContentHash(size_t hash) override
        { mHash = hash; }
        void SetName(const std::string& name) override
        { mName = name; }
        void SetGroup(const std::string& group) override
        { mGroup = group; }
        size_t GetContentHash() const override
        { return mHash; }
        bool TestFlag(Flags flag) const override
        { return mFlags.test(flag); }
        std::string GetName() const override
        { return mName; }
        std::string GetGroup() const override
        { return mGroup; }
        std::string GetId() const override
        { return mGpuId; }
        bool HasMips() const override
        { return mHasMips; }

        // internal
        bool WarnOnce() const
        {
            auto ret = mWarnOnce;
            mWarnOnce = false;
            return ret;
        }

        dev::TextureObject GetTexture() const
        {
            return mTexture;
        }

        inline void SetFrameStamp(size_t frame_number) const noexcept
        {
            mFrameNumber = frame_number;
        }

        inline std::size_t GetFrameStamp() const noexcept
        {
            return mFrameNumber;
        }

    private:
        dev::GraphicsDevice* mDevice = nullptr;
        dev::TextureObject mTexture;

    private:
        MinFilter mMinFilter = MinFilter::Default;
        MagFilter mMagFilter = MagFilter::Default;
        Wrapping mWrapX = Wrapping::Repeat;
        Wrapping mWrapY = Wrapping::Repeat;
        Format mFormat  = Texture::Format::AlphaMask;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;

        std::size_t mHash = 0;
        std::string mName;
        std::string mGroup;
        std::string mGpuId;
        base::bitflag<Flags> mFlags;
        bool mHasMips = false;
        mutable bool mWarnOnce = true;
        mutable std::size_t mFrameNumber = 0;
    };

} // namespace