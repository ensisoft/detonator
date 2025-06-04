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

#include "config.h"

#include <set>

#include "base/utility.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/tilemap_layer_class.h"
#include "game/tilemap_layer_base.h"

namespace {
template<typename Tile>
class SparseTilemapLayer : public game::detail::TilemapLayerLoader<Tile>
{
public:
    using TileCache = typename game::detail::TilemapLayerLoader<Tile>::TileCache;

    struct Header {
        uint32_t magic              = 0x8a23d33d;
        uint32_t version            = 1;
        uint16_t block_width        = 0;
        uint16_t block_height       = 0;
        uint32_t block_count        = 0;
    };
    struct BlockHeader {
        uint32_t index = 0;
    };

    static void Initialize(const game::TilemapLayerClass& klass,
                           game::TilemapData& data,
                           unsigned map_width,
                           unsigned map_height)
    {

        const auto layer_width  = klass.MapDimension(map_width);
        const auto layer_height = klass.MapDimension(map_height);

        unsigned block_width  = 0;
        unsigned block_height = 0;
        game::TilemapLayerClass::GetSparseBlockSize(klass.GetTileDataSize(),
                                                    layer_width, layer_height,
                                                    &block_width, &block_height);
        Header header;
        header.block_height = block_height;
        header.block_width  = block_width;
        data.Resize(sizeof(header));
        data.Write(&header, sizeof(header), 0);
        DEBUG("Initialized tilemap layer on data. [layer_width=%1, layer_height=%2, block_width=%3, block_height=%4]",
              layer_width, layer_height, block_width, block_height);

    }
    static void ResizeCopy(const game::TilemapLayerClass& klass,
                           const game::USize& src_map_size,
                           const game::USize& dst_map_size,
                           const game::TilemapData& src,
                           game::TilemapData& dst)
    {
        SparseTilemapLayer<Tile> src_layer;
        SparseTilemapLayer<Tile> dst_layer;

        src_layer.LoadState(src);
        dst_layer.LoadState(dst);

        const auto src_layer_width_tiles = klass.MapDimension(src_map_size.GetWidth());
        const auto src_layer_height_tiles = klass.MapDimension(src_map_size.GetHeight());
        const auto dst_layer_width_tiles = klass.MapDimension(dst_map_size.GetWidth());
        const auto dst_layer_height_tiles = klass.MapDimension(dst_map_size.GetHeight());

        const auto& default_tile = klass.template GetDefaultTileValue<Tile>();

        const auto max_rows = std::min(src_layer_height_tiles, dst_layer_height_tiles);
        const auto max_cols = std::min(src_layer_width_tiles, dst_layer_width_tiles);

        TileBlock* src_block = nullptr;
        TileBlock* dst_block = nullptr;

        auto& src_blocks = src_layer.mBlocks;
        auto& dst_blocks = dst_layer.mBlocks;

        const auto src_block_width  = src_layer.mBlockWidth;
        const auto src_block_height = src_layer.mBlockHeight;
        const auto dst_block_width  = dst_layer.mBlockWidth;
        const auto dst_block_height = dst_layer.mBlockHeight;

        const auto src_width_blocks  = base::EvenMultiple(src_layer_width_tiles, src_block_width) / src_block_width;
        const auto src_height_blocks = base::EvenMultiple(src_layer_height_tiles, src_block_height) / src_block_height;
        const auto dst_width_blocks  = base::EvenMultiple(dst_layer_width_tiles, dst_block_width) / dst_block_width;
        const auto dst_height_blocks = base::EvenMultiple(dst_layer_height_tiles, dst_block_height) / dst_block_height;

        for (unsigned row=0; row<max_rows; ++row)
        {
            for (unsigned col=0; col<max_cols; ++col)
            {
                const auto src_block_row = row / src_block_height;
                const auto dst_block_row = row / dst_block_height;
                const auto src_block_col = col / src_block_width;
                const auto dst_block_col = col / dst_block_width;
                const auto src_block_index = src_block_row * src_width_blocks + src_block_col;
                const auto dst_block_index = dst_block_row * dst_width_blocks + dst_block_col;
                const auto inside_src_block_row = row & (src_block_height - 1);
                const auto inside_src_block_col = col & (src_block_width - 1);
                const auto inside_dst_block_row = row & (dst_block_height - 1);
                const auto inside_dst_block_col = col & (dst_block_width - 1);
                const auto inside_dst_block_tile_index = inside_dst_block_row * dst_block_width + inside_dst_block_col;
                const auto inside_src_block_tile_index = inside_src_block_row * src_block_width + inside_src_block_col;

                // sanity.
                ASSERT(src_block_row < src_height_blocks && src_block_col < src_width_blocks);
                ASSERT(dst_block_row < dst_height_blocks && dst_block_col < dst_width_blocks);
                ASSERT(inside_src_block_row < src_block_height && inside_src_block_col < src_block_width);
                ASSERT(inside_dst_block_row < dst_block_height && inside_dst_block_col < dst_block_width);

                if ((src_block == nullptr) || (src_block->block_index != src_block_index))
                {
                    src_block = nullptr;
                    auto it = std::lower_bound(src_blocks.begin(), src_blocks.end(), src_block_index,
                        [](const TileBlock& block, size_t index) {
                            return block.block_index < index;
                        });
                    if (it != src_blocks.end() && (*it).block_index == src_block_index)
                        src_block = &(*it);
                }
                if (src_block == nullptr)
                    continue;

                Tile value;
                src.Read(&value, sizeof(Tile), src_block->data_byte_offset + inside_src_block_tile_index * sizeof(Tile));
                if (value == default_tile)
                    continue;

                if ((dst_block == nullptr) || (dst_block->block_index != dst_block_index))
                {
                    dst_block = nullptr;

                    auto it = std::lower_bound(dst_blocks.begin(), dst_blocks.end(), dst_block_index,
                        [](const TileBlock& block, size_t index) {
                            return block.block_index < index;
                        });
                    if (it == dst_blocks.end() || (*it).block_index != dst_block_index)
                    {
                        const auto block_size_bytes  = dst_block_height * dst_block_width * sizeof(Tile);
                        const auto block_base_offset = dst.AppendChunk(block_size_bytes + sizeof(BlockHeader));
                        const auto block_data_offset = block_base_offset + sizeof(BlockHeader);
                        BlockHeader header;
                        header.index = dst_block_index;
                        dst.Write(&header, sizeof(header), block_base_offset);

                        TileBlock next_block;
                        next_block.block_index      = dst_block_index;
                        next_block.data_byte_offset = block_data_offset;
                        dst.ClearChunk(&default_tile, sizeof(Tile), block_data_offset, dst_block_height*dst_block_width);
                        it = dst_blocks.insert(it, next_block);
                    }
                    dst_block = &(*it);
                }
                dst.Write(&value, sizeof(Tile), dst_block->data_byte_offset + inside_dst_block_tile_index * sizeof(Tile));
            }
        }
        dst_layer.SaveState(dst);
    }

