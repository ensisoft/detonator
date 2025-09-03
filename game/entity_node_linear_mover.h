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
#  include <glm/gtx/vector_query.hpp>
#include "warnpop.h"

#include <memory>
#include <cmath>

#include "base/bitflag.h"
#include "data/fwd.h"

namespace game
{
    class LinearMoverClass
    {
    public:
        enum class Integrator {
            Euler
        };
        enum class Flags {
            Enabled, RotateToDirection
        };
        LinearMoverClass()
        {
            mFlags.set(Flags::Enabled, true);
        }
        void SetIntegrator(Integrator integrator) noexcept
        { mIntegrator = integrator; }
        Integrator GetIntegrator() const noexcept
        { return mIntegrator; }
        void SetLinearVelocity(glm::vec2 velocity) noexcept
        { mLinearVelocity = velocity; }
        void SetLinearAcceleration(glm::vec2 acceleration) noexcept
        { mLinearAcceleration = acceleration; }
        glm::vec2 GetLinearVelocity() const noexcept
        { return mLinearVelocity; }
        glm::vec2 GetLinearAcceleration() const noexcept
        { return mLinearAcceleration; }
        float GetAngularVelocity() const noexcept
        { return mAngularVelocity; }
        float GetAngularAcceleration() const noexcept
        { return mAngularAcceleration; }
        void SetAngularVelocity(float velocity) noexcept
        { mAngularVelocity = velocity; }
        void SetAngularAcceleration(float acceleration) noexcept
        { mAngularAcceleration = acceleration;  }

        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }
        bool RotateToDirection() const noexcept
        { return TestFlag(Flags::RotateToDirection); }

        base::bitflag<Flags> GetFlags() const noexcept
        { return mFlags; }

        size_t GetHash() const noexcept;

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

    private:
        base::bitflag<Flags> mFlags;
        Integrator mIntegrator  = Integrator::Euler;
        glm::vec2 mLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mLinearAcceleration = {0.0f, 0.0f};
        float mAngularVelocity = 0.0f;
        float mAngularAcceleration = 0.0f;
    };

    class LinearMover
    {
    public:
        using Integrator = LinearMoverClass::Integrator;
        using Flags = LinearMoverClass::Flags;

        explicit LinearMover(std::shared_ptr<const LinearMoverClass> klass)
            : mClass(std::move(klass))
            , mFlags(mClass->GetFlags())
            , mLinearVelocity(mClass->GetLinearVelocity())
            , mLinearAcceleration(mClass->GetLinearAcceleration())
            , mAngularVelocity(mClass->GetAngularVelocity())
            , mAngularAcceleration(mClass->GetAngularAcceleration())
        {}
        void SetLinearVelocity(glm::vec2 velocity) noexcept
        { mLinearVelocity = velocity; }
        void SetLinearAcceleration(glm::vec2 acceleration) noexcept
        { mLinearAcceleration = acceleration; }
        glm::vec2 GetLinearVelocity() const noexcept
        { return mLinearVelocity; }
        glm::vec2 GetLinearAcceleration() const noexcept
        { return mLinearAcceleration; }
        float GetAngularVelocity() const noexcept
        { return mAngularVelocity; }
        float GetAngularAcceleration() const noexcept
        { return mAngularAcceleration; }
        void SetAngularVelocity(float velocity) noexcept
        { mAngularVelocity = velocity; }
        void SetAngularAcceleration(float acceleration) noexcept
        { mAngularAcceleration = acceleration;  }
        bool IsEnabled() const noexcept
        { return mFlags.test(Flags::Enabled); }
        Integrator GetIntegrator() const noexcept
        { return mClass->GetIntegrator(); }
        void SetFlag(Flags flag , bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        void Enable(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }
        bool RotateToDirection() const noexcept
        { return TestFlag(Flags::RotateToDirection); }
        void RotateToDirection(bool on_off) noexcept
        { SetFlag(Flags::RotateToDirection, on_off); }

        void SetDirection(glm::vec2 direction)
        {
            if (glm::isNull(direction, 0.0001f))
                return;
            direction = glm::normalize(direction);
            const auto speed = glm::length(mLinearVelocity);
            mLinearVelocity = direction * speed;
        }

        template<typename Target>
        void TransformObject(float dt, Target& target) noexcept
        {
            if (!IsEnabled())
                return;
            if (GetIntegrator() == Integrator::Euler)
            {
                mLinearVelocity += (mLinearAcceleration * dt);
                target.Translate(mLinearVelocity * dt);

                if (RotateToDirection())
                {
                    const auto angle = std::atan2(mLinearVelocity.y, mLinearVelocity.x);
                    target.SetRotation(angle);
                }
                else
                {
                    mAngularVelocity += (mAngularAcceleration * dt);
                    target.Rotate(mAngularVelocity * dt);
                }
            }
        }

        const LinearMoverClass& GetClass() const noexcept
        { return *mClass; }
        const LinearMoverClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const LinearMoverClass> mClass;
        base::bitflag<Flags> mFlags;
        glm::vec2 mLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mLinearAcceleration = {0.0f, 0.0f};
        float mAngularVelocity        = 0.0f;
        float mAngularAcceleration    = 0.0f;
    };

} // namespace
