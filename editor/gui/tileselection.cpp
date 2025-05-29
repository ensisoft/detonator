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
  : start_row(row)
  , start_col(col)
  , width(width)
  , height(height)
{
    for (unsigned x=0; x<width; ++x)
    {
        for (unsigned y=0; y<height; ++y)
        {
            TileSelection::Tile tile;
            tile.x = start_col + x;
            tile.y = start_row + y;
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
    if (tile_xpos < start_col ||  tile_xpos >= start_col + width)
        return false;
    if (tile_ypos < start_row || tile_ypos >= start_row + height)
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
    if (mTiles.size() != width * height)
        return true;
    return false;
}

bool TileSelection::IsEmpty() const
{
    return width == 0 || height == 0;
}

bool TileSelection::HasSelection() const
{
    return width != 0 && height != 0;
}

void TileSelection::Clear()
{
    width = 0;
    height = 0;
    start_col = 0;
    start_row = 0;
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
    ret.start_col = rect.GetX();
    ret.start_row = rect.GetY();
    ret.width = rect.GetWidth();
    ret.height = rect.GetHeight();
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