    virtual void LoadState(const game::TilemapData& data) override
    {
        Header header;
        data.Read(&header, sizeof(header), 0);

        std::vector<TileBlock> blocks;
        blocks.reserve(header.block_count);

        for (uint32_t i=0; i<header.block_count; ++i)
        {
            const auto block_size_bytes = header.block_width * header.block_height * sizeof(Tile) + sizeof(BlockHeader);
            const auto block_base_offset = block_size_bytes * i + sizeof(Header);
            const auto block_data_offset = block_base_offset + sizeof(BlockHeader);
            BlockHeader block_header;
            data.Read(&block_header, sizeof(block_header), block_base_offset);
            TileBlock block;
            block.block_index = block_header.index;
            block.data_byte_offset = block_data_offset;
            blocks.push_back(block);
        }
        std::sort(blocks.begin(), blocks.end(), [](const TileBlock& lhs, const TileBlock& rhs) {
            return lhs.block_index < rhs.block_index;
        });
        mBlockWidth  = header.block_width;
        mBlockHeight = header.block_height;
        mBlocks      = std::move(blocks);
    }
    virtual void SaveState(game::TilemapData& data) const override
    {
        Header header;
        header.block_count        = mBlocks.size();
        header.block_width        = mBlockWidth;
        header.block_height       = mBlockHeight;
        data.Write(&header, sizeof(header), 0);
    }

