// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"

#include "warnpop.h"

#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <tuple>
#include <unordered_map>
#include <limits>
#include <cstdint>

#include "base/assert.h"
#include "base/utility.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"

namespace game
{
    class TilemapData;
    class Loader;

    namespace detail {
        enum class TilemapLayerType {
            Render,
            Render_DataSInt4,
            Render_DataSInt8,
            Render_DataSInt24,

            Render_DataUInt4,
            Render_DataUInt8,
            Render_DataUInt24,

            DataSInt8,
            DataSInt16,

            DataUInt8,
            DataUInt16
        };

        template<typename IntType>
        struct RenderDataTile {
            uint8_t index = 0;
            IntType data  = 0;
        };
        template<typename IntType> inline
        bool operator==(const RenderDataTile<IntType>& lhs,
                        const RenderDataTile<IntType>& rhs)
        {
            return lhs.index == rhs.index && lhs.data  == rhs.data;
        }

        template<typename IntType>
        struct RenderDataTile_Packed_8_24 {
            uint32_t index : 8;
            IntType data  : 24;
        };
        template<typename IntType> inline
        bool operator==(const RenderDataTile_Packed_8_24<IntType>& lhs,
                        const RenderDataTile_Packed_8_24<IntType>& rhs)
        { return lhs.index == rhs.index && lhs.data == rhs.data; }

        template<typename IntType>
        struct RenderDataTile_Packed_4_4 {
            uint8_t index: 4;
            IntType data:  4;
        };
        template<typename IntType> inline
        bool operator==(const RenderDataTile_Packed_4_4<IntType>& lhs,
                        const RenderDataTile_Packed_4_4<IntType>& rhs)
        { return lhs.index == rhs.index && lhs.data == rhs.data; }

        template<typename T>
        struct DataTile {
            T data;
        };
        template<typename T> inline
        bool operator==(const DataTile<T>& lhs, const DataTile<T>& rhs)
        { return lhs.data == rhs.data; }

        struct Render_Tile {
            uint8_t index = 0;
        };
        inline bool operator==(const Render_Tile& lhs, const Render_Tile& rhs)
        { return lhs.index == rhs.index; }

        using Render_Data_Tile_UInt4  = RenderDataTile_Packed_4_4<uint8_t>;
        using Render_Data_Tile_SInt4  = RenderDataTile_Packed_4_4<int8_t>;
        using Render_Data_Tile_UInt24 = RenderDataTile_Packed_8_24<uint32_t>;
        using Render_Data_Tile_SInt24 = RenderDataTile_Packed_8_24<int32_t>;
        using Render_Data_Tile_UInt8  = RenderDataTile<uint8_t>;
        using Render_Data_Tile_SInt8  = RenderDataTile<int8_t>;

        using Data_Tile_SInt8   = DataTile<int8_t>;
        using Data_Tile_UInt8   = DataTile<uint8_t>;
        using Data_Tile_SInt16  = DataTile<int16_t>;
        using Data_Tile_UInt16  = DataTile<uint16_t>;

        static_assert(sizeof(Render_Data_Tile_UInt4) == 1);
        static_assert(sizeof(Render_Data_Tile_UInt8) == 2);
        static_assert(sizeof(Render_Data_Tile_SInt8) == 2);
        static_assert(sizeof(Render_Data_Tile_UInt24) == 4);
        static_assert(sizeof(Data_Tile_SInt8) == 1);
        static_assert(sizeof(Data_Tile_UInt8) == 1);
        static_assert(sizeof(Data_Tile_SInt16) == 2);
        static_assert(sizeof(Data_Tile_UInt16) == 2);

        template<typename T>
        struct TilemapLayerTraits;

