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
#include "base/hash.h"
#include "base/logging.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_rigid_body_joint.h"

namespace game
{

size_t RigidBodyJointClass::GetHash() const noexcept
{
    auto* joint = this;

    size_t jh = 0;
    jh = base::hash_combine(jh, joint->id);
    jh = base::hash_combine(jh, joint->type);
    jh = base::hash_combine(jh, joint->flags);
    jh = base::hash_combine(jh, joint->src_node_id);
    jh = base::hash_combine(jh, joint->dst_node_id);
    jh = base::hash_combine(jh, joint->name);
    jh = base::hash_combine(jh, joint->dst_node_anchor_point);
    jh = base::hash_combine(jh, joint->src_node_anchor_point);
    if (const auto* ptr = std::get_if<DistanceJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->min_distance.has_value());
        jh = base::hash_combine(jh, ptr->max_distance.has_value());
        jh = base::hash_combine(jh, ptr->max_distance.value_or(0.0f));
        jh = base::hash_combine(jh, ptr->min_distance.value_or(0.0f));
        jh = base::hash_combine(jh, ptr->stiffness);
        jh = base::hash_combine(jh, ptr->damping);
    }
    else if (const auto* ptr = std::get_if<RevoluteJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->enable_limit);
        jh = base::hash_combine(jh, ptr->enable_motor);
        jh = base::hash_combine(jh, ptr->lower_angle_limit);
        jh = base::hash_combine(jh, ptr->upper_angle_limit);
        jh = base::hash_combine(jh, ptr->motor_speed);
        jh = base::hash_combine(jh, ptr->motor_torque);
    }
    else if (const auto* ptr = std::get_if<WeldJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->stiffness);
        jh = base::hash_combine(jh, ptr->damping);
    }
    else if (const auto* ptr = std::get_if<PrismaticJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->enable_limit);
        jh = base::hash_combine(jh, ptr->enable_motor);
        jh = base::hash_combine(jh, ptr->lower_limit);
        jh = base::hash_combine(jh, ptr->upper_limit);
        jh = base::hash_combine(jh, ptr->motor_torque);
        jh = base::hash_combine(jh, ptr->motor_speed);
        jh = base::hash_combine(jh, ptr->direction_angle);
    }
    else if (const auto* ptr = std::get_if<MotorJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->max_force);
        jh = base::hash_combine(jh, ptr->max_torque);
    }
    else if (const auto* ptr = std::get_if<PulleyJointParams>(&joint->params))
    {
        jh = base::hash_combine(jh, ptr->anchor_nodes[0]);
        jh = base::hash_combine(jh, ptr->anchor_nodes[1]);
        jh = base::hash_combine(jh, ptr->ratio);
    }
    return jh;
}

void RigidBodyJointClass::IntoJson(data::Writer& data) const
{
    auto* chunk = &data;
    auto* joint = this;

    chunk->Write("id", joint->id);
    chunk->Write("type", joint->type);
    chunk->Write("flags", joint->flags);
    chunk->Write("src_node_id", joint->src_node_id);
    chunk->Write("dst_node_id", joint->dst_node_id);
    chunk->Write("name", joint->name);
    chunk->Write("src_node_anchor_point", joint->src_node_anchor_point);
    chunk->Write("dst_node_anchor_point", joint->dst_node_anchor_point);
    if (const auto* ptr = std::get_if<DistanceJointParams>(&joint->params))
    {
        if (ptr->min_distance.has_value())
            chunk->Write("min_dist", ptr->min_distance.value());
        if (ptr->max_distance.has_value())
            chunk->Write("max_dist", ptr->max_distance.value());
        chunk->Write("damping", ptr->damping);
        chunk->Write("stiffness", ptr->stiffness);
    }
    else if (const auto* ptr = std::get_if<RevoluteJointParams>(&joint->params))
    {
        chunk->Write("enable_limit", ptr->enable_limit);
        chunk->Write("enable_motor", ptr->enable_motor);
        chunk->Write("lower_angle_limit", ptr->lower_angle_limit);
        chunk->Write("upper_angle_limit", ptr->upper_angle_limit);
        chunk->Write("motor_speed", ptr->motor_speed);
        chunk->Write("motor_torque", ptr->motor_torque);
    }
    else if (const auto* ptr = std::get_if<WeldJointParams>(&joint->params))
    {
        chunk->Write("stiffness", ptr->stiffness);
        chunk->Write("damping", ptr->damping);
    }
    else if (const auto* ptr = std::get_if<PrismaticJointParams>(&joint->params))
    {
        chunk->Write("enable_limit", ptr->enable_limit);
        chunk->Write("enable_motor", ptr->enable_motor);
        chunk->Write("lower_limit", ptr->lower_limit);
        chunk->Write("upper_limit", ptr->upper_limit);
        chunk->Write("motor_torque", ptr->motor_torque);
        chunk->Write("motor_speed", ptr->motor_speed);
        chunk->Write("direction_angle", ptr->direction_angle);
    }
    else if (const auto* ptr = std::get_if<MotorJointParams>(&joint->params))
    {
        chunk->Write("max_force", ptr->max_force);
        chunk->Write("max_torque", ptr->max_torque);
    }
    else if (const auto* ptr = std::get_if<PulleyJointParams>(&joint->params))
    {
        chunk->Write("anchor_node_0", ptr->anchor_nodes[0]);
        chunk->Write("anchor_node_1", ptr->anchor_nodes[1]);
        chunk->Write("ratio", ptr->ratio);
    }
}

