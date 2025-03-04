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

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <optional>
#include <vector>

#include "game/enum.h"
#include "base/utility.h"
#include "base/bitflag.h"
#include "data/fwd.h"

namespace game
{
    class RigidBodyJointClass;
    class RigidBodyJoint;

    class RigidBodyClass
    {
    public:
        using CollisionShape = game::CollisionShape;
        // Simulation parameter determines the type of physics
        // simulation (or the lack of simulation) applied to the
        // rigid body by the physics engine.
        enum class Simulation {
            // Static bodies remain static in the physics simulation.
            // i.e the body exists in the physics world but no forces
            // are applied onto it.
            Static,
            // Kinematic bodies are driven by simple kinematic motion
            // i.e. by velocity of the body. No forces are applied to it.
            Kinematic,
            // Dynamic body is completely driven by the physics simulation.
            // I.e.the body is moved by the physical forces being applied to it.
            Dynamic
        };

        enum class Flags {
            // Enable bullet physics, i.e. expect the object to be
            // fast moving object. this will increase the computational
            // effort required but will mitigate issues with fast traveling
            // objects.
            Bullet,
            // Sensor only flag enables object to only be used to
            // report collisions.
            Sensor,
            // Whether the rigid body simulation is enabled or not
            // for this body.
            Enabled,
            // Whether the rigid body can go to sleep (i.e. simulation stops)
            // when the body comes to a halt
            CanSleep,
            // Discard rotational component of physics simulation for this body.
            // Useful for things such as player character that should stay "upright
            DiscardRotation
        };
        RigidBodyClass() noexcept
        {
            mBitFlags.set(Flags::Enabled, true);
            mBitFlags.set(Flags::CanSleep, true);
        }

        std::size_t GetHash() const;

