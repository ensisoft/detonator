// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <third_party/boost/catmull_rom.hpp>
#include "warnpop.h"

#include <memory>
#include <cstdlib>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/spline.h"
#include "game/curve.h"
#include "game/types.h"

namespace game
{
    class SplineMoverClass
    {
    public:
        enum class PathCoordinateSpace {
            // Spline positions are absolute node positions relative to the parent.
            // In other words when the spine is traversed the node's position is set directly
            // from the spline. This is the position relative to the node parent.
            Parent,
            // Spline positions are relative to the first point and applied as offsets from the node's
            // start position. In other words when the spline is traversed the positions are
            // integrated (summed) over time.
            Local
        };
        enum class RotationMode {
            // Use the current direction of the spline to rotate the object
            // to look along the path of travel
            ApplySplineRotation,
            // Keep the objects current rotation (independent from the spline)
            IgnoreSplineRotation
        };

        enum class IterationMode {
            // Run the spline once from beginning to the end
            Once,
            // Run the spline from beginning to the end and then
            // back indefinitely
            PingPong,
            // Run the spline from beginning to the end and then
            // return to the beginning for the next iteration
            Loop
        };

        enum class Flags {
            Enabled
        };

        using CatmullRomFunction = SplineCatmullRomFunction;
        using PolyLineFunction   = SplinePolyLineFunction;

        SplineMoverClass();

        auto GetIterationMode() const noexcept
        { return mIterationMode; }

        auto GetRotationMode() const noexcept
        { return mRotationMode; }

        auto GetAcceleration() const noexcept
        { return mAcceleration; }

        auto GetSpeed() const noexcept
        { return mSpeed; }

        auto GetPathCoordinateSpace() const noexcept
        { return mPathCoordinateSpace; }

        auto GetCurveId() const noexcept
        { return mCurveId; }

        void SetCurveId(std::string id) noexcept
        { mCurveId = std::move(id); }

        void SetAcceleration(const float acceleration) noexcept
        { mAcceleration = acceleration; }

        void SetSpeed(const float speed) noexcept
        { mSpeed = speed; }

        void SetPathCoordinateSpace(const PathCoordinateSpace mode) noexcept
        { mPathCoordinateSpace = mode; }

        void SetRotationMode(const RotationMode rotation) noexcept
        { mRotationMode = rotation; }

        void SetIterationMode(const IterationMode mode) noexcept
        { mIterationMode = mode; }

        void SetFlag(const Flags flag, const bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        void Enable(const bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }

        bool TestFlag(const Flags flag) const noexcept
        { return mFlags.test(flag); }

        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        auto GetFlags() const noexcept
        { return mFlags; }

        SplinePoint GetPoint(const CurveClass& curve, const size_t index) const noexcept
        {
            return curve.GetPoint(index);
        }

        auto GetPathRelativePoint(const CurveClass& curve, size_t index) const noexcept
        {
            if (mPathCoordinateSpace == PathCoordinateSpace::Parent)
                return GetPoint(curve, index);

            const auto& offset = GetPoint(curve, 0).GetPosition().ToVec2();

            auto point = GetPoint(curve, index);
            auto position = point.GetPosition().ToVec2();
            point.SetPosition(position - offset);
            return point;
        }

        SplinePoint Evaluate(const CurveClass& curve, const CatmullRomFunction& catmull_rom, float t) const
        {
            auto sample = EvaluateSpline(catmull_rom, t);
            if (mPathCoordinateSpace == PathCoordinateSpace::Parent)
                return sample;

            const auto& offset = GetPoint(curve, 0).GetPosition().ToVec2();
            auto position = sample.GetPosition().ToVec2();
            sample.SetPosition(position - offset);
            return sample;
        }

        SplinePoint Evaluate(const CurveClass& curve, const PolyLineFunction& polyline, float t) const
        {
            auto sample = EvaluateSpline(polyline, t);
            if (mPathCoordinateSpace == PathCoordinateSpace::Parent)
                return sample;

            const auto& offset = GetPoint(curve, 0).GetPosition().ToVec2();
            auto position = sample.GetPosition().ToVec2();
            sample.SetPosition(position - offset);
            return sample;
        }

        size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
        bool InitClassRuntime() const;

    private:
        PathCoordinateSpace mPathCoordinateSpace = PathCoordinateSpace::Parent;
        RotationMode mRotationMode = RotationMode::ApplySplineRotation;
        IterationMode mIterationMode = IterationMode::Once;
        base::bitflag<Flags> mFlags;
        std::string mCurveId;

        float mSpeed = 0.0f;
        float mAcceleration = 0.0f;
    };

