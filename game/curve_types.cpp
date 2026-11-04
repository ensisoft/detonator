// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "base/hash.h"
#include "base/utility.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/curve_types.h"

namespace game
{

CurvePoint::CurvePoint(const Float3& position, const Float3& look_at, const Float2& data, float width)
  : mPosition(position)
  , mLookAt(look_at)
  , mData(data)
  , mWidth(width)
{}

void CurvePoint::IntoJson(data::Writer& writer) const
{
    writer.Write("position", mPosition.ToVec3());
    writer.Write("look-at", mLookAt.ToVec3());
    writer.Write("data", mData.ToVec2());
    writer.Write("width", mWidth);
}

bool CurvePoint::FromJson(const data::Reader& reader)
{
    bool ok = true;

    glm::vec3 position;
    glm::vec3 look_at;
    glm::vec2 data;

    ok &= reader.Read("position", &position);
    ok &= reader.Read("look-at",  &look_at);
    ok &= reader.Read("data",     &data);
    ok &= reader.Read("width",    &mWidth);
    mPosition = position;
    mLookAt   = look_at;
    mData     = data;
    return ok;
}

Curve::Curve(const std::vector<CurvePoint>& points)
  : mPoints(points)
{}
Curve::Curve(std::vector<CurvePoint>&& points)
  : mPoints(std::move(points))
{}

void Curve::SetPoint(const CurvePoint& point, size_t index)
{
    base::SafeIndex(mPoints, index) = point;
}

void Curve::AppendPoint(const CurvePoint& point)
{
    mPoints.push_back(point);
}
void Curve::PrependPoint(const CurvePoint& point)
{
    mPoints.insert(mPoints.begin(), point);
}
void Curve::ErasePoint(size_t index)
{
    ASSERT(index < mPoints.size());
    auto it = mPoints.begin();
    mPoints.erase(it + index);
}

std::size_t Curve::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mPoints);
    return hash;
}

void Curve::IntoJson(data::Writer& data) const
{
    for (const auto& point : mPoints)
    {
        auto chunk = data.NewWriteChunk();
        point.IntoJson(*chunk);
        data.AppendChunk("curve-points", std::move(chunk));
    }
}
bool Curve::FromJson(const data::Reader& data)
{
    bool ok = true;
    for (unsigned i=0; i<data.GetNumItems("curve-points"); ++i)
    {
        const auto& chunk = data.GetReadChunk("curve-points", i);

        CurvePoint point;
        ok &= point.FromJson(*chunk);
        mPoints.push_back(point);
    }
    return ok;
}

} // namespace