    virtual void LoadCache(const game::TilemapData& data, const Tile& default_tile,
                           TileCache& cache, size_t cache_index,
                           unsigned layer_width_tiles,
                           unsigned layer_height_tiles) const override
    {
        const auto layer_tile_count  = layer_width_tiles * layer_height_tiles;
        const auto cache_size_tiles  = cache.size();
        const auto cache_index_tiles = cache_index * cache_size_tiles;
        const auto max_tiles = std::min(cache_size_tiles, layer_tile_count - cache_index_tiles);

        // the size of the layer in even blocks.
        const auto layer_width_blocks  = base::EvenMultiple(layer_width_tiles, mBlockWidth) / mBlockWidth;
        const auto layer_height_blocks = base::EvenMultiple(layer_height_tiles, mBlockHeight) / mBlockHeight;

        TileBlock* block = nullptr;

        for (size_t i=0; i<max_tiles; ++i)
        {
            const auto tile_index   = cache_index_tiles + i;
            // index of the tile in the layer expressed in layer row and column.
            const auto tile_row    = tile_index / layer_width_tiles;
            const auto tile_col    = tile_index % layer_width_tiles;
            // index of the tile expressed in block row and column.
            const auto block_row   = tile_row / mBlockHeight;
            const auto block_col   = tile_col / mBlockWidth;
            // index of the block
            const auto block_index = block_row * layer_width_blocks + block_col;
            // offset of the tile within the block.
            const auto inside_block_row = tile_row & (mBlockHeight - 1);
            const auto inside_block_col = tile_col & (mBlockWidth - 1);
            const auto inside_block_tile_index = inside_block_row * mBlockWidth + inside_block_col;
            // sanity.
            ASSERT(tile_row < layer_height_tiles && tile_col < layer_width_tiles);
            ASSERT(block_row < layer_height_blocks && block_col < layer_width_blocks);
            ASSERT(inside_block_row < mBlockHeight && inside_block_col < mBlockWidth);

            if ((block == nullptr) || (block->block_index != block_index))
            {
                block = nullptr;
                // lower bound returns an iterator pointing to a first value in the range
                // such that the contained value is equal or greater than searched value
                // or end() when no such value is found.
                auto it = std::lower_bound(mBlocks.begin(), mBlocks.end(), block_index,
                    [](const TileBlock& block, size_t index) {
                        return block.block_index < index;
                    });
                if (it != mBlocks.end() && (*it).block_index == block_index)
                    block = &(*it);
            }
            if (block)
            {
                data.Read(&cache[i], sizeof(Tile), block->data_byte_offset + inside_block_tile_index * sizeof(Tile));
            }
            else
            {
                cache[i] = default_tile;
            }
        }
    }
    virtual void SaveCache(game::TilemapData& data, const Tile& default_tile,
                           const TileCache& cache, size_t cache_index,
                           unsigned layer_width_tiles,
                           unsigned layer_height_tiles) override
    {

        const auto layer_tile_count  = layer_width_tiles * layer_height_tiles;
        const auto cache_size_tiles  = cache.size();
        const auto cache_index_tiles = cache_index * cache_size_tiles;
        const auto max_tiles = std::min(cache_size_tiles, layer_tile_count - cache_index_tiles);

        // the size of the layer in even blocks.
        const auto layer_width_blocks  = base::EvenMultiple(layer_width_tiles, mBlockWidth) / mBlockWidth;
        const auto layer_height_blocks = base::EvenMultiple(layer_height_tiles, mBlockHeight) / mBlockHeight;

        TileBlock* block = nullptr;

        for (size_t i=0; i<max_tiles; ++i)
        {
            // this optimization breaks when the intention is to actually
            // "remove" the value previously written to a tile.
            //if (cache[i] == default_tile)
            //    continue;

            // index of the tile in the layer
            const auto tile_index  = cache_index_tiles + i;
            // index of the tile in the layer expressed in layer row and column.
            const auto tile_row    = tile_index / layer_width_tiles;
            const auto tile_col    = tile_index % layer_width_tiles;
            // index of the tile expressed in block row and column.
            const auto block_row   = tile_row / mBlockHeight;
            const auto block_col   = tile_col / mBlockWidth;
            // index of the block
            const auto block_index = block_row * layer_width_blocks + block_col;
            // offset of the tile within the block.
            const auto inside_block_row = tile_row & (mBlockHeight - 1);
            const auto inside_block_col = tile_col & (mBlockWidth - 1);
            const auto inside_block_tile_index = inside_block_row * mBlockWidth + inside_block_col;

            // sanity.
            ASSERT(tile_row < layer_height_tiles && tile_col < layer_width_tiles);
            ASSERT(block_row < layer_height_blocks && block_col < layer_width_blocks);
            ASSERT(inside_block_row < mBlockHeight && inside_block_col < mBlockWidth);

            if ((block == nullptr) || (block->block_index != block_index))
            {
                // lower bound returns an iterator pointing to a first value in the range
                // such that the contained value is equal or greater than searched value
                // or end() when no such value is found.
                auto it = std::lower_bound(mBlocks.begin(), mBlocks.end(), block_index,
                    [](const TileBlock& block, size_t index) {
                        return block.block_index < index;
                    });

                if (it == mBlocks.end() && cache[i] == default_tile)
                    continue;

                if (it == mBlocks.end() || (*it).block_index != block_index)
                {
                    const auto block_size_bytes = mBlockHeight * mBlockWidth * sizeof(Tile);
                    const auto block_base_offset = data.AppendChunk(block_size_bytes + sizeof(BlockHeader));
                    const auto block_data_offset = block_base_offset + sizeof(BlockHeader);
                    BlockHeader header;
                    header.index = block_index;
                    data.Write(&header, sizeof(header), block_base_offset);

                    TileBlock next_block;
                    next_block.block_index      = block_index;
                    next_block.data_byte_offset = block_data_offset;
                    data.ClearChunk(&default_tile, sizeof(Tile), block_data_offset, mBlockHeight*mBlockWidth);
                    it = mBlocks.insert(it, next_block);
                }
                block = &(*it);
            }
            data.Write(&cache[i], sizeof(Tile),  block->data_byte_offset + inside_block_tile_index * sizeof(Tile));
        }
    }
    virtual size_t GetByteCount() const override
    {
        return mBlocks.size() * sizeof(TileBlock);
    }
private:
    struct TileBlock {
        uint32_t block_index = 0;
        // offset into the data buffer where the block's data is.
        uint32_t data_byte_offset = 0;
        bool operator < (const TileBlock& other) const
        {
            return block_index < other.block_index;
        }
    };
    uint32_t mBlockWidth  = 32;
    uint32_t mBlockHeight = 32;
    mutable std::vector<TileBlock> mBlocks;
};

template<typename Tile>
class DenseTilemapLayer : public game::detail::TilemapLayerLoader<Tile>
{
public:
    using TileCache = typename game::detail::TilemapLayerLoader<Tile>::TileCache;

    struct Header {
        uint32_t magic   = 0x87fbbeea;
        uint32_t version = 1;
    };

    static void Initialize(const game::TilemapLayerClass& klass, game::TilemapData& data,
                           unsigned map_width, unsigned map_height)
    {
        const auto layer_width  = klass.MapDimension(map_width);
        const auto layer_height = klass.MapDimension(map_height);
        const auto layer_tiles  = layer_width * layer_height;
        const auto tile_data_size = klass.GetTileDataSize();
        Header header;
        data.Resize(sizeof(header));
        data.Write(&header, sizeof(header), 0);

        const auto chunk_byte_offset = data.AppendChunk(layer_width * layer_height * tile_data_size);

        data.ClearChunk(klass.GetDefaultTileValue(),
                        klass.GetTileDataSize(),
                        chunk_byte_offset, layer_tiles);
    }

