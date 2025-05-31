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

#pragma once

#include <vector>

#include "base/types.h"
#include "game/tilemap.h"

namespace gui
{

    // tile selection. dimensions in tiles.
    struct TileSelection {
        unsigned start_row = 0;
        unsigned start_col = 0;
        unsigned width  = 0;
        unsigned height = 0;
        game::TilemapLayerClass::Resolution resolution =
                game::TilemapLayerClass::Resolution::Original;

        struct Tile {
            unsigned x = 0;
            unsigned y = 0;
        };


    public:
        explicit TileSelection(const base::URect& rect);
        TileSelection(unsigned col, unsigned row, unsigned width, unsigned height);
        TileSelection() = default;

        bool IsSelected(unsigned tile_xpos, unsigned tile_ypos) const;
        bool IsSelected(const Tile& tile) const;
        bool DisjointSelection() const;
        bool IsEmpty() const;
        bool HasSelection() const;
        void Clear();
        bool CanCombine(const TileSelection& other) const;

        void Deselect(const Tile& tile);

        bool ShiftSelection(int dx, int dy, unsigned map_width, unsigned map_height);

        // get selection width in tiles. this does not account
        // for holes in the selection, i.e when the selection is
        // disjoint.
        auto GetWidth() const noexcept
        { return width; }
        auto GetHeight() const noexcept
        { return height; }
        auto GetRow() const noexcept
        { return start_row; }
        auto GetCol() const noexcept
        { return start_col; }

        void SetResolution(game::TilemapLayerClass::Resolution res)
        { resolution = res; }
        auto GetResolution() const noexcept
        { return resolution; }
        auto ToRect() const noexcept
        { return base::URect(start_col, start_row, width, height); }
        auto GetTileCount() const noexcept
        { return mTiles.size(); }
        const auto& GetTile(size_t index) const noexcept
        { return mTiles[index]; }

        static TileSelection Combine(const TileSelection& one, const TileSelection& two);
    private:
        std::vector<Tile> mTiles;
    };

} // namespace