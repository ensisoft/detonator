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

#include "config.h"

#include <set>

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_light.h"

namespace game
{

void LightClass::IntoJson(data::Writer& data) const
{
    data.Write("type", mLightType);
    data.Write("flags", mFlags);
    data.Write("layer", mLayer);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> keys;
    for (const auto& uniform : mLightParams)
        keys.insert(uniform.first);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mLightParams, key);
        auto chunk = data.NewWriteChunk();
        chunk->Write("key", key);
        chunk->Write("val", *param);
        data.AppendChunk("parameters", std::move(chunk));
    }

}
bool LightClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("type", &mLightType);
    ok &= data.Read("flags", &mFlags);
    ok &= data.Read("layer", &mLayer);

    for (unsigned i=0; i<data.GetNumChunks("parameters"); ++i)
    {
        const auto& chunk = data.GetReadChunk("parameters", i);

        std::string key;
        LightParam  val;
        ok &= chunk->Read("key", &key);
        ok &= chunk->Read("val", &val);
        mLightParams[key] = std::move(val);
    }
    return ok;
}

size_t LightClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mLightType);
    hash = base::hash_combine(hash, mLayer);

    std::set<std::string> keys;
    for (const auto& param : mLightParams)
        keys.insert(param.first);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mLightParams, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *param);
    }
    return hash;
}

} // namespace
