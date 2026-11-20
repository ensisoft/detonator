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

#include <atomic>
#include <unordered_map>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/curve.h"
#include "game/spline.h"

namespace {

    // about the spline cache, this code file is built into several different
    // link units, the editor and the engine.dll/.so (and any other engine).
    // when the editor (or the game runner) loads the engine so and runs the
    // game it might be the .dll that first accesses the curve data and creates
    // an instance.
    // If we save/cache this instance in the *class* object we might end up
    // in a situation where the object is allocated by the thread running
    // in the DLL code. When the engine stops running the cached object
    // would then get deallocated in the editor/runner code. these two
    // modules might not be able to share objects this way and pass the
    // ownership around (depends on the runtime and the memory allocator
    // they link to etc. Generally a BAD FUCKING IDEA).

    // so to avoid that problem (while creating other problems) we can have
    // the curves cached in a data storage that doesn't require passing
    // objects across binary boundaries.

    struct SplineCache {
        std::shared_ptr<const game::CatmullRomSpline> catmull_rom;
        std::shared_ptr<const game::PolyLineSpline> polyline;
    };
    std::unordered_map<std::string, SplineCache> curve_cache;

    std::vector<game::SplinePoint> ToSpline(const game::CurveData& curve)
    {
        std::vector<game::SplinePoint> spline_points;
        for (size_t i=0; i<curve.GetPointCount(); ++i)
        {
            const auto& point = curve.GetPoint(i);

            const auto& position = point.GetPosition();
            const auto& look_at = point.GetLookAt();

            game::SplinePoint sp;
            sp.SetPosition(position.x, position.y);
            sp.SetLookAt(look_at.x, look_at.y);
            spline_points.push_back(sp);
        }

        return spline_points;
    }

    std::string FastId()
    {
        static std::atomic<std::uint64_t> counter = 1;
        return std::to_string(counter++);
    }

} //namespace

namespace game
{

CurveClass::CurveClass(std::string id)
    : mClassId(std::move(id))
{}

CurveClass::CurveClass()
    : mClassId(base::RandomString(10))
{}

CurveClass::CurveClass(const CurveClass& other)
    : mClassId(other.mClassId)
    , mName(other.mName)
    , mCurve(other.mCurve)
    , mCurveSource(other.mCurveSource)
    , mCurveType(other.mCurveType)
    , mJointStyle(other.mJointStyle)
    , mEndCapStyle(other.mEndCapStyle)
    , mWidthOverride(other.mWidthOverride)
{}

CurveClass::CurveClass(const CurveClass& other, std::string id)
    : mClassId(std::move(id))
    , mName(other.mName)
    , mCurve(other.mCurve)
    , mCurveSource(other.mCurveSource)
    , mCurveType(other.mCurveType)
    , mJointStyle(other.mJointStyle)
    , mEndCapStyle(other.mEndCapStyle)
    , mWidthOverride(other.mWidthOverride)
{}

CurveClass::~CurveClass()
{
    curve_cache.erase(mClassId);
}

void CurveClass::SetCurve(CurveData curve) noexcept
{
    mCurve = std::move(curve);
    curve_cache.erase(mClassId);
}

void CurveClass::SetPoint(const CurvePoint& point, const size_t index)
{
    mCurve.SetPoint(point, index);
    curve_cache.erase(mClassId);
}

void CurveClass::AppendPoint(const CurvePoint& point)
{
    mCurve.AppendPoint(point);
    curve_cache.erase(mClassId);
}

void CurveClass::PrependPoint(const CurvePoint& point)
{
    mCurve.PrependPoint(point);
    curve_cache.erase(mClassId);
}

void CurveClass::ErasePoint(const size_t index)
{
    mCurve.ErasePoint(index);
    curve_cache.erase(mClassId);
}

std::size_t CurveClass::GetPointHash(size_t index) const noexcept
{
    const auto& p = mCurve.GetPoint(index);
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, p.GetPosition());
    hash = base::hash_combine(hash, p.GetLookAt());
    hash = base::hash_combine(hash, p.GetData());
    hash = base::hash_combine(hash, p.GetWidth());
    return hash;
}

