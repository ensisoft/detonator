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

#include "game/entity_node_tilemap_node.h"

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"

namespace game
{

size_t MapNodeClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mMapSortPoint);
    hash = base::hash_combine(hash, mMapLayer);
    return hash;
}

void MapNodeClass::IntoJson(data::Writer& data) const
{
    data.Write("map_sort_point", mMapSortPoint);
    data.Write("map_layer", mMapLayer);
}
bool MapNodeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("map_sort_point", &mMapSortPoint);
    ok &= data.Read("map_layer", &mMapLayer);
    return ok;
}

} // namespace

