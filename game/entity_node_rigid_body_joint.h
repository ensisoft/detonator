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

#pragma once

#include "config.h"

#include <string>
#include <variant>
#include <optional>
#include <memory>
#include <vector>
#include <optional>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "game/types.h"
#include "game/enum.h"
#include "data/fwd.h"

namespace game
{
    class EntityNode;

    // PhysicsJoint defines an optional physics engine constraint
    // between two bodies in the physics world. In other words
    // between two entity nodes that both have a rigid body
    // attachment. The two entity nodes are identified by their
    // node IDs. The distinction between "source" and "destination"
    // is arbitrary and not relevant.
    class RigidBodyJointClass
    {
    public:
        enum class JointType {
            Distance, Revolute, Weld, Prismatic, Motor, Pulley
        };
        enum class Flags {
            // Let the bodies connected to the joint collide
            CollideConnected,
            // Let the joint settings be changed at runtime
            // i.e., after the joint has been created.
            StaticSettings
        };

        struct RevoluteJointParams {
            bool enable_limit = false;
            bool enable_motor = false;
            FRadians lower_angle_limit;
            FRadians upper_angle_limit;
            float motor_speed = 0.0f; // radians per second
            float motor_torque = 0.0f; // newton-meters
        };

        struct DistanceJointParams {
            std::optional<float> min_distance;
            std::optional<float> max_distance;
            float stiffness = 0.0f; // Newtons per meter, N/m
            float damping   = 0.0f; // Newton seconds per meter
            DistanceJointParams()
            {
                min_distance.reset();
                max_distance.reset();
            }
        };

        struct WeldJointParams {
            float stiffness = 0.0f; // Newton-meters
            float damping   = 0.0f; // Newton-meters per second
        };

        struct MotorJointParams {
            float max_force = 1.0f; // Newtons
            float max_torque = 1.0f; // Newton-meters
        };

        struct PrismaticJointParams {
            bool enable_limit = false;
            bool enable_motor = false;
            float lower_limit = 0.0f; // distance, meters
            float upper_limit = 0.0f; // distance, meters
            float motor_torque = 0.0f; // newton-meters
            float motor_speed = 0.0f; // radians per second
            FRadians direction_angle;
        };

        struct PulleyJointParams {
            std::string anchor_nodes[2];
            float ratio = 1.0f;
        };

        using JointParams = std::variant<DistanceJointParams,
                RevoluteJointParams,
                WeldJointParams,
                MotorJointParams,
                PrismaticJointParams,
                PulleyJointParams>;

        // The type of the joint.
        JointType type = JointType::Distance;
        // The source node ID.
        std::string src_node_id;
        // The destination node ID.
        std::string dst_node_id;
        // ID of this joint.
        std::string id;
        // human-readable name of the joint.
        std::string name;
        // the anchor point within the body
        glm::vec2 src_node_anchor_point = {0.0f, 0.0f};
        // the anchor point within the body.
        glm::vec2 dst_node_anchor_point = {0.0f, 0.0f};
        // PhysicsJoint parameters (depending on the type)
        JointParams params;

        base::bitflag<Flags> flags;

        RigidBodyJointClass();

        std::size_t GetHash() const noexcept;

        inline std::string GetId() const noexcept
        { return id; }
        inline std::string GetName() const noexcept
        { return name; }
        inline std::string GetSrcNodeId() const noexcept
        { return src_node_id; }
        inline std::string GetDstNodeId() const noexcept
        { return dst_node_id; }

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        inline bool IsValid() const noexcept
        {
            if (src_node_id.empty() || dst_node_id.empty())
                return false;
            if (src_node_id == dst_node_id)
                return false;
            if (const auto* ptr = std::get_if<PulleyJointParams>(&params))
            {
                if (ptr->anchor_nodes[0].empty() || ptr->anchor_nodes[1].empty())
                    return false;
            }
            return true;
        }
        inline bool CollideConnected() const noexcept
        { return flags.test(Flags::CollideConnected); }
        inline bool CanSettingsChangeRuntime() const noexcept
        { return !flags.test(Flags::StaticSettings); }
        inline bool IsStatic() const noexcept
        { return flags.test(Flags::StaticSettings); }