    static void ResizeCopy(const game::TilemapLayerClass& klass,
                           const game::USize& src_map_size,
                           const game::USize& dst_map_size,
                           const game::TilemapData& src,
                           game::TilemapData& dst)
    {
        const auto src_num_rows = klass.MapDimension(src_map_size.GetHeight());
        const auto dst_num_rows = klass.MapDimension(dst_map_size.GetHeight());
        const auto src_num_cols = klass.MapDimension(src_map_size.GetWidth());
        const auto dst_num_cols = klass.MapDimension(dst_map_size.GetWidth());

        const void* def_tile = klass.GetDefaultTileValue();
        const auto tile_size = klass.GetTileDataSize();
        const auto num_rows = std::min(src_num_rows, dst_num_rows);
        const auto src_row_size = src_num_cols * tile_size;
        const auto dst_row_size = dst_num_cols * tile_size;

        ASSERT(dst_row_size % tile_size == 0);
        ASSERT(src_row_size % tile_size == 0);

        const auto header_size = sizeof(Header);

        std::vector<unsigned char> row_buffer;

        for (size_t row=0; row<num_rows; ++row)
        {
            const auto src_row_offset = row * src_row_size;
            const auto dst_row_offset = row * dst_row_size;
            const auto row_copy_bytes = std::min(src_row_size, dst_row_size);
            row_buffer.resize(row_copy_bytes);
            src.Read(&row_buffer[0], row_copy_bytes, src_row_offset + header_size);
            dst.Write(&row_buffer[0], row_copy_bytes, dst_row_offset + header_size);

            // if the dst buffer is wider than the src buffer,
            // then the new columns will be initialized with the default class value
            // since the src doesn't have any data to copy over.
            // the default value needs to be set in the new buffer allocation time.
            if (dst_row_size <= src_row_size)
                continue;

            const auto fill_bytes = dst_row_size - src_row_size;
            row_buffer.resize(fill_bytes);

            // fill the temp row buffer with default data.
            auto* tile_ptr = row_buffer.data();
            for (size_t i=0; i<fill_bytes / tile_size; ++i, tile_ptr+=tile_size)
                std::memcpy(tile_ptr, def_tile, tile_size);
            // write the default data buffer out.
            dst.Write(&row_buffer[0], fill_bytes, dst_row_offset + src_row_size + header_size);
        }

        // append new rows in the dst buffer when the map height has grown
        row_buffer.resize(dst_row_size);
        auto* tile_ptr = row_buffer.data();
        for (size_t col=0; col<dst_num_cols; ++col, tile_ptr+=tile_size)
            std::memcpy(tile_ptr, def_tile, tile_size);

        for (size_t row=num_rows; row<dst_num_rows; ++row)
        {
            const auto dst_row_offset = row * dst_row_size;
            dst.Write(&row_buffer[0], dst_row_size, dst_row_offset + header_size);
        }
    }
    virtual void LoadState(const game::TilemapData& data) override
    {}
    virtual void SaveState(game::TilemapData& data) const override
    {}

    virtual void LoadCache(const game::TilemapData& data, const Tile& default_tile,
                           TileCache& cache, size_t cache_index,
                           unsigned layer_width_tiles,
                           unsigned layer_height_tiles) const override
    {
        const auto [bytes_read, buff_byte_offset] = ToBytes(cache_index, cache.size(), layer_width_tiles, layer_height_tiles);

        data.Read(cache.data(), bytes_read, buff_byte_offset + sizeof(Header));
    }
    virtual void SaveCache(game::TilemapData& data, const Tile& default_tile,
                           const TileCache& cache, size_t cache_index,
                           unsigned layer_width_tiles,
                           unsigned layer_height_tiles) override
    {
        const auto [bytes_write, buff_byte_offset] = ToBytes(cache_index, cache.size(), layer_width_tiles, layer_height_tiles);

        data.Write(cache.data(), bytes_write, buff_byte_offset + sizeof(Header));
    }
    virtual size_t GetByteCount() const override
    {
        return 0;
    }
private:
    std::tuple<size_t, size_t> ToBytes(size_t cache_index, size_t cache_size, unsigned layer_width, unsigned layer_height) const
    {
        const auto cache_size_bytes = cache_size * sizeof(Tile);
        const auto buff_size_bytes  = layer_width * layer_height * sizeof(Tile);
        const auto buff_byte_offset = cache_index * cache_size_bytes;
        const auto max_bytes = std::min(buff_size_bytes - buff_byte_offset, cache_size_bytes);
        return {max_bytes, buff_byte_offset};
    }
};


typedef std::unique_ptr<game::TilemapLayer> (*LayerFactoryFunction)(const std::shared_ptr<const game::TilemapLayerClass>& klass,
                                                                    unsigned  map_width, unsigned map_height);
template<typename Tile, template<typename> class TileLoader>
std::unique_ptr<game::TilemapLayer> CreateLayer(const std::shared_ptr<const game::TilemapLayerClass>& klass,
                                                unsigned map_width, unsigned map_height)
{
    using TileLoaderType = TileLoader<Tile>;
    using TileLayerType  = game::detail::TilemapLayerBase<Tile>;
    auto loader = std::make_unique<TileLoaderType>();
    auto layer  = std::make_unique<TileLayerType>(klass, std::move(loader), map_width, map_height);
    return layer;
}

} // namespace


