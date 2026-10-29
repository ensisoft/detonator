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
#include "base/math.h"
#include "base/polyline.h"
#include "graphics/curve.h"

#include "data/reader.h"
#include "data/writer.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/shader_source.h"
#include "graphics/geometry.h"
#include "graphics/paint_log.h"
#include "graphics/vertex.h"

namespace {
bool ConstructCurve(const gfx::CurveType curve_type,
                    const gfx::CurveJointStyle joint_style,
                    const gfx::CurveEndCapStyle end_cap_style,
                    const std::vector<gfx::CurvePoint>& points,
                    gfx::GeometryBuffer* buffer,
                    std::string* error)
    {
    using CatmullRomFunction = boost::math::catmull_rom<gfx::CurvePoint, 6>;
    using PolylineFunction = base::PolyLine<gfx::CurvePoint>;

    if (curve_type == gfx::CurveType::CatmullRomSpline)
    {
        if (points.size() < 4)
        {
            *error = "The Catmull-Rom curve requires at least 4 points.";
            return false;
        }

        // the spline treats the control points as a closed loop when computing
        // the curve at the first and last segment, i.e. the point before the
        // first point is the last point and the point after the last point is
        // the first point. this distorts the curve at both ends and can make the
        // curve turn back on itself (zero tangent) at the first/last point.
        // to avoid this add ghost points before the first point and after the
        // last point by mirroring the neighbouring point through the end point
        // and only evaluate the spline between the real first and last points.
        const auto MakeGhostPoint = [](const gfx::CurvePoint& end_point, const gfx::CurvePoint& neighbour) {
            const auto end_point_position = end_point.GetPosition().ToVec3();
            const auto other_point_position = neighbour.GetPosition().ToVec3();
            const auto ghost_point_position = 2.0f * end_point_position - other_point_position;

            gfx::CurvePoint ghost = end_point;
            ghost.SetPosition(ghost_point_position);
            return ghost;
        };
        const auto point_count = points.size();

        std::vector<gfx::CurvePoint> points_copy;
        points_copy.reserve(point_count + 2);
        points_copy.push_back(MakeGhostPoint(points[0], points[1]));
        base::AppendVector(points_copy, points);
        points_copy.push_back(MakeGhostPoint(points[point_count-1], points[point_count-2]));

        CatmullRomFunction catmull_rom(std::move(points_copy));
        if (!catmull_rom.is_valid())
        {
            *error = catmull_rom.get_error();
            return false;
        }

        // evaluate the spline at regular intervals. at every sample position
        // compute the curve direction from the curve tangent and the normal
        // that is used to offset the curve point by half the (interpolated)
        // curve width on both sides of the curve.
        struct Sample {
            glm::vec2 position;
            glm::vec2 direction;
            glm::vec2 normal;
            float half_width = 0.0f;
            float t = 0.0f;
        };

        constexpr auto SamplesPerSegment = 16u;
        const auto sample_count = static_cast<unsigned>(points.size() - 1) * SamplesPerSegment + 1;
        // parameter range of the real control points, excluding the ghost points.
        const auto min_param = catmull_rom.parameter_at_point(1);
        const auto max_param = catmull_rom.parameter_at_point(point_count);

        std::vector<Sample> samples;
        samples.reserve(sample_count);

        glm::vec2 previous_direction = {1.0f, 0.0f};

        for (unsigned i=0; i<sample_count; ++i)
        {
            // clamp to avoid floating point error from taking us
            // beyond the max parameter.
            const float t = static_cast<float>(i) / static_cast<float>(sample_count - 1);
            const float s = std::min(min_param + t * (max_param - min_param), max_param);

            const auto point = catmull_rom(s);
            const auto prime = catmull_rom.prime(s);

            const auto& position = glm::vec2 { point.GetPosition().x, point.GetPosition().y };
            const auto& tangent  = glm::vec2 { prime.GetPosition().x, prime.GetPosition().y };
            const auto tangent_length = glm::length(tangent);

            // if the tangent degenerates (no movement in xy) then
            // keep using the previous direction.
            glm::vec2 direction = previous_direction;
            if (tangent_length > 0.0001f)
                direction = tangent / tangent_length;
            previous_direction = direction;

            Sample sample;
            sample.position   = position;
            sample.direction  = direction;
            sample.normal     = math::ComputePerpendicularVector(direction);
            sample.half_width = point.GetWidth() * 0.5f;
            sample.t          = t;
            samples.push_back(sample);
        }

        // extend the curve at both ends by half the curve width along
        // the curve direction for square end caps.
        if (end_cap_style == gfx::CurveEndCapStyle::Square)
        {
            auto& first = samples.front();
            auto& last  = samples.back();
            first.position -= first.direction * first.half_width;
            last.position  += last.direction * last.half_width;
        }

        // tessellate into a triangle strip from pairs of vertices on the left
        // and on the right side of the curve. the normal points to the left of
        // the curve direction (CCW perpendicular).
        //
        // with butt and square end caps the strip is L0, R0, L1, R1 ... which
        // makes the first triangle (L0, R0, L1) counter-clockwise.
        //
        // with round end caps the strip starts with the tip vertex of the start
        // cap followed by right/left pairs T, R0, L0, R1, L1 ... and ends with the
        // tip vertex of the end cap. this makes the first triangle (T, R0, L0)
        // counter-clockwise.
        //
        // since the triangle strip flips the vertex order on every odd triangle
        // all the triangles end up counter-clockwise. don't change the order
        // of the vertices!
        const bool round_caps = end_cap_style == gfx::CurveEndCapStyle::Round;

        // the number of segments in a quarter circle of a round end cap.
        static constexpr auto RoundCapSegments = 8u;

        std::vector<gfx::Vertex2D> vertices;
        vertices.reserve(sample_count * 2 + (round_caps ? 2 * (2 * (RoundCapSegments - 1) + 1) : 0));

        const auto AddVertex = [&vertices](const glm::vec2& position, float u, float v) {
            gfx::Vertex2D vertex;
            vertex.aPosition = gfx::Vec2 { position.x, position.y };
            vertex.aTexCoord = gfx::Vec2 { u, v };
            vertices.push_back(vertex);
        };
        const auto AddVertexPair = [&AddVertex, round_caps](const glm::vec2& left, const glm::vec2& right,
                                                            float u, float v_left, float v_right) {
            if (round_caps)
            {
                AddVertex(right, u, v_right);
                AddVertex(left,  u, v_left);
            }
            else
            {
                AddVertex(left,  u, v_left);
                AddVertex(right, u, v_right);
            }
        };

        // round end cap is a half circle around the end point of the curve
        // with the radius of half the curve width. the angle is measured from
        // the curve direction so that angle 0 is the tip of the cap and at
        // angle 90 degrees the cap meets the left and right edges of the curve.
        // the angles 0 and 90 degrees are not included, the tip is added
        // separately and the curve edge vertices are the first/last curve
        // sample vertices.
        const auto AddRoundCap = [&AddVertexPair](const Sample& sample, float sign, unsigned segment) {
            const auto angle = static_cast<float>(math::Pi) * 0.5f * static_cast<float>(segment) / static_cast<float>(RoundCapSegments);
            const auto cos_angle = std::cos(angle);
            const auto sin_angle = std::sin(angle);
            const auto forward = sample.direction * (sign * cos_angle * sample.half_width);
            const auto sideways = sample.normal * (sin_angle * sample.half_width);
            AddVertexPair(sample.position + forward + sideways,
                          sample.position + forward - sideways,
                          sample.t, 0.5f - 0.5f * sin_angle, 0.5f + 0.5f * sin_angle);
        };

        if (round_caps)
        {
            const auto& first = samples.front();
            AddVertex(first.position - first.direction * first.half_width, first.t, 0.5f);
            for (unsigned i=1; i<RoundCapSegments; ++i)
                AddRoundCap(first, -1.0f, i);
        }

        for (const auto& sample : samples)
        {
            AddVertexPair(sample.position + sample.normal * sample.half_width,
                          sample.position - sample.normal * sample.half_width,
                          sample.t, 0.0f, 1.0f);
        }

        if (round_caps)
        {
            const auto& last = samples.back();
            for (unsigned i=RoundCapSegments-1; i>0; --i)
                AddRoundCap(last, 1.0f, i);
            AddVertex(last.position + last.direction * last.half_width, last.t, 0.5f);
        }

        buffer->SetVertexBuffer(vertices);
        buffer->SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        buffer->AddDrawCmd(gfx::DrawType::TriangleStrip);
        return true;
    }
    else if (curve_type == gfx::CurveType::PolylineSpline)
    {
        if (points.size() < 2)
            return true;

        std::vector<gfx::Vertex2D> vertices;

        auto points_copy = points;
        PolylineFunction line_function(std::move(points_copy));

        // evaluate the spline and tessellate into a triangle strip


        buffer->SetVertexBuffer(std::move(vertices));
        buffer->SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
        return true;
    }
    else BUG("Missing curve type handling.");
    return false;
}

} // namespace

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
    auto copy = std::make_unique<CurveClass>(*this);
    copy->mId = base::RandomString(10);
    return copy;
}

