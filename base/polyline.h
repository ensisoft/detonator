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

#include <vector>
#include <initializer_list>

#include "base/assert.h"

namespace base
{
    template<typename Point>
    class PolyLine
    {
    public:
        PolyLine() = default;
        explicit PolyLine(const std::vector<Point>& points)
        {
            if (points.size() < 2)
                return;

            mPoints.reserve(points.size());

            P first;
            first.distance = 0.0f;
            first.point = points[0];
            mPoints.push_back(first);

            float total_distance = 0.0f;
            for (size_t i=1; i<points.size(); ++i)
            {
                const auto& p0 = points[i-1];
                const auto& p1 = points[i-0];
                const auto point_dist = GetPointDistance(p0, p1);

                total_distance += point_dist;
                P p;
                p.distance = total_distance;
                p.point    = p1;
                mPoints.push_back(p);
            }
            mPolylineLength = total_distance;
        }

        // Interpolate between polyline points based on the given
        // displacement into the polyline. Displacement is expected
        // to be between 0.0f and m (inclusive) where m is the combined
        // length of poly line segments.
        Point Interpolate(float displacement) const
        {
            ASSERT(!mPoints.empty());
            if (displacement < 0.0f)
                return mPoints.front().point;

            const auto point_count = mPoints.size() - 1;
            for (size_t i=0; i<point_count; ++i)
            {
                const auto sI = i;
                const auto eI = i + 1;
                ASSERT(sI < mPoints.size() && eI < mPoints.size());
                if (displacement >= mPoints[sI].distance && displacement < mPoints[eI].distance)
                {
                    const auto span_length = mPoints[eI].distance - mPoints[sI].distance;
                    const auto p0 = mPoints[sI].point;
                    const auto p1 = mPoints[eI].point;
                    const auto t = (displacement - mPoints[sI].distance) / span_length;
                    return InterpolatePoint(p0, p1, t);
                }
            }
            return mPoints.back().point;
        }

        Point FindTangent(float displacement) const
        {
            ASSERT(!mPoints.empty());
            if (displacement < 0.0f)
                return {};

            const auto point_count = mPoints.size() - 1;
            for (size_t i=0; i<point_count; ++i)
            {
                const auto sI = i;
                const auto eI = i + 1;
                ASSERT(sI < mPoints.size() && eI < mPoints.size());
                if (displacement >= mPoints[sI].distance && displacement < mPoints[eI].distance)
                {
                    const auto p0 = mPoints[sI].point;
                    const auto p1 = mPoints[eI].point;
                    const auto point_dist = mPoints[eI].distance - mPoints[sI].distance;
                    return ComputePointTangent(p0, p1, point_dist);
                }
            }
            return {};
        }

        auto GetLineLength() const noexcept
        { return mPolylineLength; }
        auto GetPointCount() const noexcept
        { return mPoints.size(); }
        const auto& GetPoint(size_t index) const noexcept
        { return mPoints[index].point; }
        auto GetPoint(size_t index) noexcept
        { return mPoints[index].point; }
    private:
        struct P {
            float distance = 0.0f;
            Point point;
        };
        std::vector<P> mPoints;
        float mPolylineLength = 0.0;
    };

} // namespace