        template<>
        struct TilemapLayerTraits<Render_Tile> {
            static constexpr auto LayerType = TilemapLayerType::Render;
            static constexpr auto MaxPaletteIndex = 0xff;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_UInt4> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataUInt4;
            static constexpr auto MaxPaletteIndex = 0xf;
            static constexpr auto MaxValue = 0xf;
            static constexpr auto MinValue = 0x0;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_SInt4> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataUInt4;
            static constexpr auto MaxPaletteIndex = 0xf;
            static constexpr int32_t MaxValue =  0x7;
            static constexpr int32_t MinValue = -0x8;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_UInt8> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataUInt8;
            static constexpr auto MaxPaletteIndex = 0xff;
            static constexpr int32_t MaxValue = 0xff;
            static constexpr int32_t MinValue = 0x0;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_SInt8> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataSInt8;
            static constexpr auto MaxPaletteIndex = 0xff;
            static constexpr int32_t MaxValue =  0x7f;
            static constexpr int32_t MinValue = -0x80;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_UInt24> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataUInt24;
            static constexpr auto MaxPaletteIndex = 0xff;
            static constexpr int32_t MaxValue = 0xffffff;
            static constexpr int32_t MinValue = 0x0;
        };
        template<>
        struct TilemapLayerTraits<Render_Data_Tile_SInt24> {
            static constexpr auto LayerType = TilemapLayerType::Render_DataUInt24;
            static constexpr auto MaxPaletteIndex = 0xff;
            static constexpr int32_t MaxValue = 0x7fffff;
            static constexpr int32_t MinValue = -0x800000;
        };
        template<>
        struct TilemapLayerTraits<Data_Tile_SInt8> {
            static constexpr auto LayerType = TilemapLayerType::DataSInt8;
            static constexpr auto MaxValue = std::numeric_limits<int8_t>::max();
            static constexpr auto MinValue = std::numeric_limits<int8_t>::min();
        };
        template<>
        struct TilemapLayerTraits<Data_Tile_UInt8> {
            static constexpr auto LayerType = TilemapLayerType::DataUInt8;
            static constexpr auto MaxValue = std::numeric_limits<uint8_t>::max();
            static constexpr auto MinValue = std::numeric_limits<uint8_t>::min();
        };
        template<>
        struct TilemapLayerTraits<Data_Tile_SInt16> {
            static constexpr auto LayerType = TilemapLayerType::DataSInt16;
            static constexpr auto MaxValue = std::numeric_limits<int16_t>::max();
            static constexpr auto MinValue = std::numeric_limits<int16_t>::min();
        };
        template<>
        struct TilemapLayerTraits<Data_Tile_UInt16> {
            static constexpr auto LayerType = TilemapLayerType::DataUInt16;
            static constexpr auto MaxValue = std::numeric_limits<uint16_t>::max();
            static constexpr auto MinValue = std::numeric_limits<uint16_t>::min();
        };

        inline bool SetTilePaletteIndex(Render_Tile& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_UInt4& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_SInt4& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_UInt8& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_SInt8& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_UInt24& tile, uint8_t index)
        { tile.index = index; return true; }
        inline bool SetTilePaletteIndex(Render_Data_Tile_SInt24& tile, uint8_t index)
        { tile.index = index; return true; }
        template<typename OtherTileType>
        inline bool SetTilePaletteIndex(OtherTileType& tile, uint8_t index)
        { return false; }

        inline bool GetTilePaletteIndex(const Render_Tile& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_UInt4& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_SInt4& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_UInt8& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_SInt8& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_UInt24& tile, uint8_t* index)
        { *index = tile.index; return true; }
        inline bool GetTilePaletteIndex(const Render_Data_Tile_SInt24& tile, uint8_t* index)
        { *index = tile.index; return true; }
        template<typename OtherTileType>
        inline bool GetTilePaletteIndex(const OtherTileType& tile, uint8_t* index)
        { return false; }

