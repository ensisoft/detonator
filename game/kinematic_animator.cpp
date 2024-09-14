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

#include "base/assert.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/kinematic_animator.h"
#include "game/entity_node.h"
#include "game/entity_node_transformer.h"
#include "game/entity_node_rigid_body.h"

namespace game
{

std::size_t KinematicAnimatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mTarget);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndLinearVelocity);
    hash = base::hash_combine(hash, mEndLinearAcceleration);
    hash = base::hash_combine(hash, mEndAngularVelocity);
    hash = base::hash_combine(hash, mEndAngularAcceleration);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void KinematicAnimatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",                   mId);
    data.Write("name",                 mName);
    data.Write("node",                 mNodeId);
    data.Write("method",               mInterpolation);
    data.Write("target",               mTarget);
    data.Write("starttime",            mStartTime);
    data.Write("duration",             mDuration);
    data.Write("linear_velocity",      mEndLinearVelocity);
    data.Write("linear_acceleration",  mEndLinearAcceleration);
    data.Write("angular_velocity",     mEndAngularVelocity);
    data.Write("angular_acceleration", mEndAngularAcceleration);
    data.Write("flags",                mFlags);
}

bool KinematicAnimatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                   &mId);
    ok &= data.Read("name",                 &mName);
    ok &= data.Read("node",                 &mNodeId);
    ok &= data.Read("method",               &mInterpolation);
    ok &= data.Read("target",               &mTarget);
    ok &= data.Read("starttime",            &mStartTime);
    ok &= data.Read("duration",             &mDuration);
    ok &= data.Read("linear_velocity",      &mEndLinearVelocity);
    ok &= data.Read("linear_acceleration",  &mEndLinearAcceleration);
    ok &= data.Read("angular_velocity",     &mEndAngularVelocity);
    ok &= data.Read("angular_acceleration", &mEndAngularAcceleration);
    ok &= data.Read("flags",                &mFlags);
    return ok;
}

void KinematicAnimator::Start(EntityNode& node)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicAnimatorClass::Target::RigidBody)
    {
        if (const auto* body = node.GetRigidBody())
        {
            mStartLinearVelocity = body->GetLinearVelocity();
            mStartAngularVelocity = body->GetAngularVelocity();
            if (body->GetSimulation() == RigidBodyClass::Simulation::Static)
            {
                WARN("Kinematic actuator can't apply on a static rigid body. [actuator='%1', node='%2']", mClass->GetName(), node.GetName());
            }
        }
        else
        {
            WARN("Kinematic animator can't apply on a node without rigid body. [actuator='%1']", mClass->GetName());
        }
    }
    else if (target == KinematicAnimatorClass::Target::Transformer)
    {
        if (const auto* transformer = node.GetTransformer())
        {
            mStartLinearVelocity      = transformer->GetLinearVelocity();
            mStartLinearAcceleration  = transformer->GetLinearAcceleration();
            mStartAngularVelocity     = transformer->GetAngularVelocity();
            mStartAngularAcceleration = transformer->GetAngularAcceleration();
        }
        else
        {
            WARN("Kinematic animator can't apply on a node without a transformer. [actuator='%1']", mClass->GetName());
        }
    } else BUG("Missing kinematic actuator target.");
}

void KinematicAnimator::Apply(EntityNode& node, float t)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicAnimatorClass::Target::RigidBody)
    {
        if (auto* body = node.GetRigidBody())
        {
            const auto method = mClass->GetInterpolation();
            const auto linear_velocity = math::interpolate(mStartLinearVelocity,
                                                           mClass->GetEndLinearVelocity(), t, method);
            const auto angular_velocity = math::interpolate(mStartAngularVelocity,
                                                            mClass->GetEndAngularVelocity(), t, method);
            body->AdjustLinearVelocity(linear_velocity);
            body->AdjustAngularVelocity(angular_velocity);
        }
    }
    else if (target == KinematicAnimatorClass::Target::Transformer)
    {
        if (auto* transformer = node.GetTransformer())
        {
            const auto method = mClass->GetInterpolation();
            const auto linear_velocity = math::interpolate(mStartLinearVelocity,
                                                           mClass->GetEndLinearVelocity(), t, method);
            const auto linear_acceleration = math::interpolate(mStartLinearAcceleration,
                                                               mClass->GetEndLinearAcceleration(), t, method);
            const auto angular_velocity = math::interpolate(mStartAngularVelocity,
                                                            mClass->GetEndAngularVelocity(), t, method);
            const auto angular_acceleration = math::interpolate(mStartAngularAcceleration,
                                                                mClass->GetEndAngularAcceleration(), t, method);

            transformer->SetLinearVelocity(linear_velocity);
            transformer->SetLinearAcceleration(linear_acceleration);
            transformer->SetAngularVelocity(angular_velocity);
            transformer->SetAngularAcceleration(angular_acceleration);
        }
    } else BUG("Missing kinematic actuator target.");
}

void KinematicAnimator::Finish(EntityNode& node)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicAnimatorClass::Target::RigidBody)
    {
        if (auto* body = node.GetRigidBody())
        {
            body->AdjustLinearVelocity(mClass->GetEndLinearVelocity());
            body->AdjustAngularVelocity(mClass->GetEndAngularVelocity());
        }
    }
    else if (target == KinematicAnimatorClass::Target::Transformer)
    {
        if (auto* transformer = node.GetTransformer())
        {
            transformer->SetLinearVelocity(mClass->GetEndLinearVelocity());
            transformer->SetLinearAcceleration(mClass->GetEndLinearAcceleration());
            transformer->SetAngularVelocity(mClass->GetEndAngularVelocity());
            transformer->SetAngularAcceleration(mClass->GetEndAngularAcceleration());
        }
    }
    else BUG("Missing kinematic actuator target.");
}

} // namespace