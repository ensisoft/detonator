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

namespace {
    struct CatmullRomCache {
        std::shared_ptr<const game::Spline::CatmullRomFunction> spline;
        double spline_path_length = 0.0;

        struct DisplacementMapping {
            double d = 0.0f; // displacement along the  spline path
            double t = 0.0f; // spline sampling t value at that point.
        };
        std::vector<DisplacementMapping> displacement_mappings;
    };

    struct PolyLineCache {
        std::shared_ptr<const game::Spline::PolyLineFunction> polyline;
    };

    std::unordered_map<const game::SplineMoverClass*, CatmullRomCache> catmull_rom_cache;
    std::unordered_map<const game::SplineMoverClass*, PolyLineCache> polyline_cache;
} // namespace

namespace game
{

SplineMoverClass::SplineMoverClass()
{
    mFlags.set(Flags::Enabled, true);
}

SplineMoverClass::~SplineMoverClass()
{
    catmull_rom_cache.erase(this);
    polyline_cache.erase(this);
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

std::shared_ptr<const SplineMoverClass::PolyLineFunction> SplineMoverClass::GetPolyLine() const
{
    auto it = polyline_cache.find(this);
    if (it != polyline_cache.end())
        return it->second.polyline;

    return mSpline.MakePolyLine();
}
std::shared_ptr<const SplineMoverClass::PolyLineFunction> SplineMoverClass::MakePolyLine() const
{
    return mSpline.MakePolyLine();
}

double SplineMoverClass::GetPathLength() const
{
    if (mPathCurveType == PathCurveType::CatmullRom)
    {
        auto it = catmull_rom_cache.find(this);
        if (it != catmull_rom_cache.end())
            return it->second.spline_path_length;

        auto spline = MakeCatmullRom();
        return Spline::CalcArcLength(*spline, 0.0f, 1.0f);
    }
    if (mPathCurveType == PathCurveType::Linear)
    {
        auto it = polyline_cache.find(this);
        if (it != polyline_cache.end())
            return it->second.polyline->GetLineLength();

        auto poly_line = MakePolyLine();
        return poly_line->GetLineLength();
    }
    BUG("Missing spline path type handling");
    return 0.0;
}

double SplineMoverClass::Reparametrize(double displacement) const
{
    auto it = catmull_rom_cache.find(this);
    if (it == catmull_rom_cache.end())
    {
        if (!InitClassRuntime())
            return 0.0;
        it = catmull_rom_cache.find(this);
        ASSERT(it != catmull_rom_cache.end());
    }

    // our mapping table maps distances along the path to t values.
    // do a lookup and find which t values are the closest to the
    // current given displacement

    const auto& table  = it->second.displacement_mappings;

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

size_t SplineMoverClass::GetHash() const noexcept
{
    size_t hash = mSpline.GetHash();
    hash = base::hash_combine(hash, mPathCoordinateSpace);
    hash = base::hash_combine(hash, mPathCurveType);
    hash = base::hash_combine(hash, mSpeed);
    hash = base::hash_combine(hash, mAcceleration);
    hash = base::hash_combine(hash, mRotationMode);
    hash = base::hash_combine(hash, mIterationMode);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void SplineMoverClass::IntoJson(data::Writer& data) const
{
    mSpline.IntoJson(data);
    data.Write("path-coordinate-space", mPathCoordinateSpace);
    data.Write("path-curve-type"      , mPathCurveType);
    data.Write("rotation-mode"        , mRotationMode);
    data.Write("iteration-mode"       , mIterationMode);
    data.Write("acceleration"         , mAcceleration);
    data.Write("speed"                , mSpeed);
    data.Write("flags"                , mFlags);
}

bool SplineMoverClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= mSpline.FromJson(data);
    ok &= data.Read("path-coordinate-space", &mPathCoordinateSpace);
    ok &= data.Read("path-curve-type"      , &mPathCurveType);
    ok &= data.Read("rotation-mode"        , &mRotationMode);
    ok &= data.Read("iteration-mode"       , &mIterationMode);
    ok &= data.Read("acceleration"         , &mAcceleration);
    ok &= data.Read("speed"                , &mSpeed);
    ok &= data.Read("flags"                , &mFlags);
    return ok;
}

bool SplineMoverClass::InitClassRuntime() const
{
    bool ok = true;
    if (mRotationMode == RotationMode::ApplySplineRotation && mPathCoordinateSpace == PathCoordinateSpace::Relative)
    {
        WARN("Spline rotator set to apply object rotation from spline while using relative coordinates.");
        WARN("Applying the rotation from spline will rotate the direction of the spline itself.");
        ok = false;
    }

    if (mPathCurveType == PathCurveType::CatmullRom)
        ok &= InitCatmullRomCache();

    return ok;
}

bool SplineMoverClass::InitCatmullRomCache() const
{
    auto catmull_rom = mSpline.MakeCatmullRom();
    if (!catmull_rom || !catmull_rom->is_valid())
    {
        WARN("Entity node spline mover spline definition is invalid.");
        return false;
    }

    const auto max_parameter = catmull_rom->max_parameter();
    // todo: maybe use a more elaborate mechanism to refine the sampling based on the
    // "size" of the spline
    constexpr auto max_samples = 50;

    std::vector<SplinePoint> samples;
    for (size_t i=0; i<max_samples; ++i)
    {
        const auto t = static_cast<float>(i) /
                static_cast<float>(max_samples-1);
        samples.push_back(catmull_rom->evaluate(t * max_parameter));
    }

    std::vector<CatmullRomCache::DisplacementMapping> mappings;
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

        CatmullRomCache::DisplacementMapping m;
        m.d = displacement;
        m.t = t;
        mappings.push_back(m);
    }

    CatmullRomCache cached_spline;
    cached_spline.spline = catmull_rom;
    cached_spline.spline_path_length = Spline::CalcArcLength(*catmull_rom, 0.0f, 1.0f);
    cached_spline.displacement_mappings = std::move(mappings);

    VERBOSE("Computed and cached spline length value. [length='%1']", cached_spline.spline_path_length);
    catmull_rom_cache[this] = cached_spline;
    return true;
}

SplineMover::SplineMover(std::shared_ptr<const SplineMoverClass> klass)
  : mClass(std::move(klass))
  , mFlags(mClass->GetFlags())
  , mSpeed(mClass->GetSpeed())
  , mAcceleration(mClass->GetAcceleration())
{
    const auto curve = mClass->GetPathCurveType();
    if (curve == SplineMoverClass::PathCurveType::CatmullRom)
    {
        mCatmullRom = mClass->GetCatmullRom();
        if (!mCatmullRom)
            return;

        if (mClass->GetPathCoordinateSpace() == PathCoordinateSpace::Relative)
            mStartPos = mCatmullRom->evaluate(0.0f).GetPosition();

        mPathLength = (float)mClass->GetPathLength();
    }
    else if (curve == SplineMoverClass::PathCurveType::Linear)
    {
        mPolyLine = mClass->GetPolyLine();
        mPathLength = mPolyLine->GetLineLength();

        if (mClass->GetPathCoordinateSpace() == PathCoordinateSpace::Relative)
            mStartPos = mPolyLine->GetPoint(0).GetPosition();
    }
}

}// namespace
