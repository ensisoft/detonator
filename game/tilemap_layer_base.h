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

#include "config.h"

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "base/utility.h"
#include "game/tilemap_data.h"
#include "game/tilemap_layer.h"
#include "game/tilemap_types.h"

namespace game
{
    namespace detail {
        template<typename Tile>
        class TilemapLayerLoader
        {
        public:
            virtual ~TilemapLayerLoader() = default;
            virtual void LoadState(const TilemapData& data) = 0;
            virtual void SaveState(TilemapData& data) const = 0;

            using TileCache = std::vector<Tile>;

            virtual void LoadCache(const TilemapData& data, const Tile& default_tile,
                                   TileCache& cache, size_t cache_index,
                                   unsigned layer_width_tiles,
                                   unsigned layer_height_tiles) const = 0;
            virtual void SaveCache(TilemapData& data, const Tile& default_tile,
                                   const TileCache& cache, size_t cache_index,
                                   unsigned layer_width_tiles,
                                   unsigned layer_height_tiles) = 0;
            virtual size_t GetByteCount() const = 0;
        private:
        };

        template<typename Tile>
        class TilemapLayerBase : public TilemapLayer
        {
        public:
            using TileType   = Tile;
            using TileLoader = TilemapLayerLoader<Tile>;
            static constexpr auto LayerType = detail::TilemapLayerTraits<Tile>::LayerType;

            TilemapLayerBase(std::shared_ptr<const TilemapLayerClass> klass,
                             std::unique_ptr<TileLoader> loader,
                             unsigned map_width, unsigned map_height)
              : mClass(std::move(klass))
              , mLoader(std::move(loader))
              , mMapWidth(map_width)
              , mMapHeight(map_height)
            {
                mFlags = mClass->GetFlags();
            }
            std::string GetClassId() const override
            { return mClass->GetId(); }
            std::string GetClassName() const override
            { return mClass->GetName(); }

            std::string GetPaletteMaterialId(size_t palette_index) const override
            {
                if (const auto* entry = base::SafeFind(mPalette, palette_index))
                    return entry->materialId;
                return mClass->GetPaletteMaterialId(palette_index);
            }
            std::uint8_t GetPaletteFlags(size_t palette_index) const override
            {
                if (const auto* entry = base::SafeFind(mPalette, palette_index))
                    return entry->flags;
                return mClass->GetPaletteFlags(palette_index);
            }

            base::bitflag<Flags> GetFlags() const override
            { return mFlags; }
            Type GetType() const override
            { return mClass->GetType(); }
            bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            bool IsLoaded() const override
            { return mData != nullptr; }

            void Load(const std::shared_ptr<TilemapData>& data) override
            {
                const auto cache = mClass->GetCache();
                mData = data;
                mDirtyCache = false;
                mTileCache.clear();
                mTileCache.resize(mClass->GetCacheSize());
                mLoader->LoadState(*mData);
                mLoader->LoadCache(*mData, mClass->GetDefaultTileValue<Tile>(), mTileCache, 0,
                                   mClass->MapDimension(mMapWidth),
                                   mClass->MapDimension(mMapHeight));
            }
            void FlushCache() override
            {
                if (mDirtyCache)
                    mLoader->SaveCache(*mData, mClass->GetDefaultTileValue<Tile>(), mTileCache, mCacheIndex,
                                       mClass->MapDimension(mMapWidth),
                                       mClass->MapDimension(mMapHeight));
                mDirtyCache = false;
            }
            void Save() override
            {
                mLoader->SaveState(*mData);
            }

            void SetPaletteMaterialId(const std::string& material, size_t palette_index) override
            { mPalette[palette_index].materialId = material; }
            void SetMapDimensions(unsigned width, unsigned height) override
            { mMapWidth = width; mMapHeight = height; }
            unsigned GetWidth() const override
            { return mClass->MapDimension(mMapWidth); }
            unsigned GetHeight() const override
            { return mClass->MapDimension(mMapHeight); }
            int GetDepth() const override
            { return mClass->GetDepth(); }
            unsigned GetLayer() const override
            { return mClass->GetLayer(); }
            float GetTileSizeScaler() const override
            { return mClass->GetTileSizeScaler(); }
            bool SetTilePaletteIndex(uint8_t index, unsigned row, unsigned col) override
            { return detail::SetTilePaletteIndex(get_tile(row, col, true), index);}
            bool GetTilePaletteIndex(uint8_t* index, unsigned row, unsigned col) const override
            { return detail::GetTilePaletteIndex(get_tile(row, col, false), index); }
            bool SetTileValue(int32_t value, unsigned row, unsigned col) override
            { return detail::SetTileValue(get_tile(row, col, true), value); }
            bool GetTileValue(int32_t* value, unsigned row, unsigned col) const override
            { return detail::GetTileValue(get_tile(row, col, false), value); }
            void SetFlags(base::bitflag<Flags> flags) override
            { mFlags = flags; }

