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
#  include <third_party/boost/catmull_rom.hpp>
#include "warnpop.h"

#include <vector>
#include <memory>
#include <array>

#include "base/utility.h"
#include "base/polyline.h"
#include "data/fwd.h"
#include "game/types.h"
#include "game/curve_types.h"

namespace game
{
    class SplinePoint
    {
    public:
        using value_type = float;

        SplinePoint() = default;
        SplinePoint(const Float2& position, const Float2& look_at) noexcept
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = look_at.x;
            mData[3] = look_at.y;
        }
        SplinePoint(float v0, float v1, float v2, float v3) noexcept
        {
            mData[0] = v0;
            mData[1] = v1;
            mData[2] = v2;
            mData[3] = v3;
        }
        SplinePoint(const CurvePoint& point) noexcept
        {
            const auto& position = point.GetPosition();
            const auto& look_at = point.GetLookAt();
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = look_at.x;
            mData[3] = look_at.y;
        }

        CurvePoint ToCurvePoint() const
        {
            CurvePoint point;
            point.SetPosition(mData[0], mData[1], 0.0f);
            point.SetLookAt(mData[2], mData[3], 0.0f);
            return point;
        }

        float operator[](size_t index) const noexcept
        {
            return mData[index];
        }

        float& operator[](size_t index) noexcept
        {
            return mData[index];
        }
        Float2 GetPosition() const noexcept
        {
            return { mData[0], mData[1] };
        }
        Float2 GetLookAt() const noexcept
        {
            return { mData[2], mData[3] };
        }
        void SetPosition(float x, float y) noexcept
        {
            mData[0] = x;
            mData[1] = y;
        }
        void SetPosition(const Float2& position) noexcept
        {
            mData[0] = position.x;
            mData[1] = position.y;
        }
        void SetLookAt(const Float2& look_at) noexcept
        {
            mData[2] = look_at.x;
            mData[3] = look_at.y;
        }
        void SetLookAt(float x, float y) noexcept
        {
            mData[2] = x;
            mData[3] = y;
        }
    private:
        friend class Spline;
        std::array<float, 4> mData = {0};
    };

    float GetPointDistance(const SplinePoint& p0, const SplinePoint& p1) noexcept;

    SplinePoint InterpolatePoint(const SplinePoint& p0, const SplinePoint& p1, float t) noexcept;
    SplinePoint ComputePointTangent(const SplinePoint& p0, const SplinePoint& p1, float dist) noexcept;

    using SplineCatmullRomFunction = boost::math::catmull_rom<SplinePoint, 4>;
    using SplinePolyLineFunction = base::PolyLine<SplinePoint>;

    SplinePoint EvaluateSpline(const SplineCatmullRomFunction& spline, float t);
    SplinePoint EvaluateSpline(const SplinePolyLineFunction& spline, float dt);
    double CalcSplineArcLength(const SplineCatmullRomFunction& spline, float threshold = 1e-4);
    double CalcSplineArcLength(const SplineCatmullRomFunction& spline, float t0, float t1, float threshold = 1e-4);

    class CatmullRomSpline
    {
    public:
        CatmullRomSpline(std::vector<SplinePoint> points);

        bool IsValid() const noexcept
        {
            return mCatmullRom.is_valid();
        }

        // take the curve displacement (travel along the curve)
        // and map that to a smoothed interpolated t value for sampling
        // the spline.
        float Reparametrize(float t) const noexcept;

        auto GetError() const noexcept
        { return mCatmullRom.get_error(); }

        auto Evaluate(const float t) const
        {
            const auto max = mCatmullRom.max_parameter();
            const auto& sample = mCatmullRom.evaluate(t * max);
            return sample;
        }
        auto Tangent(const float t) const
        {
            const auto max = mCatmullRom.max_parameter();
            const auto& sample = mCatmullRom.prime(t * max);
            return sample;
        }

        auto GetArcLength() const noexcept
        {
            return CalcSplineArcLength(mCatmullRom, 0.0f, 1.0f);
        }

        auto& GetFunction() const noexcept
        {
            return mCatmullRom;
        }

    private:
        const SplineCatmullRomFunction mCatmullRom;
        double mPathLength = 0.0f;
        struct DisplacementMapping {
            double d = 0.0f; // displacement along the  spline path
            double t = 0.0f; // spline sampling t value at that point.
        };
        std::vector<DisplacementMapping> mDisplacementMappings;
    };

    class PolyLineSpline
    {
    public:
        PolyLineSpline(std::vector<SplinePoint> points)
            : mPolyline(std::move(points))
        {}

        auto Evaluate(const float t) const
        {
            return mPolyline.Interpolate(t);
        }
        auto Tangent(const float t) const
        {
            return mPolyline.FindTangent(t);
        }

        auto GetLineLength() const noexcept
        {
            return mPolyline.GetLineLength();
        }

        bool IsValid() const noexcept
        {
            return mPolyline.IsValid();
        }

        auto& GetFunction() const noexcept
        {
            return mPolyline;
        }
    private:
        SplinePolyLineFunction mPolyline;
    };

} // namespace