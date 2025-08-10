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

#include "base/logging.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node_spline_mover.h"

namespace {
    struct CatmullRomCache {
        std::shared_ptr<const game::Spline::CatmullRomFunction> spline;
        double spline_path_length = 0.0;
    };

    std::unordered_map<const game::SplineMoverClass*,
        CatmullRomCache> catmull_rom_cache;
} // namespace

namespace game
{

SplineMoverClass::~SplineMoverClass()
{
    catmull_rom_cache.erase(this);
}

std::shared_ptr<const SplineMoverClass::CatmullRomFunction> SplineMoverClass::GetCatmullRom() const
{
    auto it = catmull_rom_cache.find(this);
    if (it != catmull_rom_cache.end())
        return it->second.spline;

    return mSpline.MakeCatmullRom();
}

std::shared_ptr<const SplineMoverClass::CatmullRomFunction> SplineMoverClass::MakeCatmullRom() const
{
    return mSpline.MakeCatmullRom();
}

double SplineMoverClass::GetPathLength() const
{
    auto it = catmull_rom_cache.find(this);
    if (it != catmull_rom_cache.end())
        return it->second.spline_path_length;

    auto spline = MakeCatmullRom();
    return Spline::CalcArcLength(*spline, 0.0f, 1.0f);
}

size_t SplineMoverClass::GetHash() const noexcept
{
    size_t hash = mSpline.GetHash();
    hash = base::hash_combine(hash, mPathCoordinateSpace);
    hash = base::hash_combine(hash, mPathCurveType);
    hash = base::hash_combine(hash, mSpeed);
    hash = base::hash_combine(hash, mAcceleration);
    hash = base::hash_combine(hash, mRotationMode);
    return hash;
}

void SplineMoverClass::IntoJson(data::Writer& data) const
{
    mSpline.IntoJson(data);
    data.Write("path-coordinate-space", mPathCoordinateSpace);
    data.Write("path-curve-type"      , mPathCurveType);
    data.Write("rotation-mode"        , mRotationMode);
    data.Write("acceleration"         , mAcceleration);
    data.Write("speed"                , mSpeed);
}

bool SplineMoverClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= mSpline.FromJson(data);
    ok &= data.Read("path-coordinate-space", &mPathCoordinateSpace);
    ok &= data.Read("path-curve-type"      , &mPathCurveType);
    ok &= data.Read("rotation-mode"        , &mRotationMode);
    ok &= data.Read("acceleration"         , &mAcceleration);
    ok &= data.Read("speed"                , &mSpeed);
    return ok;
}

bool SplineMoverClass::InitClassRuntime() const
{
    auto catmull_rom = mSpline.MakeCatmullRom();
    if (!catmull_rom || !catmull_rom->is_valid())
    {
        WARN("Entity node spline mover spline definition is invalid.");
        return false;
    }
    bool ok = true;

    if (mRotationMode == RotationMode::ApplySplineRotation && mPathCoordinateSpace == PathCoordinateSpace::Relative)
    {
        WARN("Spline rotator set to apply object rotation from spline while using relative coordinates.");
        WARN("Applying the rotation from spline will rotate the direction of the spline itself.");
        ok = false;
    }

    CatmullRomCache cached_spline;
    cached_spline.spline = catmull_rom;
    cached_spline.spline_path_length = Spline::CalcArcLength(*catmull_rom, 0.0f, 1.0f);
    VERBOSE("Computed and cached spline length value. [length='%1']", cached_spline.spline_path_length);
    catmull_rom_cache[this] = cached_spline;
    return true;
}

SplineMover::SplineMover(std::shared_ptr<const SplineMoverClass> klass)
  : mClass(std::move(klass))
  , mSpeed(mClass->GetSpeed())
  , mAcceleration(mClass->GetAcceleration())
{
    mCatmullRom = mClass->GetCatmullRom();
    if (!mCatmullRom)
        return;

    if (mClass->GetPathCoordinateSpace() == PathCoordinateSpace::Relative)
        mStartPos = mCatmullRom->evaluate(0.0f).GetPosition();

    mPathLength = (float)mClass->GetPathLength();
}

}// namespace