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

#include "game/tilemap_layer_class.h"

namespace game
{
    class TilemapData;
    class Loader;

    class TilemapLayer
    {
    public:
        using PaletteFlags  = TilemapLayerClass::PaletteFlags;
        using TileOcclusion = TilemapLayerClass::TileOcclusion;
        using Flags = TilemapLayerClass::Flags;
        using Type  = TilemapLayerClass::Type;
        using Class = TilemapLayerClass;

        virtual ~TilemapLayer() = default;
        virtual std::string GetClassId() const = 0;
        virtual std::string GetClassName() const = 0;
        virtual std::string GetPaletteMaterialId(size_t palette_index) const = 0;
        virtual std::uint8_t GetPaletteFlags(size_t palette_index) const = 0;
        virtual TileOcclusion GetPaletteOcclusion(size_t palette_index) const = 0;
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
        virtual unsigned GetLayer() const = 0;

        virtual void SetMapDimensions(unsigned width, unsigned  height) = 0;

        virtual float GetTileSizeScaler() const = 0;

        virtual bool SetTilePaletteIndex(uint8_t index, unsigned row, unsigned col) = 0;
        virtual bool GetTilePaletteIndex(uint8_t* index, unsigned row, unsigned col) const = 0;

        virtual bool SetTileValue(int32_t value, unsigned row, unsigned col) = 0;
        virtual bool GetTileValue(int32_t* value, unsigned row, unsigned col) const = 0;

        virtual void SetFlags(base::bitflag<Flags> flags) = 0;

        virtual bool TestPaletteFlag(PaletteFlags flags, size_t palette_index) const = 0;

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

    std::unique_ptr<TilemapLayer> CreateTilemapLayer(const std::shared_ptr<const TilemapLayerClass>& klass,
                                                     unsigned map_width, unsigned map_height);

} // namespace