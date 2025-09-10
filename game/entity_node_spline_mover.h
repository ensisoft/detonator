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

        using CatmullRomFunction = Spline::CatmullRomFunction;
        using PolyLineFunction   = Spline::PolyLineFunction;

        SplineMoverClass();
        ~SplineMoverClass();

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

        auto GetPathCurveType() const noexcept
        { return mPathCurveType; }

        // Get the current number of spline control points.
        auto GetPointCount() const noexcept
        { return mSpline.GetPointCount(); }

        // Get a spline control point at the given index.
        // The index must be valid.
        const auto& GetPoint(size_t index) const noexcept
        { return mSpline.GetPoint(index); }

        auto GetPathRelativePoint(size_t index) const noexcept
        {
            if (mPathCoordinateSpace == PathCoordinateSpace::Absolute)
                return GetPoint(index);

            const auto& offset = GetPoint(0).GetPosition().ToVec2();

            auto point = GetPoint(index);
            auto position = point.GetPosition().ToVec2();
            point.SetPosition(position - offset);
            return point;
        }
        void SetAcceleration(float acceleration) noexcept
        { mAcceleration = acceleration; }

        void SetSpeed(float speed) noexcept
        { mSpeed = speed; }

        void SetPoints(std::vector<SplinePoint> points) noexcept
        { mSpline.SetPoints(std::move(points)); }

        // Redefine a spline control point at the given index.
        // The index must be valid.
        void SetPoint(const SplinePoint& point, size_t index)
        { mSpline.SetPoint(point, index); }

        // Append a new control point to the spline.
        void AppendPoint(const SplinePoint& point)
        { mSpline.AppendPoint(point); }

        void PrependPoint(const SplinePoint& point)
        { mSpline.PrependPoint(point); }

        void ErasePoint(size_t index)
        { mSpline.ErasePoint(index); }

        void SetPathCoordinateSpace(PathCoordinateSpace mode) noexcept
        { mPathCoordinateSpace = mode; }

        void SetPathCurveType(PathCurveType curve) noexcept
        { mPathCurveType = curve; }

        void SetRotationMode(RotationMode rotation) noexcept
        { mRotationMode = rotation; }

        void SetIterationMode(IterationMode mode) noexcept
        { mIterationMode = mode; }

        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        void Enable(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }

        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        std::shared_ptr<const CatmullRomFunction> GetCatmullRom() const;
        std::shared_ptr<const CatmullRomFunction> MakeCatmullRom() const;

        std::shared_ptr<const PolyLineFunction> GetPolyLine() const;
        std::shared_ptr<const PolyLineFunction> MakePolyLine() const;

        auto GetFlags() const noexcept
        { return mFlags; }

        double GetPathLength() const;

        // take the curve displacement (travel along the curve)
        // and map that to a smoothed interpolated t value for sampling
        // the spline.
        double Reparametrize(double displacement) const;

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

        SplinePoint Evaluate(const PolyLineFunction& polyline, float t) const
        {
            auto sample = Spline::Evaluate(polyline, t);
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
        bool InitCatmullRomCache() const;
    private:
        PathCoordinateSpace mPathCoordinateSpace = PathCoordinateSpace::Absolute;
        PathCurveType mPathCurveType = PathCurveType::CatmullRom;
        RotationMode mRotationMode = RotationMode::ApplySplineRotation;
        IterationMode mIterationMode = IterationMode::Once;
        base::bitflag<Flags> mFlags;
        Spline mSpline;
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
        using CatmullRomFunction  = SplineMoverClass::CatmullRomFunction;
        using PolyLineFunction    = SplineMoverClass::PolyLineFunction;

        explicit SplineMover(std::shared_ptr<const SplineMoverClass> klass);

        template<typename TargetObject>
        void TransformObject(float dt, TargetObject& object)
        {
            const auto has_curve_function = mCatmullRom || mPolyLine;
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
            Float2 position;
            Float2 tangent;
            if (mCatmullRom)
            {
                const auto t = (float)mClass->Reparametrize(mDisplacement);
                const auto max = mCatmullRom->max_parameter();
                const auto& sample = mCatmullRom->evaluate(t * max);
                position = sample.GetPosition();

                if (rotation_mode == RotationMode::ApplySplineRotation)
                    tangent = mCatmullRom->prime(t * max).GetPosition();
            }
            else if (mPolyLine)
            {
                const auto& sample = mPolyLine->Interpolate(mDisplacement);
                position = sample.GetPosition();

                if (rotation_mode == RotationMode::ApplySplineRotation)
                    tangent  = mPolyLine->FindTangent(mDisplacement).GetPosition();
            }

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

    private:
        std::shared_ptr<const SplineMoverClass> mClass;
        std::shared_ptr<const CatmullRomFunction> mCatmullRom;
        std::shared_ptr<const PolyLineFunction> mPolyLine;
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