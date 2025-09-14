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

#include <cstddef>
#include <string>
#include <memory>

#include "base/bitflag.h"
#include "base/hash.h"
#include "data/fwd.h"
#include "game/color.h"
#include "game/enum.h"

namespace game
{

    // TextItem allows human-readable text entity node attachment with
    // some simple properties that defined how the text should look.
    class TextItemClass
    {
    public:
        // How to align the text inside the node vertically.
        enum class HorizontalTextAlign {
            // Align to the node's left edge.
            Left,
            // Align around center of the node.
            Center,
            // Align to the node's right edge.
            Right
        };
        // How to align the text inside the node vertically.
        enum class VerticalTextAlign {
            // Align to the top of the node.
            Top,
            // Align around the center of the node.
            Center,
            // Align to the bottom of the node.
            Bottom
        };
        enum class Flags {
            // Whether the item is currently visible or not.
            VisibleInGame,
            // Make the text blink annoyingly
            BlinkText,
            // Set text to underline
            UnderlineText,
            // Static content, i.e. the text/color/ etc. properties
            // are not expected to change.
            StaticContent,
            // Contribute to bloom post-processing effect.
            PP_EnableBloom,
            // Enable light on this text item  (if the scene is lit)
            EnableLight
        };
        using CoordinateSpace = game::CoordinateSpace;

        TextItemClass()
        {
            mBitFlags.set(Flags::VisibleInGame, true);
            mBitFlags.set(Flags::PP_EnableBloom, true);
            mBitFlags.set(Flags::EnableLight, true);
        }
        std::size_t GetHash() const;

        // class setters
        void SetText(const std::string& text)
        { mText = text; }
        void SetText(std::string&& text) noexcept
        { mText = std::move(text); }
        void SetFontName(const std::string& font)
        { mFontName = font; }
        void SetFontSize(unsigned size) noexcept
        { mFontSize = size; }
        void SetLayer(int layer) noexcept
        { mLayer = layer; }
        void SetLineHeight(float height) noexcept
        { mLineHeight = height; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetAlign(VerticalTextAlign align) noexcept
        { mVAlign = align; }
        void SetAlign(HorizontalTextAlign align) noexcept
        { mHAlign = align; }
        void SetTextColor(const Color4f& color) noexcept
        { mTextColor = color; }
        void SetRasterWidth(unsigned width) noexcept
        { mRasterWidth = width;}
        void SetRasterHeight(unsigned height) noexcept
        { mRasterHeight = height; }
        void SetCoordinateSpace(CoordinateSpace space) noexcept
        { mCoordinateSpace = space; }

        // class getters
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        bool IsStatic() const noexcept
        { return TestFlag(Flags::StaticContent); }
        const Color4f& GetTextColor() const noexcept
        { return mTextColor; }
        const std::string& GetText() const noexcept
        { return mText; }
        const std::string& GetFontName() const noexcept
        { return mFontName; }
        int GetLayer() const noexcept
        { return mLayer; }
        float GetLineHeight() const noexcept
        { return mLineHeight; }
        unsigned GetFontSize() const noexcept
        { return mFontSize; }
        unsigned GetRasterWidth() const noexcept
        { return mRasterWidth; }
        unsigned GetRasterHeight() const noexcept
        { return mRasterHeight; }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }
        HorizontalTextAlign GetHAlign() const noexcept
        { return mHAlign; }
        VerticalTextAlign GetVAlign() const noexcept
        { return mVAlign; }
        CoordinateSpace  GetCoordinateSpace() const noexcept
        { return mCoordinateSpace; }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        // item's bit flags
        base::bitflag<Flags> mBitFlags;
        HorizontalTextAlign mHAlign = HorizontalTextAlign::Center;
        VerticalTextAlign mVAlign = VerticalTextAlign::Center;
        int mLayer = 0;
        std::string mText;
        std::string mFontName;
        unsigned mFontSize = 0;
        unsigned mRasterWidth = 0;
        unsigned mRasterHeight = 0;
        float mLineHeight = 1.0f;
        Color4f mTextColor = Color::White;
        CoordinateSpace mCoordinateSpace = CoordinateSpace::Scene;
    };

    class TextItem
    {
    public:
        using Flags = TextItemClass::Flags;
        using VerticalTextAlign = TextItemClass::VerticalTextAlign;
        using HorizontalTextAlign = TextItemClass::HorizontalTextAlign;
        using CoordinateSpace = TextItemClass::CoordinateSpace;

        explicit TextItem(std::shared_ptr<const TextItemClass> klass) noexcept
          : mClass(std::move(klass))
        {
            mText  = mClass->GetText();
            mFlags = mClass->GetFlags();
            mTextColor = mClass->GetTextColor();
        }

        // instance getters.
        const Color4f& GetTextColor() const noexcept
        { return mTextColor; }
        const std::string& GetText() const noexcept
        { return mText; }
        const std::string& GetFontName() const noexcept
        { return mClass->GetFontName(); }
        unsigned GetFontSize() const noexcept
        { return mClass->GetFontSize(); }
        float GetLineHeight() const noexcept
        { return mClass->GetLineHeight(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        unsigned GetRasterWidth() const noexcept
        { return mClass->GetRasterWidth(); }
        unsigned GetRasterHeight() const noexcept
        { return mClass->GetRasterHeight(); }
        CoordinateSpace  GetCoordinateSpace() const noexcept
        { return mClass->GetCoordinateSpace(); }
        HorizontalTextAlign GetHAlign() const noexcept
        { return mClass->GetHAlign(); }
        VerticalTextAlign GetVAlign() const noexcept
        { return mClass->GetVAlign(); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool IsStatic() const noexcept
        { return TestFlag(Flags::StaticContent); }
        std::size_t GetHash() const noexcept
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mText);
            hash = base::hash_combine(hash, mTextColor);
            hash = base::hash_combine(hash, mFlags);
            return hash;
        }

        // instance setters.
        void SetText(const std::string& text)
        { mText = text; }
        void SetText(std::string&& text) noexcept
        { mText = std::move(text); }
        void SetTextColor(const Color4f& color) noexcept
        { mTextColor = color; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        // class access
        const TextItemClass& GetClass() const noexcept
        { return *mClass.get(); }
        const TextItemClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const TextItemClass> mClass;
        // instance text.
        std::string mText;
        // instance text color
        Color4f mTextColor;
        // instance flags.
        base::bitflag<Flags> mFlags;
    };

} // namespace