namespace game
{

TilemapLayerClass::TilemapLayerClass()
  : mId(base::RandomString(10))
{
    SetType(Type::Render);

    SetFlag(Flags::VisibleInEditor, true);
    SetFlag(Flags::Visible, true);
    SetFlag(Flags::Enabled, true);
    SetFlag(Flags::ReadOnly, false);
}

std::size_t TilemapLayerClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mDataUri);
    hash = base::hash_combine(hash, mDataId);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mStorage);
    hash = base::hash_combine(hash, GetType());
    hash = base::hash_combine(hash, mCache);
    hash = base::hash_combine(hash, mResolution);
    hash = base::hash_combine(hash, mDefault);
    hash = base::hash_combine(hash, mDepth);
    hash = base::hash_combine(hash, mLayer);

    std::set<size_t> keys;
    for (const auto& [key, val] : mPalette)
        keys.insert(key);

    for (auto key : keys)
    {
        const auto* entry = base::SafeFind(mPalette, key);
        hash = base::hash_combine(hash, entry->materialId);
        hash = base::hash_combine(hash, entry->tile_index);
        hash = base::hash_combine(hash, entry->flags);
    }
    return hash;
}

std::string TilemapLayerClass::GetPaletteMaterialId(std::size_t index) const
{
    if (const auto* entry = base::SafeFind(mPalette, index))
        return entry->materialId;

    return "";
}
std::uint8_t TilemapLayerClass::GetPaletteMaterialTileIndex(std::size_t index) const
{
    if (const auto* entry = base::SafeFind(mPalette, index))
        return entry->tile_index;

    return 0;
}

std::uint8_t TilemapLayerClass::GetPaletteFlags(std::size_t index) const
{
    if (const auto* entry = base::SafeFind(mPalette, index))
        return entry->flags;

    return 0;
}

std::size_t TilemapLayerClass::FindMaterialIndexInPalette(const std::string& material) const
{
    for (const auto& [index, val] : mPalette)
    {
        if (val.materialId == material)
            return index;
    }
    return 0xff;
}
std::size_t TilemapLayerClass::FindMaterialIndexInPalette(const std::string& materialId, std::uint8_t tile_index) const
{
    for (const auto& [index, val] : mPalette)
    {
        if (val.materialId == materialId && val.tile_index == tile_index)
            return index;
    }
    return 0xff;
}

std::size_t TilemapLayerClass::FindNextAvailablePaletteIndex() const
{
    for (size_t i=0; i<GetMaxPaletteIndex(GetType()); ++i)
    {
        if (mPalette.find(i) == mPalette.end())
            return i;
    }
    return 0xff;
}

void TilemapLayerClass::SetDefaultTilePaletteMaterialIndex(uint8_t index)
{
    std::visit([&index](auto& tile) {
        ASSERT(detail::SetTilePaletteIndex(tile, index));
    }, mDefault);
}

void TilemapLayerClass::SetDefaultTileDataValue(int32_t value)
{
    std::visit([&value](auto& tile) {
        ASSERT(detail::SetTileValue(tile, value));
    }, mDefault);
}

uint8_t TilemapLayerClass::GetDefaultTilePaletteMaterialIndex() const
{
    uint8_t index = 0;
    std::visit([&index](const auto& tile) {
        ASSERT(detail::GetTilePaletteIndex(tile, &index));
    }, mDefault);
    return index;
}

int32_t TilemapLayerClass::GetDefaultTileDataValue() const
{
    int32_t value = 0;
    std::visit([&value](const auto& tile) {
        ASSERT(detail::GetTileValue(tile, &value));
    }, mDefault);
    return value;
}

void TilemapLayerClass::SetPaletteFlag(PaletteFlags flag, bool on_off, size_t palette_index)
{
    auto& entry = mPalette[palette_index];
    if (on_off)
        entry.flags |= static_cast<int8_t>(flag);
    else entry.flags &= ~static_cast<uint8_t>(flag);
}

bool TilemapLayerClass::TestPaletteFlag(PaletteFlags flag, size_t palette_index) const
{
    if (const auto* entry = base::SafeFind(mPalette, palette_index))
    {
        return entry->flags & static_cast<uint8_t>(flag);
    }
    return false;
}

void TilemapLayerClass::SetType(Type type) noexcept
{
    if (type == Type::Render)
        mDefault = detail::Render_Tile {};
    else if (type == Type::Render_DataSInt4)
        mDefault = detail::Render_Data_Tile_UInt4 {};
    else if (type == Type::Render_DataUInt4)
        mDefault = detail::Render_Data_Tile_UInt4 {};
    else if (type == Type::Render_DataSInt8)
        mDefault = detail::Render_Data_Tile_SInt8 {};
    else if (type == Type::Render_DataUInt8)
        mDefault = detail::Render_Data_Tile_UInt8 {};
    else if (type == Type::Render_DataSInt24)
        mDefault = detail::Render_Data_Tile_UInt24 {};
    else if (type == Type::Render_DataUInt24)
        mDefault = detail::Render_Data_Tile_UInt24 {};
    else if (type == Type::DataSInt8)
        mDefault = detail::Data_Tile_SInt8 {};
    else if (type == Type::DataUInt8)
        mDefault = detail::Data_Tile_UInt8 {};
    else if (type == Type::DataSInt16)
        mDefault = detail::Data_Tile_SInt16 {};
    else if (type == Type::DataUInt16)
        mDefault = detail::Data_Tile_UInt16 {};
    else BUG("Missing tilemap layer type mapping.");
}

TilemapLayerClass::Type TilemapLayerClass::GetType() const noexcept
{
    Type ret;
    std::visit([&ret](const auto& variant_value) {
        using VariantValueType = std::decay_t<decltype(variant_value)>;
        ret = detail::TilemapLayerTraits<VariantValueType>::LayerType;
    }, mDefault);
    return ret;
}

