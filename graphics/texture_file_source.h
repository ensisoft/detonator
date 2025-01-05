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

    // Source texture data from an image file.
    class TextureFileSource : public TextureSource
    {
    public:
        enum class Flags {
            AllowPacking,
            AllowResizing,
            PremulAlpha
        };
        TextureFileSource()
          : mId(base::RandomString(10))
        {
            mFlags.set(Flags::AllowResizing, true);
            mFlags.set(Flags::AllowPacking, true);
        }
        explicit TextureFileSource(std::string file, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mFile(std::move(file))
        {
            mFlags.set(Flags::AllowResizing, true);
            mFlags.set(Flags::AllowPacking, true);
        }
        ColorSpace GetColorSpace() const override
        { return mColorSpace; }
        Source GetSourceType() const override
        { return Source::Filesystem; }
        base::bitflag<Effect> GetEffects() const override
        { return mEffects; }
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        void SetEffect(Effect effect, bool on_off) override
        { mEffects.set(effect, on_off); }
        void SetColorSpace(ColorSpace colorspace)
        { mColorSpace = colorspace; }

        std::string GetGpuId() const override;
        std::size_t GetHash() const override;
        std::shared_ptr<IBitmap> GetData() const override;
        Texture* Upload(const Environment& env, Device& device) const override;

        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
        void BeginPacking(TexturePacker* packer) const override;
        void FinishPacking(const TexturePacker* packer) override;

        void SetFileName(const std::string& file)
        { mFile = file; }
        const std::string& GetFilename() const
        { return mFile; }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
    protected:
        virtual std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
        {
            auto ret = std::make_unique<TextureFileSource>(*this);
            ret->mId = std::move(id);
            return ret;
        }
    private:
        std::string mId;
        std::string mFile;
        std::string mName;
        base::bitflag<Flags> mFlags;
        base::bitflag<Effect> mEffects;
        ColorSpace mColorSpace = ColorSpace::sRGB;
    private:
    };


    inline auto LoadTextureFromFile(std::string uri, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureFileSource>(std::move(uri), std::move(id));
    }

} // namespace