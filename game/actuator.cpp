// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <cmath>

#include "base/logging.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/utility.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/actuator.h"
#include "game/entity.h"

namespace game
{

std::size_t SetFlagActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mFlagName);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mFlagAction);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mTime);
    return hash;
}

void SetFlagActuatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",        mId);
    data.Write("name",      mName);
    data.Write("node",      mNodeId);
    data.Write("flag",      mFlagName);
    data.Write("starttime", mStartTime);
    data.Write("duration",  mDuration);
    data.Write("action",    mFlagAction);
    data.Write("flags",     mFlags);
    data.Write("time",      mTime);
}

bool SetFlagActuatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",        &mId);
    ok &= data.Read("name",      &mName);
    ok &= data.Read("node",      &mNodeId);
    ok &= data.Read("flag",      &mFlagName);
    ok &= data.Read("starttime", &mStartTime);
    ok &= data.Read("duration",  &mDuration);
    ok &= data.Read("action",    &mFlagAction);
    ok &= data.Read("flags",     &mFlags);
    ok &= data.Read("time",      &mTime);
    return ok;
}

std::size_t KinematicActuatorClass::GetHash() const
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

void KinematicActuatorClass::IntoJson(data::Writer& data) const
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

bool KinematicActuatorClass::FromJson(const data::Reader& data)
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

size_t SetValueActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mParamName);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndValue);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void SetValueActuatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",        mId);
    data.Write("cname",     mName);
    data.Write("node",      mNodeId);
    data.Write("method",    mInterpolation);
    data.Write("name",      mParamName);
    data.Write("starttime", mStartTime);
    data.Write("duration",  mDuration);
    data.Write("value",     mEndValue);
    data.Write("flags",     mFlags);
}

bool SetValueActuatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",        &mId);
    ok &= data.Read("cname",     &mName);
    ok &= data.Read("node",      &mNodeId);
    ok &= data.Read("method",    &mInterpolation);
    ok &= data.Read("name",      &mParamName);
    ok &= data.Read("starttime", &mStartTime);
    ok &= data.Read("duration",  &mDuration);
    ok &= data.Read("value",     &mEndValue);
    ok &= data.Read("flags",     &mFlags);
    return ok;
}

void TransformActuatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",        mId);
    data.Write("name",      mName);
    data.Write("node",      mNodeId);
    data.Write("method",    mInterpolation);
    data.Write("starttime", mStartTime);
    data.Write("duration",  mDuration);
    data.Write("position",  mEndPosition);
    data.Write("size",      mEndSize);
    data.Write("scale",     mEndScale);
    data.Write("rotation",  mEndRotation);
    data.Write("flags",     mFlags);
}

bool TransformActuatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",        &mId);
    ok &= data.Read("name",      &mName);
    ok &= data.Read("node",      &mNodeId);
    ok &= data.Read("starttime", &mStartTime);
    ok &= data.Read("duration",  &mDuration);
    ok &= data.Read("position",  &mEndPosition);
    ok &= data.Read("size",      &mEndSize);
    ok &= data.Read("scale",     &mEndScale);
    ok &= data.Read("rotation",  &mEndRotation);
    ok &= data.Read("method",    &mInterpolation);
    ok &= data.Read("flags",     &mFlags);
    return ok;
}

std::size_t TransformActuatorClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);
    hash = base::hash_combine(hash, mEndPosition);
    hash = base::hash_combine(hash, mEndSize);
    hash = base::hash_combine(hash, mEndScale);
    hash = base::hash_combine(hash, mEndRotation);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void MaterialActuatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",       mId);
    data.Write("cname",    mName);
    data.Write("node",     mNodeId);
    data.Write("method",   mInterpolation);
    data.Write("start",    mStartTime);
    data.Write("duration", mDuration);
    data.Write("flags",    mFlags);
    for (const auto& [key, val] : mMaterialParams)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        chunk->Write("value", val);
        data.AppendChunk("params", std::move(chunk));
    }
}

