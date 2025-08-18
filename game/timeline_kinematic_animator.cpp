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
#include "game/timeline_kinematic_animator.h"
#include "game/entity_node.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_rigid_body.h"

namespace game
{

std::size_t KinematicAnimatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mTimelineId);
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
    data.Write("timeline",             mTimelineId);
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
    ok &= data.Read("timeline",             &mTimelineId);
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
                WARN("Kinematic actuator can't apply on a static rigid body. [animator='%1', node='%2']", mClass->GetName(), node.GetName());
            }
        }
        else
        {
            WARN("Kinematic animator can't apply on a node without rigid body. [animator='%1']", mClass->GetName());
        }
    }
    else if (target == KinematicAnimatorClass::Target::LinearMover)
    {
        if (const auto* mover = node.GetLinearMover())
        {
            mStartLinearVelocity      = mover->GetLinearVelocity();
            mStartLinearAcceleration  = mover->GetLinearAcceleration();
            mStartAngularVelocity     = mover->GetAngularVelocity();
            mStartAngularAcceleration = mover->GetAngularAcceleration();
        }
        else
        {
            WARN("Kinematic animator can't apply on a node without a linear mover. [animator='%1']", mClass->GetName());
        }
    } else BUG("Missing kinematic animator target.");
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
            // don't set any adjustment on the rigid body if we still have a
            // pending  adjustment. currently this causes a warning which is
            // really intended for the *game*
            // we might be producing values here faster than the physics engine
            // can consume since likely the animation runs at a higher Hz than
            // the physics.
            // todo: maybe the better thing to do here would be just to overwrite
            // the value? But how would that work with the game doing adjustments?
            // (which it probably shouldn't)
            if (!body->HasLinearVelocityAdjustment())
                body->AdjustLinearVelocity(linear_velocity);
            if (!body->HasAngularVelocityAdjustment())
                body->AdjustAngularVelocity(angular_velocity);
        }
    }
    else if (target == KinematicAnimatorClass::Target::LinearMover)
    {
        if (auto* mover = node.GetLinearMover())
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

            mover->SetLinearVelocity(linear_velocity);
            mover->SetLinearAcceleration(linear_acceleration);
            mover->SetAngularVelocity(angular_velocity);
            mover->SetAngularAcceleration(angular_acceleration);
        }
    } else BUG("Missing kinematic animator target.");
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
    else if (target == KinematicAnimatorClass::Target::LinearMover)
    {
        if (auto* mover = node.GetLinearMover())
        {
            mover->SetLinearVelocity(mClass->GetEndLinearVelocity());
            mover->SetLinearAcceleration(mClass->GetEndLinearAcceleration());
            mover->SetAngularVelocity(mClass->GetEndAngularVelocity());
            mover->SetAngularAcceleration(mClass->GetEndAngularAcceleration());
        }
    }
    else BUG("Missing kinematic animator target.");
}

} // namespace