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
        explicit TextureTextureSource(std::string gpu_id,  std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mGpuId(std::move(gpu_id))
        {}
        explicit TextureTextureSource(gfx::Texture* texture, std::string id = base::RandomString((10)));

        ColorSpace GetColorSpace() const override;

        Source GetSourceType() const override
        { return Source::Texture; }
        std::string GetId() const override
        { return mId; }
        std::string GetGpuId() const override
        { return mGpuId; }
        std::string GetName() const override
        { return mName; }
        std::size_t GetHash() const override
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mName);
            hash = base::hash_combine(hash, mGpuId);
            return hash;
        }
        void SetName(const std::string& name) override
        { mName = name; }
        std::shared_ptr<const IBitmap> GetData() const override
        { return nullptr; }
        Texture* Upload(const Environment& env, Device& device) const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

        void SetTexture(Texture* texture);
        void SetTextureGpuId(std::string texture_gpu_id);
    protected:
        std::unique_ptr<TextureSource> MakeCopy(std::string copy_id) const override
        {
            auto ret = std::make_unique<TextureTextureSource>(*this);
            ret->mId = std::move(copy_id);
            return ret;
        }
    private:
        std::string mId;
        std::string mName;
        std::string mGpuId;
        mutable Texture* mTexture = nullptr;
    };

    inline auto  UseExistingTexture(Texture* texture, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureTextureSource>(texture, std::move(id));
    }

} // namespacex