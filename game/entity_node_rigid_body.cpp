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
#include "base/logging.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_rigid_body_joint.h"

namespace game
{

std::size_t RigidBodyClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mSimulation);
    hash = base::hash_combine(hash, mCollisionShape);
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mPolygonShapeId);
    hash = base::hash_combine(hash, mFriction);
    hash = base::hash_combine(hash, mRestitution);
    hash = base::hash_combine(hash, mAngularDamping);
    hash = base::hash_combine(hash, mLinearDamping);
    hash = base::hash_combine(hash, mDensity);
    return hash;
}

void RigidBodyClass::IntoJson(data::Writer& data) const
{
    data.Write("simulation",      mSimulation);
    data.Write("shape",           mCollisionShape);
    data.Write("flags",           mBitFlags);
    data.Write("polygon",         mPolygonShapeId);
    data.Write("friction",        mFriction);
    data.Write("restitution",     mRestitution);
    data.Write("angular_damping", mAngularDamping);
    data.Write("linear_damping",  mLinearDamping);
    data.Write("density",         mDensity);
}

bool RigidBodyClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("simulation",      &mSimulation);
    ok &= data.Read("shape",           &mCollisionShape);
    ok &= data.Read("flags",           &mBitFlags);
    ok &= data.Read("polygon",         &mPolygonShapeId);
    ok &= data.Read("friction",        &mFriction);
    ok &= data.Read("restitution",     &mRestitution);
    ok &= data.Read("angular_damping", &mAngularDamping);
    ok &= data.Read("linear_damping",  &mLinearDamping);
    ok &= data.Read("density",         &mDensity);
    return ok;
}

void RigidBody::ApplyLinearImpulseToCenter(const glm::vec2& impulse) noexcept
{
    if (mCenterImpulse.has_value())
        WARN("Overwriting pending impulse on rigid body.");

    mCenterImpulse = impulse;
}

void RigidBody::ApplyForceToCenter(const glm::vec2& force) noexcept
{
    if (mCenterForce.has_value())
        WARN("Overwriting pending force on rigid body.");

    mCenterForce = force;
}

void RigidBody::AddLinearImpulseToCenter(const glm::vec2& impulse) noexcept
{
    mCenterImpulse = GetLinearImpulseToCenter() + impulse;
}

void RigidBody::AdjustLinearVelocity(const glm::vec2& velocity) noexcept
{
    if (mLinearVelocityAdjustment.has_value())
        WARN("Overwriting pending rigid body linear adjustment.");

    mLinearVelocityAdjustment = velocity;
}

void RigidBody::AdjustAngularVelocity(float radians) noexcept
{
    if (mAngularVelocityAdjustment.has_value())
        WARN("Overwriting pending angular velocity adjustment.");

    mAngularVelocityAdjustment = radians;
}

RigidBodyJoint* RigidBody::FindJointByName(const std::string& name) noexcept
{
    for (auto& joint : mJointConnections)
    {
        if (joint->GetName() == name)
            return joint;
    }
    return nullptr;
}
RigidBodyJoint* RigidBody::FindJointByClassId(const std::string& id) noexcept
{
    for (auto& joint : mJointConnections)
    {
        if (joint->GetClassId() == id)
            return joint;
    }
    return nullptr;
}

} // namespace