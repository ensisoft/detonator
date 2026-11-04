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

#include <unordered_map>
#include <vector>
#include <algorithm>

#include "base/logging.h"
#include "base/hash.h"
#include "base/math.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_spline_mover.h"

namespace game
{

SplineMoverClass::SplineMoverClass()
{
    mFlags.set(Flags::Enabled, true);
}

size_t SplineMoverClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mPathCoordinateSpace);
    hash = base::hash_combine(hash, mSpeed);
    hash = base::hash_combine(hash, mAcceleration);
    hash = base::hash_combine(hash, mRotationMode);
    hash = base::hash_combine(hash, mIterationMode);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mCurveId);
    return hash;
}

void SplineMoverClass::IntoJson(data::Writer& data) const
{
    data.Write("path-coordinate-space", mPathCoordinateSpace);
    data.Write("rotation-mode"        , mRotationMode);
    data.Write("iteration-mode"       , mIterationMode);
    data.Write("acceleration"         , mAcceleration);
    data.Write("speed"                , mSpeed);
    data.Write("flags"                , mFlags);
    data.Write("curve-id"             , mCurveId);
}

bool SplineMoverClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.ReadEnum("path-coordinate-space", &mPathCoordinateSpace,
        { { PathCoordinateSpace::Parent, "Absolute" },
          { PathCoordinateSpace::Local,  "Relative" }});
    ok &= data.Read("rotation-mode"        , &mRotationMode);
    ok &= data.Read("iteration-mode"       , &mIterationMode);
    ok &= data.Read("acceleration"         , &mAcceleration);
    ok &= data.Read("speed"                , &mSpeed);
    ok &= data.Read("flags"                , &mFlags);
    ok &= data.Read("curve-id"             , &mCurveId);
    return ok;
}

bool SplineMoverClass::InitClassRuntime() const
{
    bool ok = true;
    if (mRotationMode == RotationMode::ApplySplineRotation && mPathCoordinateSpace == PathCoordinateSpace::Local)
    {
        WARN("Spline rotator set to apply object rotation from spline while using local coordinates.");
        WARN("Applying the rotation from spline will rotate the direction of the spline itself.");
        ok = false;
    }
    return ok;
}

SplineMover::SplineMover(std::shared_ptr<const SplineMoverClass> klass)
  : mClass(std::move(klass))
  , mFlags(mClass->GetFlags())
  , mSpeed(mClass->GetSpeed())
  , mAcceleration(mClass->GetAcceleration())
{}

void SplineMover::Initialize(const Curve* curve)
{
    mCurve = curve;

    if (mClass->GetPathCoordinateSpace() == PathCoordinateSpace::Local)
        mStartPos = mCurve->GetCurvePosition(0.0f);

    mPathLength = static_cast<float>(curve->GetCurveLength());
}

}// namespace