std::unique_ptr<DrawableClass> CurveClass::Copy() const
{
    return std::make_unique<CurveClass>(*this);
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
    std::string error;
    if (ConstructCurve(mCurveType,
        mJointStyle, mEndCapStyle, mPoints, buffer, &error))
        return true;

    ERROR("Failed to tessellate curve. [name='%1', type=%2]", mName, mCurveType);
    ERROR("%1", error);
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
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    return true;
}

ShaderSource CurveInstance::GetShader(const Environment& env, const Device& device) const
{
    // we're not supporting instancing yet
    ASSERT(env.use_instancing == false);

    return Drawable::CreateShader(env.use_instancing, false, device, Shader::Simple2D);
}

std::string CurveInstance::GetShaderId(const Environment& env) const
{
    // we're not supporting instancing yet
    ASSERT(env.use_instancing == false);

    return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple2D);
}

std::string CurveInstance::GetShaderName(const Environment& env) const
{
    return Drawable::GetShaderName(env, Shader::Simple2D);
}

DrawGeometryHandle CurveInstance::GetGeometry(const Environment& env, Device& device) const
{
    std::string id;
    std::string name;
    if (env.mesh_type == MeshType::Wireframe)
    {
        GFX_PAINT_ERROR("Curve wireframe is not yet supported.");
        return DrawGeometryHandle::Null;
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        GFX_PAINT_ERROR("Curve debug mesh is not yet supported.");
        return DrawGeometryHandle::Null;
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        id = mClass->GetId();
        name = mClass->GetName();
    }

    const auto static_content = mClass->IsStatic();

    if (auto geometry = device.FindGeometry(id))
    {
        if (static_content)
        {
            if (!env.editing_mode)
                return DrawGeometryHandle { std::move(geometry) };

            const auto content_hash = mClass->GetContentHash();
            if (geometry->GetContentHash() == content_hash)
                return DrawGeometryHandle { std::move(geometry) };
        }
        else
        {
            const auto content_hash = mClass->GetContentHash();
            if (geometry->GetContentHash() == content_hash)
                return DrawGeometryHandle { std::move(geometry) };
        }
    }

    auto buffer = Construct(env);
    if (buffer.IsNull())
        return DrawGeometryHandle::CreateErrorGeometry(id,
            base::FormatString("Failed to tessellate curve mesh '%1'", mClass->GetName()),
            base::FormatString("Curve '%1' fallback", mClass->GetName()),
            mClass->GetContentHash(), device);

    Geometry::CreateArgs args;
    args.buffer = buffer.TransferGeometryBuffer();
    args.content_hash = mClass->GetContentHash();
    args.content_name = std::move(name);
    args.usage = static_content ? BufferUsage::Static : BufferUsage::Dynamic;
    auto geometry = device.CreateGeometry(id, std::move(args));
    return DrawGeometryHandle { std::move(geometry) };
}
DrawGeometryBuffer CurveInstance::Construct(const Environment& env) const
{
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
        GeometryBuffer buffer;
        if (!mClass->Construct(&buffer))
            return DrawGeometryBuffer::Null;

        if (static_content)
            DEBUG("Created paint mesh on curve. [name='%1']", mClass->GetName());

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

Curve::Curve(const Args& args, std::string id)
    : mArgs(args)
    , mCurveId(std::move(id))
{
    if (mCurveId.empty())
        ComputeId();
}
Curve::Curve(Args&& args,  std::string id)
    : mArgs(std::move(args))
    , mCurveId(std::move(id))
{
    if (mCurveId.empty())
        ComputeId();
}

void Curve::ComputeId()
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mArgs.points);
    hash = base::hash_combine(hash, mArgs.curve_type);
    hash = base::hash_combine(hash, mArgs.joint_style);
    hash = base::hash_combine(hash, mArgs.end_cap_style);
    mCurveId = std::to_string(hash);
    mName = base::FormatString("%1", mArgs.curve_type);
}

