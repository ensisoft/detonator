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
    private:
        friend class Spline;
        std::array<float, 4> mData = {0};
    };

    float GetPointDistance(const SplinePoint& p0, const SplinePoint& p1) noexcept;

    SplinePoint InterpolatePoint(const SplinePoint& p0, const SplinePoint& p1, float t) noexcept;
    SplinePoint ComputePointTangent(const SplinePoint& p0, const SplinePoint& p1, float dist) noexcept;

    class Spline
    {
    public:
        using CatmullRomFunction = boost::math::catmull_rom<SplinePoint, 4>;
        using PolyLineFunction = base::PolyLine<SplinePoint>;

        void SetPoints(std::vector<SplinePoint> points) noexcept
        { mPoints = std::move(points); }

        // Get the current number of spline control points.
        auto GetPointCount() const noexcept
        { return mPoints.size(); }

        // Get a spline control point at the given index.
        // The index must be valid.
        const auto& GetPoint(size_t i) const noexcept
        { return base::SafeIndex(mPoints, i); }

        // Redefine a spline control point at the given index.
        // The index must be valid.
        void SetPoint(const SplinePoint& point, size_t index);

        // Append a new control point to the spline.
        void AppendPoint(const SplinePoint& point);

        void PrependPoint(const SplinePoint& point);

        // Erase a spline control point. The index must be valid.i
        void ErasePoint(size_t index);

        std::shared_ptr<const CatmullRomFunction> MakeCatmullRom() const;
        std::shared_ptr<const PolyLineFunction> MakePolyLine() const;

        std::size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        static SplinePoint Evaluate(const CatmullRomFunction& spline, float t);
        static SplinePoint Evaluate(const PolyLineFunction& spline, float t);

        static double CalcArcLength(const CatmullRomFunction& spline, float threshold = 1e-4);
        static double CalcArcLength(const CatmullRomFunction& spline, float t0, float t1, float threshold = 1e-4);
    private:
        std::vector<SplinePoint> mPoints;
    private:
    };

} // namespace