bool MaterialActuatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",       &mId);
    ok &= data.Read("cname",    &mName);
    ok &= data.Read("node",     &mNodeId);
    ok &= data.Read("method",   &mInterpolation);
    ok &= data.Read("start",    &mStartTime);
    ok &= data.Read("duration", &mDuration);
    ok &= data.Read("flags",    &mFlags);
    for (unsigned i=0; i<data.GetNumChunks("params"); ++i)
    {
        const auto& chunk = data.GetReadChunk("params", i);
        std::string name;
        MaterialParam  value;
        ok &= chunk->Read("name",  &name);
        ok &= chunk->Read("value", &value);
        mMaterialParams[std::move(name)] = value;
    }
    return ok;
}
std::size_t MaterialActuatorClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);

    std::set<std::string> keys;
    for (const auto& [key, val] : mMaterialParams)
        keys.insert(key);

    for (const auto& key : keys)
    {
        const auto& val = *base::SafeFind(mMaterialParams, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, val);
    }
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void KinematicActuator::Start(EntityNode& node)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicActuatorClass::Target::RigidBody)
    {
        if (const auto* body = node.GetRigidBody())
        {
            mStartLinearVelocity = body->GetLinearVelocity();
            mStartAngularVelocity = body->GetAngularVelocity();
            if (body->GetSimulation() == RigidBodyItemClass::Simulation::Static)
            {
                WARN("Kinematic actuator can't apply on a static rigid body. [actuator='%1', node='%2']", mClass->GetName(), node.GetName());
            }
        }
        else
        {
            WARN("Kinematic actuator can't apply on a node without rigid body. [actuator='%1']", mClass->GetName());
        }
    }
    else if (target == KinematicActuatorClass::Target::Transformer)
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
            WARN("Kinematic actuator can't apply on a node without a transformer. [actuator='%1']", mClass->GetName());
        }
    } else BUG("Missing kinematic actuator target.");
}
void KinematicActuator::Apply(EntityNode& node, float t)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicActuatorClass::Target::RigidBody)
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
    else if (target == KinematicActuatorClass::Target::Transformer)
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

