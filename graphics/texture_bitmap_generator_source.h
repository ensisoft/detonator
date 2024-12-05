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
#include "graphics/bitmap.h"
#include "graphics/bitmap_generator.h"
#include "graphics/bitmap_noise.h"
#include "graphics/texture_source.h"

namespace gfx
{
    // Source texture data from a bitmap
    class TextureBitmapGeneratorSource : public TextureSource
    {
    public:
        TextureBitmapGeneratorSource()
          : mId(base::RandomString(10))
        {}

        explicit TextureBitmapGeneratorSource(std::unique_ptr<IBitmapGenerator>&& generator, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mGenerator(std::move(generator))
        {}

        TextureBitmapGeneratorSource(const TextureBitmapGeneratorSource& other)
          : mId(other.mId)
          , mName(other.mName)
          , mGenerator(other.mGenerator->Clone())
        {}

        base::bitflag<Effect> GetEffects() const override
        { return mEffects; }
        Source GetSourceType() const override
        { return Source::BitmapGenerator; }
        std::string GetId() const override
        { return mId; }
        std::string GetGpuId() const override
        { return mId; }
        std::size_t GetHash() const override
        {
            auto hash = mGenerator->GetHash();
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mName);
            hash = base::hash_combine(hash, mEffects);
            return hash;
        }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        void SetEffect(Effect effect, bool on_off) override
        { mEffects.set(effect, on_off); }
        std::shared_ptr<IBitmap> GetData() const override
        { return mGenerator->Generate(); }

        Texture* Upload(const Environment& env, Device& device) const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

        IBitmapGenerator& GetGenerator()
        { return *mGenerator; }
        const IBitmapGenerator& GetGenerator() const
        { return *mGenerator; }

        void SetGenerator(std::unique_ptr<IBitmapGenerator> generator)
        { mGenerator = std::move(generator); }

        template<typename T>
        void SetGenerator(T&& generator)
        {
            // generator is a "universal reference"
            // Meyer's Item. 24
            mGenerator = std::make_unique<std::remove_reference_t<T>>(std::forward<T>(generator));
        }
    protected:
        virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
        {
            auto ret = std::make_unique<TextureBitmapGeneratorSource>(*this);
            ret->mId = std::move(id);
            return ret;
        }
    private:
        std::string mId;
        std::string mName;
        std::unique_ptr<IBitmapGenerator> mGenerator;
        base::bitflag<Effect> mEffects;
    };

    inline auto GenerateNoiseTexture(const NoiseBitmapGenerator& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(generator);
        return std::make_unique<TextureBitmapGeneratorSource>(std::move(gen), std::move(id));
    }
    inline auto GenerateNoiseTexture(NoiseBitmapGenerator&& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<NoiseBitmapGenerator>(std::move(generator));
        return std::make_unique<TextureBitmapGeneratorSource>(std::move(gen), std::move(id));
    }

    inline auto GenerateTexture(std::unique_ptr<IBitmapGenerator> generator, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureBitmapGeneratorSource>(std::move(generator), std::move(id));
    }

    template<typename T> inline
    auto GenerateTexture(T&& generator, std::string id = base::RandomString(10))
    {
        auto gen = std::make_unique<std::remove_reference_t<T>>(std::forward<T>(generator));
        return GenerateTexture(std::move(gen), std::move(id));
    }

} // namespace