        inline bool SetTileValue(Render_Data_Tile_UInt4& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Render_Data_Tile_SInt4& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Render_Data_Tile_UInt8& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Render_Data_Tile_SInt8& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Render_Data_Tile_UInt24& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Render_Data_Tile_SInt24& tile, int32_t value)
        { tile.data = value; return true;}
        inline bool SetTileValue(Data_Tile_SInt8& tile, int32_t value)
        { tile.data = value; return true; }
        inline bool SetTileValue(Data_Tile_UInt8& tile, int32_t value)
        { tile.data = value; return true; }
        inline bool SetTileValue(Data_Tile_SInt16& tile, int32_t value)
        { tile.data = value; return true; }
        inline bool SetTileValue(Data_Tile_UInt16& tile, int32_t value)
        { tile.data = value; return true; }
        template<typename OtherTileType>
        inline bool SetTileValue(OtherTileType&, int32_t)
        { return false; }

        inline bool GetTileValue(const Render_Data_Tile_UInt4& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Render_Data_Tile_SInt4& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Render_Data_Tile_UInt8& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Render_Data_Tile_SInt8& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Render_Data_Tile_UInt24& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Render_Data_Tile_SInt24& tile, int32_t* value)
        { *value = tile.data; return true;}
        inline bool GetTileValue(const Data_Tile_SInt8& tile, int32_t* value)
        { *value = tile.data; return true; }
        inline bool GetTileValue(const Data_Tile_UInt8& tile, int32_t* value)
        { *value = tile.data; return true; }
        inline bool GetTileValue(const Data_Tile_SInt16& tile, int32_t* value)
        { *value = tile.data; return true; }
        inline bool GetTileValue(const Data_Tile_UInt16& tile, int32_t* value)
        { *value = tile.data; return true; }
        template<typename OtherTileType>
        inline bool GetTileValue(const OtherTileType&, int32_t*)
        { return false; }

        template<typename TileType> inline
        float NormalizeTileDataValue(const TileType& tile)
        {
            using Traits = TilemapLayerTraits<TileType>;
            constexpr float max_val = Traits::MaxValue;
            constexpr float min_val = Traits::MinValue;
            constexpr float range   = max_val - min_val;
            const float val = tile.data;
            return (val - min_val) / range;
        }

    } // namespace

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

    class TilemapLayer
    {
    public:
        using Flags = TilemapLayerClass::Flags;
        using Type  = TilemapLayerClass::Type;
        using Class = TilemapLayerClass;

        virtual ~TilemapLayer() = default;
        virtual std::string GetClassId() const = 0;
        virtual std::string GetClassName() const = 0;
        virtual std::string GetPaletteMaterialId(size_t index) const = 0;
        virtual base::bitflag<Flags> GetFlags() const = 0;
        virtual Type GetType() const = 0;
        virtual bool TestFlag(Flags flag) const = 0;
        virtual bool IsLoaded() const = 0;

        virtual void Load(const std::shared_ptr<TilemapData>& data) = 0;
        virtual void Save() = 0;

        virtual void FlushCache() = 0;

        virtual unsigned GetWidth() const = 0;
        virtual unsigned GetHeight() const = 0;
        virtual int GetDepth() const = 0;
        virtual int GetRenderLayer() const = 0;
        virtual void SetPaletteMaterialId(const std::string& material, size_t index) = 0;
        virtual void SetMapDimensions(unsigned width, unsigned  height) = 0;

        virtual float GetTileSizeScaler() const = 0;

        virtual bool SetTilePaletteIndex(uint8_t index, unsigned row, unsigned col) = 0;
        virtual bool GetTilePaletteIndex(uint8_t* index, unsigned row, unsigned col) const = 0;

        virtual bool SetTileValue(int32_t value, unsigned row, unsigned col) = 0;
        virtual bool GetTileValue(int32_t* value, unsigned row, unsigned col) const = 0;

        virtual void SetFlags(base::bitflag<Flags> flags) = 0;

        virtual const Class& GetClass() const = 0;

        virtual size_t GetByteCount() const = 0;

