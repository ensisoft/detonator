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
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "game/animator_base.h"

#include "base/snafu.h"

namespace game
{

    // Modify the kinematic physics body properties, i.e.
    // the instantaneous linear and angular velocities.
    class KinematicAnimatorClass : public detail::AnimatorClassBase<KinematicAnimatorClass>
    {
    public:
        enum class Target {
            RigidBody,
            Transformer
        };
        // The interpolation method.
        using Interpolation = math::Interpolation;

        inline Target GetTarget() const noexcept
        { return mTarget; }
        inline Interpolation GetInterpolation() const noexcept
        { return mInterpolation; }
        inline void SetInterpolation(Interpolation method) noexcept
        { mInterpolation = method; }
        inline void SetTarget(Target target) noexcept
        { mTarget = target; }
        inline glm::vec2 GetEndLinearVelocity() const noexcept
        { return mEndLinearVelocity; }
        inline glm::vec2 GetEndLinearAcceleration() const noexcept
        { return mEndLinearAcceleration; }
        inline float GetEndAngularVelocity() const noexcept
        { return mEndAngularVelocity;}
        inline float GetEndAngularAcceleration() const noexcept
        { return mEndAngularAcceleration; }
        inline void SetEndLinearVelocity(glm::vec2 velocity) noexcept
        { mEndLinearVelocity = velocity; }
        inline void SetEndLinearAcceleration(glm::vec2 acceleration) noexcept
        { mEndLinearAcceleration = acceleration; }
        inline void SetEndAngularVelocity(float velocity) noexcept
        { mEndAngularVelocity = velocity; }
        inline void SetEndAngularAcceleration(float acceleration) noexcept
        { mEndAngularAcceleration = acceleration; }

        virtual Type GetType() const override
        { return Type::KinematicAnimator; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        Target mTarget = Target::RigidBody;
        // The ending linear velocity in meters per second.
        glm::vec2 mEndLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mEndLinearAcceleration = {0.0f, 0.0f};
        // The ending angular velocity in radians per second.
        float mEndAngularVelocity = 0.0f;
        float mEndAngularAcceleration = 0.0f;
    };

    // Apply a kinematic change to a rigid body's linear or angular velocity.
    class KinematicAnimator final : public Animator
    {
    public:
        explicit KinematicAnimator(std::shared_ptr<const KinematicAnimatorClass> klass) noexcept
            : mClass(std::move(klass))
        {}
        explicit KinematicAnimator(const KinematicAnimatorClass& klass)
            : mClass(std::make_shared<KinematicAnimatorClass>(klass))
        {}
        explicit KinematicAnimator(KinematicAnimatorClass&& klass)
            : mClass(std::make_shared<KinematicAnimatorClass>(std::move(klass)))
        {}
        virtual void Start(EntityNode& node) override;
        virtual void Apply(EntityNode& node, float t) override;
        virtual void Finish(EntityNode& node) override;

        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual std::string GetClassName() const override
        { return mClass->GetName(); }
        virtual std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<KinematicAnimator>(*this); }
        virtual Type GetType() const override
        { return Type::KinematicAnimator;}
    private:
        std::shared_ptr<const KinematicAnimatorClass> mClass;
        glm::vec2 mStartLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mStartLinearAcceleration = {0.0f, 0.0f};
        float mStartAngularVelocity     = 0.0f;
        float mStartAngularAcceleration = 0.0f;
    };

} // namespace