// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "base/assert.h"
#include "graphics/bitmap.h"
#include "graphics/texture_map.h"
#include "graphics/texture_source.h"
#include "graphics/material_class_api.h"

namespace gfx
{
namespace detail {
    std::shared_ptr<const IBitmap> MaterialClassBaseTexture::ExtractPreview() const
    {
        const auto map_count = mClass->GetNumTextureMaps();
        if (map_count == 0)
            return nullptr;

        const auto* texture = mClass->GetTextureMap(0);
        if (!texture->GetNumTextures())
            return nullptr;

        const auto* texture_source = texture->GetTextureSource(0);
        const auto& texture_source_rect = texture->GetTextureRect(0);
        const auto& texture_source_data = texture_source->GetData();
        if (!texture_source_data)
            return nullptr;

        const auto texture_rect = base::MapToGlobalExpand(texture_source_data->GetRect(),
                                                  texture_source_rect);
        if (texture_rect == texture_source_data->GetRect())
            return std::move(texture_source_data);

        return texture_source_data->CopyRect(texture_rect);
    }
} // namespace

size_t TilemapMaterialClass::GetTileCount() const noexcept
{
    const auto* tilemap = GetTilemap();
    if (!tilemap)
        return 0;

    const auto bitmap_width  = tilemap->GetWidth();
    const auto bitmap_height = tilemap->GetHeight();

    const auto [tile_width, tile_height] = GetTileSizeU();
    const auto [padding_x,  padding_y] = GetTilePaddingU();
    const auto [offset_x, offset_y] = GetTileOffsetU();

    const auto tile_box_width = tile_width + 2*padding_x;
    const auto tile_box_height = tile_height + 2*padding_y;

    const auto tile_grid_width = bitmap_width - offset_x;
    const auto tile_grid_height = bitmap_height - offset_y;

    const auto num_cols = tile_grid_width  / tile_box_width;
    const auto num_rows = tile_grid_height / tile_box_height;
    const auto tile_count = num_cols * num_rows;

    return tile_count;
}

std::shared_ptr<const IBitmap> TilemapMaterialClass::ExtractTile(size_t index) const
{
    const auto* tilemap = GetTilemap();
    if (!tilemap)
        return nullptr;

    const auto bitmap_width  = tilemap->GetWidth();
    const auto bitmap_height = tilemap->GetHeight();

    const auto [tile_width, tile_height] = GetTileSizeU();
    const auto [padding_x,  padding_y] = GetTilePaddingU();
    const auto [offset_x, offset_y] = GetTileOffsetU();

    const auto tile_box_width = tile_width + 2*padding_x;
    const auto tile_box_height = tile_height + 2*padding_y;

    if (offset_x > bitmap_width || offset_y > bitmap_height)
        return nullptr;

    const auto tile_grid_width = bitmap_width - offset_x;
    const auto tile_grid_height = bitmap_height - offset_y;

    const auto num_cols = tile_grid_width  / tile_box_width;
    const auto num_rows = tile_grid_height / tile_box_height;
    const auto tile_count = num_cols * num_rows;
    if (index >= tile_count)
        return nullptr;

    const auto tile_row = index / num_cols;
    const auto tile_col = index % num_cols;

    // sanity
    ASSERT(tile_row < num_rows);
    ASSERT(tile_col < num_cols);

    const auto x = offset_x + tile_col * tile_box_width + padding_x;
    const auto y = offset_y + tile_row * tile_box_height + padding_y;
    const auto end_x = x + tile_width;
    const auto end_y = y + tile_height;

    ASSERT(end_x <= bitmap_width);
    ASSERT(end_y <= bitmap_height);
    return tilemap->CopyRect(URect(x, y, tile_width, tile_height));
}

const IBitmap *TilemapMaterialClass::GetTilemap() const
{
    if (mTilemap == nullptr)
    {
        const auto texture_map_count = mClass->GetNumTextureMaps();
        if (texture_map_count != 1)
            return nullptr;

        const auto* texture_map = mClass->GetTextureMap(0);
        const auto texture_src_count = texture_map->GetNumTextures();
        if (texture_src_count != 1)
            return nullptr;

        const auto* texture_src = texture_map->GetTextureSource(0);
        const auto texture_rect = texture_map->GetTextureRect(0);
        const auto& bitmap_src_data = texture_src->GetData();
        if (!bitmap_src_data)
            return nullptr;

        mTilemap = bitmap_src_data->CopyRect(
            MapToGlobalExpand(bitmap_src_data->GetRect(), texture_rect));
    }
    return mTilemap.get();
}


} // namespace