            bool TestPaletteFlag(PaletteFlags flag, size_t palette_index) const override
            {
                if (const auto* entry = base::SafeFind(mPalette, palette_index))
                {
                    return entry->flags & static_cast<uint8_t>(flag);
                }
                return mClass->TestPaletteFlag(flag, palette_index);
            }

            const TilemapLayerClass& GetClass() const override
            { return *mClass; }
            size_t GetByteCount() const override
            { return mLoader->GetByteCount(); }
            void SetTile(const Tile& tile, unsigned row, unsigned col)
            { get_tile(row, col, true) = tile; }
            const Tile& GetTile(unsigned row, unsigned col) const
            { return get_tile(row, col, false); }
        private:
            const Tile& get_tile(unsigned row, unsigned col, bool dirty) const
            { return const_cast<TilemapLayerBase*>(this)->get_tile(row, col, dirty); }

            Tile& get_tile(unsigned row, unsigned col, bool dirty)
            {
                const auto layer_width  = mClass->MapDimension(mMapWidth);
                const auto layer_height = mClass->MapDimension(mMapHeight);
                const auto& default_tile = mClass->GetDefaultTileValue<Tile>();

                ASSERT(col < layer_width);
                ASSERT(row < layer_height);
                // the units here are *tiles*
                const auto tile_offset = row * layer_width + col;
                const auto cache_size  = mTileCache.size();
                const auto cache_index = tile_offset / cache_size;

                if (cache_index == mCacheIndex)
                {
                    mDirtyCache = mDirtyCache || dirty;
                    return mTileCache[tile_offset & (cache_size -1)];
                }
                if (mDirtyCache)
                {
                    mLoader->SaveCache(*mData, default_tile, mTileCache, mCacheIndex, layer_width, layer_height);
                    mDirtyCache = false;
                }
                mLoader->LoadCache(*mData, default_tile, mTileCache, cache_index, layer_width, layer_height);
                mCacheIndex = cache_index;
                mDirtyCache = dirty;
                return mTileCache[tile_offset & (cache_size -1)];
            }
        protected:
            struct PaletteEntry {
                std::string materialId;
                std::uint8_t tile_index = 0;
                std::uint8_t flags = 0;
            };

            std::shared_ptr<const TilemapLayerClass> mClass;
            std::unique_ptr<TileLoader> mLoader;
            std::shared_ptr<TilemapData> mData;
            std::unordered_map<size_t, PaletteEntry> mPalette;
            std::vector<Tile> mTileCache;
            std::size_t mCacheIndex = 0;
            base::bitflag<Flags> mFlags;
            unsigned mMapWidth  = 0;
            unsigned mMapHeight = 0;
            bool mDirtyCache = false;
        };
    }

    using TilemapLayer_Render            = detail::TilemapLayerBase<detail::Render_Tile>;
    using TilemapLayer_Render_DataSInt4  = detail::TilemapLayerBase<detail::Render_Data_Tile_SInt4>;
    using TilemapLayer_Render_DataUInt4  = detail::TilemapLayerBase<detail::Render_Data_Tile_UInt4>;
    using TilemapLayer_Render_DataUInt8  = detail::TilemapLayerBase<detail::Render_Data_Tile_UInt8>;
    using TilemapLayer_Render_DataSInt8  = detail::TilemapLayerBase<detail::Render_Data_Tile_SInt8>;
    using TilemapLayer_Render_DataSInt24 = detail::TilemapLayerBase<detail::Render_Data_Tile_SInt24>;
    using TilemapLayer_Render_DataUInt24 = detail::TilemapLayerBase<detail::Render_Data_Tile_UInt24>;
    using TilemapLayer_Data_SInt8        = detail::TilemapLayerBase<detail::Data_Tile_SInt8>;
    using TilemapLayer_Data_UInt8        = detail::TilemapLayerBase<detail::Data_Tile_UInt8>;
    using TilemapLayer_Data_SInt16       = detail::TilemapLayerBase<detail::Data_Tile_SInt16>;
    using TilemapLayer_Data_UInt16       = detail::TilemapLayerBase<detail::Data_Tile_UInt16>;

    template<typename T>
    T* TilemapLayerCast(TilemapLayer* layer)
    {
        if (layer->GetType() == T::LayerType)
            return static_cast<T*>(layer);
        return nullptr;
    }
    template<typename T>
    const T* TilemapLayerCast(const TilemapLayer* layer)
    {
        if (layer->GetType() == T::LayerType)
            return static_cast<const T*>(layer);
        return nullptr;
    }
    template<typename T> inline
    T* TilemapLayerCast(std::unique_ptr<TilemapLayer>& layer)
    {
        return TilemapLayerCast<T>(layer.get());
    }

    template<typename T> inline
    const T* TilemapLayerCast(const std::unique_ptr<TilemapLayer>& layer)
    {
        return TilemapLayerCast<T>(layer.get());
    }

} // namespace