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

#include <limits>
#include <string>
#include <variant>
#include <unordered_map>
#include <cstdint>

#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/types.h"
#include "game/tilemap_types.h"

namespace game
{
    class TilemapData;
    class Loader;

    // Description of a tilemap layer.
    // Each layer defines the logical function of the layer
    // i.e. render or data and the data type (if any).
    class TilemapLayerClass
    {
    public:
        using Type = detail::TilemapLayerType;

        enum class Flags {
            VisibleInEditor,
            Visible,
            ReadOnly,
            Enabled
        };

        enum class Storage {
            Sparse,
            Dense
        };
        enum class Cache {
            Automatic,
            Cache8,
            Cache16,
            Cache32,
            Cache64,
            Cache128,
            Cache256,
            Cache512,
            Cache1024
        };
        enum class Resolution {
            Original,
            DownScale8,
            DownScale4,
            DownScale2,
            UpScale2,
            UpScale4,
            UpScale8
        };
        using DefaultValue = std::variant<
                detail::Data_Tile_SInt8,
                detail::Data_Tile_UInt8,
                detail::Data_Tile_SInt16,
                detail::Data_Tile_UInt16,
                detail::Render_Tile,
                detail::Render_Data_Tile_SInt4,
                detail::Render_Data_Tile_UInt4,
                detail::Render_Data_Tile_SInt8,
                detail::Render_Data_Tile_UInt8,
                detail::Render_Data_Tile_UInt24,
                detail::Render_Data_Tile_SInt24>;

        TilemapLayerClass();

        inline std::string GetId() const
        { return mId; }
        inline std::string GetName() const
        { return mName; }
        inline std::string GetDataUri() const
        { return mDataUri; }
        inline std::string GetDataId() const
        { return mDataId; }
        inline base::bitflag<Flags> GetFlags() const noexcept
        { return mFlags; }
        inline bool IsReadOnly() const noexcept
        { return mFlags.test(Flags::ReadOnly); }
        inline bool IsVisible() const noexcept
        { return mFlags.test(Flags::Visible); }
        inline bool IsEnabled() const noexcept
        { return mFlags.test(Flags::Enabled); }
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        inline Cache GetCache() const noexcept
        { return mCache; }
        inline Resolution GetResolution() const noexcept
        { return mResolution; }
        inline Storage GetStorage() const noexcept
        { return mStorage; }
        inline int GetDepth() const noexcept
        { return mDepth; }
        inline int GetRenderLayer() const noexcept
        { return mRenderLayer; }
        inline void SetId(std::string id) noexcept
        { mId = std::move(id); }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline void SetDataUri(std::string uri) noexcept
        { mDataUri = std::move(uri); }
        inline void SetDataId(std::string id) noexcept
        { mDataId = std::move(id); }
        inline void ResetDataId() noexcept
        { mDataId.clear(); }
        inline void ResetDataUri() noexcept
        { mDataUri.clear(); }
        inline void SetStorage(Storage storage) noexcept
        { mStorage = storage; }
        inline void SetCache(Cache cache) noexcept
        { mCache = cache; }
        inline void SetResolution(Resolution res) noexcept
        { mResolution = res; }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        inline void SetVisible(bool on_off) noexcept
        { mFlags.set(Flags::Visible, on_off); }
        inline void SetEnabled(bool on_off) noexcept
        { mFlags.set(Flags::Enabled, on_off); }
        inline void SetReadOnly(bool on_off) noexcept
        { mFlags.set(Flags::ReadOnly, on_off); }
        inline void SetFlags(base::bitflag<Flags> flags) noexcept
        { mFlags = flags; }
        inline void SetDepth(int depth) noexcept
        { mDepth = depth; }
        inline void SetRenderLayer(int layer) noexcept
        { mRenderLayer = layer; }
        inline size_t GetCacheSize() const noexcept
        { return GetCacheSize(mCache); }
        inline void SetPaletteMaterialId(std::string material, std::size_t palette_index)
        { mPalette[palette_index].materialId = std::move(material); }
        inline void SetPaletteMaterialTileIndex(std::uint8_t tile_index, std::size_t palette_index)
        { mPalette[palette_index].tile_index = tile_index; }
        inline void ClearPalette() noexcept
        { mPalette.clear(); }
        inline void ClearMaterialId(std::size_t index) noexcept
        { mPalette.erase(index); }

