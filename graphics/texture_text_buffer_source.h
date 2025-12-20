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

#include <string>
#include <memory>

#include "base/bitflag.h"
#include "base/utility.h"
#include "graphics/text_buffer.h"
#include "graphics/texture_source.h"

namespace gfx
{
    // Rasterize text buffer and provide as a texture source.
    class TextureTextBufferSource : public TextureSource
    {
    public:
        TextureTextBufferSource()
          : mId(base::RandomString(10))
        {}

        explicit TextureTextBufferSource(const TextBuffer& text, std::string id = base::RandomString(10))
          : mId(std::move(id))
          , mTextBuffer(text)
        {}

        explicit TextureTextBufferSource(TextBuffer&& text, std::string id = base::RandomString(10)) noexcept
          : mId(std::move(id))
          , mTextBuffer(std::move(text))
        {}

        base::bitflag<Effect> GetEffects() const override
        {return mEffects; }
        Source GetSourceType() const override
        { return Source::TextBuffer; }
        std::string GetId() const override
        { return mId; }
        std::string GetGpuId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        void SetEffect(Effect effect, bool on_off) override
        { mEffects.set(effect, on_off); }

        std::shared_ptr<const IBitmap> GetData() const override;
        std::size_t GetHash() const override;
        Texture* Upload(const Environment&env, Device& device) const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

        TextBuffer& GetTextBuffer()
        { return mTextBuffer; }
        const TextBuffer& GetTextBuffer() const
        { return mTextBuffer; }

        void SetTextBuffer(const TextBuffer& text)
        { mTextBuffer = text; }
        void SetTextBuffer(TextBuffer&& text)
        { mTextBuffer = std::move(text); }
    protected:
        std::unique_ptr<TextureSource> MakeCopy(std::string id) const override
        {
            auto ret = std::make_unique<TextureTextBufferSource>(*this);
            ret->mId = std::move(id);
            return ret;
        }

    private:
        std::string mId;
        std::string mName;
        TextBuffer mTextBuffer;
        base::bitflag<Effect> mEffects;
    };

    inline auto CreateTextureFromText(const TextBuffer& text, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureTextBufferSource>(text, std::move(id));
    }
    inline auto CreateTextureFromText(TextBuffer&& text, std::string id = base::RandomString(10))
    {
        return std::make_unique<TextureTextBufferSource>(std::move(text), std::move(id));
    }

} // namespace