        inline unsigned GetMaxPaletteIndex() const
        { return TilemapLayerClass::GetMaxPaletteIndex(GetType()); }
        inline bool HasRenderComponent() const
        { return TilemapLayerClass::HasRenderComponent(GetType()); }
        inline bool HasDataComponent() const
        { return TilemapLayerClass::HasDataComponent(GetType()); }
        inline bool IsEnabled() const
        { return TestFlag(Flags::Enabled); }
        inline bool IsVisible() const
        { return TestFlag(Flags::Visible); }
        inline bool IsReadOnly() const
        { return TestFlag(Flags::ReadOnly); }
        inline const Class* operator->() const
        { return &GetClass(); }
    private:
    };

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

            TilemapLayerBase(const std::shared_ptr<const TilemapLayerClass>& klass,
                             std::unique_ptr<TileLoader> loader,
                             unsigned map_width, unsigned map_height)
              : mClass(klass)
              , mLoader(std::move(loader))
              , mMapWidth(map_width)
              , mMapHeight(map_height)
            {
                mFlags = mClass->GetFlags();
            }
            virtual std::string GetClassId() const override
            { return mClass->GetId(); }
            virtual std::string GetClassName() const override
            { return mClass->GetName(); }
            virtual std::string GetPaletteMaterialId(size_t index) const override
            {
                if (const auto* material = base::SafeFind(mPalette, index))
                    return *material;
                return mClass->GetPaletteMaterialId(index);
            }
            virtual base::bitflag<Flags> GetFlags() const override
            { return mFlags; }
            virtual Type GetType() const override
            { return mClass->GetType(); }
            virtual bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            virtual bool IsLoaded() const override
            { return mData != nullptr; }

            virtual void Load(const std::shared_ptr<TilemapData>& data) override
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
            virtual void FlushCache() override
            {
                if (mDirtyCache)
                    mLoader->SaveCache(*mData, mClass->GetDefaultTileValue<Tile>(), mTileCache, mCacheIndex,
                                       mClass->MapDimension(mMapWidth),
                                       mClass->MapDimension(mMapHeight));
                mDirtyCache = false;
            }
            virtual void Save() override
            {
                mLoader->SaveState(*mData);
            }