size_t CurveClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mCurve.GetHash());
    hash = base::hash_combine(hash, mCurveSource);
    hash = base::hash_combine(hash, mCurveType);
    hash = base::hash_combine(hash, mJointStyle);
    hash = base::hash_combine(hash, mEndCapStyle);
    if (mWidthOverride.has_value())
        hash = base::hash_combine(hash, mWidthOverride.value());
    return hash;
}

size_t CurveClass::GetCurveHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mCurve.GetHash());
    hash = base::hash_combine(hash, mCurveType);
    hash = base::hash_combine(hash, mJointStyle);
    hash = base::hash_combine(hash, mEndCapStyle);
    if (mWidthOverride.has_value())
        hash = base::hash_combine(hash, mWidthOverride.value());
    return hash;
}

void CurveClass::IntoJson(data::Writer& writer) const
{
    auto curve_data = writer.NewWriteChunk();
    mCurve.IntoJson(*curve_data);

    writer.Write("curve-class-id",      mClassId);
    writer.Write("curve-name",          mName);
    writer.Write("curve-type",          mCurveType);
    writer.Write("curve-source",        mCurveSource);
    writer.Write("curve-joint-style",   mJointStyle);
    writer.Write("curve-end-cap-style", mEndCapStyle);
    writer.Write("curve-data",          std::move(curve_data));
    writer.Write("curve-width",         mWidthOverride);
}

bool CurveClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    if (const auto& chunk = reader.GetReadChunk("curve-data"))
        ok &= mCurve.FromJson(*chunk);

    // migration
    if (!reader.HasValue("curve-source"))
        mCurveSource = CurveSource::SplineMover;
    else ok &= reader.Read("curve-source", &mCurveSource);

    ok &= reader.Read("curve-class-id",      &mClassId);
    ok &= reader.Read("curve-name",          &mName);
    ok &= reader.Read("curve-type",          &mCurveType);
    ok &= reader.Read("curve-joint-style",   &mJointStyle);
    ok &= reader.Read("curve-end-cap-style", &mEndCapStyle);
    ok &= reader.Read("curve-width",         &mWidthOverride);
    return ok;
}

std::shared_ptr<const CatmullRomSpline> CurveClass::MakeCatmullRomSpline() const
{
    ASSERT(mCurveType == Type::CatmullRom);

    if (auto* curve = base::SafeFind(curve_cache, mClassId))
    {
        if (curve->catmull_rom)
            return curve->catmull_rom;
    }

    auto catmull_rom = std::make_shared<CatmullRomSpline>(ToSpline(mCurve));
    curve_cache[mClassId].catmull_rom = catmull_rom;
    return catmull_rom;
}

std::shared_ptr<const PolyLineSpline> CurveClass::MakePolylineSpline() const
{
    ASSERT(mCurveType == Type::Polyline);

    if (auto*  curve = base::SafeFind(curve_cache, mClassId))
    {
        if (curve->polyline)
            return curve->polyline;
    }

    auto polyline = std::make_shared<PolyLineSpline>(ToSpline(mCurve));
    curve_cache[mClassId].polyline = polyline;
    return polyline;
}

bool CurveClass::InitClassRuntime(const std::string& debug_name) const
{
    curve_cache.erase(mClassId);

    if (mCurveType == CurveType::CatmullRom)
    {
        auto catmull_rom = MakeCatmullRomSpline();
        if (catmull_rom->IsValid())
        {
            DEBUG("Computed and cached catmull-rom spline curve. [entity='%1', curve='%2']", debug_name, mName);
        }
        else
        {
            ERROR("Failed to compute Catmull-Rom function. [entity='%1', curve='%2']",debug_name, mName);
            ERROR("%1", catmull_rom->GetError());
        }
        return !!catmull_rom;
    }
    else if (mCurveType == CurveType::Polyline)
    {
        auto polyline = MakePolylineSpline();
        if (polyline->IsValid())
        {
            DEBUG("Computed and cached a polyline spline. [entity='%1', curve='%2']", debug_name, mName);
        }
        else
        {
            ERROR("Failed to compute Polyline function. [entity='%1', curve='%2']", debug_name, mName);
            ERROR("%1", "Polyline requires a minimum of 2 points.");
        }
        return !!polyline;
    }
    else BUG("Unhandled curve type.");
    return true;
}

