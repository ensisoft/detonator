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

#include <memory>

#include "game/tilemap_data.h"

namespace game
{
    using TilemapDataHandle = std::shared_ptr<TilemapData>;

    class Loader
    {
    public:
        virtual ~Loader() = default;

        // Descriptor for loading tilemap layer data.
        struct TilemapDataDesc {
            // the layer ID
            std::string layer;
            // the data object ID.
            std::string data;
            // data object URI.
            std::string uri;
            bool read_only = false;
        };
        // Load the data for a tilemap layer based on the layer ID and the
        // associated data file URI. The read only flag indicates whether
        // the map is allowed to be modified by the running game itself.
        // Returns a handle to the tilemap data object or nullptr if the
        // loading fails.
        virtual TilemapDataHandle LoadTilemapData(const TilemapDataDesc& desc) const = 0;
    private:

    };
} // namespace

