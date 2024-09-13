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

#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/entity_state.h"

namespace game
{

EntityStateClass::EntityStateClass(std::string id)
  : mId(std::move(id))
{}

std::size_t EntityStateClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    return hash;
}

void EntityStateClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
}
bool EntityStateClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    return ok;
}

EntityStateClass EntityStateClass::Clone() const
{
    EntityStateClass dolly(base::RandomString(10));
    dolly.mName = mName;
    return dolly;
}

EntityStateTransitionClass::EntityStateTransitionClass(std::string id)
  : mId(std::move(id))
{}

std::size_t EntityStateTransitionClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mDstStateId);
    hash = base::hash_combine(hash, mSrcStateId);
    hash = base::hash_combine(hash, mDuration);
    return hash;
}

void EntityStateTransitionClass::IntoJson(data::Writer& data) const
{
    data.Write("name", mName);
    data.Write("id", mId);
    data.Write("src_state_id", mSrcStateId);
    data.Write("dst_state_id", mDstStateId);
    data.Write("duration", mDuration);
}
bool EntityStateTransitionClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    ok &= data.Read("src_state_id", &mSrcStateId);
    ok &= data.Read("dst_state_id", &mDstStateId);
    ok &= data.Read("duration", &mDuration);
    return ok;
}

EntityStateTransitionClass EntityStateTransitionClass::Clone() const
{
    EntityStateTransitionClass dolly(base::RandomString(10));
    dolly.mName = mName;
    dolly.mDstStateId = mDstStateId;
    dolly.mSrcStateId = mSrcStateId;
    dolly.mDuration   = mDuration;
    return dolly;
}
} // namespace