const void* TilemapLayerClass::GetDefaultTileValue() const
{
    const void* ret = nullptr;
    std::visit([&ret](const auto& variant_value) {
        ret = &variant_value;
    }, mDefault);
    return ret;
}

void TilemapLayerClass::Initialize(unsigned map_width, unsigned map_height,
                                   TilemapData& data) const
{
    if (mStorage == Storage::Dense)
    {
        std::visit([&data, this, map_width, map_height](const auto& variant_value) {
            using TileType = std::decay_t<decltype(variant_value)>;
            DenseTilemapLayer<TileType>::Initialize(*this, data, map_width, map_height);
        }, mDefault);
    }
    else
    {
        std::visit([&data, this, map_width, map_height](const auto& variant_value) {
            using TileType = std::decay_t<decltype(variant_value)>;
            SparseTilemapLayer<TileType>::Initialize(*this, data, map_width, map_height);
        }, mDefault);
    }
}

void TilemapLayerClass::ResizeCopy(const USize& src_map_size,
                                   const USize& dst_map_size,
                                   const TilemapData& src,
                                   TilemapData& dst) const
{
    if (mStorage == Storage::Dense)
    {
        std::visit([&src_map_size, &dst_map_size, &src, &dst, this](const auto& variant_value) {
            using TileType = std::decay_t<decltype(variant_value)>;
            DenseTilemapLayer<TileType>::ResizeCopy(*this, src_map_size, dst_map_size, src, dst);
        }, mDefault);
    }
    else
    {
        std::visit([&src_map_size, &dst_map_size, &src, &dst, this](const auto& variant_value) {
            using TileType = std::decay_t<decltype(variant_value)>;
            SparseTilemapLayer<TileType>::ResizeCopy(*this, src_map_size, dst_map_size, src, dst);
        }, mDefault);
    }
}

void TilemapLayerClass::IntoJson(data::Writer& data) const
{
    const auto type = GetType();
    data.Write("id",           mId);
    data.Write("name",         mName);
    data.Write("data_uri",     mDataUri);
    data.Write("data_id",      mDataId);
    data.Write("flags",        mFlags);
    data.Write("storage",      mStorage);
    data.Write("type",         type);
    data.Write("cache",        mCache);
    data.Write("rez",          mResolution);
    data.Write("depth",        mDepth);
    data.Write("layer",        mLayer);

    std::set<size_t> keys;
    for (const auto [key, val] : mPalette)
        keys.insert(key);

    for (size_t key : keys)
    {
        const auto* entry = base::SafeFind(mPalette, key);

        auto chunk = data.NewWriteChunk();
        chunk->Write("index", (unsigned)key);
        chunk->Write("value", entry->materialId);
        chunk->Write("tile_index", (unsigned)entry->tile_index);
        chunk->Write("flags", (unsigned)entry->flags);
        data.AppendChunk("palette", std::move(chunk));
    }

    uint32_t value = 0;
    std::visit([&value](auto variant_value) {
        ASSERT(sizeof(variant_value) <= 4);
        std::memcpy(&value, &variant_value, sizeof(variant_value));
    }, mDefault);
    const unsigned hi = (value >> 16) & 0xffff;
    const unsigned lo = (value >> 0 ) & 0xffff;
    auto chunk = data.NewWriteChunk();
    chunk->Write("hi_bits", hi);
    chunk->Write("lo_bits", lo);
    data.Write("default", std::move(chunk));
}

bool TilemapLayerClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",           &mId);
    ok &= data.Read("name",         &mName);
    ok &= data.Read("data_uri",     &mDataUri);
    ok &= data.Read("data_id",      &mDataId);
    ok &= data.Read("flags",        &mFlags);
    ok &= data.Read("storage",      &mStorage);
    ok &= data.Read("cache",        &mCache);
    ok &= data.Read("rez",          &mResolution);
    ok &= data.Read("depth",        &mDepth);
    ok &= data.Read("layer",        &mLayer);

    for (unsigned i=0; i<data.GetNumChunks("palette"); ++i)
    {
        std::string material;
        unsigned tile_index;
        unsigned key = 0;
        unsigned flags = 0;
        const auto& chunk = data.GetReadChunk("palette", i);
        ok &= chunk->Read("index", &key);
        ok &= chunk->Read("value", &material);
        ok &= chunk->Read("flags", &flags);
        ok &= chunk->Read("tile_index", &tile_index);

        PaletteEntry entry;
        entry.materialId = std::move(material);
        entry.tile_index = tile_index;
        entry.flags      = flags;
        mPalette[key] = std::move(entry);
    }

    Type type;
    if (!data.Read("type", &type))
        return false;

    SetType(type);

    unsigned hi = 0;
    unsigned lo = 0;
    auto chunk = data.GetReadChunk("default");
    if (chunk)
    {
        ok &= chunk->Read("hi_bits", &hi);
        ok &= chunk->Read("lo_bits", &lo);
    }
    const uint32_t value = ((hi & 0xffff) << 16) | (lo & 0xffff);
    std::visit([&value](auto& variant_value) {
        std::memcpy(&variant_value, &value, sizeof(variant_value));
    }, mDefault);

    return ok;
}


// static
bool TilemapLayerClass::HasRenderComponent(Type type)
{
    if (type == Type::Render ||
        type == Type::Render_DataSInt4 ||
        type == Type::Render_DataUInt4 ||
        type == Type::Render_DataSInt8 ||
        type == Type::Render_DataUInt8 ||
        type == Type::Render_DataSInt24 ||
        type == Type::Render_DataUInt24)
        return true;
    return false;
}