void KinematicActuator::Finish(EntityNode& node)
{
    const auto target = mClass->GetTarget();
    if (target == KinematicActuatorClass::Target::RigidBody)
    {
        if (auto* body = node.GetRigidBody())
        {
            body->AdjustLinearVelocity(mClass->GetEndLinearVelocity());
            body->AdjustAngularVelocity(mClass->GetEndAngularVelocity());
        }
    }
    else if (target == KinematicActuatorClass::Target::Transformer)
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

void SetFlagActuator::Start(EntityNode& node)
{
    if (!CanApply(node, true /*verbose*/))
        return;

    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();
    const auto* spatial = node.GetSpatialNode();
    const auto* transformer= node.GetTransformer();

    using FlagName = SetFlagActuatorClass::FlagName;
    const auto flag = mClass->GetFlagName();

    if (flag == FlagName::Drawable_VisibleInGame)
        mStartState = draw->TestFlag(DrawableItem::Flags::VisibleInGame);
    else if (flag == FlagName::Drawable_UpdateMaterial)
        mStartState = draw->TestFlag(DrawableItem::Flags::UpdateMaterial);
    else if (flag == FlagName::Drawable_UpdateDrawable)
        mStartState = draw->TestFlag(DrawableItem::Flags::UpdateDrawable);
    else if (flag == FlagName::Drawable_Restart)
        mStartState = draw->TestFlag(DrawableItem::Flags::RestartDrawable);
    else if (flag == FlagName::Drawable_FlipHorizontally)
        mStartState = draw->TestFlag(DrawableItem::Flags::FlipHorizontally);
    else if (flag == FlagName::Drawable_FlipVertically)
        mStartState = draw->TestFlag(DrawableItem::Flags::FlipVertically);
    else if (flag == FlagName::Drawable_DoubleSided)
        mStartState = draw->TestFlag(DrawableItem::Flags::DoubleSided);
    else if (flag == FlagName::Drawable_DepthTest)
        mStartState = draw->TestFlag(DrawableItem::Flags::DepthTest);
    else if (flag == FlagName::Drawable_PPEnableBloom)
        mStartState = draw->TestFlag(DrawableItem::Flags::PP_EnableBloom);
    else if (flag == FlagName::RigidBody_Bullet)
        mStartState = body->TestFlag(RigidBodyItem::Flags::Bullet);
    else if (flag == FlagName::RigidBody_Sensor)
        mStartState = body->TestFlag(RigidBodyItem::Flags::Sensor);
    else if (flag == FlagName::RigidBody_Enabled)
        mStartState = body->TestFlag(RigidBodyItem::Flags::Enabled);
    else if (flag == FlagName::RigidBody_CanSleep)
        mStartState = body->TestFlag(RigidBodyItem::Flags::CanSleep);
    else if (flag == FlagName::RigidBody_DiscardRotation)
        mStartState = body->TestFlag(RigidBodyItem::Flags::DiscardRotation);
    else if (flag == FlagName::TextItem_VisibleInGame)
        mStartState = text->TestFlag(TextItem::Flags::VisibleInGame);
    else if (flag == FlagName::TextItem_Blink)
        mStartState = text->TestFlag(TextItem::Flags::BlinkText);
    else if (flag == FlagName::TextItem_Underline)
        mStartState = text->TestFlag(TextItem::Flags::UnderlineText);
    else if (flag == FlagName::TextItem_PPEnableBloom)
        mStartState = text->TestFlag(TextItem::Flags::PP_EnableBloom);
    else if (flag == FlagName::SpatialNode_Enabled)
        mStartState = spatial->TestFlag(SpatialNode::Flags::Enabled);
    else if (flag == FlagName::Transformer_Enabled)
        mStartState = transformer->TestFlag(NodeTransformer::Flags::Enabled);
    else BUG("Unhandled flag in set flag actuator.");

    if (mTime == 0.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }

}
void SetFlagActuator::Apply(EntityNode& node, float t)
{
    if (t >= mTime && mTime != -1.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }
}
void SetFlagActuator::Finish(EntityNode& node)
{
    if (mTime == 1.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }
}

void SetFlagActuator::SetFlag(EntityNode& node) const
{
    if (mTime == -1)
        return;

    if (!CanApply(node, false))
        return;

    bool next_value = false;
    const auto action = mClass->GetFlagAction();
    if (action == FlagAction::Toggle)
        next_value = !mStartState;
    else if (action == FlagAction::On)
        next_value = true;
    else if (action == FlagAction::Off)
        next_value = false;

    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();
    auto* spatial = node.GetSpatialNode();
    auto* transformer = node.GetTransformer();

    using FlagName = SetFlagActuatorClass::FlagName;
    const auto flag = mClass->GetFlagName();

    if (flag == FlagName::Drawable_VisibleInGame)
        draw->SetFlag(DrawableItem::Flags::VisibleInGame, next_value);
    else if (flag == FlagName::Drawable_UpdateMaterial)
        draw->SetFlag(DrawableItem::Flags::UpdateMaterial, next_value);
    else if (flag == FlagName::Drawable_UpdateDrawable)
        draw->SetFlag(DrawableItem::Flags::UpdateDrawable, next_value);
    else if (flag == FlagName::Drawable_Restart)
        draw->SetFlag(DrawableItem::Flags::RestartDrawable, next_value);
    else if (flag == FlagName::Drawable_FlipHorizontally)
        draw->SetFlag(DrawableItem::Flags::FlipHorizontally, next_value);
    else if (flag == FlagName::Drawable_FlipVertically)
        draw->SetFlag(DrawableItem::Flags::FlipVertically, next_value);
    else if (flag == FlagName::Drawable_DoubleSided)
        draw->SetFlag(DrawableItem::Flags::DoubleSided, next_value);
    else if (flag == FlagName::Drawable_DepthTest)
        draw->SetFlag(DrawableItem::Flags::DepthTest, next_value);
    else if (flag == FlagName::Drawable_PPEnableBloom)
        draw->SetFlag(DrawableItem::Flags::PP_EnableBloom, next_value);
    else if (flag == FlagName::RigidBody_Bullet)
        body->SetFlag(RigidBodyItem::Flags::Bullet, next_value);
    else if (flag == FlagName::RigidBody_Sensor)
        body->SetFlag(RigidBodyItem::Flags::Sensor, next_value);
    else if (flag == FlagName::RigidBody_Enabled)
        body->SetFlag(RigidBodyItem::Flags::Enabled, next_value);
    else if (flag == FlagName::RigidBody_CanSleep)
        body->SetFlag(RigidBodyItem::Flags::CanSleep, next_value);
    else if (flag == FlagName::RigidBody_DiscardRotation)
        body->SetFlag(RigidBodyItem::Flags::DiscardRotation, next_value);
    else if (flag == FlagName::TextItem_VisibleInGame)
        text->SetFlag(TextItem::Flags::VisibleInGame, next_value);
    else if (flag == FlagName::TextItem_Blink)
        text->SetFlag(TextItem::Flags::BlinkText, next_value);
    else if (flag == FlagName::TextItem_Underline)
        text->SetFlag(TextItem::Flags::UnderlineText, next_value);
    else if (flag == FlagName::TextItem_PPEnableBloom)
        text->SetFlag(TextItem::Flags::PP_EnableBloom, next_value);
    else if (flag == FlagName::SpatialNode_Enabled)
        spatial->SetFlag(SpatialNode::Flags::Enabled, next_value);
    else if (flag == FlagName::Transformer_Enabled)
        transformer->SetFlag(NodeTransformer::Flags::Enabled, next_value);
    else BUG("Unhandled flag in set flag actuator.");

    // spams the log
    // DEBUG("Set EntityNode '%1' flag '%2' to '%3'.", node.GetName(), flag, next_value ? "On" : "Off");
}

bool SetFlagActuator::CanApply(EntityNode& node, bool verbose) const
{
    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();
    auto* spatial = node.GetSpatialNode();
    auto* transformer = node.GetTransformer();

    using FlagName = SetFlagActuatorClass::FlagName;
    const auto flag = mClass->GetFlagName();

    if (flag == FlagName::Drawable_VisibleInGame ||
        flag == FlagName::Drawable_UpdateMaterial ||
        flag == FlagName::Drawable_UpdateDrawable ||
        flag == FlagName::Drawable_Restart ||
        flag == FlagName::Drawable_FlipHorizontally ||
        flag == FlagName::Drawable_FlipVertically ||
        flag == FlagName::Drawable_DoubleSided ||
        flag == FlagName::Drawable_DepthTest ||
        flag == FlagName::Drawable_PPEnableBloom)
    {
        if (!draw && verbose)
        {
            WARN("Can't apply a drawable flag on a node without drawable item. [actuator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return draw != nullptr;
    }
    else if (flag == FlagName::RigidBody_Bullet ||
             flag == FlagName::RigidBody_Sensor ||
             flag == FlagName::RigidBody_Enabled ||
             flag == FlagName::RigidBody_CanSleep ||
             flag == FlagName::RigidBody_DiscardRotation)
    {
        if (!body && verbose)
        {
            WARN("Can't apply a rigid body flag on a node without a rigid body. [actuator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return body != nullptr;
    }
    else if (flag == FlagName::TextItem_VisibleInGame ||
             flag == FlagName::TextItem_Underline ||
             flag == FlagName::TextItem_Blink ||
             flag == FlagName::TextItem_PPEnableBloom)
    {
        if (!text && verbose)
        {
            WARN("Can't apply a text item flag on a node without a text item. [actuator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return text != nullptr;
    }
    else if (flag == FlagName::SpatialNode_Enabled)
    {
        if (!spatial && verbose)
        {
            WARN("Can't apply a spatial node flag on a node without a spatial item. [actuator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return spatial != nullptr;
    }
    else if (flag == FlagName::Transformer_Enabled)
    {
        if (!transformer && verbose)
        {
            WARN("Can't apply a node transformer flag on a node without a transformer. [actuator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
    }
    else BUG("Unhandled flag in set flag actuator.");
    return true;
}

void SetValueActuator::Start(EntityNode& node)
{
    if (!CanApply(node, true /*verbose */))
        return;

    const auto param = mClass->GetParamName();
    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();

    if (param == ParamName::Drawable_TimeScale)
        mStartValue = draw->GetTimeScale();
    else if (param == ParamName::RigidBody_AngularVelocity)
        mStartValue = body->GetAngularVelocity();
    else if (param == ParamName::RigidBody_LinearVelocityX)
        mStartValue = body->GetLinearVelocity().x;
    else if (param == ParamName::RigidBody_LinearVelocityY)
        mStartValue = body->GetLinearVelocity().y;
    else if (param == ParamName::RigidBody_LinearVelocity)
        mStartValue = body->GetLinearVelocity();
    else if (param == ParamName::TextItem_Text)
        mStartValue = text->GetText();
    else if (param == ParamName::TextItem_Color)
        mStartValue = text->GetTextColor();
    else BUG("Unhandled node item value.");
}

void SetValueActuator::Apply(EntityNode& node, float t)
{
    if (!CanApply(node, false /*verbose*/))
        return;

    const auto method = mClass->GetInterpolation();
    const auto param  = mClass->GetParamName();
    const auto end    = mClass->GetEndValue();
    const auto start  = mStartValue;

    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();

    if (param == ParamName::Drawable_TimeScale)
        draw->SetTimeScale(Interpolate<float>(t));
    else if (param == ParamName::RigidBody_AngularVelocity)
        body->AdjustAngularVelocity(Interpolate<float>(t));
    else if (param == ParamName::RigidBody_LinearVelocityX)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.x = Interpolate<float>(t);
        body->AdjustLinearVelocity(velocity);
    }
    else if (param == ParamName::RigidBody_LinearVelocityY)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.y = Interpolate<float>(t);
        body->AdjustLinearVelocity(velocity);
    }
    else if (param == ParamName::RigidBody_LinearVelocity)
        body->AdjustLinearVelocity(Interpolate<glm::vec2>(t));
    else if (param == ParamName::TextItem_Color)
        text->SetTextColor(Interpolate<Color4f>(t));
    else if (param == ParamName::TextItem_Text) {
        if (method == math::Interpolation::StepStart)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (method == math::Interpolation::Step && t >= 0.5f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (method == math::Interpolation::StepEnd && t >= 1.0f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (t >= 1.0f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
    } else BUG("Unhandled value actuator param type.");
}

void SetValueActuator::Finish(EntityNode& node)
{
    if (!CanApply(node, false))
        return;

    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();

    const auto param = mClass->GetParamName();
    const auto end   = mClass->GetEndValue();

    if (param == ParamName::Drawable_TimeScale)
        draw->SetTimeScale(std::get<float>(end));
    else if (param == ParamName::RigidBody_AngularVelocity)
        body->AdjustAngularVelocity(std::get<float>(end));
    else if (param == ParamName::RigidBody_LinearVelocityX)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.x = std::get<float>(end);
        body->AdjustLinearVelocity(velocity);
    }
    else if (param == ParamName::RigidBody_LinearVelocityY)
    {
        auto velocity = body->GetLinearVelocity();
        velocity.y = std::get<float>(end);
        body->AdjustLinearVelocity(velocity);
    }
    else if (param == ParamName::RigidBody_LinearVelocity)
        body->AdjustLinearVelocity(std::get<glm::vec2>(end));
    else if (param == ParamName::TextItem_Color)
        text->SetTextColor(std::get<Color4f>(end));
    else if (param == ParamName::TextItem_Text) {
        text->SetText(std::get<std::string>(mClass->GetEndValue()));
    } else BUG("Unhandled value actuator param type.");
}

bool SetValueActuator::CanApply(EntityNode& node, bool verbose) const
{
    const auto param = mClass->GetParamName();
    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();

    if ((param == ParamName::Drawable_TimeScale))
    {
        if (!draw && verbose)
        {
            WARN("Can't set a drawable value on a node without a drawable item. [actuator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return draw != nullptr;
    }
    else if ((param == ParamName::RigidBody_LinearVelocityY ||
              param == ParamName::RigidBody_LinearVelocityX ||
              param == ParamName::RigidBody_LinearVelocity  ||
              param == ParamName::RigidBody_AngularVelocity))
    {
        if (!body && verbose)
        {
            WARN("Can't set a rigid body value on a node without a rigid body. [actuator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return body != nullptr;
    }
    else if ((param == ParamName::TextItem_Text ||
              param == ParamName::TextItem_Color))
    {
        if (!text && verbose)
        {
            WARN("Can't set a text item value on a node without a text item. [actuator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        if (text)
        {
            const auto interpolation = mClass->GetInterpolation();
            const auto step_change = interpolation == math::Interpolation::Step ||
                                     interpolation == math::Interpolation::StepEnd ||
                                     interpolation == math::Interpolation::StepStart;
            if (!step_change && verbose)
            {
                WARN("Can't apply interpolation on text. [actuator='%1', node='%2', interpolation=%3]",
                     mClass->GetName(), node.GetName(), interpolation);
            }
        }

        return text != nullptr;
    } else BUG("Unhandled value actuator param type.");
    return false;
}

TransformActuator::TransformActuator(const std::shared_ptr<const TransformActuatorClass>& klass)
    : mClass(klass)
{
    if (!mClass->TestFlag(Flags::StaticInstance))
    {
        Instance instance;
        instance.end_position = mClass->GetEndPosition();
        instance.end_size     = mClass->GetEndSize();
        instance.end_scale    = mClass->GetEndScale();
        instance.end_rotation = mClass->GetEndRotation();
        mDynamicInstance      = instance;
    }
}

void TransformActuator::Start(EntityNode& node)
{
    mStartPosition = node.GetTranslation();
    mStartSize     = node.GetSize();
    mStartScale    = node.GetScale();
    mStartRotation = node.GetRotation();
}
void TransformActuator::Apply(EntityNode& node, float t)
{
    const auto& inst = GetInstance();

    // apply interpolated state on the node.
    const auto method = mClass->GetInterpolation();
    const auto& p = math::interpolate(mStartPosition, inst.end_position, t, method);
    const auto& s = math::interpolate(mStartSize,     inst.end_size,     t, method);
    const auto& r = math::interpolate(mStartRotation, inst.end_rotation, t, method);
    const auto& f = math::interpolate(mStartScale,    inst.end_scale,    t, method);
    node.SetTranslation(p);
    node.SetSize(s);
    node.SetRotation(r);
    node.SetScale(f);
}
void TransformActuator::Finish(EntityNode& node)
{
    const auto& inst = GetInstance();

    node.SetTranslation(inst.end_position);
    node.SetRotation(inst.end_rotation);
    node.SetSize(inst.end_size);
    node.SetScale(inst.end_scale);
}

void TransformActuator::SetEndPosition(const glm::vec2& pos)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform actuator position set on static actuator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_position = pos;
}
void TransformActuator::SetEndScale(const glm::vec2& scale)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform actuator scale set on static actuator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_position = scale;
}
void TransformActuator::SetEndSize(const glm::vec2& size)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform actuator size set on static actuator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_position = size;
}
void TransformActuator::SetEndRotation(float angle)
{
    if (mClass->TestFlag(Flags::StaticInstance))
    {
        WARN("Ignoring transform actuator rotation set on static actuator instance. [name=%1]", mClass->GetName());
        return;
    }
    mDynamicInstance.value().end_rotation = angle;
}

TransformActuator::Instance TransformActuator::GetInstance() const
{
    if (mDynamicInstance.has_value())
        return mDynamicInstance.value();
    Instance inst;
    inst.end_size     = mClass->GetEndSize();
    inst.end_scale    = mClass->GetEndScale();
    inst.end_rotation = mClass->GetEndRotation();
    inst.end_position = mClass->GetEndPosition();
    return inst;
}

void MaterialActuator::Start(EntityNode& node)
{
    const auto* draw = node.GetDrawable();
    if (draw == nullptr)
    {
        WARN("Entity node has no drawable item. [node='%1']", node.GetName());
        return;
    }
    const auto& params = mClass->GetMaterialParams();
    for (const auto& [key, val] : params)
    {
        if (const auto* p = draw->FindMaterialParam(key))
            mStartValues[key] = *p;
        else WARN("Entity node material parameter was not found. [node='%1', param='%2']", node.GetName(), key);
    }
}

void MaterialActuator::Apply(EntityNode& node, float t)
{
    if (auto* draw = node.GetDrawable())
    {
        for (const auto& [key, beg_value] : mStartValues)
        {
            const auto end_value = *mClass->FindMaterialParam(key);

            if (std::holds_alternative<int>(end_value))
                draw->SetMaterialParam(key, Interpolate<int>(beg_value, end_value, t));
            else if (std::holds_alternative<float>(end_value))
                draw->SetMaterialParam(key, Interpolate<float>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec2>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec2>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec3>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec3>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec4>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec4>(beg_value, end_value, t));
            else if (std::holds_alternative<Color4f>(end_value))
                draw->SetMaterialParam(key, Interpolate<Color4f>(beg_value, end_value, t));
            else if (std::holds_alternative<std::string>(end_value)) {
                // intentionally empty, can't interpolate
            } else  BUG("Unhandled material parameter type.");
        }
    }
}
void MaterialActuator::Finish(EntityNode& node)
{
    if (auto* draw = node.GetDrawable())
    {
        const auto& params = mClass->GetMaterialParams();
        for (const auto& [key, val] : params)
        {
            draw->SetMaterialParam(key, val);
        }
    }
}



} // namespace
