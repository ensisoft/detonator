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
#include <optional>

#include "base/bitflag.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "game/enum.h"

namespace game
{
    class FixtureClass
    {
    public:
        using CollisionShape = game::CollisionShape;

        enum class Flags {
            // Sensor only flag enables this fixture to only be
            // used as a sensor, i.e. it doesn't affect the body's
            // simulation under forces etc.
            Sensor
        };
        FixtureClass() noexcept
        {
            mBitFlags.set(Flags::Sensor, true);
        }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }
        void SetRigidBodyNodeId(const std::string& id)
        { mRigidBodyNodeId = id; }
        void SetCollisionShape(CollisionShape shape) noexcept
        { mCollisionShape = shape; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value) noexcept
        { mFriction = value; }
        void SetDensity(float value) noexcept
        { mDensity = value; }
        void SetRestitution(float value) noexcept
        { mRestitution = value; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        bool HasFriction() const noexcept
        { return mFriction.has_value(); }
        bool HasDensity() const noexcept
        { return mDensity.has_value(); }
        bool HasRestitution() const noexcept
        { return mRestitution.has_value(); }
        bool HasPolygonShapeId() const noexcept
        { return !mPolygonShapeId.empty(); }
        const float* GetFriction() const noexcept
        { return base::GetOpt(mFriction); }
        const float* GetDensity() const noexcept
        { return base::GetOpt(mDensity); }
        const float* GetRestitution() const noexcept
        { return base::GetOpt(mRestitution); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }
        CollisionShape GetCollisionShape() const noexcept
        { return mCollisionShape; }
        const std::string& GetPolygonShapeId() const noexcept
        { return mPolygonShapeId; }
        const std::string& GetRigidBodyNodeId() const noexcept
        {return mRigidBodyNodeId; }
        void ResetRigidBodyId() noexcept
        { mRigidBodyNodeId.clear(); }
        void ResetPolygonShapeId() noexcept
        { mPolygonShapeId.clear(); }
        void ResetFriction() noexcept
        { mFriction.reset(); }
        void ResetDensity() noexcept
        { mDensity.reset(); }
        void ResetRestitution() noexcept
        { mRestitution.reset(); }

        std::size_t GetHash() const;

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        CollisionShape  mCollisionShape = CollisionShape::Box;
        base::bitflag<Flags> mBitFlags;
        // The ID of the custom polygon shape id. Only used when the
        // collision shape is set to Polygon.
        std::string mPolygonShapeId;
        // The ID of the node that has the rigid body to which this
        // fixture is attached to. If
        std::string mRigidBodyNodeId;
        // Fixture specific friction parameter. if not set then
        // the friction from the associated rigid body is used.
        std::optional<float> mFriction;
        // Fixture specific density parameter. if not set then
        // the density from the associated rigid body is used.
        std::optional<float> mDensity;
        // Fixture specific restitution parameter. If not set then
        // the restitution from the associated rigid body is used.
        std::optional<float> mRestitution;
    };

    class Fixture
    {
    public:
        using Flags = FixtureClass::Flags;
        using CollisionShape = FixtureClass::CollisionShape;

        explicit Fixture(std::shared_ptr<const FixtureClass> klass) noexcept
          : mClass(std::move(klass))
        {}
        const std::string& GetPolygonShapeId() const noexcept
        { return mClass->GetPolygonShapeId(); }
        const std::string& GetRigidBodyNodeId() const noexcept
        { return mClass->GetRigidBodyNodeId(); }
        const float* GetFriction() const noexcept
        { return mClass->GetFriction(); }
        const float* GetDensity() const noexcept
        { return mClass->GetDensity(); }
        const float* GetRestitution() const noexcept
        { return mClass->GetRestitution(); }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mClass->GetFlags(); }
        CollisionShape GetCollisionShape() const noexcept
        { return mClass->GetCollisionShape(); }
        bool TestFlag(Flags flag) const noexcept
        { return mClass->TestFlag(flag); }
        const FixtureClass& GetClass() const noexcept
        { return *mClass; }
        const FixtureClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const FixtureClass> mClass;
    };

} // namespace game