Curve::Curve(std::shared_ptr<const CurveClass> klass)
    : mClass(std::move(klass))
    , mInstanceId(FastId())
{}
Curve::Curve(const CurveClass& klass)
    : Curve(std::make_shared<CurveClass>(klass))
{}

Curve::Curve(CurveClass&& klass)
    : Curve(std::make_shared<CurveClass>(std::move(klass)))
{}

std::size_t Curve::GetCurveHash() const noexcept
{
    if (mInstanceCurve.has_value())
    {
        size_t hash = 0;
        hash = base::hash_combine(hash, mInstanceCurve->GetHash());
        hash = base::hash_combine(hash, mClass->GetCurveType());
        hash = base::hash_combine(hash, mClass->GetJointStyle());
        hash = base::hash_combine(hash, mClass->GetEndCapStyle());
        return hash;
    }
    return mClass->GetCurveHash();
}

float Curve::Reparametrize(float t) const
{
    if (mCatmullRom)
    {
        return mCatmullRom->Reparametrize(t);
    }
    return t;
}

Float2 Curve::GetCurvePosition(float t) const
{
    if (mCatmullRom)
    {
        const auto& sample = mCatmullRom->Evaluate(t);
        return sample.GetPosition();
    }
    else if (mPolyline)
    {
        const auto& sample = mPolyline->Evaluate(t);
        return sample.GetPosition();
    }
    else
    {
        BUG("Unhandled curve type.");
    }
    return {0.0f, 0.0f};
}
Float2 Curve::GetCurveTangent(float t) const
{
    if (mCatmullRom)
    {
        const auto& sample = mCatmullRom->Tangent(t);
        return sample.GetPosition();
    }
    else if (mPolyline)
    {
        const auto& sample = mPolyline->Tangent(t);
        return sample.GetPosition();
    }
    else
    {
        BUG("Unhandled curve type.");
    }
    return {0.0f, 0.0f};
}

double Curve::GetCurveLength() const noexcept
{
    if (mCatmullRom)
    {
        return mCatmullRom->GetArcLength();
    }
    else if (mPolyline)
    {
        return mPolyline->GetLineLength();
    }
    else
    {
        BUG("Unhandled curve type.");
    }
    return 0.0f;
}

bool Curve::PrepareCurve()
{
    const auto type = mClass->GetCurveType();
    if (type == CurveType::CatmullRom)
    {
        ASSERT(!mCatmullRom);

        if (mInstanceCurve)
        {
            mCatmullRom = std::make_shared<CatmullRomSpline>(ToSpline(mInstanceCurve.value()));
            if (mCatmullRom->IsValid())
            {

            }
            else
            {
                // todo: error
            }
        }
        else mCatmullRom = mClass->MakeCatmullRomSpline();
    }
    else if (type == CurveType::Polyline)
    {
        ASSERT(!mPolyline);

        if (mInstanceCurve)
        {
            mPolyline = std::make_shared<PolyLineSpline>(ToSpline(mInstanceCurve.value()));
            if (mPolyline->IsValid())
            {

            }
            else
            {
                // todo: error
            }
        } else mPolyline = mClass->MakePolylineSpline();
    }

    if (type == CurveType::CatmullRom)
        return mCatmullRom && mCatmullRom->IsValid();
    else if (type == CurveType::Polyline)
        return mPolyline && mPolyline->IsValid();
    else BUG("Unhandled curve type.");
    return false;
}

} // namespace
