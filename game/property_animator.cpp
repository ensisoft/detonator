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

#include "base/utility.h"
#include "base/hash.h"
#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/entity.h"
#include "game/entity_node.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_rigid_body_joint.h"
#include "game/entity_node_light.h"
#include "game/property_animator.h"

namespace game
{

 std::size_t BooleanPropertyAnimatorClass::GetHash() const
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
    hash = base::hash_combine(hash, mJointId);
    return hash;
}

void BooleanPropertyAnimatorClass::IntoJson(data::Writer& data) const
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
    data.Write("joint_id",  mJointId);
}

bool BooleanPropertyAnimatorClass::FromJson(const data::Reader& data)
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
    ok &= data.Read("joint_id",  &mJointId);
    return ok;
}

void BooleanPropertyAnimator::Start(EntityNode& node)
{
    if (!CanApply(node, true /*verbose*/))
        return;

    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();
    const auto* spatial = node.GetSpatialNode();
    const auto* light = node.GetBasicLight();
    const auto* mover = node.GetLinearMover();

    using FlagName = BooleanPropertyAnimatorClass::PropertyName;
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
        mStartState = body->TestFlag(RigidBody::Flags::Bullet);
    else if (flag == FlagName::RigidBody_Sensor)
        mStartState = body->TestFlag(RigidBody::Flags::Sensor);
    else if (flag == FlagName::RigidBody_Enabled)
        mStartState = body->TestFlag(RigidBody::Flags::Enabled);
    else if (flag == FlagName::RigidBody_CanSleep)
        mStartState = body->TestFlag(RigidBody::Flags::CanSleep);
    else if (flag == FlagName::RigidBody_DiscardRotation)
        mStartState = body->TestFlag(RigidBody::Flags::DiscardRotation);
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
    else if (flag == FlagName::LinearMover_Enabled)
        mStartState = mover->TestFlag(LinearMover::Flags::Enabled);
    else if (flag == FlagName::RigidBodyJoint_EnableLimits)
    {
        const auto* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        mStartState = joint->GetCurrentJointValue<bool>(RigidBodyJointSetting::EnableLimit);
    }
    else if (flag == FlagName::RigidBodyJoint_EnableMotor)
    {
        const auto* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        mStartState = joint->GetCurrentJointValue<bool>(RigidBodyJointSetting::EnableMotor);
    }
    else if (flag == FlagName::BasicLight_Enabled)
        mStartState = light->IsEnabled();
    else BUG("Unhandled property.");

    if (mTime == 0.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }

}
void BooleanPropertyAnimator::Apply(EntityNode& node, float t)
{
    if (t >= mTime && mTime != -1.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }
}
void BooleanPropertyAnimator::Finish(EntityNode& node)
{
    if (mTime == 1.0f)
    {
        SetFlag(node);
        mTime = -1.0f;
    }
}

void BooleanPropertyAnimator::SetFlag(EntityNode& node) const
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
    auto* light = node.GetBasicLight();
    auto* spatial = node.GetSpatialNode();
    auto* mover = node.GetLinearMover();

    using FlagName = BooleanPropertyAnimatorClass::PropertyName;
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
        body->SetFlag(RigidBody::Flags::Bullet, next_value);
    else if (flag == FlagName::RigidBody_Sensor)
        body->SetFlag(RigidBody::Flags::Sensor, next_value);
    else if (flag == FlagName::RigidBody_Enabled)
        body->SetFlag(RigidBody::Flags::Enabled, next_value);
    else if (flag == FlagName::RigidBody_CanSleep)
        body->SetFlag(RigidBody::Flags::CanSleep, next_value);
    else if (flag == FlagName::RigidBody_DiscardRotation)
        body->SetFlag(RigidBody::Flags::DiscardRotation, next_value);
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
    else if (flag == FlagName::LinearMover_Enabled)
        mover->SetFlag(LinearMover::Flags::Enabled, next_value);
    else if (flag == FlagName::RigidBodyJoint_EnableMotor)
    {
        auto* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        joint->AdjustJoint(RigidBodyJointSetting::EnableMotor, next_value);
    }
    else if (flag == FlagName::RigidBodyJoint_EnableLimits)
    {
        auto* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        joint->AdjustJoint(RigidBodyJointSetting::EnableLimit, next_value);
    }
    else if (flag == FlagName::BasicLight_Enabled)
        light->SetFlag(BasicLight::Flags::Enabled, next_value);
    else BUG("Unhandled property.");

    // spams the log
    // DEBUG("Set EntityNode '%1' flag '%2' to '%3'.", node.GetName(), flag, next_value ? "On" : "Off");
}

