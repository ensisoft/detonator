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

#include "base/utility.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/curve.h"

namespace game
{

CurveClass::CurveClass(std::string id)
    : mClassId(std::move(id))
{}

CurveClass::CurveClass()
    : mClassId(base::RandomString(10))
{}

CurveClass::CurveClass(const CurveClass& other, std::string id)
    : mClassId(std::move(id))
    , mName(other.mName)
    , mCurve(other.mCurve)
    , mCurveType(other.mCurveType)
    , mJointStyle(other.mJointStyle)
    , mEndCapStyle(other.mEndCapStyle)
{}

size_t CurveClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mCurve.GetHash());
    hash = base::hash_combine(hash, mCurveType);
    hash = base::hash_combine(hash, mJointStyle);
    hash = base::hash_combine(hash, mEndCapStyle);
    return hash;
}

void CurveClass::IntoJson(data::Writer& writer) const
{
    auto curve_data = writer.NewWriteChunk();
    mCurve.IntoJson(*curve_data);

    writer.Write("curve-class-id",      mClassId);
    writer.Write("curve-name",          mName);
    writer.Write("curve-type",          mCurveType);
    writer.Write("curve-joint-style",   mJointStyle);
    writer.Write("curve-end-cap-style", mEndCapStyle);
    writer.Write("curve-data",          std::move(curve_data));
}

bool CurveClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    if (const auto& chunk = reader.GetReadChunk("curve-data"))
        ok &= mCurve.FromJson(*chunk);

    ok &= reader.Read("curve-class-id",      &mClassId);
    ok &= reader.Read("curve-name",          &mName);
    ok &= reader.Read("curve-type",          &mCurveType);
    ok &= reader.Read("curve-joint-style",   &mJointStyle);
    ok &= reader.Read("curve-end-cap-style", &mEndCapStyle);
    return ok;
}

} // namespace