bool RigidBodyJointClass::FromJson(const data::Reader& data)
{
    auto* chunk = &data;
    auto& jref = *this;

    bool ok = true;
    ok &= chunk->Read("id",                    &jref.id);
    ok &= chunk->Read("type",                  &jref.type);
    ok &= chunk->Read("flags",                 &jref.flags);
    ok &= chunk->Read("src_node_id",           &jref.src_node_id);
    ok &= chunk->Read("dst_node_id",           &jref.dst_node_id);
    ok &= chunk->Read("name",                  &jref.name);
    ok &= chunk->Read("src_node_anchor_point", &jref.src_node_anchor_point);
    ok &= chunk->Read("dst_node_anchor_point", &jref.dst_node_anchor_point);
    if (jref.type == JointType::Distance)
    {
        DistanceJointParams params;
        ok &= chunk->Read("damping", &params.damping);
        ok &= chunk->Read("stiffness", &params.stiffness);
        if (chunk->HasValue("min_dist"))
        {
            float value = 0.0f;
            ok &= chunk->Read("min_dist", &value);
            params.min_distance = value;
        }
        if (chunk->HasValue("max_dist"))
        {
            float value = 0.0f;
            ok &= chunk->Read("max_dist", &value);
            params.max_distance = value;
        }
        jref.params = params;
    }
    else if (jref.type == JointType::Revolute)
    {
        RevoluteJointParams params;
        ok &= chunk->Read("enable_limit", &params.enable_limit);
        ok &= chunk->Read("enable_motor", &params.enable_motor);
        ok &= chunk->Read("lower_angle_limit", &params.lower_angle_limit);
        ok &= chunk->Read("upper_angle_limit", &params.upper_angle_limit);
        ok &= chunk->Read("motor_speed", &params.motor_speed);
        ok &= chunk->Read("motor_torque", &params.motor_torque);
        jref.params = params;
    }
    else if (jref.type == JointType::Weld)
    {
        WeldJointParams params;
        ok &= chunk->Read("stiffness", &params.stiffness);
        ok &= chunk->Read("damping", &params.damping);
        jref.params = params;
    }
    else if (jref.type == JointType::Prismatic)
    {
        PrismaticJointParams params;
        ok &= chunk->Read("enable_limit", &params.enable_limit);
        ok &= chunk->Read("enable_motor", &params.enable_motor);
        ok &= chunk->Read("lower_limit", &params.lower_limit);
        ok &= chunk->Read("upper_limit", &params.upper_limit);
        ok &= chunk->Read("motor_torque", &params.motor_torque);
        ok &= chunk->Read("motor_speed", &params.motor_speed);
        ok &= chunk->Read("direction_angle", &params.direction_angle);
        jref.params = params;
    }
    else if (jref.type == JointType::Motor)
    {
        MotorJointParams params;
        ok &= chunk->Read("max_force", &params.max_force);
        ok &= chunk->Read("max_torque", &params.max_torque);
        jref.params = params;
    }
    else if (jref.type == JointType::Pulley)
    {
        PulleyJointParams params;
        ok &= chunk->Read("anchor_node_0", &params.anchor_nodes[0]);
        ok &= chunk->Read("anchor_node_1", &params.anchor_nodes[1]);
        ok &= chunk->Read("ratio", &params.ratio);
        jref.params = params;
    }
    return ok;
}