            virtual void SetPaletteMaterialId(const std::string& material, size_t index) override
            { mPalette[index] = material; }
            virtual void SetMapDimensions(unsigned width, unsigned height) override
            { mMapWidth = width; mMapHeight = height; }
            virtual unsigned GetWidth() const override
            { return mClass->MapDimension(mMapWidth); }
            virtual unsigned GetHeight() const override
            { return mClass->MapDimension(mMapHeight); }
            virtual int GetDepth() const override
            { return mClass->GetDepth(); }
            virtual int GetRenderLayer() const override
            { return mClass->GetRenderLayer(); }
            virtual float GetTileSizeScaler() const override
            { return mClass->GetTileSizeScaler(); }
            virtual bool SetTilePaletteIndex(uint8_t index, unsigned row, unsigned col) override
            { return detail::SetTilePaletteIndex(get_tile(row, col, true), index);}
            virtual bool GetTilePaletteIndex(uint8_t* index, unsigned row, unsigned col) const override
            { return detail::GetTilePaletteIndex(get_tile(row, col, false), index); }
            virtual bool SetTileValue(int32_t value, unsigned row, unsigned col) override
            { return detail::SetTileValue(get_tile(row, col, true), value); }
            virtual bool GetTileValue(int32_t* value, unsigned row, unsigned col) const override
            { return detail::GetTileValue(get_tile(row, col, false), value); }
            virtual void SetFlags(base::bitflag<Flags> flags) override
            { mFlags = flags; }
            virtual const TilemapLayerClass& GetClass() const override
            { return *mClass; }
            virtual size_t GetByteCount() const override
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
            std::shared_ptr<const TilemapLayerClass> mClass;
            std::unique_ptr<TileLoader> mLoader;
            std::shared_ptr<TilemapData> mData;
            std::unordered_map<size_t, std::string> mPalette;
            std::vector<Tile> mTileCache;
            std::size_t mCacheIndex = 0;
            base::bitflag<Flags> mFlags;
            unsigned mMapWidth  = 0;
            unsigned mMapHeight = 0;
            bool mDirtyCache = false;
        };

    } // namespace

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
    { return TilemapLayerCast<T>(layer.get()); }
    template<typename T> inline
    const T* TilemapLayerCast(const std::unique_ptr<TilemapLayer>& layer)
    { return TilemapLayerCast<T>(layer.get()); }


    class TilemapClass
    {
    public:
        enum class Perspective {
            // Map from top down or side-on view
            AxisAligned,
            // Dimetric orthographic view from a fixed angle.
            Dimetric
        };

        TilemapClass();
        TilemapClass(const TilemapClass& other);

        inline void SetName(std::string name) noexcept
        { mName = std::move(name);}
        inline void SetScriptFile(std::string file) noexcept
        { mScriptFile = std::move(file); }
        inline void SetMapWidth(unsigned width) noexcept
        { mWidth = width; }
        inline void SetMapHeight(unsigned height) noexcept
        { mHeight = height;}
        inline void SetTileWidth(float width) noexcept
        { mTileWorldSize.x = width; }
        inline void SetTileHeight(float height) noexcept
        { mTileWorldSize.y = height; }
        inline void SetTileDepth(float depth) noexcept
        { mTileWorldSize.z = depth; }
        inline void SetPerspective(Perspective perspective) noexcept
        { mPerspective = perspective; }
        inline void SetTileRenderScale(glm::vec2 scale) noexcept
        { mTileRenderScale = scale; }
        inline void SetTileSize(glm::vec3 size) noexcept
        { mTileWorldSize = size; }
        inline void SetTileRenderWidthScale(float scale) noexcept
        { mTileRenderScale.x = scale; }
        inline void SetTileRenderHeightScale(float scale) noexcept
        { mTileRenderScale.y = scale; }

        inline std::size_t GetNumLayers() const noexcept
        { return mLayers.size(); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetScriptFile() const noexcept
        { return mScriptFile;}
        inline unsigned GetMapWidth() const noexcept
        { return mWidth; }
        inline unsigned GetMapHeight() const noexcept
        { return mHeight;}
        inline float GetTileWidth() const noexcept
        { return mTileWorldSize.x; }
        inline float GetTileHeight() const noexcept
        { return mTileWorldSize.y; }
        inline float GetTileDepth() const noexcept
        { return mTileWorldSize.z; }
        inline Perspective GetPerspective() const noexcept
        { return mPerspective; }
        inline glm::vec3 GetTileSize() const noexcept
        { return mTileWorldSize; }
        inline glm::vec2 GetTileRenderScale() const noexcept
        { return mTileRenderScale; }
        inline float GetTileRenderWidthScale() const noexcept
        { return mTileRenderScale.x; }
        inline float GetTileRenderHeightScale() const noexcept
        { return mTileRenderScale.y; }

        void AddLayer(const TilemapLayerClass& layer);
        void AddLayer(TilemapLayerClass&& layer);
        void AddLayer(std::shared_ptr<TilemapLayerClass> klass);
        void DeleteLayer(size_t index);

        void SwapLayers(size_t src, size_t dst)
        {
            ASSERT(src < mLayers.size());
            ASSERT(dst < mLayers.size());
            std::swap(mLayers[src], mLayers[dst]);
        }

        TilemapLayerClass& GetLayer(size_t index);
        TilemapLayerClass* FindLayerById(const std::string& id);
        TilemapLayerClass* FindLayerByName(const std::string& name);

        const TilemapLayerClass& GetLayer(size_t index) const;
        const TilemapLayerClass* FindLayerById(const std::string& id) const;
        const TilemapLayerClass* FindLayerByName(const std::string& name) const;

        std::size_t FindLayerIndex(const TilemapLayerClass* layer) const;

        std::size_t GetHash() const;
        std::shared_ptr<TilemapLayerClass> GetSharedLayerClass(size_t index);
        std::shared_ptr<const TilemapLayerClass> GetSharedLayerClass(size_t index) const;
        std::shared_ptr<TilemapLayerClass> FindSharedLayerClass(const std::string& id);
        std::shared_ptr<const TilemapLayerClass> FindSharedLayerClass(const std::string& id) const;

        TilemapClass Clone() const;

        TilemapClass& operator=(const TilemapClass& other);

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        std::string mId;
        std::string mName;
        std::string mScriptFile;
        unsigned mWidth = 0;
        unsigned mHeight = 0;
        glm::vec3 mTileWorldSize = {0.0f, 0.0f, 0.0f};
        glm::vec2 mTileRenderScale = {1.0f, 1.0f};
        Perspective mPerspective = Perspective::AxisAligned;
        std::vector<std::shared_ptr<TilemapLayerClass>> mLayers;
    };


    class Tilemap
    {
    public:
        using Class = TilemapClass;
        using Perspective = TilemapClass::Perspective;

        Tilemap(const std::shared_ptr<const TilemapClass>& klass);
        Tilemap(const TilemapClass& klass);

        // Load the contents of the tilemap instance layers.
        // Returns true if all layers loaded successfully or
        // false if a layer failed to load.
        bool Load(const Loader& loader);

        void AddLayer(std::unique_ptr<TilemapLayer> layer);
        void DeleteLayer(std::size_t index);

        const TilemapLayer& GetLayer(size_t index) const;
        const TilemapLayer* FindLayerByClassName(const std::string& name) const;
        const TilemapLayer* FindLayerByClassId(const std::string& id) const;

        TilemapLayer& GetLayer(size_t index);
        TilemapLayer* FindLayerByClassName(const std::string& name);
        TilemapLayer* FindLayerByClassId(const std::string& id);

        void SwapLayers(size_t src, size_t dst) noexcept
        {
            ASSERT(src < mLayers.size());
            ASSERT(dst < mLayers.size());
            std::swap(mLayers[src], mLayers[dst]);
        }
        std::size_t FindLayerIndex(const TilemapLayer* layer) const noexcept;

        inline std::size_t GetNumLayers() const noexcept
        { return mLayers.size(); }
        inline std::string GetClassName() const noexcept
        { return mClass->GetName(); }
        inline std::string GetClassId() const noexcept
        { return mClass->GetId(); }
        inline unsigned GetMapWidth() const noexcept
        { return mClass->GetMapWidth(); }
        inline unsigned GetMapHeight() const noexcept
        { return mClass->GetMapHeight(); }
        inline float GetTileWidth() const noexcept
        { return mClass->GetTileWidth(); }
        inline float GetTileHeight() const noexcept
        { return mClass->GetTileHeight(); }
        inline float GetTileDepth() const noexcept
        { return mClass->GetTileDepth(); }
        inline Perspective GetPerspective() const noexcept
        { return mClass->GetPerspective(); }
        inline float GetTileRenderWidthScale() const noexcept
        { return mClass->GetTileRenderWidthScale(); }
        inline float GetTileRenderHeightScale() const noexcept
        { return mClass->GetTileRenderHeightScale(); }
        inline const Class& GetClass() const noexcept
        { return *mClass; }
        inline const Class* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const TilemapClass> mClass;
        std::vector<std::unique_ptr<TilemapLayer>> mLayers;

    };
    std::unique_ptr<TilemapLayer> CreateTilemapLayer(const std::shared_ptr<const TilemapLayerClass>& klass,
                                                     unsigned map_width, unsigned map_height);
    std::unique_ptr<Tilemap> CreateTilemap(const std::shared_ptr<const TilemapClass>& klass);
    std::unique_ptr<Tilemap> CreateTilemap(const TilemapClass& klass);

} // namespace
