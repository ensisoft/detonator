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
#include <cstdint>

#include "warnpush.h"
#include "warnpop.h"

namespace game
{
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

    struct TileRowCol {
        unsigned row = 0;
        unsigned col = 0;
    };

} // namespace