// static
bool TilemapLayerClass::HasDataComponent(Type type)
{
    if (type == Type::Render)
        return false;
    return true;
}

// static
unsigned TilemapLayerClass::GetMaxPaletteIndex(Type type)
{
    if (type == Type::Render)
        return detail::TilemapLayerTraits<detail::Render_Tile>::MaxPaletteIndex;
    else if (type == Type::Render_DataSInt4)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_SInt4>::MaxPaletteIndex;
    else if (type == Type::Render_DataUInt4)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_UInt4>::MaxPaletteIndex;
    else if (type == Type::Render_DataSInt8)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_SInt8>::MaxPaletteIndex;
    else if (type == Type::Render_DataUInt8)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_UInt8>::MaxPaletteIndex;
    else if(type == Type::Render_DataSInt24)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_UInt24>::MaxPaletteIndex;
    else if(type == Type::Render_DataUInt24)
        return detail::TilemapLayerTraits<detail::Render_Data_Tile_UInt24>::MaxPaletteIndex;
    else BUG("Not a render layer");
    return 0;
}

// static
size_t TilemapLayerClass::ComputeLayerSize(Type type, unsigned map_width, unsigned map_height)
{
    return map_width * map_height * GetTileDataSize(type);
}

// static
size_t TilemapLayerClass::GetTileDataSize(Type type)
{
    if (type == Type::Render) return 1;
    else if (type == Type::Render_DataSInt4) return 1;
    else if (type == Type::Render_DataUInt4) return 1;
    else if (type == Type::Render_DataSInt8) return 2;
    else if (type == Type::Render_DataUInt8) return 2;
    else if (type == Type::Render_DataSInt24) return 4;
    else if (type == Type::Render_DataUInt24) return 4;
    else if (type == Type::DataSInt8) return 1;
    else if (type == Type::DataUInt8) return 1;
    else if (type == Type::DataSInt16) return 2;
    else if (type == Type::DataUInt16) return 2;
    else BUG("Missing tilemap layer data unit size mapping.");
    return 0;
}
// static
size_t TilemapLayerClass::GetCacheSize(Cache cache) noexcept
{
    if (cache == Cache::Automatic) return 512;
    else if (cache == Cache::Cache8)  return 8;
    else if (cache == Cache::Cache16) return 16;
    else if (cache == Cache::Cache32) return 32;
    else if (cache == Cache::Cache64) return 64;
    else if (cache == Cache::Cache128) return 128;
    else if (cache == Cache::Cache256) return 256;
    else if (cache == Cache::Cache512) return 512;
    else if (cache == Cache::Cache1024) return 1024;
    else BUG("Missing tilemap layer cache mapping.");
    return 0;
}
// static
unsigned TilemapLayerClass::MapDimension(Resolution res, unsigned dim)
{
    if (res == Resolution::Original) return dim;
    else if (res == Resolution::DownScale2) return dim / 2;
    else if (res == Resolution::DownScale4) return dim / 4;
    else if (res == Resolution::DownScale8) return dim / 8;
    else if (res == Resolution::UpScale2) return dim * 2;
    else if (res == Resolution::UpScale4) return dim * 4;
    else if (res == Resolution::UpScale8) return dim * 8;
    else BUG("Missing tilemap layer resolution mapping.");
    return 0;
}
// static
float TilemapLayerClass::GetTileSizeScaler(Resolution res)
{
    if (res == Resolution::Original) return 1.0f;
    else if (res == Resolution::DownScale2) return 2.0f;
    else if (res == Resolution::DownScale4) return 4.0f;
    else if (res == Resolution::DownScale8) return 8.0f;
    else if (res == Resolution::UpScale2) return 1.0f/2.0f;
    else if (res == Resolution::UpScale4) return 1.0f/4.0f;
    else if (res == Resolution::UpScale8) return 1.0f/8.0f;
    else BUG("MIssing tilemap layer resolution mapping.");
    return 0.0f;
}

// static
void TilemapLayerClass::GetSparseBlockSize(unsigned tile_data_size,
                                           unsigned layer_width_tiles,
                                           unsigned layer_height_tiles,
                                           unsigned* block_width_tiles,
                                           unsigned* block_height_tiles)
{
    struct Combination {
        unsigned block_width  = 0;
        unsigned block_height = 0;
    };
    std::vector<Combination> possibilities;
    const static unsigned BlockDimension[] = {8, 16, 32, 64, 128, 256, 512, 1024};
    for (unsigned height : BlockDimension)
    {
        for (unsigned width : BlockDimension)
            possibilities.push_back({width, height});
    }

    // if the layer is going to be 100% full the sparse layer
    // can never beat dense layer since there's some overhead
    // associated with the sparse data structure itself.
    // so instead try to figure out which combination would have
    // the least overhead when being at most 50% full and each
    // tile block being at most 50% full.

    const auto max_tiles = layer_width_tiles * layer_height_tiles;
    const auto capacity_target_tiles =  (unsigned)max_tiles * 0.5;

    Combination selected = possibilities[0];

    unsigned current_best = std::numeric_limits<unsigned>::max();

    for (size_t i=0; i< possibilities.size(); ++i)
    {
        const auto& maybe = possibilities[i];
        const auto num_block_rows = base::EvenMultiple(layer_height_tiles, maybe.block_height);
        const auto num_block_cols = base::EvenMultiple(layer_width_tiles, maybe.block_width);
        const auto num_blocks = num_block_cols * num_block_rows;
        const auto block_array_overhead_bytes = num_blocks * sizeof(8); // todo: use TileBlock type here.

        const auto empty_rows = (num_block_rows * maybe.block_height) - layer_height_tiles;
        const auto empty_cols = (num_block_cols * maybe.block_width) - layer_width_tiles;
        const auto empty_tiles_overhead_bytes = empty_rows * empty_cols * tile_data_size;

        const auto block_size_tiles = maybe.block_width * maybe.block_height;
        const auto block_tiles_used  = block_size_tiles * 0.5;
        const auto block_tiles_empty = block_size_tiles - block_tiles_used;
        const auto block_overhead_bytes = block_tiles_empty * tile_data_size;

        const auto total_overhead = block_array_overhead_bytes +
                                    empty_tiles_overhead_bytes +
                                    block_overhead_bytes;

        const auto total_tiles_usable = num_blocks * block_tiles_used;
        if (total_tiles_usable >= capacity_target_tiles)
        {
            if (total_overhead < current_best)
            {
                selected = maybe;
                current_best = total_overhead;
            }
        }
    }

    *block_width_tiles  = selected.block_width;
    *block_height_tiles = selected.block_height;
}

