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

#include "game/entity_node_fixture.h"

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"

namespace game
{
std::size_t FixtureClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mCollisionShape);
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mPolygonShapeId);
    hash = base::hash_combine(hash, mRigidBodyNodeId);
    hash = base::hash_combine(hash, mFriction);
    hash = base::hash_combine(hash, mDensity);
    hash = base::hash_combine(hash, mRestitution);
    return hash;
}

void FixtureClass::IntoJson(data::Writer& data) const
{
    data.Write("shape",       mCollisionShape);
    data.Write("flags",       mBitFlags);
    data.Write("polygon",     mPolygonShapeId);
    data.Write("rigid_body",  mRigidBodyNodeId);
    data.Write("friction",    mFriction);
    data.Write("density",     mDensity);
    data.Write("restitution", mRestitution);
}

bool FixtureClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("shape",       &mCollisionShape);
    ok &= data.Read("flags",       &mBitFlags);
    ok &= data.Read("polygon",     &mPolygonShapeId);
    ok &= data.Read("rigid_body",  &mRigidBodyNodeId);
    ok &= data.Read("friction",    &mFriction);
    ok &= data.Read("density",     &mDensity);
    ok &= data.Read("restitution", &mRestitution);
    return ok;
}

} // namespace