        Simulation GetSimulation() const noexcept
        { return mSimulation; }
        CollisionShape GetCollisionShape() const noexcept
        { return mCollisionShape; }
        float GetFriction() const noexcept
        { return mFriction; }
        float GetRestitution() const noexcept
        { return mRestitution; }
        float GetAngularDamping() const noexcept
        { return mAngularDamping; }
        float GetLinearDamping() const noexcept
        { return mLinearDamping; }
        float GetDensity() const noexcept
        { return mDensity; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        const std::string& GetPolygonShapeId() const noexcept
        { return mPolygonShapeId; }
        void ResetPolygonShapeId() noexcept
        { mPolygonShapeId.clear(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }

        void SetCollisionShape(CollisionShape shape) noexcept
        { mCollisionShape = shape; }
        void SetSimulation(Simulation simulation) noexcept
        { mSimulation = simulation; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value) noexcept
        { mFriction = value; }
        void SetRestitution(float value) noexcept
        { mRestitution = value; }
        void SetAngularDamping(float value) noexcept
        { mAngularDamping = value; }
        void SetLinearDamping(float value) noexcept
        { mLinearDamping = value; }
        void SetDensity(float value) noexcept
        { mDensity = value; }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        Simulation mSimulation = Simulation::Dynamic;
        CollisionShape mCollisionShape = CollisionShape::Box;
        base::bitflag<Flags> mBitFlags;
        std::string mPolygonShapeId;
        float mFriction = 0.3f;
        float mRestitution = 0.5f;
        float mAngularDamping = 0.5f;
        float mLinearDamping = 0.5f;
        float mDensity = 1.0f;
    };

    class RigidBody
    {
    public:
        using Simulation = RigidBodyClass::Simulation;
        using Flags      = RigidBodyClass::Flags;
        using CollisionShape = RigidBodyClass::CollisionShape;

        explicit RigidBody(std::shared_ptr<const RigidBodyClass> klass) noexcept
          : mClass(std::move(klass))
          , mInstanceFlags(mClass->GetFlags())
        {}

        void AddJointConnection(RigidBodyJoint* joint)
        { mJointConnections.push_back(joint); }

        size_t GetNumJoints() const noexcept
        { return mJointConnections.size(); }
        RigidBodyJoint* GetJoint(size_t index) noexcept
        { return base::SafeIndex(mJointConnections, index); }
        const RigidBodyJoint* GetJoint(size_t index) const noexcept
        { return base::SafeIndex(mJointConnections, index); }

        RigidBodyJoint* FindJointByName(const std::string& name) noexcept;
        RigidBodyJoint* FindJointByClassId(const std::string& id) noexcept;

        Simulation GetSimulation() const noexcept
        { return mClass->GetSimulation(); }
        CollisionShape GetCollisionShape() const noexcept
        { return mClass->GetCollisionShape(); }
        float GetFriction() const noexcept
        { return mClass->GetFriction(); }
        float GetRestitution() const noexcept
        { return mClass->GetRestitution(); }
        float GetAngularDamping() const noexcept
        { return mClass->GetAngularDamping(); }
        float GetLinearDamping() const noexcept
        { return mClass->GetLinearDamping(); }
        float GetDensity() const noexcept
        { return mClass->GetDensity(); }
        bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        const std::string& GetPolygonShapeId() const noexcept
        { return mClass->GetPolygonShapeId(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mInstanceFlags; }
        // Get the current instantaneous linear velocity of the
        // rigid body under the physics simulation.
        // The linear velocity expresses how fast the object is
        // moving and to which direction. It's expressed in meters per second
        // using the physics world coordinate space.
        const glm::vec2& GetLinearVelocity() const noexcept
        { return mLinearVelocity; }
        // Get the current instantaneous angular velocity of the
        // rigid body under the physics simulation.
        // The angular velocity expresses how fast the object is rotating
        // around its own center in radians per second.
        float GetAngularVelocity() const noexcept
        { return mAngularVelocity; }
        // Update the current linear velocity in m/s.
        // The updates are coming from the physics engine.
        void SetLinearVelocity(const glm::vec2& velocity) noexcept
        { mLinearVelocity = velocity; }
        // Update the current angular velocity in r/s
        // The updates are coming from the physics engine.
        void SetAngularVelocity(float velocity) noexcept
        { mAngularVelocity = velocity; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }

        void ResetTransform() noexcept;

        // Set an impulse to be applied to the center of the rigid
        // body on the next update of the physics engine. Warning,
        // this method will overwrite any previous impulse.
        // Use AddLinearImpulseToCenter in order to combine the impulses.
        void ApplyLinearImpulseToCenter(const glm::vec2& impulse) noexcept;

        void ApplyForceToCenter(const glm::vec2& force) noexcept;

        // Add (accumulate) a linear impulse to be applied to the center
        // of the rigid body on the next update of the physics engine.
        // The impulse is added to any previous impulse and the combination
        // is the final impulse applied on the body.
        void AddLinearImpulseToCenter(const glm::vec2& impulse) noexcept;
        // Set a new linear velocity adjustment to be applied
        // on the next update of the physics engine. The velocity
        // is in meters per second.
        void AdjustLinearVelocity(const glm::vec2& velocity) noexcept;

        // Set a new angular velocity adjustment to be applied
        // on the next update of the physics engine. The velocity
        // is in radians per second.
        void AdjustAngularVelocity(float radians) noexcept;

        bool HasAnyPhysicsAdjustment() const noexcept
        {
            return mCenterForce.has_value() ||
                   mCenterImpulse.has_value() ||
                   mLinearVelocityAdjustment.has_value() ||
                   mAngularVelocityAdjustment.has_value() ||
                   mResetTransform;
        }
        bool HasTransformReset() const noexcept
        { return mResetTransform; }

        bool HasCenterForce() const noexcept
        { return mCenterForce.has_value(); }

        bool HasCenterImpulse() const noexcept
        { return mCenterImpulse.has_value(); }
        bool HasLinearVelocityAdjustment() const noexcept
        { return mLinearVelocityAdjustment.has_value(); }
        bool HasAngularVelocityAdjustment() const noexcept
        { return mAngularVelocityAdjustment.has_value(); }
        float GetAngularVelocityAdjustment() const noexcept
        { return mAngularVelocityAdjustment.value_or(0.0f); }
        glm::vec2 GetLinearVelocityAdjustment() const noexcept
        { return mLinearVelocityAdjustment.value_or(glm::vec2(0.0f, 0.0f)); }
        glm::vec2 GetLinearImpulseToCenter() const noexcept
        { return mCenterImpulse.value_or(glm::vec2(0.0f, 0.0f)); }
        glm::vec2 GetForceToCenter() const noexcept
        { return mCenterForce.value_or(glm::vec2(0.0f, 0.0f)); }
        void ClearPhysicsAdjustments() const noexcept
        {
            mLinearVelocityAdjustment.reset();
            mAngularVelocityAdjustment.reset();
            mCenterImpulse.reset();
            mCenterForce.reset();
            mResetTransform = false;
        }
        void ClearImpulse() noexcept
        { mCenterImpulse.reset(); }
        void Enable(bool value) noexcept
        { SetFlag(Flags::Enabled, value); }
        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }
        bool IsSensor() const noexcept
        { return TestFlag(Flags::Sensor); }
        bool IsBullet() const noexcept
        { return TestFlag(Flags::Bullet); }
        bool CanSleep() const noexcept
        { return TestFlag(Flags::CanSleep); }
        bool DiscardRotation() const noexcept
        { return TestFlag(Flags::DiscardRotation); }

        const RigidBodyClass& GetClass() const noexcept
        { return *mClass.get(); }
        const RigidBodyClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const RigidBodyClass> mClass;
        // List of joints this rigid body is connected to.
        std::vector<RigidBodyJoint*> mJointConnections;
        // Current linear velocity in meters per second.
        // For dynamically driven bodies
        // the physics engine will update this value, whereas for
        // kinematic bodies the animation system can set this value
        // and the physics engine will read it.
        glm::vec2 mLinearVelocity = {0.0f, 0.0f};
        // Current angular velocity in radians per second.
        // For dynamically driven bodies the physics engine
        // will update this value, whereas for kinematic bodies
        // the animation system can provide a new value which will
        // then be set in the physics engine.
        float mAngularVelocity = 0.0f;
        // Flags specific to this instance.
        base::bitflag<Flags> mInstanceFlags;
        // Current adjustment to be made to the body's linear velocity.
        mutable std::optional<glm::vec2> mLinearVelocityAdjustment;
        // Current adjustment to be made to the body's angular velocity.
        mutable std::optional<float> mAngularVelocityAdjustment;
        // Current pending impulse in the center of the body.
        mutable std::optional<glm::vec2> mCenterImpulse;
        mutable std::optional<glm::vec2> mCenterForce;
        mutable bool mResetTransform = false;
    };


} // namespace