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

#include "base/utility.h"
#include "editor/gui/tileselection.h"

namespace gui
{

TileSelection::TileSelection(unsigned col, unsigned row, unsigned width, unsigned height)
  : mStartRow(row)
  , mStartCol(col)
  , mWidth(width)
  , mHeight(height)
{
    for (unsigned x=0; x<width; ++x)
    {
        for (unsigned y=0; y<height; ++y)
        {
            TileSelection::Tile tile;
            tile.x = mStartCol + x;
            tile.y = mStartRow + y;
            mTiles.push_back(tile);
        }
    }
}

TileSelection::TileSelection(const base::URect& rect)
  : TileSelection(rect.GetX(), rect.GetY(),
                  rect.GetWidth(), rect.GetHeight())
{}

bool TileSelection::IsSelected(unsigned tile_xpos, unsigned tile_ypos) const
{
    if (tile_xpos < mStartCol || tile_xpos >= mStartCol + mWidth)
        return false;
    if (tile_ypos < mStartRow || tile_ypos >= mStartRow + mHeight)
        return false;

    for (const auto& tile : mTiles)
    {
        if (tile.x == tile_xpos && tile.y == tile_ypos)
            return true;
    }
    return false;
}

bool TileSelection::IsSelected(const Tile& tile) const
{
    return IsSelected(tile.x, tile.y);
}

bool TileSelection::DisjointSelection() const
{
    if (mTiles.size() != mWidth * mHeight)
        return true;
    return false;
}

bool TileSelection::IsEmpty() const
{
    return mWidth == 0 || mHeight == 0;
}

bool TileSelection::HasSelection() const
{
    return mWidth != 0 && mHeight != 0;
}

void TileSelection::Clear()
{
    mWidth = 0;
    mHeight = 0;
    mStartCol = 0;
    mStartRow = 0;
    mTiles.clear();
}

bool TileSelection::CanCombine(const TileSelection& other) const
{
    if (other.resolution == resolution)
        return true;

    return false;
}

void TileSelection::Deselect(const Tile& tile)
{
    base::EraseRemove(mTiles, [tile](const auto& t) {
        return t.x == tile.x && t.y == tile.y;
    });
    if (mTiles.empty())
    {
        Clear();
    }
}

bool TileSelection::ShiftSelection(int dx, int dy, unsigned map_width, unsigned map_height)
{
    const auto start_col = static_cast<int64_t>(mStartCol);
    const auto start_row = static_cast<int64_t>(mStartRow);
    const auto width     = static_cast<int64_t>(mWidth);
    const auto height    = static_cast<int64_t>(mHeight);

    if (start_col + dx < 0 || start_col + width + dx > map_width)
        dx = 0;

    if (start_row + dy < 0 || start_row + height + dy > map_height)
        dy = 0;

    if (dx == 0 && dy == 0)
        return false;

    mStartRow = start_row + dy;
    mStartCol = start_col + dx;

    for (auto& tile : mTiles)
    {
        tile.x += dx;
        tile.y += dy;
    }

    ASSERT(mStartCol + mWidth <= map_width);
    ASSERT(mStartRow + mHeight <= map_height);
    return true;
}

// static
TileSelection TileSelection::Combine(const TileSelection& one, const TileSelection& two)
{
    if (one.IsEmpty())
        return two;
    else if (two.IsEmpty())
        return one;

    const auto& rect = base::Union(one.ToRect(), two.ToRect());

    ASSERT(one.resolution == two.resolution);

    TileSelection ret;
    ret.mStartCol = rect.GetX();
    ret.mStartRow = rect.GetY();
    ret.mWidth = rect.GetWidth();
    ret.mHeight = rect.GetHeight();
    ret.mTiles = one.mTiles;
    ret.resolution = one.resolution;

    for (unsigned i=0; i<two.GetTileCount(); ++i)
    {
        const auto& tile = two.GetTile(i);
        if (!ret.IsSelected(tile))
            ret.mTiles.push_back(tile);
    }

    return ret;
}

} // namespace