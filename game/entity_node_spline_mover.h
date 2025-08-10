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

#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "data/fwd.h"
#include "game/spline.h"
#include "game/types.h"

namespace game
{
    class SplineMoverClass
    {
    public:
        enum class PathCoordinateSpace {
            Absolute,  // positions are absolute in spline's local space
            Relative   // positions are deltas; integrate over time
        };
        enum class PathCurveType {
            Linear, CatmullRom
        };
        enum class RotationMode {
            // Use the current direction of the spline to rotate the object
            // to look along the path of travel
            ApplySplineRotation,
            // Keep the objects current rotation (independent from the spline)
            IgnoreSplineRotation
        };

        using CatmullRomFunction = Spline::CatmullRomFunction;

        ~SplineMoverClass();

        inline auto GetRotationMode() const noexcept
        { return mRotationMode; }

        inline auto GetAcceleration() const noexcept
        { return mAcceleration; }

        inline auto GetSpeed() const noexcept
        { return mSpeed; }

        inline auto GetPathCoordinateSpace() const noexcept
        { return mPathCoordinateSpace; }

        inline auto GetPathCurveType() const noexcept
        { return mPathCurveType; }

        // Get the current number of spline control points.
        inline auto GetPointCount() const noexcept
        { return mSpline.GetPointCount(); }

        // Get a spline control point at the given index.
        // The index must be valid.
        inline const auto& GetPoint(size_t index) const noexcept
        { return mSpline.GetPoint(index); }

        inline auto GetPathRelativePoint(size_t index) const noexcept
        {
            if (mPathCoordinateSpace == PathCoordinateSpace::Absolute)
                return GetPoint(index);

            const auto& offset = GetPoint(0).GetPosition().ToVec2();

            auto point = GetPoint(index);
            auto position = point.GetPosition().ToVec2();
            point.SetPosition(position - offset);
            return point;
        }
        inline void SetAcceleration(float acceleration) noexcept
        { mAcceleration = acceleration; }

        inline void SetSpeed(float speed) noexcept
        { mSpeed = speed; }

        // Redefine a spline control point at the given index.
        // The index must be valid.
        inline void SetPoint(const SplinePoint& point, size_t index)
        { mSpline.SetPoint(point, index); }

        // Append a new control point to the spline.
        inline void AppendPoint(const SplinePoint& point)
        { mSpline.AppendPoint(point); }

        inline void PrependPoint(const SplinePoint& point)
        { mSpline.PrependPoint(point); }

        inline void ErasePoint(size_t index)
        { mSpline.ErasePoint(index); }

        inline void SetPathCoordinateSpace(PathCoordinateSpace mode) noexcept
        { mPathCoordinateSpace = mode; }

        inline void SetPathCurveType(PathCurveType curve) noexcept
        { mPathCurveType = curve; }

        inline void SetRotationMode(RotationMode rotation) noexcept
        { mRotationMode = rotation; }

        std::shared_ptr<const CatmullRomFunction> GetCatmullRom() const;
        std::shared_ptr<const CatmullRomFunction> MakeCatmullRom() const;

        double GetPathLength() const;

        SplinePoint Evaluate(const CatmullRomFunction& catmull_rom, float t) const
        {
            auto sample = Spline::Evaluate(catmull_rom, t);
            if (mPathCoordinateSpace == PathCoordinateSpace::Absolute)
                return sample;

            const auto& offset = GetPoint(0).GetPosition().ToVec2();
            auto position = sample.GetPosition().ToVec2();
            sample.SetPosition(position - offset);
            return sample;
        }

        size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
        bool InitClassRuntime() const;
    private:
        PathCoordinateSpace mPathCoordinateSpace = PathCoordinateSpace::Absolute;
        PathCurveType mPathCurveType = PathCurveType::CatmullRom;
        RotationMode mRotationMode = RotationMode::ApplySplineRotation;
        Spline mSpline;
        float mSpeed = 0.0f;
        float mAcceleration = 0.0f;
    };

    class SplineMover
    {
    public:
        using RotationMode        = SplineMoverClass::RotationMode;
        using PathCoordinateSpace = SplineMoverClass::PathCoordinateSpace;
        using CatmullRomFunction  = SplineMoverClass::CatmullRomFunction;

        explicit SplineMover(std::shared_ptr<const SplineMoverClass> klass);

        template<typename TargetObject>
        void TransformObject(float dt, TargetObject& object)
        {
            if (!mCatmullRom || mPathComplete)
                return;

            // integrate, euler
            mSpeed += (mAcceleration * dt);
            mDisplacement += (dt * mSpeed);

            mDisplacement = math::clamp(0.0f, mPathLength, mDisplacement);

            // normalize displacement.
            const float t = mDisplacement / mPathLength;

            const auto coordinate_space = mClass->GetPathCoordinateSpace();
            const auto rotation_mode = mClass->GetRotationMode();
            const auto max = mCatmullRom->max_parameter();
            const auto& sample = mCatmullRom->evaluate(t * max);
            const auto& position = sample.GetPosition();

            if (coordinate_space == PathCoordinateSpace::Absolute)
            {
                object.SetTranslation(position);
            }
            else if (coordinate_space == PathCoordinateSpace::Relative)
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
                const auto& tangent = mCatmullRom->prime(t * max).GetPosition();
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

        inline void SetAcceleration(float acceleration) noexcept
        { mAcceleration = acceleration; }

        inline void SetSpeed(float speed) noexcept
        { mSpeed = speed; }

        inline auto GetAcceleration() const noexcept
        { return mAcceleration; }

        inline auto GetSpeed() const noexcept
        { return mSpeed; }

    private:
        std::shared_ptr<const SplineMoverClass> mClass;
        std::shared_ptr<const CatmullRomFunction> mCatmullRom;
        Float2 mStartPos;
        float mSpeed        = 0.0f;
        float mAcceleration = 0.0f;
        float mDisplacement = 0.0f;
        float mPathLength   = 0.0f;
        bool mPathComplete  = false;
    };

} // namespace