std::unique_ptr<TilemapLayer> CreateTilemapLayer(const std::shared_ptr<const TilemapLayerClass>& klass,
                                                 unsigned map_width, unsigned map_height)
{
    using Type    = TilemapLayerClass::Type;
    using Storage = TilemapLayerClass::Storage;
    using namespace  detail;

    struct LayerParamCombination {
        Type type;
        Storage storage;
        LayerFactoryFunction function;
    };
    static LayerParamCombination combinations[] = {
        {Type::Render,            Storage::Dense,  &CreateLayer<Render_Tile, DenseTilemapLayer>},
        {Type::Render,            Storage::Dense,  &CreateLayer<Render_Tile, DenseTilemapLayer>},
        {Type::Render,            Storage::Sparse, &CreateLayer<Render_Tile, SparseTilemapLayer>},
        {Type::Render,            Storage::Sparse, &CreateLayer<Render_Tile, SparseTilemapLayer>},

        {Type::Render_DataSInt4,  Storage::Dense,  &CreateLayer<Render_Data_Tile_SInt4,  DenseTilemapLayer>},
        {Type::Render_DataSInt4,  Storage::Sparse, &CreateLayer<Render_Data_Tile_SInt4,  SparseTilemapLayer>},
        {Type::Render_DataUInt4,  Storage::Dense,  &CreateLayer<Render_Data_Tile_UInt4,  DenseTilemapLayer>},
        {Type::Render_DataUInt4,  Storage::Sparse, &CreateLayer<Render_Data_Tile_UInt4,  SparseTilemapLayer>},

        {Type::Render_DataUInt8,  Storage::Dense,  &CreateLayer<Render_Data_Tile_UInt8,  DenseTilemapLayer>},
        {Type::Render_DataUInt8,  Storage::Sparse, &CreateLayer<Render_Data_Tile_UInt8,  SparseTilemapLayer>},
        {Type::Render_DataSInt8,  Storage::Dense,  &CreateLayer<Render_Data_Tile_SInt8,  DenseTilemapLayer>},
        {Type::Render_DataSInt8,  Storage::Sparse, &CreateLayer<Render_Data_Tile_SInt8,  SparseTilemapLayer>},

        {Type::Render_DataSInt24, Storage::Dense,  &CreateLayer<Render_Data_Tile_SInt24, DenseTilemapLayer>},
        {Type::Render_DataSInt24, Storage::Sparse, &CreateLayer<Render_Data_Tile_SInt24, SparseTilemapLayer>},
        {Type::Render_DataUInt24, Storage::Dense,  &CreateLayer<Render_Data_Tile_UInt24, DenseTilemapLayer>},
        {Type::Render_DataUInt24, Storage::Sparse, &CreateLayer<Render_Data_Tile_UInt24, SparseTilemapLayer>},

        {Type::DataUInt8,         Storage::Dense,  &CreateLayer<Data_Tile_UInt8, DenseTilemapLayer>},
        {Type::DataUInt8,         Storage::Sparse, &CreateLayer<Data_Tile_UInt8, SparseTilemapLayer>},
        {Type::DataSInt8,         Storage::Dense,  &CreateLayer<Data_Tile_SInt8, DenseTilemapLayer>},
        {Type::DataSInt8,         Storage::Sparse, &CreateLayer<Data_Tile_SInt8, SparseTilemapLayer>},

        {Type::DataUInt16,        Storage::Dense,  &CreateLayer<Data_Tile_UInt16, DenseTilemapLayer>},
        {Type::DataUInt16,        Storage::Sparse, &CreateLayer<Data_Tile_UInt16, SparseTilemapLayer>},
        {Type::DataSInt16,        Storage::Dense,  &CreateLayer<Data_Tile_SInt16, DenseTilemapLayer>},
        {Type::DataSInt16,        Storage::Sparse, &CreateLayer<Data_Tile_SInt16, SparseTilemapLayer>},
    };
    const auto type = klass->GetType();
    const auto storage = klass->GetStorage();
    for (const auto& combination : combinations)
    {
        if (combination.type == type && combination.storage == storage)
        {
            return combination.function(klass, map_width, map_height);
        }
    }
    return nullptr;
}

} // namespace