bool BooleanPropertyAnimator::CanApply(EntityNode& node, bool verbose) const
{
    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();
    auto* light = node.GetBasicLight();
    auto* spatial = node.GetSpatialNode();
    auto* mover = node.GetLinearMover();

    using FlagName = BooleanPropertyAnimatorClass::PropertyName;
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
            WARN("Property animator can't apply a drawable flag on a node without drawable item. [animator='%1', node='%2', flag=%3]",
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
            WARN("Property animator can't apply a rigid body flag on a node without a rigid body. [animator='%1', node='%2', flag=%3]",
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
            WARN("Property animator can't apply a text item flag on a node without a text item. [animator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return text != nullptr;
    }
    else if (flag == FlagName::SpatialNode_Enabled)
    {
        if (!spatial && verbose)
        {
            WARN("Property animator can't apply a spatial node flag on a node without a spatial item. [animator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return spatial != nullptr;
    }
    else if (flag == FlagName::LinearMover_Enabled)
    {
        if (!mover && verbose)
        {
            WARN("Property animator can't apply a node linear mover flag on a node without a linear mover. [animator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
    }
    else if (flag == FlagName::RigidBodyJoint_EnableMotor ||
             flag == FlagName ::RigidBodyJoint_EnableLimits)
    {
        const auto* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        if (!joint)
        {
            if (verbose)
            {
                WARN("Property animator can't apply joint setting since the joint is not found. [animator='%1', node='%2', joint='%3']",
                     mClass->GetName(), node.GetName(), mClass->GetJointId());
            }
            return false;
        }
        else if (!joint->CanSettingsChangeRuntime())
        {
            if (verbose)
            {
                WARN("Property animator can't change joint settings since the joint settings are static. [animator='%1, node='%2', joint='%3'",
                     mClass->GetName(), node.GetName(), joint->GetName());
            }
            return  false;
        }
    }
    else if (flag == FlagName::BasicLight_Enabled)
    {
        if (!light && verbose)
        {
            WARN("Property animator can't apply a basic light flag on a node without a basic light. [animator='%1', node='%2', flag=%3]",
                 mClass->GetName(), node.GetName(), flag);
        }
        return light != nullptr;
    }
    else BUG("Unhandled property.");
    return true;
}

size_t PropertyAnimatorClass::GetHash() const
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
    hash = base::hash_combine(hash, mJointId);
    return hash;
}

void PropertyAnimatorClass::IntoJson(data::Writer& data) const
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
    data.Write("joint_id",  mJointId);
}

bool PropertyAnimatorClass::FromJson(const data::Reader& data)
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
    ok &= data.Read("joint_id",  &mJointId);
    return ok;
}

void PropertyAnimator::Start(EntityNode& node)
{
    if (!CanApply(node, true /*verbose */))
        return;

    const auto param = mClass->GetPropertyName();
    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();
    const auto* light = node.GetBasicLight();
    const auto* mover = node.GetLinearMover();

    const RigidBodyJoint* joint = nullptr;
    if (param == PropertyName::RigidBodyJoint_MotorTorque ||
        param == PropertyName::RigidBodyJoint_MotorSpeed ||
        param == PropertyName::RigidBodyJoint_MotorForce ||
        param == PropertyName::RigidBodyJoint_Stiffness ||
        param == PropertyName::RigidBodyJoint_Damping)
    {
        joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
    }

    if (param == PropertyName::Drawable_TimeScale)
        mStartValue = draw->GetTimeScale();
    else if (param == PropertyName::Drawable_RotationX)
        mStartValue = draw->GetRotator().GetEulerAngleX().ToRadians();
    else if (param == PropertyName::Drawable_RotationY)
        mStartValue = draw->GetRotator().GetEulerAngleY().ToRadians();
    else if (param == PropertyName::Drawable_RotationZ)
        mStartValue = draw->GetRotator().GetEulerAngleZ().ToRadians();
    else if (param == PropertyName::Drawable_TranslationX)
        mStartValue = draw->GetOffset().x;
    else if (param == PropertyName::Drawable_TranslationY)
        mStartValue = draw->GetOffset().y;
    else if (param == PropertyName::Drawable_TranslationZ)
        mStartValue = draw->GetOffset().z;
    else if (param == PropertyName::Drawable_SizeZ)
        mStartValue = draw->GetDepth();
    else if (param == PropertyName::RigidBody_AngularVelocity)
        mStartValue = body->GetAngularVelocity();
    else if (param == PropertyName::RigidBody_LinearVelocityX)
        mStartValue = body->GetLinearVelocity().x;
    else if (param == PropertyName::RigidBody_LinearVelocityY)
        mStartValue = body->GetLinearVelocity().y;
    else if (param == PropertyName::RigidBody_LinearVelocity)
        mStartValue = body->GetLinearVelocity();
    else if (param == PropertyName::TextItem_Text)
        mStartValue = text->GetText();
    else if (param == PropertyName::TextItem_Color)
        mStartValue = text->GetTextColor();
    else if (param == PropertyName::LinearMover_LinearVelocity)
        mStartValue = mover->GetLinearVelocity();
    else if (param == PropertyName::LinearMover_LinearVelocityX)
        mStartValue = mover->GetLinearVelocity().x;
    else if (param == PropertyName::LinearMover_LinearVelocityY)
        mStartValue = mover->GetLinearVelocity().y;
    else if (param == PropertyName::LinearMover_LinearAcceleration)
        mStartValue = mover->GetLinearAcceleration();
    else if (param == PropertyName::LinearMover_LinearAccelerationX)
        mStartValue = mover->GetLinearAcceleration().x;
    else if (param == PropertyName::LinearMover_LinearAccelerationY)
        mStartValue = mover->GetLinearAcceleration().y;
    else if (param == PropertyName::LinearMover_AngularVelocity)
        mStartValue = mover->GetAngularVelocity();
    else if (param == PropertyName::LinearMover_AngularAcceleration)
        mStartValue = mover->GetAngularAcceleration();
    else if (param == PropertyName::RigidBodyJoint_MotorTorque)
        mStartValue = joint->GetCurrentJointValue<float>(RigidBodyJointSetting::MotorTorque);
    else if (param == PropertyName::RigidBodyJoint_MotorSpeed)
        mStartValue = joint->GetCurrentJointValue<float>(RigidBodyJointSetting::MotorSpeed);
    else if (param == PropertyName::RigidBodyJoint_MotorForce)
        mStartValue = joint->GetCurrentJointValue<float>(RigidBodyJointSetting::MotorForce);
    else if (param == PropertyName::RigidBodyJoint_Stiffness)
        mStartValue = joint->GetCurrentJointValue<float>(RigidBodyJointSetting::Stiffness);
    else if (param == PropertyName::RigidBodyJoint_Damping)
        mStartValue = joint->GetCurrentJointValue<float>(RigidBodyJointSetting::Damping);
    else if (param == PropertyName::BasicLight_Direction)
        mStartValue = light->GetDirection();
    else if (param == PropertyName::BasicLight_Translation)
        mStartValue = light->GetTranslation();
    else if (param == PropertyName::BasicLight_AmbientColor)
        mStartValue = light->GetAmbientColor();
    else if (param == PropertyName::BasicLight_DiffuseColor)
        mStartValue = light->GetDiffuseColor();
    else if (param == PropertyName::BasicLight_SpecularColor)
        mStartValue = light->GetSpecularColor();
    else if (param == PropertyName::BasicLight_SpotHalfAngle)
        mStartValue = light->GetSpotHalfAngle().ToDegrees();
    else if (param == PropertyName::BasicLight_ConstantAttenuation)
        mStartValue = light->GetConstantAttenuation();
    else if (param == PropertyName::BasicLight_LinearAttenuation)
        mStartValue = light->GetLinearAttenuation();
    else if (param == PropertyName::BasicLight_QuadraticAttenuation)
        mStartValue = light->GetQuadraticAttenuation();
    else BUG("Unhandled property.");
}

void PropertyAnimator::Apply(EntityNode& node, float t)
{
    SetValue(node, t, true);
}

void PropertyAnimator::SetValue(EntityNode& node, float t, bool interpolate) const
{
    if (!CanApply(node, false /*verbose*/))
        return;

    const auto method = mClass->GetInterpolation();
    const auto param  = mClass->GetPropertyName();
    const auto end    = mClass->GetEndValue();
    const auto start  = mStartValue;

    auto* draw = node.GetDrawable();
    auto* body = node.GetRigidBody();
    auto* text = node.GetTextItem();
    auto* light = node.GetBasicLight();
    auto* mover = node.GetLinearMover();

    if (param == PropertyName::Drawable_TimeScale)
    {
        draw->SetTimeScale(Interpolate<float>(t, interpolate));
    }
    else if (param == PropertyName::Drawable_RotationX)
    {
        const auto& rotator = draw->GetRotator();
        const auto& angles  = rotator.GetEulerAngles();
        auto x = Interpolate<float>(t, interpolate);
        auto y = std::get<1>(angles);
        auto z = std::get<2>(angles);
        draw->SetRotator(Rotator(FRadians(x), y, z));
    }
    else if (param == PropertyName::Drawable_RotationY)
    {
        const auto& rotator = draw->GetRotator();
        const auto& angles  = rotator.GetEulerAngles();
        auto x = std::get<0>(angles);
        auto y = Interpolate<float>(t, interpolate);
        auto z = std::get<2>(angles);
        draw->SetRotator(Rotator(x, FRadians(y), z));
    }
    else if (param == PropertyName::Drawable_RotationZ)
    {
        const auto& rotator = draw->GetRotator();
        const auto& angles  = rotator.GetEulerAngles();
        auto x = std::get<0>(angles);
        auto y = std::get<1>(angles);
        auto z = Interpolate<float>(t, interpolate);
        draw->SetRotator(Rotator(x, y, FRadians(z)));
    }
    else if (param == PropertyName::Drawable_TranslationX)
    {
        auto vec = draw->GetOffset();
        vec.x = Interpolate<float>(t, interpolate);
        draw->SetOffset(vec);
    }
    else if (param == PropertyName::Drawable_TranslationY)
    {
        auto vec = draw->GetOffset();
        vec.y = Interpolate<float>(t, interpolate);
        draw->SetOffset(vec);
    }
    else if (param == PropertyName::Drawable_TranslationZ)
    {
        auto vec = draw->GetOffset();
        vec.z = Interpolate<float>(t, interpolate);
        draw->SetOffset(vec);
    }
    else if (param == PropertyName::Drawable_SizeZ)
    {
        auto size = draw->GetDepth();
        size = Interpolate<float>(t, interpolate);
        draw->SetDepth(size);
    }
    else if (param == PropertyName::RigidBody_AngularVelocity)
    {
        if (!body->HasAngularVelocityAdjustment())
        {
            body->AdjustAngularVelocity(Interpolate<float>(t, interpolate));
        }
    }
    else if (param == PropertyName::RigidBody_LinearVelocityX)
    {
        if (!body->HasLinearVelocityAdjustment())
        {
            auto velocity = body->GetLinearVelocity();
            velocity.x = Interpolate<float>(t, interpolate);
            body->AdjustLinearVelocity(velocity);
        }
    }
    else if (param == PropertyName::RigidBody_LinearVelocityY)
    {
        if (!body->HasLinearVelocityAdjustment())
        {
            auto velocity = body->GetLinearVelocity();
            velocity.y = Interpolate<float>(t, interpolate);
            body->AdjustLinearVelocity(velocity);
        }
    }
    else if (param == PropertyName::RigidBody_LinearVelocity)
    {
        if (!body->HasLinearVelocityAdjustment())
        {
            body->AdjustLinearVelocity(Interpolate<glm::vec2>(t, interpolate));
        }
    }
    else if (param == PropertyName::LinearMover_LinearVelocity)
    {
        mover->SetLinearVelocity(Interpolate<glm::vec2>(t, interpolate));
    }
    else if (param == PropertyName::LinearMover_LinearVelocityX)
    {
        auto velocity = mover->GetLinearVelocity();
        velocity.x = Interpolate<float>(t, interpolate);
        mover->SetLinearVelocity(velocity);
    }
    else if (param == PropertyName::LinearMover_LinearVelocityY)
    {
        auto velocity = mover->GetLinearVelocity();
        velocity.y = Interpolate<float>(t, interpolate);
        mover->SetLinearVelocity(velocity);
    }
    else if (param == PropertyName::LinearMover_LinearAcceleration)
    {
        mover->SetLinearAcceleration(Interpolate<glm::vec2>(t, interpolate));
    }
    else if (param == PropertyName::LinearMover_LinearAccelerationX)
    {
        auto accel = mover->GetLinearAcceleration();
        accel.x = Interpolate<float>(t, interpolate);
        mover->SetLinearAcceleration(accel);
    }
    else if (param == PropertyName::LinearMover_LinearAccelerationY)
    {
        auto accel = mover->GetLinearAcceleration();
        accel.y = Interpolate<float>(t, interpolate);
        mover->SetLinearAcceleration(accel);
    }
    else if (param == PropertyName::LinearMover_AngularVelocity)
    {
        mover->SetAngularVelocity(Interpolate<float>(t, interpolate));
    }
    else if (param == PropertyName::LinearMover_AngularAcceleration)
    {
        mover->SetAngularAcceleration(Interpolate<float>(t, interpolate));
    }
    else if (param == PropertyName::TextItem_Color)
    {
        text->SetTextColor(Interpolate<Color4f>(t, interpolate));
    }
    else if (param == PropertyName::TextItem_Text)
    {
        if (method == math::Interpolation::StepStart)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (method == math::Interpolation::Step && t >= 0.5f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (method == math::Interpolation::StepEnd && t >= 1.0f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));
        else if (t >= 1.0f)
            text->SetText(std::get<std::string>(mClass->GetEndValue()));

    }
    else if (param == PropertyName::RigidBodyJoint_MotorTorque ||
             param == PropertyName::RigidBodyJoint_MotorSpeed ||
             param == PropertyName::RigidBodyJoint_MotorForce ||
             param == PropertyName::RigidBodyJoint_Stiffness ||
             param == PropertyName::RigidBodyJoint_Damping)
    {
        RigidBodyJoint* joint = node.GetEntity()->FindJointByClassId(mClass->GetJointId());
        ASSERT(joint);

        if (param == PropertyName::RigidBodyJoint_MotorTorque)
            joint->AdjustJoint(RigidBodyJointSetting::MotorTorque, Interpolate<float>(t, interpolate));
        else if (param == PropertyName::RigidBodyJoint_MotorSpeed)
            joint->AdjustJoint(RigidBodyJointSetting::MotorSpeed, Interpolate<float>(t, interpolate));
        else if (param == PropertyName::RigidBodyJoint_MotorForce)
            joint->AdjustJoint(RigidBodyJointSetting::MotorForce, Interpolate<float>(t, interpolate));
        else if (param == PropertyName::RigidBodyJoint_Stiffness)
            joint->AdjustJoint(RigidBodyJointSetting::Stiffness, Interpolate<float>(t, interpolate));
        else if (param == PropertyName::RigidBodyJoint_Damping)
            joint->AdjustJoint(RigidBodyJointSetting::Damping, Interpolate<float>(t, interpolate));
        else BUG("Missing joint setting");
    }
    else if (param == PropertyName::BasicLight_Direction)
        light->SetDirection(Interpolate<glm::vec3>(t, interpolate));
    else if (param == PropertyName::BasicLight_Translation)
        light->SetTranslation(Interpolate<glm::vec3>(t, interpolate));
    else if (param == PropertyName::BasicLight_AmbientColor)
        light->SetAmbientColor(Interpolate<Color4f>(t, interpolate));
    else if (param == PropertyName::BasicLight_DiffuseColor)
        light->SetDiffuseColor(Interpolate<Color4f>(t, interpolate));
    else if (param == PropertyName::BasicLight_SpecularColor)
        light->SetSpecularColor(Interpolate<Color4f>(t, interpolate));
    else if (param == PropertyName::BasicLight_SpotHalfAngle)
        light->SetSpotHalfAngle(Interpolate<float>(t, interpolate));
    else if (param == PropertyName::BasicLight_ConstantAttenuation)
        light->SetConstantAttenuation(Interpolate<float>(t, interpolate));
    else if (param == PropertyName::BasicLight_LinearAttenuation)
        light->SetLinearAttenuation(Interpolate<float>(t, interpolate));
    else if (param == PropertyName::BasicLight_QuadraticAttenuation)
        light->SetQuadraticAttenuation(Interpolate<float>(t, interpolate));
    else BUG("Unhandled property.");
}

void PropertyAnimator::Finish(EntityNode& node)
{
    SetValue(node, 1.0f, false);
}

bool PropertyAnimator::CanApply(EntityNode& node, bool verbose) const
{
    const auto param = mClass->GetPropertyName();
    const auto* draw = node.GetDrawable();
    const auto* body = node.GetRigidBody();
    const auto* text = node.GetTextItem();
    const auto* light = node.GetBasicLight();
    const auto* mover = node.GetLinearMover();

    if (param == PropertyName::Drawable_TimeScale ||
        param == PropertyName::Drawable_RotationX ||
        param == PropertyName::Drawable_RotationY ||
        param == PropertyName::Drawable_RotationZ ||
        param == PropertyName::Drawable_TranslationX ||
        param == PropertyName::Drawable_TranslationY ||
        param == PropertyName::Drawable_TranslationZ ||
        param == PropertyName::Drawable_SizeZ)
    {
        if (!draw && verbose)
        {
            WARN("Property animator can't set a drawable value on a node without a drawable item. [animator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return draw != nullptr;
    }
    else if ((param == PropertyName::RigidBody_LinearVelocityY ||
              param == PropertyName::RigidBody_LinearVelocityX ||
              param == PropertyName::RigidBody_LinearVelocity  ||
              param == PropertyName::RigidBody_AngularVelocity))
    {
        if (!body && verbose)
        {
            WARN("Property animator can't set a rigid body value on a node without a rigid body. [animator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return body != nullptr;
    }
    else if ((param == PropertyName::TextItem_Text ||
              param == PropertyName::TextItem_Color))
    {
        if (!text && verbose)
        {
            WARN("Property animator can't set a text item value on a node without a text item. [animator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        if (text)
        {
            const auto interpolation = mClass->GetInterpolation();
            const auto step_change = interpolation == math::Interpolation::Step ||
                                     interpolation == math::Interpolation::StepEnd ||
                                     interpolation == math::Interpolation::StepStart;
            if (!step_change && verbose && param == PropertyName ::TextItem_Text)
            {
                WARN("Property animator can't apply interpolation on text. [animator='%1', node='%2', interpolation=%3]",
                     mClass->GetName(), node.GetName(), interpolation);
            }
        }
        return text != nullptr;
    }
    else if (param == PropertyName::LinearMover_LinearVelocity ||
             param == PropertyName::LinearMover_LinearVelocityX ||
             param == PropertyName::LinearMover_LinearVelocityY ||
             param == PropertyName::LinearMover_LinearAcceleration  ||
             param == PropertyName::LinearMover_LinearAccelerationX ||
             param == PropertyName::LinearMover_LinearAccelerationY ||
             param == PropertyName::LinearMover_AngularVelocity ||
             param == PropertyName::LinearMover_AngularAcceleration)
    {
        if (!mover && verbose)
        {
            WARN("Property animator can't set a linear mover value on a node without a linear mover. [animator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return mover != nullptr;
    }
    else if (param == PropertyName::RigidBodyJoint_MotorTorque ||
             param == PropertyName::RigidBodyJoint_MotorSpeed ||
             param == PropertyName::RigidBodyJoint_MotorForce ||
             param == PropertyName::RigidBodyJoint_Stiffness ||
             param == PropertyName::RigidBodyJoint_Damping)
    {
        const auto* entity = node.GetEntity();
        const auto* joint = entity->FindJointByClassId(mClass->GetJointId());
        if (!joint)
        {
            if (verbose)
            {
                WARN("Property animator can't apply joint setting since the joint is not found. [animator='%1', node='%2', joint='%3'",
                     mClass->GetName(), node.GetName(), mClass->GetJointId());
            }
            return false;
        }
        else if (!joint->CanSettingsChangeRuntime())
        {
            if (verbose)
            {
                WARN("Property animator can't change joint settings since the joint settings are static. [animator='%1, node='%2', joint='%3'",
                     mClass->GetName(), node.GetName(), joint->GetName());
            }
            return false;
        }
        return true;
    }
    else if (param == PropertyName::BasicLight_Direction ||
             param == PropertyName::BasicLight_Translation ||
             param == PropertyName::BasicLight_AmbientColor ||
             param == PropertyName::BasicLight_DiffuseColor ||
             param == PropertyName::BasicLight_SpecularColor ||
             param == PropertyName::BasicLight_SpotHalfAngle ||
             param == PropertyName::BasicLight_ConstantAttenuation ||
             param == PropertyName::BasicLight_LinearAttenuation ||
             param == PropertyName::BasicLight_QuadraticAttenuation)
    {
        if (!light && verbose)
        {
            WARN("Property animator can't set a light value on a a node without a light attachment. [animator='%1', node='%2', value=%3]",
                 mClass->GetName(), node.GetName(), param);
        }
        return light != nullptr;
    }
    else BUG("Unhandled property.");

    return false;
}

} // namespace
