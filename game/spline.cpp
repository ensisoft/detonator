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

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "base/logging.h"
#include "base/math.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/spline.h"

namespace game
{

float GetPointDistance(const SplinePoint& p0, const SplinePoint& p1) noexcept
{
    return glm::length(p1.GetPosition().ToVec2() -
                       p0.GetPosition().ToVec2());
}
SplinePoint InterpolatePoint(const SplinePoint& p0, const SplinePoint& p1, float t) noexcept
{
    const auto& pos0 = p0.GetPosition().ToVec2();
    const auto& pos1 = p1.GetPosition().ToVec2();
    const auto& lookAt0 = p0.GetLookAt().ToVec2();
    const auto& lookAt1 = p1.GetLookAt().ToVec2();
    const auto mix_pos = glm::mix(pos0, pos1, t);
    const auto mix_lookAt = glm::mix(lookAt0,lookAt1, t);
    return { mix_pos, mix_lookAt };
}

SplinePoint ComputePointTangent(const SplinePoint& p0, const SplinePoint& p1, float dist) noexcept
{
    // difference in points over distance.
    const auto& pos0 = p0.GetPosition().ToVec2();
    const auto& pos1 = p1.GetPosition().ToVec2();
    const auto& lookAt0 = p0.GetLookAt().ToVec2();
    const auto& lookAt1 = p1.GetLookAt().ToVec2();
    const auto pos = (pos1 - pos0) / dist;
    const auto lookAt = (lookAt1 - lookAt0) / dist;
    return { pos, lookAt };
}

CatmullRomSpline::CatmullRomSpline(std::vector<SplinePoint> points)
    : mCatmullRom(std::move(points))
{
    if (!mCatmullRom.is_valid())
        return;

    const auto max_parameter = mCatmullRom.max_parameter();
    // todo: maybe use a more elaborate mechanism to refine the sampling based on the
    // "size" of the spline
    constexpr auto max_samples = 50;

    std::vector<SplinePoint> samples;
    for (size_t i=0; i<max_samples; ++i)
    {
        const auto t = static_cast<float>(i) /
                static_cast<float>(max_samples-1);
        samples.push_back(mCatmullRom.evaluate(t * max_parameter));
    }

    std::vector<DisplacementMapping> mappings;
    mappings.push_back({ 0.0, 0.0f });
    double displacement = 0.0;

    for (unsigned i=1; i<max_samples; ++i)
    {
        const auto& s0 = samples[i-1];
        const auto& s1 = samples[i-0];
        const auto d = glm::length(s1.GetPosition().ToVec2() - s0.GetPosition().ToVec2());
        displacement += d;

        const auto t = static_cast<float>(i) /
                static_cast<float>(max_samples - 1);

        DisplacementMapping m;
        m.d = displacement;
        m.t = t;
        mappings.push_back(m);
    }
    mPathLength = CalcSplineArcLength(mCatmullRom, 0.0f, 1.0f);
    mDisplacementMappings = std::move(mappings);
}

float CatmullRomSpline::Reparametrize(float displacement) const noexcept
{
    // our mapping table maps distances along the path to t values.
    // do a lookup and find which t values are the closest to the
    // current given displacement

    const auto& table = mDisplacementMappings;

    const auto span_count = table.size() - 1;
    for (size_t i=0; i<span_count; ++i)
    {
        const auto start_index = i;
        const auto end_index   = i+1;
        ASSERT(start_index < table.size() && end_index < table.size());

        if (displacement >= table[start_index].d  && displacement < table[end_index].d)
        {
            const auto  span_distance  = table[end_index].d - table[start_index].d;
            const auto t0 = table[start_index].t;
            const auto t1 = table[end_index].t;
            const auto span_t = (displacement - table[start_index].d) / span_distance;
            return math::lerp(t0, t1, span_t);
        }
    }
    if (displacement < 0.0f)
        return 0.0;
    if (displacement >= table.back().d)
        return 1.0;

    return 0.0;
}

SplinePoint EvaluateSpline(const SplineCatmullRomFunction& spline_func, float t)
{
    t = math::clamp(0.0f, 1.0f, t);
    const auto max = spline_func.max_parameter();
    const auto& val = spline_func.evaluate(t * max);
    return val;
}

double CalcSplineArcLength(const SplineCatmullRomFunction& spline, float t0, float t1, float threshold)
{
    // basic idea take the linear length between two points and compare to
    // poly-line length (with midpoint in the middle of t0 and t1) and
    // if the linear line is within the threshold accept the value otherwise
    // subdivide.

    const auto mid = 0.5f * (t0 + t1);

    const auto p0 = EvaluateSpline(spline, t0).GetPosition().ToVec2();
    const auto p1 = EvaluateSpline(spline, mid).GetPosition().ToVec2();
    const auto p2 = EvaluateSpline(spline, t1).GetPosition().ToVec2();

    const auto chord = glm::length(p0 - p2);
    const auto poly  = glm::length(p0 - p1) + glm::length(p1 - p2);
    // for very short units don't bother with subdivision
    if (chord < 1.0f)
        return chord;

    const auto diff = std::abs(chord - poly);
    if (diff < threshold)
        return chord;

    // recurse and subdivide
    return CalcSplineArcLength(spline, t0, mid) +
           CalcSplineArcLength(spline, mid, t1);

}

double CalcSplineArcLength(const SplineCatmullRomFunction& spline, float threshold)
{
    return CalcSplineArcLength(spline, 0.0f, 1.0f, threshold);
}

} // namespace