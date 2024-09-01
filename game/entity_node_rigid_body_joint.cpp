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

} // namespace