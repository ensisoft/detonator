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

#include "game/entity_node_text_item.h"

#include "data/reader.h"
#include "data/writer.h"

namespace game
{
size_t TextItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mBitFlags.value());
    hash = base::hash_combine(hash, mHAlign);
    hash = base::hash_combine(hash, mVAlign);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mText);
    hash = base::hash_combine(hash, mFontName);
    hash = base::hash_combine(hash, mFontSize);
    hash = base::hash_combine(hash, mRasterWidth);
    hash = base::hash_combine(hash, mRasterHeight);
    hash = base::hash_combine(hash, mLineHeight);
    hash = base::hash_combine(hash, mTextColor);
    return hash;
}

void TextItemClass::IntoJson(data::Writer& data) const
{
    data.Write("flags",            mBitFlags);
    data.Write("horizontal_align", mHAlign);
    data.Write("vertical_align",   mVAlign);
    data.Write("layer",            mLayer);
    data.Write("text",             mText);
    data.Write("font_name",        mFontName);
    data.Write("font_size",        mFontSize);
    data.Write("raster_width",     mRasterWidth);
    data.Write("raster_height",    mRasterHeight);
    data.Write("line_height",      mLineHeight);
    data.Write("text_color",       mTextColor);
}


bool TextItemClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("flags",            &mBitFlags);
    ok &= data.Read("horizontal_align", &mHAlign);
    ok &= data.Read("vertical_align",   &mVAlign);
    ok &= data.Read("layer",            &mLayer);
    ok &= data.Read("text",             &mText);
    ok &= data.Read("font_name",        &mFontName);
    ok &= data.Read("font_size",        &mFontSize);
    ok &= data.Read("raster_width",     &mRasterWidth);
    ok &= data.Read("raster_height",    &mRasterHeight);
    ok &= data.Read("line_height",      &mLineHeight);
    ok &= data.Read("text_color",       &mTextColor);
    return ok;
}
} // namespace