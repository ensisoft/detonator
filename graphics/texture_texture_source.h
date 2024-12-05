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

#pragma once

#include "config.h"

#include "base/hash.h"
#include "graphics/texture_source.h"

namespace gfx
{
    class TextureTextureSource : public TextureSource
    {
    public:
        TextureTextureSource()
          : mId(base::RandomString(10))
        {}
        //  Persistence only works with a known GPU texture ID.
        explicit TextureTextureSource(std::string gpu_id, gfx::Texture* texture =  nullptr, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mGpuId(std::move(gpu_id))
          , mTexture(texture)
        {}
        virtual ColorSpace GetColorSpace() const override
        { return ColorSpace::Linear; }
        virtual Source GetSourceType() const override
        { return Source::Texture; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetGpuId() const override
        { return mGpuId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual std::size_t GetHash() const override
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mName);
            hash = base::hash_combine(hash, mGpuId);
            return hash;
        }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual std::shared_ptr<IBitmap> GetData() const override
        { return nullptr; }
        virtual Texture* Upload(const Environment& env, Device& device) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;

        inline void SetTexture(gfx::Texture* texture) noexcept
        { mTexture = texture; }
    protected:
        virtual std::unique_ptr<TextureSource> MakeCopy(std::string copy_id) const override
        {
            auto ret = std::make_unique<TextureTextureSource>(*this);
            ret->mId = std::move(copy_id);
            return ret;
        }
    private:
        std::string mId;
        std::string mName;
        std::string mGpuId;
        mutable gfx::Texture* mTexture = nullptr;
    };

    inline auto  UseExistingTexture(std::string gpu_id, gfx::Texture* texture = nullptr, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureTextureSource>(std::move(gpu_id), texture, std::move(id));
    }

} // namespacex