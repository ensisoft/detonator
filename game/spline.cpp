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


// Redefine a spline control point at the given index.
// The index must be valid.
void Spline::SetPoint(const SplinePoint& point, size_t index)
{
    base::SafeIndex(mPoints, index) = point;
}

// Append a new control point to the spline.
void Spline::AppendPoint(const SplinePoint& point)
{
    mPoints.push_back(point);
}

void Spline::PrependPoint(const SplinePoint& point)
{
    mPoints.insert(mPoints.begin(), point);
}

void Spline::ErasePoint(size_t index)
{
    ASSERT(index < mPoints.size());
    auto it = mPoints.begin();
    mPoints.erase(it + index);
}

size_t Spline::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mPoints);
    return hash;
}

std::shared_ptr<const Spline::CatmullRomFunction> Spline::MakeCatmullRom() const
{
    if (mPoints.size() < 4)
        return nullptr;

    auto copy = mPoints;
    auto func = std::make_shared<CatmullRomFunction>(std::move(copy));
    return func;
}

std::shared_ptr<const Spline::PolyLineFunction> Spline::MakePolyLine() const
{
    if (mPoints.size() < 2)
        return nullptr;

    auto copy = mPoints;
    auto func = std::make_shared<PolyLineFunction>(std::move(copy));
    return func;
}

void Spline::IntoJson(data::Writer& data) const
{
    for (const auto& point : mPoints)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("x", point.mData[0]);
        chunk->Write("y", point.mData[1]);
        chunk->Write("z", point.mData[2]);
        chunk->Write("w", point.mData[3]);
        data.AppendChunk("spline_points", std::move(chunk));
    }
}

bool Spline::FromJson(const data::Reader& data)
{
    bool ok = true;
    for (unsigned i=0; i<data.GetNumItems("spline_points"); ++i)
    {
        const auto& chunk = data.GetReadChunk("spline_points", i);

        SplinePoint point;
        ok &= chunk->Read("x", &point.mData[0]);
        ok &= chunk->Read("y", &point.mData[1]);
        ok &= chunk->Read("z", &point.mData[2]);
        ok &= chunk->Read("w", &point.mData[3]);
        mPoints.push_back(point);
    }
    return ok;
}

// static
SplinePoint Spline::Evaluate(const CatmullRomFunction& spline_func, float t)
{
    t = math::clamp(0.0f, 1.0f, t);
    const auto max = spline_func.max_parameter();
    const auto& val = spline_func.evaluate(t * max);
    return val;
}

// static
double Spline::CalcArcLength(const CatmullRomFunction& spline, float t0, float t1, float threshold)
{
    // basic idea take the linear length between two points and compare to
    // poly-line length (with midpoint in the middle of t0 and t1) and
    // if the linear line is within the threshold accept the value otherwise
    // subdivide.

    const auto mid = 0.5f * (t0 + t1);

    const auto p0 = Evaluate(spline, t0).GetPosition().ToVec2();
    const auto p1 = Evaluate(spline, mid).GetPosition().ToVec2();
    const auto p2 = Evaluate(spline, t1).GetPosition().ToVec2();

    const auto chord = glm::length(p0 - p2);
    const auto poly  = glm::length(p0 - p1) + glm::length(p1 - p2);
    // for very short units don't bother with subdivision
    if (chord < 1.0f)
        return chord;

    const auto diff = std::abs(chord - poly);
    if (diff < threshold)
        return chord;

    // recurse and subdivide
    return CalcArcLength(spline, t0, mid) +
           CalcArcLength(spline, mid, t1);

}

double Spline::CalcArcLength(const CatmullRomFunction& spline, float threshold)
{
    return CalcArcLength(spline, 0.0f, 1.0f, threshold);
}


} // namespace