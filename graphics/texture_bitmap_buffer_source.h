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
#include "graphics/texture_source.h"

namespace gfx
{
    // Source texture data from a bitmap
    class TextureBitmapBufferSource : public TextureSource
    {
    public:
        TextureBitmapBufferSource()
          : mId(base::RandomString(10))
        {}
        explicit TextureBitmapBufferSource(std::shared_ptr<const IBitmap> bitmap, std::string id = base::RandomString(10))
            : mId(std::move(id))
            , mBitmap(std::move(bitmap))
        {}

        explicit TextureBitmapBufferSource(std::unique_ptr<IBitmap>&& bitmap, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mBitmap(std::move(bitmap))
        {}

        template<typename T>
        explicit TextureBitmapBufferSource(const Bitmap<T>& bmp, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mBitmap(new Bitmap<T>(bmp))
        {}

        template<typename T>
        explicit TextureBitmapBufferSource(Bitmap<T>&& bmp, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mBitmap(new Bitmap<T>(std::move(bmp)))
        {}

        TextureBitmapBufferSource(const TextureBitmapBufferSource& other)
          : mId(other.mId)
          , mName(other.mName)
          , mBitmap(other.mBitmap)
          , mEffects(other.mEffects)
          , mColorSpace(other.mColorSpace)
          , mGarbageCollect(other.mGarbageCollect)
          , mTransient(other.mTransient)
        {}

        base::bitflag<Effect> GetEffects() const override
        { return mEffects; }
        Source GetSourceType() const override
        { return Source::BitmapBuffer; }
        std::string GetGpuId() const override
        { return mId; }
        std::string GetId() const override
        { return mId; }

        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        std::shared_ptr<const IBitmap> GetData() const override
        { return mBitmap; }
        void SetEffect(Effect effect, bool on_off) override
        { mEffects.set(effect, on_off); }
        ColorSpace GetColorSpace() const override
        { return mColorSpace; }

        std::size_t GetHash()  const override;
        Texture* Upload(const Environment& env, Device& device) const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

        void SetBitmap(std::unique_ptr<IBitmap> bitmap)
        { mBitmap = std::move(bitmap); }
        template<typename T>
        void SetBitmap(const Bitmap<T>& bitmap)
        { mBitmap = std::make_unique<gfx::Bitmap<T>>(bitmap); }
        template<typename T>
        void SetBitmap(Bitmap<T>&& bitmap)
        { mBitmap = std::make_unique<gfx::Bitmap<T>>(std::move(bitmap)); }
        const IBitmap& GetBitmap() const
        { return *mBitmap; }

        void SetColorSpace(ColorSpace colorspace) override
        {
            mColorSpace = colorspace;
        }
        void SetGarbageCollect(bool on_off) noexcept
        {
            mGarbageCollect = on_off;
        }
        void SetTransient(bool on_off) noexcept
        {
            mTransient = on_off;
        }

        template<typename Pixel>
        const Bitmap<Pixel>* GetBitmap() const
        {
            // todo: maybe use some kind of format type.
            const auto bytes = mBitmap->GetDepthBits() / 8;
            if (sizeof(Pixel) == bytes)
                return static_cast<Bitmap<Pixel>*>(mBitmap.get());
            return nullptr;
        }
        template<typename Pixel> inline
        bool GetBitmap(const Bitmap<Pixel>** out) const
        {
            const auto* ret = GetBitmap<Pixel>();
            if (ret) {
                *out = ret;
                return true;
            }
            return false;
        }
    protected:
        std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
        {
            auto ret = std::make_unique<TextureBitmapBufferSource>(*this);
            ret->mId = std::move(id);
            return ret;
        }
    private:
        std::string mId;
        std::string mName;
        std::shared_ptr<const IBitmap> mBitmap;
        base::bitflag<Effect> mEffects;
        ColorSpace mColorSpace = ColorSpace::sRGB;
        bool mGarbageCollect = false;
        bool mTransient = false;
    };

    template<typename T> inline
    auto CreateTextureFromBitmap(const Bitmap<T>& bitmap, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureBitmapBufferSource>(bitmap, id);
    }

    template<typename T> inline
    auto CreateTextureFromBitmap(Bitmap<T>&& bitmap, std::string gpu_id = base::RandomString(10))
    {
        return std::make_unique<TextureBitmapBufferSource>(std::forward<Bitmap<T>>(bitmap), std::move(gpu_id));
    }
    inline auto CreateTextureFromBitmap(std::unique_ptr<IBitmap> bitmap, std::string gpu_id = base::RandomString(10))
    {
        return std::make_unique<TextureBitmapBufferSource>(std::move(bitmap), std::move(gpu_id));
    }

    inline auto CreateTextureFromBitmap(std::shared_ptr<const IBitmap> bitmap, std::string gpu_id = base::RandomString(10))
    {
        return std::make_unique<TextureBitmapBufferSource>(std::move(bitmap), std::move(gpu_id));
    }

} // namespace