bool RigidBodyJoint::ValidateJointSetting(game::RigidBodyJoint::JointSetting setting, JointSettingValue value) const noexcept
{
    const auto type = mClass->GetJointType();

    if (setting == JointSetting::EnableMotor || setting == JointSetting::EnableLimit)
    {
        if (!std::holds_alternative<bool>(value))
            return false;

        if (type == JointType::Prismatic || type == JointType::Revolute)
            return true;

    }
    else if (setting == JointSetting::MotorTorque)
    {
        if (!std::holds_alternative<float>(value))
            return false;

        if (type == JointType::Revolute || type == JointType::Motor || type == JointType::Prismatic)
            return true;
    }
    else if (setting == JointSetting::MotorSpeed)
    {
        if (!std::holds_alternative<float>(value))
            return false;

        if (type == JointType::Revolute || type == JointType::Prismatic)
            return true;
    }
    else if (setting == JointSetting::MotorForce)
    {
        if (!std::holds_alternative<float>(value))
            return false;

        if (type == JointType::Motor)
            return true;
    }
    else if (setting == JointSetting::Stiffness || setting == JointSetting::Damping)
    {
        if (!std::holds_alternative<float>(value))
            return false;

        if (type == JointType::Weld || type == JointType::Distance)
            return true;
    } else BUG("Missing joint setting.");
    return false;
}

void RigidBodyJoint::AdjustJoint(JointSetting setting, JointSettingValue value)
{
    // schedule a pending setting change on the joint.
    // the physics subsystem will then set the setting on the joint
    // in the next update
    for (auto& pending : mAdjustments)
    {
        if (pending.setting == setting)
        {
            WARN("Overwriting previous joint setting with a new value. [setting='%1']", setting);
            pending.value = value;
            return;
        }
    }
    JointValueSetting pending;
    pending.setting = setting;
    pending.value   = value;
    mAdjustments.push_back(pending);
}

void RigidBodyJoint::UpdateCurrentJointValue(game::RigidBodyJoint::JointSetting setting,
                                             game::RigidBodyJoint::JointSettingValue value)
{
    for (auto& current : mCurrentValues)
    {
        if (current.setting == setting)
        {
            current.value = value;
            return;
        }
    }
}

void RigidBodyJoint::InitializeCurrentValues() const
{
    // we only provide dynamic joint settings if the flag
    // is set to indicate this feature

    if (!CanSettingsChangeRuntime())
        return;

    auto SetValue = [this](JointSetting setting, JointSettingValue  value) {
        JointValueSetting jvs;
        jvs.setting = setting;
        jvs.value   = value;
        mCurrentValues.push_back(jvs);
    };

    const auto& params= GetParams();

    if (const auto* ptr = std::get_if<RigidBodyJointClass::RevoluteJointParams>(&params))
    {
        SetValue(JointSetting::EnableLimit, ptr->enable_limit);
        SetValue(JointSetting::EnableMotor, ptr->enable_motor);
        SetValue(JointSetting::MotorSpeed, ptr->motor_speed);
        SetValue(JointSetting::MotorTorque, ptr->motor_torque);
    }
    else if (const auto* ptr = std::get_if<RigidBodyJointClass::DistanceJointParams>(&params))
    {
        SetValue(JointSetting::Damping, ptr->damping);
        SetValue(JointSetting::Stiffness, ptr->stiffness);
    }
    else if (const auto* ptr = std::get_if<RigidBodyJointClass::WeldJointParams>(&params))
    {
        SetValue(JointSetting::Damping, ptr->damping);
        SetValue(JointSetting::Stiffness, ptr->stiffness);
    }
    else if (const auto* ptr = std::get_if<RigidBodyJointClass::MotorJointParams>(&params))
    {
        SetValue(JointSetting::MotorForce, ptr->max_force);
        SetValue(JointSetting::MotorTorque, ptr->max_torque);
    }
    else if (const auto* ptr = std::get_if<RigidBodyJointClass::PrismaticJointParams>(&params))
    {
        SetValue(JointSetting::EnableLimit, ptr->enable_limit);
        SetValue(JointSetting::EnableMotor, ptr->enable_motor);
        SetValue(JointSetting::MotorSpeed, ptr->motor_speed);
        SetValue(JointSetting::MotorTorque, ptr->motor_torque);
    }
}

void RigidBodyJoint::RealizePendingAdjustments()
{
    if (!CanSettingsChangeRuntime())
        return;

    // Right now the only API to change joint settings is through
    // the joint adjustment API. that means that any adjustment
    // will then become the current value and it's only ever
    // reflected towards the physics engine joint.

    auto SetCurrent = [this](JointSetting setting, JointSettingValue value) {
        for (auto& s : mCurrentValues) {
            if (s.setting == setting) {
                s.value = value;
                return true;
            }
        }
        return false;
    };
    for (auto& adjustment : mAdjustments)
    {
        ASSERT(SetCurrent(adjustment.setting, adjustment.value));
    }
}

std::optional<RigidBodyJoint::JointValueSetting> RigidBodyJoint::FindCurrentJointValue(
        game::RigidBodyJoint::JointSetting setting) const
{
    for (const auto& current : mCurrentValues)
    {
        if (current.setting == setting)
            return current;
    }
    return std::nullopt;
}

} // namespace