        inline void SetFlag(Flags flag, bool on_off) noexcept
        { flags.set(flag, on_off); }
        inline JointType GetJointType() const noexcept
        { return type; }
    private:
    };

    class RigidBodyJoint
    {
    public:
        using JointType         = RigidBodyJointClass::JointType;
        using JointParams       = RigidBodyJointClass::JointParams;
        using JointSetting      = RigidBodyJointSetting;
        using JointSettingValue = std::variant<float, bool>;

        RigidBodyJoint(std::shared_ptr<const RigidBodyJointClass> joint,
                       std::string joint_id,
                       EntityNode* src_node,
                       EntityNode* dst_node) noexcept
          : mClass(std::move(joint))
          , mId(std::move(joint_id))
          , mSrcNode(src_node)
          , mDstNode(dst_node)
        {}

        inline JointType GetType() const noexcept
        { return mClass->GetJointType(); }

        inline const EntityNode* GetSrcNode() const noexcept
        { return mSrcNode; }
        inline const EntityNode* GetDstNode() const noexcept
        { return mDstNode; }

        inline EntityNode* GetSrcNode() noexcept
        { return mSrcNode; }

        inline EntityNode* GetDstNode() noexcept
        { return mDstNode; }

        std::string GetSrcId() const noexcept
        { return mClass->GetSrcNodeId(); }
        std::string GetDstId() const noexcept
        { return mClass->GetDstNodeId(); }
        const std::string& GetId() const noexcept
        { return mId; }
        const auto& GetSrcAnchor() const noexcept
        { return mClass->src_node_anchor_point; }
        const auto& GetDstAnchor() const noexcept
        { return mClass->dst_node_anchor_point; }
        const std::string& GetName() const noexcept
        { return mClass->name; }
        const std::string& GetClassId() const noexcept
        { return mClass->id; }
        const RigidBodyJointClass* operator->() const noexcept
        { return mClass.get(); }
        const RigidBodyJointClass& GetClass() const noexcept
        { return *mClass; }
        const JointParams& GetParams() const noexcept
        { return mClass->params; }

        inline bool HasPendingAdjustments() const noexcept
        { return !mAdjustments.empty(); }

        inline void ClearPendingAdjustments() noexcept
        { mAdjustments.clear(); }

        inline bool CanSettingsChangeRuntime() const noexcept
        { return mClass->CanSettingsChangeRuntime(); }

        bool ValidateJointSetting(JointSetting setting, JointSettingValue value) const noexcept;
        void AdjustJoint(JointSetting setting, JointSettingValue value);

        struct JointValueSetting {
            JointSetting setting;
            JointSettingValue  value;
        };
        inline size_t GetNumPendingAdjustments() const noexcept
        { return mAdjustments.size(); }

        const JointValueSetting* GetPendingAdjustment(size_t index) const noexcept
        { return &base::SafeIndex(mAdjustments, index); }

        void UpdateCurrentJointValue(JointSetting setting, JointSettingValue value);

        void InitializeCurrentValues() const;

        void RealizePendingAdjustments();

        std::optional<JointSettingValue> FindCurrentJointValue(JointSetting setting) const;

        template<typename T>
        T GetCurrentJointValue(JointSetting setting) const
        {
            const auto& value = FindCurrentJointValue(setting);
            if (!value.has_value())
                return T();

            ASSERT(value.has_value());
            ASSERT(std::holds_alternative<T>(value.value()));
            return std::get<T>(value.value());
        }

    private:
        std::shared_ptr<const RigidBodyJointClass> mClass;
        std::string mId;
        EntityNode* mSrcNode = nullptr;
        EntityNode* mDstNode = nullptr;
        std::vector<JointValueSetting> mAdjustments;
        // current joint values as provided by the physics
        // engine. only filled in when the joint has been
        // flagged with EnableRuntimeSettings (dynamic)
        mutable std::vector<JointValueSetting> mCurrentValues;
    };

} // namespace