        void SetType(Type type) noexcept;

        Type GetType() const noexcept;

        std::size_t GetHash() const noexcept;
        std::string GetPaletteMaterialId(std::size_t index) const;
        std::uint8_t GetPaletteMaterialTileIndex(std::size_t index) const;
        std::size_t FindMaterialIndexInPalette(const std::string& materialId) const;
        std::size_t FindMaterialIndexInPalette(const std::string& materialId, std::uint8_t tile_index) const;
        std::size_t FindNextAvailablePaletteIndex() const;

        template<typename T>
        const T& GetDefaultTileValue() const
        {
            ASSERT(std::holds_alternative<T>(mDefault));
            return std::get<T>(mDefault);
        }
        template<typename T>
        T& GetDefaultTileValue()
        {
            ASSERT(std::holds_alternative<T>(mDefault));
            return std::get<T>(mDefault);
        }

        template<typename T>
        void SetDefaultTileValue(const T& val)
        {
            ASSERT(std::holds_alternative<T>(mDefault));
            mDefault = val;
        }
        const void* GetDefaultTileValue() const;

        void SetDefaultTilePaletteMaterialIndex(uint8_t index);
        void SetDefaultTileDataValue(int32_t value);
        uint8_t GetDefaultTilePaletteMaterialIndex() const;
        int32_t GetDefaultTileDataValue() const;

        inline bool HasRenderComponent() const
        { return HasRenderComponent(GetType()); }
        inline bool HasDataComponent() const
        { return HasDataComponent(GetType()); }
        inline unsigned MapDimension(unsigned map_width) const
        { return MapDimension(GetResolution(), map_width); }
        inline size_t GetTileDataSize() const
        { return GetTileDataSize(GetType()); }
        inline float GetTileSizeScaler() const
        { return GetTileSizeScaler(GetResolution()); }
        inline unsigned GetMaxPaletteIndex() const
        { return GetMaxPaletteIndex(GetType()); }

        void Initialize(unsigned map_width, unsigned map_height, TilemapData& data) const ;

        void ResizeCopy(const USize& src_map_size,
                        const USize& dst_map_size,
                        const TilemapData& src,
                        TilemapData& dst) const;

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        static bool HasRenderComponent(Type type);
        static bool HasDataComponent(Type type);
        static unsigned GetMaxPaletteIndex(Type type);
        // compute the layer size in bytes.
        static size_t ComputeLayerSize(Type type, unsigned map_width, unsigned map_height);
        // Get the size of the layer's data unit.
        static size_t GetTileDataSize(Type type);
        static size_t GetCacheSize(Cache cache) noexcept;
        static unsigned MapDimension(Resolution res, unsigned dim);
        static float GetTileSizeScaler(Resolution res);

        static void GetSparseBlockSize(unsigned tile_data_size,
                                       unsigned layer_width_tiles,
                                       unsigned layer_height_tiles,
                                       unsigned* block_width_tiles,
                                       unsigned* block_height_tiles);
    private:
        std::string mId;
        std::string mName;
        std::string mDataUri;
        std::string mDataId;
        base::bitflag<Flags> mFlags;

        struct PaletteEntry {
            std::string materialId;
            std::uint8_t tile_index = 0;
        };
        std::unordered_map<std::size_t, PaletteEntry> mPalette;
        Storage mStorage = Storage::Dense;
        Cache mCache = Cache::Cache64;
        Resolution mResolution = Resolution::Original;
        DefaultValue  mDefault;
        int mDepth = 0;
        int mRenderLayer = 0;
    };

} // namespace