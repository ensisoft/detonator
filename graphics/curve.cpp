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

#include "warnpush.h"
#  include <third_party/boost/catmull_rom.hpp>
#include "warnpop.h"

#include "base/logging.h"
#include "base/hash.h"
#include "base/format.h"
#include "base/polyline.h"
#include "graphics/curve.h"

#include "data/reader.h"
#include "data/writer.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/shader_code.h"
#include "graphics/shader_source.h"
#include "graphics/geometry.h"

namespace gfx
{

void CurvePoint::IntoJson(data::Writer& writer) const
{
    writer.Write("pos-x", mData[0]);
    writer.Write("pos-y", mData[1]);
    writer.Write("pos-z", mData[2]);
    writer.Write("data-x", mData[3]);
    writer.Write("data-y", mData[4]);
    writer.Write("width", mData[5]);
}

bool CurvePoint::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("pos-x", &mData[0]);
    ok &= reader.Read("pos-y", &mData[1]);
    ok &= reader.Read("pos-z", &mData[2]);
    ok &= reader.Read("data-x", &mData[3]);
    ok &= reader.Read("data-y", &mData[4]);
    ok &= reader.Read("width", &mData[5]);
    return true;
}

float GetPointDistance(const CurvePoint& p0, const CurvePoint& p1) noexcept
{
    return glm::length(p1.GetPosition().ToVec3() -
                       p0.GetPosition().ToVec3());
}

SpatialMode CurveClass::GetSpatialMode() const
{
    return SpatialMode::Flat2D;
}

DrawableClass::Type CurveClass::GetType() const
{
    return Type::Curve;
}
std::unique_ptr<DrawableClass> CurveClass::Clone() const
{
    return std::make_unique<CurveClass>(*this);
}
std::unique_ptr<DrawableClass> CurveClass::Copy() const
{
    auto copy = std::make_unique<CurveClass>(*this);
    copy->mId = base::RandomString(10);
    return copy;
}
std::size_t CurveClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mContentHash);
    hash = base::hash_combine(hash, mCurveType);
    hash = base::hash_combine(hash, mJointStyle);
    hash = base::hash_combine(hash,  mEndCapStyle);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}
std::string CurveClass::GetName() const
{
    return mName;
}
std::string CurveClass::GetId() const
{
    return mId;
}

void CurveClass::SetName(const std::string& name)
{
    mName = name;
}

void CurveClass::IntoJson(data::Writer& writer) const
{
    for (const auto& point : mPoints)
    {
        auto chunk = writer.NewWriteChunk();
        point.IntoJson(*chunk);
        writer.AppendChunk("curve-points", std::move(chunk));
    }
    writer.Write("curve-id", mId);
    writer.Write("curve-name", mName);
    writer.Write("curve-type", mCurveType);
    writer.Write("curve-joint-style", mJointStyle);
    writer.Write("curve-end-cap-style", mEndCapStyle);
    writer.Write("curve-flgas", mFlags);

    const uint64_t hash = mContentHash;
    const unsigned hi = (hash >> 32) & 0xffffffff;
    const unsigned lo = (hash >> 0 ) & 0xffffffff;
    writer.Write("curve-content-hash-lo", lo);
    writer.Write("curve-content-hash-hi", hi);
}

bool CurveClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    for (unsigned i=0; i<reader.GetNumChunks("curve-points"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("curve-points", i);
        ControlPoint point;
        ok &= point.FromJson(*chunk);
        mPoints.push_back(point);
    }

    ok &= reader.Read("curve-id", &mId);
    ok &= reader.Read("curve-name", &mName);
    ok &= reader.Read("curve-type", &mCurveType);
    ok &= reader.Read("curve-joint-style", &mJointStyle);
    ok &= reader.Read("curve-end-cap-style", &mEndCapStyle);
    ok &= reader.Read("curve-flgas", &mFlags);

    uint64_t hash = 0;
    unsigned hi = 0;
    unsigned lo = 0;
    ok &= reader.Read("curve-content-hash-lo", &lo);
    ok &= reader.Read("curve-content-hash-hi", &hi);
    hash = static_cast<uint64_t>(hi) << 32 | static_cast<uint64_t>(lo);
    mContentHash = hash;

    return ok;
}

bool CurveClass::Construct(GeometryBuffer* buffer) const
{
    using CatmullRomFunction = boost::math::catmull_rom<ControlPoint, 6>;
    using PolylineFunction = base::PolyLine<ControlPoint>;

    if (mCurveType == CurveType::PolylineSpline)
    {
        if (mPoints.size() < 4)
            return true;

        std::vector<Vertex2D> vertices;

        auto points_copy = mPoints;
        CatmullRomFunction catmull_rom(std::move(points_copy));

        // evaluate the spline and tessellate into a triangle strip

        buffer->SetVertexBuffer(std::move(vertices));
        buffer->SetVertexLayout(GetVertexLayout<Vertex2D>());
        return true;
    }
    else if (mCurveType == CurveType::CatmullRomSpline)
    {
        if (mPoints.size() < 2)
            return true;

        std::vector<Vertex2D> vertices;

        auto points_copy = mPoints;
        PolylineFunction line_function(std::move(points_copy));

        // evaluate the spline and tessellate into a triangle strip


        buffer->SetVertexBuffer(std::move(vertices));
        buffer->SetVertexLayout(GetVertexLayout<Vertex2D>());
        return true;
    }
    else BUG("Missing curve type handling.");
    return false;
}

CurveInstance::CurveInstance(std::shared_ptr<const CurveClass> klass) noexcept
    : mClass(std::move(klass))
{
}
CurveInstance::CurveInstance(const CurveClass& klass)
    : mClass(std::make_shared<CurveClass>(klass))
{
}
CurveInstance::CurveInstance(CurveClass&& klass)
    : mClass(std::make_shared<CurveClass>(std::move(klass)))
{
}

bool CurveInstance::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState& raster) const
{

}

ShaderSource CurveInstance::GetShader(const Environment& env, const Device& device) const
{

}

std::string CurveInstance::GetShaderId(const Environment& env) const
{

}

std::string CurveInstance::GetShaderName(const Environment& env) const
{

}

DrawGeometryHandle CurveInstance::GetGeometry(const Environment& env, Device& device) const
{
    auto buffer = Construct(env);

    Geometry::CreateArgs args;
    args.content_hash = mClass->GetContentHash();
    args.content_name = mClass->GetName();

}
DrawGeometryBuffer CurveInstance::Construct(const Environment& env) const
{
    GeometryBuffer buffer;
    if (!mClass->Construct(&buffer))
        return DrawGeometryBuffer::Null;

    const bool static_content = mClass->IsStatic();

    if (env.mesh_type == MeshType::Wireframe)
    {
        // not yet supported.
        return DrawGeometryBuffer::Null;
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        // not yet supported.
        return DrawGeometryBuffer::Null;
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        if (static_content)
            DEBUG("Crated paint mesh on curve. [name='%1']", mClass->GetName());

        return std::move(buffer);
    }
    else BUG("Missing mesh type handling.");
    return DrawGeometryBuffer::Null;
}

Drawable::Type CurveInstance::GetType() const
{
    return Type::Curve;
}

DrawPrimitive CurveInstance::GetDrawPrimitive() const
{
    return DrawPrimitive::Triangles;
}
SpatialMode CurveInstance::GetSpatialMode() const
{
    return SpatialMode::Flat2D;
}

} // namespace