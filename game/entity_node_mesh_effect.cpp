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

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_mesh_effect.h"

namespace  game
{
void MeshEffectClass::IntoJson(data::Writer& data) const
{
    data.Write("mesh-effect-type", mEffectType);
    if (const auto* ptr = std::get_if<MeshExplosionEffectArgs>(&mEffectArgs))
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("mesh-subdivision-count",        ptr->mesh_subdivision_count);
        chunk->Write("shard-linear-speed",            ptr->shard_linear_speed);
        chunk->Write("shard-linear-acceleration",     ptr->shard_linear_acceleration);
        chunk->Write("shard-rotational-speed",        ptr->shard_rotational_speed);
        chunk->Write("shard-rotational-acceleration", ptr->shard_rotational_acceleration);
        data.Write("mesh-explosion-args", std::move(chunk));
    }
}

bool MeshEffectClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("mesh-effect-type", &mEffectType);
    if (data.HasChunk("mesh-explosion-args"))
    {
        const auto& chunk = data.GetReadChunk("mesh-explosion-args");
        MeshExplosionEffectArgs args;
        chunk->Read("mesh-subdivision-count",        &args.mesh_subdivision_count);
        chunk->Read("shard-linear-speed",            &args.shard_linear_speed);
        chunk->Read("shard-linear-acceleration",     &args.shard_linear_acceleration);
        chunk->Read("shard-rotational-speed",        &args.shard_rotational_speed);
        chunk->Read("shard-rotational-acceleration", &args.shard_rotational_acceleration);
        mEffectArgs = args;
    }
    return ok;
}
size_t MeshEffectClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mEffectType);
    hash = base::hash_combine(hash, mEffectArgs);
    return hash;
}
} // namespace