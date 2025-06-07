// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <cstdint>

#include "data/fwd.h"
#include "game/enum.h"

namespace game
{
    class MapNodeClass
    {
    public:
        using TileOcclusion = game::TileOcclusion;

        inline void SetTileOcclusion(TileOcclusion occlusion) noexcept
        { mTileOcclusion = occlusion; }
        inline void SetMapSortPoint(glm::vec2 point) noexcept
        { mMapSortPoint = point; }
        inline auto GetSortPoint() const noexcept
        { return mMapSortPoint; }
        inline void SetMapLayer(uint16_t layer) noexcept
        { mMapLayer = layer; }
        inline auto GetMapLayer() const noexcept
        { return mMapLayer; }
        inline auto GetTileOcclusion() const noexcept
        { return mTileOcclusion; }

        std::size_t GetHash() const noexcept;

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        glm::vec2 mMapSortPoint = {0.5f, 1.0f};
        // layer in the map when using a tilemap world
        uint16_t mMapLayer = 0;

        TileOcclusion mTileOcclusion = TileOcclusion::None;
    };

    class MapNode
    {
    public:
        using TileOcclusion = MapNodeClass::TileOcclusion;

        explicit MapNode(std::shared_ptr<const MapNodeClass> klass) noexcept
           : mClass(std::move(klass))
        {}
        inline auto GetTileOcclusion() const noexcept
        { return mClass->GetTileOcclusion(); }
        inline auto GetSortPoint() const noexcept
        { return mClass->GetSortPoint(); }
        inline auto GetMapLayer() const noexcept
        { return mClass->GetMapLayer(); }
        inline const MapNodeClass& GetClass() const noexcept
        { return *mClass; }
        inline const MapNodeClass* operator ->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const MapNodeClass> mClass;
    };


} // namespace