bool Curve::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
        Device& device, ProgramState& program, RasterState& raster) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    return true;
}

 ShaderSource Curve::GetShader(const Environment& env, const Device& device) const
 {
    // we're not supporting instancing yet
    ASSERT(env.use_instancing == false);

    return Drawable::CreateShader(env.use_instancing, false, device, Shader::Simple2D);
 }
 std::string Curve::GetShaderId(const Environment& env) const
 {
    // we're not supporting instancing yet
    ASSERT(env.use_instancing == false);

    return Drawable::GetShaderId(env.use_instancing, false, Shader::Simple2D);
 }
 std::string Curve::GetShaderName(const Environment& env) const
 {
    return Drawable::GetShaderName(env, Shader::Simple2D);
 }

DrawGeometryHandle Curve::GetGeometry(const Environment& env, Device& device) const
{
    std::string id;
    std::string name;

    if (env.mesh_type == MeshType::Wireframe)
    {
        GFX_PAINT_ERROR("Curve wireframe is not yet supported.");
        return DrawGeometryHandle::Null;
    }
    else if (env.mesh_type == MeshType::DebugMesh)
    {
        GFX_PAINT_ERROR("Curve debug mesh is not yet supported.");
        return DrawGeometryHandle::Null;
    }
    else if (env.mesh_type == MeshType::PaintMesh)
    {
        id = mCurveId;
        name = mName;
    }

    if (auto geometry = device.FindGeometry(id))
        return geometry;

    auto buffer = Construct(env);
    if (buffer.IsNull())
    {
        if (!mArgs.make_fallback_on_fail)
            return DrawGeometryHandle::Null;

        return DrawGeometryHandle::CreateErrorGeometry(id,
            base::FormatString("Failed to tessellate curve mesh '%1'", mName),
            base::FormatString("Curve '%1' fallback", mName),
            0, device);
    }

    Geometry::CreateArgs args;
    args.buffer = buffer.TransferGeometryBuffer();
    args.content_hash = 0; // we dont use this.
    args.content_name = std::move(name);
    args.usage = BufferUsage::Static;
    auto geometry = device.CreateGeometry(id, std::move(args));
    return DrawGeometryHandle { std::move(geometry) };
}

DrawGeometryBuffer Curve::Construct(const Environment& env) const
{
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
        std::string error;
        GeometryBuffer buffer;
        if (ConstructCurve(mArgs.curve_type,
            mArgs.joint_style, mArgs.end_cap_style, mArgs.points, &buffer, &error))
            return std::move(buffer);

        if (mArgs.make_fallback_on_fail)
        {
            ERROR("Failed to tessellate curve. ");
            ERROR("%1", error);
        }
        else
        {
            GFX_PAINT_ERROR("%1", error);
        }
        return DrawGeometryBuffer::Null;
    }
    else BUG("Missing mesh type handling.");
    return DrawGeometryBuffer::Null;
}

Drawable::Type Curve::GetType() const
{
    return Type::Curve;
}
DrawPrimitive Curve::GetDrawPrimitive() const
{
    return DrawPrimitive::Triangles;
}
SpatialMode Curve::GetSpatialMode() const
{
    return SpatialMode::Flat2D;
}

} // namespace