    class SplineMover
    {
    public:
        using Flags               = SplineMoverClass::Flags;
        using IterationMode       = SplineMoverClass::IterationMode;
        using RotationMode        = SplineMoverClass::RotationMode;
        using PathCoordinateSpace = SplineMoverClass::PathCoordinateSpace;

        explicit SplineMover(std::shared_ptr<const SplineMoverClass> klass);

        void Initialize(const Curve* curve);

        template<typename TargetObject>
        void TransformObject(float dt, TargetObject& object)
        {
            const auto has_curve_function = mCurve;
            if (!has_curve_function || mPathComplete || !IsEnabled())
                return;

            const auto iteration_mode = mClass->GetIterationMode();

            // integrate, euler
            mSpeed += (mAcceleration * dt);
            mDisplacement += (dt * mSpeed * mDirection);

            if (iteration_mode == IterationMode::Once)
            {
                mDisplacement = math::clamp(0.0f, mPathLength, mDisplacement);
            }
            else if (iteration_mode == IterationMode::PingPong)
            {
                if (mDisplacement > mPathLength)
                {
                    const auto diff = mDisplacement - mPathLength;
                    mDisplacement = mPathLength - diff;
                    mDirection = -1.0f;
                }
                else if (mDisplacement < 0.0f)
                {
                    mDisplacement = std::abs(mDisplacement);
                    mDirection = 1.0f;
                }
            }
            else if (iteration_mode == IterationMode::Loop)
            {
                if (mDisplacement > mPathLength)
                {
                    const auto diff = mDisplacement - mPathLength;
                    mDisplacement = diff;
                    mDirection = 1.0;
                }
            } else BUG("Bug on iteration mode");

            const auto coordinate_space = mClass->GetPathCoordinateSpace();
            const auto rotation_mode = mClass->GetRotationMode();

            Float2 tangent;
            const auto t = mCurve->Reparametrize(mDisplacement);
            const auto position = mCurve->GetCurvePosition(t);
            if (rotation_mode == RotationMode::ApplySplineRotation)
                tangent = mCurve->GetCurveTangent(t);

            if (coordinate_space == PathCoordinateSpace::Parent)
            {
                object.SetTranslation(position);
            }
            else if (coordinate_space == PathCoordinateSpace::Local)
            {
                // cumulative error can happen here..
                const auto delta = position.ToVec2() - mStartPos.ToVec2();
                const auto x = object.GetXVector();
                const auto y = object.GetYVector();
                object.Translate(delta.x * x + delta.y * y);
                mStartPos = position;

            } else BUG("Bug on spline coordinate space.");

            if (rotation_mode == RotationMode::ApplySplineRotation)
            {
                // get the tangent value and from the tangent compute
                // the rotation value.
                const auto angle = std::atan2(tangent.y, tangent.x);
                object.SetRotation(angle);
            }
            else if (rotation_mode == RotationMode::IgnoreSplineRotation)
            {
                // nothing to do here, we just simply leave objects rotation
                // as-is. this code branch is for posterity only
            }
            else BUG("Bug on spline rotation mode.");
        }

        void SetAcceleration(float acceleration) noexcept
        { mAcceleration = acceleration; }

        void SetSpeed(float speed) noexcept
        { mSpeed = speed; }

        auto GetAcceleration() const noexcept
        { return mAcceleration; }

        auto GetSpeed() const noexcept
        { return mSpeed; }

        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        void Enable(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }

        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        auto GetCurveId() const noexcept
        { return mClass->GetCurveId(); }

    private:
        std::shared_ptr<const SplineMoverClass> mClass;
        const Curve* mCurve = nullptr;
        base::bitflag<Flags> mFlags;
        Float2 mStartPos;
        float mDirection    = 1.0f;
        float mSpeed        = 0.0f;
        float mAcceleration = 0.0f;
        float mDisplacement = 0.0f;
        float mPathLength   = 0.0f;
        bool mPathComplete  = false;
    };

} // namespace
