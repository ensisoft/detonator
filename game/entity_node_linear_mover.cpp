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

#include "game/entity_node_linear_mover.h"

#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"

namespace game
{

 size_t LinearMoverClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mIntegrator);
    hash = base::hash_combine(hash, mLinearVelocity);
    hash = base::hash_combine(hash, mLinearAcceleration);
    hash = base::hash_combine(hash, mAngularVelocity);
    hash = base::hash_combine(hash, mAngularAcceleration);
    return hash;
}

void LinearMoverClass::IntoJson(data::Writer& data) const
{
    data.Write("flags",                mFlags);
    data.Write("integrator",           mIntegrator);
    data.Write("linear_velocity",      mLinearVelocity);
    data.Write("linear_acceleration",  mLinearAcceleration);
    data.Write("angular_velocity",     mAngularVelocity);
    data.Write("angular_acceleration", mAngularAcceleration);
}

bool LinearMoverClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("flags",                &mFlags);
    ok &= data.Read("integrator",           &mIntegrator);
    ok &= data.Read("linear_velocity",      &mLinearVelocity);
    ok &= data.Read("linear_acceleration",  &mLinearAcceleration);
    ok &= data.Read("angular_velocity",     &mAngularVelocity);
    ok &= data.Read("angular_acceleration", &mAngularAcceleration);
    return ok;
}

} // namespace