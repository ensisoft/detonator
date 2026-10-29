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
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <cmath>
#include <limits>

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
void ComputeCurveId(gfx::CurveArgs& curve)
{
    size_t hash = 0;
    hash = base::hash_combine(hash, curve.points);
    hash = base::hash_combine(hash, curve.curve_type);
    hash = base::hash_combine(hash, curve.joint_style);
    hash = base::hash_combine(hash, curve.end_cap_style);
    curve.curve_id = std::to_string(hash);
    curve.curve_name = base::FormatString("%1", curve.curve_type);
}

// The curve control points are in the normalized node space, i.e. in the unit box
// and the model matrix scales that to the size of the node. When the node isn't
// square the scaling is anisotropic, the same width would result in different
// line thickness along different axes and the round joints and caps would turn
// into ellipses. In order to keep the line thickness isotropic the curve is
// tessellated in an isotropic space where the points have been scaled by the
// node aspect, a = size / max(size.x, size.y). When rendering the inverse
// of that scaling is baked into the model matrix (model * scale(1/a)) so that
// the vertices end up at the same place as if the aspect wasn't considered
// but the line has the same thickness in all directions.
// The width then means a fraction of the longer side of the node.
//
// In order to limit the number of different geometries the aspect ratio is
// quantized into logarithmic buckets, k = round(N * log2(width / height)),
// and the geometry is tessellated for the aspect ratio of the bucket,
// 2^(k/N). The worst case relative error in the thickness is about
// 2^(1/(2N)) - 1, i.e. ~2.5% for N=14. Both the geometry and the model matrix
// use the bucket aspect (not the true aspect) so that the curve control
// points are always rendered exactly where the unit box says they are.
constexpr auto CurveAspectRatioBucketsPerOctave = 14.0;

// Map the aspect ratio of the model matrix (the length of the x axis vector over
// the length of the y axis vector) to a bucket index. k > 0 for wide, k < 0 for tall
// and k = 0 for square. A degenerate model matrix (or no model matrix at all)
// is mapped to a square.
int ComputeCurveAspectRatioBucket(const glm::mat4* model_matrix)
{
    if (!model_matrix)
        return 0;

    const auto width  = glm::length((*model_matrix) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    const auto height = glm::length((*model_matrix) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    // the negated comparison also catches NaN.
    if (!(width >= 1e-3f) || !(height >= 1e-3f) ||
        !std::isfinite(width) || !std::isfinite(height))
        return 0;

    const auto aspect = static_cast<double>(width) / static_cast<double>(height);
    const auto x = std::log2(aspect) * CurveAspectRatioBucketsPerOctave;
    return static_cast<int>(std::round(x));
}

// Get the isotropic scale 'a' = size / max(size) that corresponds to the bucket.
glm::vec2 GetCurveAspectScale(int bucket)
{
    const auto aspect = static_cast<float>(std::exp2(bucket / CurveAspectRatioBucketsPerOctave));
    if (aspect >= 1.0f)
        return {1.0f, 1.0f / aspect};
    return {aspect, 1.0f};
}

std::vector<gfx::CurvePoint> ScaleCurvePoints(const std::vector<gfx::CurvePoint>& points, const glm::vec2& scale)
{
    auto ret = points;
    for (auto& point : ret)
    {
        const auto pos = point.GetPosition();
        point.SetPosition(base::Float2(pos.x * scale.x, pos.y * scale.y));
    }
    return ret;
}

// the model matrix with the inverse of the isotropic scaling baked in.
glm::mat4 GetCurveModelMatrix(const glm::mat4& model_matrix, int bucket)
{
    const auto scale = GetCurveAspectScale(bucket);
    return model_matrix * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / scale.x, 1.0f / scale.y, 1.0f));
}

// the number of segments in a quarter circle of a round end cap / round joint.
constexpr auto RoundCapSegments = 8u;

// when the ratio of the miter length to the half width of the line
// exceeds this the miter joint falls back to a bevel joint. (same
// default as the miter limit in SVG)
constexpr auto MiterLimit = 4.0f;

// polyline points that are closer than this to the previous point
// are coincident and don't define a line segment.
constexpr auto MinSegmentLength = 0.00001f;

// Tessellate a polyline into a list of triangles. Every line segment is
// a quad that is offset by half the line width to both sides of the segment
// (the width is interpolated between the width of the segment end points).
// The consecutive segments are connected with the selected joint style
// and the ends of the line are finished with the selected end cap style.
//
// at a joint between two segments the inner side of the turn is mitered,
// i.e. the quads are trimmed to meet at the inner miter point so that they
// don't overlap (which would be visible when the material is translucent).
// if the segments are too short to be trimmed then the quads are left to
// overlap on the inner side. The outer side of the turn is filled with a
// bevel, miter or round joint.
//
// all the triangles are generated counter-clockwise.
bool TessellatePolyline(const gfx::CurveJointStyle joint_style,
                        const gfx::CurveEndCapStyle end_cap_style,
                        const std::vector<gfx::CurvePoint>& points,
                        gfx::GeometryBuffer* buffer,
                        std::string* error)
{
    struct Node {
        glm::vec2 position;
        float half_width = 0.0f;
        // the distance along the line normalized to [0, 1]
        float t = 0.0f;
    };
    struct Segment {
        glm::vec2 direction;
        // normal that points to the left of the line direction (CCW perpendicular)
        glm::vec2 normal;
        float length = 0.0f;
    };
    // a joint is the connection between two consecutive segments
    // in the node between them.
    struct Joint {
        // false when the segments are in line and no joint is needed.
        bool needed = false;
        // true when the line turns back on itself (180 degrees)
        bool reversal = false;
        // the side of the line where the turn is on the outside.
        // +1.0 is on the left (along the normal) -1.0 is on the right.
        float outer_side = 1.0f;
        // the inner miter point (where the offset lines on the inner
        // side of the turn intersect) and how far along the segments
        // the inner miter point is from the joint node.
        bool trimmed = false;
        float trim_distance = 0.0f;
        glm::vec2 inner_point;
    };

    // remove coincident points. these would produce segments without
    // a direction.
    std::vector<Node> nodes;
    nodes.reserve(points.size());
    for (const auto& point : points)
    {
        const auto position = point.GetPosition().ToVec2();
        if (!nodes.empty() && glm::distance(position, nodes.back().position) < MinSegmentLength)
            continue;
        Node node;
        node.position   = position;
        node.half_width = std::max(point.GetWidth(), 0.0f) * 0.5f;
        nodes.push_back(node);
    }
    if (nodes.size() < 2)
    {
        *error = "The polyline requires at least 2 distinct points.";
        return false;
    }

    const auto node_count    = nodes.size();
    const auto segment_count = node_count - 1;

    std::vector<Segment> segments(segment_count);
    float total_length = 0.0f;
    for (size_t i=0; i<segment_count; ++i)
    {
        const auto delta = nodes[i+1].position - nodes[i].position;
        segments[i].length    = glm::length(delta);
        segments[i].direction = delta / segments[i].length;
        segments[i].normal    = math::ComputePerpendicularVector(segments[i].direction);
        total_length += segments[i].length;
    }
    {
        float distance = 0.0f;
        for (size_t i=1; i<node_count; ++i)
        {
            distance += segments[i-1].length;
            nodes[i].t = distance / total_length;
        }
    }

    // compute the joints between the segments.
    // joint[i] is the joint in node i. The first and the last node
    // don't have a joint (these have the end caps)
    std::vector<Joint> joints(node_count);
    for (size_t i=1; i<node_count-1; ++i)
    {
        const auto& a = segments[i-1];
        const auto& b = segments[i];
        const auto half_width = nodes[i].half_width;
        const auto cross = a.direction.x * b.direction.y - a.direction.y * b.direction.x;
        const auto dot   = glm::dot(a.direction, b.direction);

        auto& joint = joints[i];
        if (std::abs(cross) < 0.000001f)
        {
            if (dot > 0.0f)
                continue; // the segments are in line
            joint.needed   = true;
            joint.reversal = true;
            continue;
        }
        joint.needed = true;
        // when the line turns left (CCW) the outside of the turn is on the right.
        joint.outer_side = cross > 0.0f ? -1.0f : 1.0f;

        // the inner miter point is on the inner side of the turn at the
        // intersection of the offset lines of the two segments.
        const auto denominator = 1.0f + dot;
        if (denominator > 0.0001f && half_width > 0.0f)
        {
            joint.trimmed       = true;
            joint.trim_distance = half_width * std::abs(cross) / denominator;
            joint.inner_point   = nodes[i].position - joint.outer_side * half_width * (a.normal + b.normal) / denominator;
        }
    }

    // the trimmed segment must be long enough to be trimmed at both ends
    // without the trimmed ends crossing each other. if not then don't
    // trim the segment (at either end). The trimming of an end only
    // ever reduces the used length of a segment so un-trimming joints here
    // cannot invalidate the segments that have been checked already.
    for (size_t i=0; i<segment_count; ++i)
    {
        const auto& start = joints[i];
        const auto& end   = joints[i+1];
        float used = 0.0f;
        if (i > 0 && start.trimmed)
            used += start.trim_distance;
        if (i+1 < node_count-1 && end.trimmed)
            used += end.trim_distance;
        if (used <= segments[i].length)
            continue;
        if (i > 0)
            joints[i].trimmed = false;
        if (i+1 < node_count-1)
            joints[i+1].trimmed = false;
    }

    std::vector<gfx::Vertex2D> vertices;
    vertices.reserve(segment_count * 6 + node_count * 6);

    struct Vertex {
        glm::vec2 position;
        float u = 0.0f;
        float v = 0.0f;
    };
    const auto MakeVertex = [](const glm::vec2& position, float u, float v) {
        Vertex vertex;
        vertex.position = position;
        vertex.u = u;
        vertex.v = v;
        return vertex;
    };
    // add a triangle with the vertices ordered counter-clockwise.
    // degenerate triangles are dropped.
    const auto AddTriangle = [&vertices](Vertex a, Vertex b, Vertex c) {
        const auto signed_area = (b.position.x - a.position.x) * (c.position.y - a.position.y) -
                                 (c.position.x - a.position.x) * (b.position.y - a.position.y);
        if (std::abs(signed_area) < 0.0000000001f)
            return;
        if (signed_area < 0.0f)
            std::swap(b, c);

        for (const auto& vertex : {a, b, c})
        {
            gfx::Vertex2D v;
            v.aPosition = gfx::Vec2 { vertex.position.x, vertex.position.y };
            v.aTexCoord = gfx::Vec2 { vertex.u, vertex.v };
            vertices.push_back(v);
        }
    };

    // add a triangle fan around the center that sweeps from the start
    // offset vector (relative to the center) by the given signed angle.
    // the v texture coordinate is computed by the given function from
    // the unit offset vector.
    const auto AddFan = [&AddTriangle, &MakeVertex](const glm::vec2& center, const glm::vec2& start_offset,
                                                     float angle, unsigned segment_count, float u, auto&& ComputeV) {
        const auto radius = glm::length(start_offset);
        const auto start_direction = start_offset / radius;
        const auto center_vertex = MakeVertex(center, u, 0.5f);

        const auto MakeFanVertex = [&](unsigned i) {
            const auto a = angle * static_cast<float>(i) / static_cast<float>(segment_count);
            const auto c = std::cos(a);
            const auto s = std::sin(a);
            const glm::vec2 direction = {
                start_direction.x * c - start_direction.y * s,
                start_direction.x * s + start_direction.y * c
            };
            return MakeVertex(center + direction * radius, u, ComputeV(direction));
        };

        auto previous = MakeFanVertex(0);
        for (unsigned i=1; i<=segment_count; ++i)
        {
            const auto current = MakeFanVertex(i);
            AddTriangle(center_vertex, previous, current);
            previous = current;
        }
    };

    // the segments
    for (size_t i=0; i<segment_count; ++i)
    {
        const auto& segment = segments[i];
        const auto& start = nodes[i];
        const auto& end   = nodes[i+1];

        // left (v=0) and right (v=1) vertices at both ends of the segment.
        auto start_left  = start.position + segment.normal * start.half_width;
        auto start_right = start.position - segment.normal * start.half_width;
        auto end_left    = end.position   + segment.normal * end.half_width;
        auto end_right   = end.position   - segment.normal * end.half_width;

        // trim the inner side at the joints.
        if (i > 0 && joints[i].trimmed)
        {
            // the inner side is the opposite of the outer side.
            if (joints[i].outer_side > 0.0f)
                start_right = joints[i].inner_point;
            else start_left = joints[i].inner_point;
        }
        if (i+1 < node_count-1 && joints[i+1].trimmed)
        {
            if (joints[i+1].outer_side > 0.0f)
                end_right = joints[i+1].inner_point;
            else end_left = joints[i+1].inner_point;
        }
        const auto sl = MakeVertex(start_left,  start.t, 0.0f);
        const auto sr = MakeVertex(start_right, start.t, 1.0f);
        const auto el = MakeVertex(end_left,    end.t,   0.0f);
        const auto er = MakeVertex(end_right,   end.t,   1.0f);
        AddTriangle(sl, sr, el);
        AddTriangle(sr, er, el);
    }

    // the joints
    for (size_t i=1; i<node_count-1; ++i)
    {
        const auto& joint = joints[i];
        if (!joint.needed)
            continue;

        const auto& node = nodes[i];
        const auto& a = segments[i-1];
        const auto& b = segments[i];
        const auto half_width = node.half_width;
        if (half_width <= 0.0f)
            continue;

        if (joint.reversal)
        {
            // the line turns back on itself. there's no turn direction.
            // only a round joint produces any geometry, a half circle that
            // sweeps around the tip (in the direction of the first segment)
            if (joint_style == gfx::CurveJointStyle::Round)
            {
                AddFan(node.position, a.normal * half_width, -static_cast<float>(math::Pi),
                       2 * RoundCapSegments, node.t, [&a](const glm::vec2& direction) {
                    return 0.5f - 0.5f * glm::dot(direction, a.normal);
                });
            }
            continue;
        }

        const auto side = joint.outer_side;
        // v coordinate for the vertices on the outer side.
        const auto v = side > 0.0f ? 0.0f : 1.0f;

        const auto outer_a = node.position + side * a.normal * half_width;
        const auto outer_b = node.position + side * b.normal * half_width;
        const auto center_vertex = MakeVertex(node.position, node.t, 0.5f);
        const auto vertex_a = MakeVertex(outer_a, node.t, v);
        const auto vertex_b = MakeVertex(outer_b, node.t, v);

        const auto dot   = glm::dot(a.direction, b.direction);
        const auto cross = a.direction.x * b.direction.y - a.direction.y * b.direction.x;

        // when the inner side was trimmed the end edges of the two segment
        // quads run from the inner miter point to the outer offset points
        // so there's a wedge left between the quads and the outer joint
        // (that fans out from the node) that needs to be filled.
        if (joint.trimmed)
        {
            const auto inner_vertex = MakeVertex(joint.inner_point, node.t, v == 0.0f ? 1.0f : 0.0f);
            AddTriangle(inner_vertex, vertex_a, center_vertex);
            AddTriangle(inner_vertex, center_vertex, vertex_b);
        }

        auto style = joint_style;
        if (style == gfx::CurveJointStyle::Miter)
        {
            // the ratio of the miter length to the half width is 1/cos(angle/2)
            // where angle is the angle between the segment directions.
            const auto miter_ratio = std::sqrt(2.0f / (1.0f + dot));
            if (miter_ratio > MiterLimit)
                style = gfx::CurveJointStyle::Bevel;
        }

        if (style == gfx::CurveJointStyle::Bevel)
        {
            AddTriangle(center_vertex, vertex_a, vertex_b);
        }
        else if (style == gfx::CurveJointStyle::Miter)
        {
            const auto miter_point = node.position + side * half_width * (a.normal + b.normal) / (1.0f + dot);
            const auto vertex_m = MakeVertex(miter_point, node.t, v);
            AddTriangle(center_vertex, vertex_a, vertex_m);
            AddTriangle(center_vertex, vertex_m, vertex_b);
        }
        else if (style == gfx::CurveJointStyle::Round)
        {
            // the angle between the normals is the signed turning angle
            // and sweeping the normal that much on the outer side from
            // the first segment to the second goes around the outside.
            const auto turn_angle = std::atan2(cross, dot);
            const auto turn_size  = std::abs(turn_angle);
            const auto segments   = std::max(1u, static_cast<unsigned>(
                    std::ceil(turn_size / (static_cast<float>(math::Pi) * 0.5f) * static_cast<float>(RoundCapSegments))));
            AddFan(node.position, side * a.normal * half_width, turn_angle, segments, node.t,
                   [v](const glm::vec2&) { return v; });
        }
        else BUG("Missing curve joint style handling.");
    }

    // the end caps
    if (end_cap_style != gfx::CurveEndCapStyle::Butt)
    {
        for (int end=0; end<2; ++end)
        {
            const bool is_start = end == 0;
            const auto& node = nodes[is_start ? 0 : node_count-1];
            const auto& segment = segments[is_start ? 0 : segment_count-1];
            const auto half_width = node.half_width;
            if (half_width <= 0.0f)
                continue;

            // the direction pointing out from the line end
            const auto out = is_start ? -segment.direction : segment.direction;

            if (end_cap_style == gfx::CurveEndCapStyle::Square)
            {
                const auto left  = node.position + segment.normal * half_width;
                const auto right = node.position - segment.normal * half_width;
                const auto extension = out * half_width;
                const auto l0 = MakeVertex(left,  node.t, 0.0f);
                const auto r0 = MakeVertex(right, node.t, 1.0f);
                const auto l1 = MakeVertex(left + extension,  node.t, 0.0f);
                const auto r1 = MakeVertex(right + extension, node.t, 1.0f);
                AddTriangle(l0, r0, l1);
                AddTriangle(r0, r1, l1);
            }
            else if (end_cap_style == gfx::CurveEndCapStyle::Round)
            {
                // a half circle that sweeps counter-clockwise around the
                // line end from the left side of the start (or the right
                // side of the end) over the tip.
                const auto start_offset = (is_start ? segment.normal : -segment.normal) * half_width;
                AddFan(node.position, start_offset, static_cast<float>(math::Pi),
                       2 * RoundCapSegments, node.t, [&segment](const glm::vec2& direction) {
                    return 0.5f - 0.5f * glm::dot(direction, segment.normal);
                });
            }
            else BUG("Missing curve end cap style handling.");
        }
    }

    if (vertices.empty())
    {
        // all the triangles were degenerate (zero width line)
        *error = "The polyline has no area. (is the line width zero?)";
        return false;
    }

    buffer->SetVertexBuffer(vertices);
    buffer->SetVertexLayout(gfx::GetVertexLayout<gfx::Vertex2D>());
    buffer->AddDrawCmd(gfx::DrawType::Triangles);
    return true;
}

// Tessellate a Catmull-Rom spline into a triangle strip. The curve passes through
// all the control points. The curve is evaluated at regular intervals and at every
// sample the curve is offset by half the (interpolated) curve width to both sides.
bool TessellateCatmullRomSpline(const gfx::CurveEndCapStyle end_cap_style,
                                const std::vector<gfx::CurvePoint>& points,
                                gfx::GeometryBuffer* buffer,
                                std::string* error)
{
    using CatmullRomFunction = boost::math::catmull_rom<gfx::CurvePoint, 5>;

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
        const auto end_point_position = end_point.GetPosition().ToVec2();
        const auto other_point_position = neighbour.GetPosition().ToVec2();
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

    std::vector<gfx::Vertex2D> vertices;
    vertices.reserve(sample_count * 2 + (round_caps ? 2 * (2 * (RoundCapSegments - 1) + 1) : 0));

    const auto AddVertex = [&vertices](const glm::vec2& position, float u, float v) {
        gfx::Vertex2D vertex;
        vertex.aPosition = gfx::Vec2 { position.x, position.y };
        vertex.aTexCoord = gfx::Vec2 { u, v };
        vertices.push_back(vertex);
    };
    // the round cap vertices are emitted in the reverse order, swap the
    // order of the vertices in each pair to keep the triangles CCW.
    const bool swap_pair_order = round_caps;
    const auto AddVertexPair = [&AddVertex, swap_pair_order](const glm::vec2& left, const glm::vec2& right,
                                                        float u, float v_left, float v_right) {
        if (swap_pair_order)
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

bool ConstructCurve(const gfx::CurveType curve_type,
                    const gfx::CurveJointStyle joint_style,
                    const gfx::CurveEndCapStyle end_cap_style,
                    const std::vector<gfx::CurvePoint>& points,
                    gfx::GeometryBuffer* buffer,
                    std::string* error)
{
    if (curve_type == gfx::CurveType::CatmullRomSpline)
    {
        return TessellateCatmullRomSpline(end_cap_style, points, buffer, error);
    }
    else if (curve_type == gfx::CurveType::PolylineSpline)
    {
        return TessellatePolyline(joint_style, end_cap_style, points, buffer, error);
    }
    else BUG("Missing curve type handling.");
    return false;
}

} // namespace

namespace gfx
{

void NormalizeCurve(std::vector<CurvePoint>& curve, bool flip_y)
{
    if (curve.empty())
        return;

    if (flip_y)
    {
        for (auto& point : curve)
        {
            auto position = point.GetPosition();
            position.y = -position.y;
            point.SetPosition(position);
        }
    }

    // normalize the points into the engine's unit box where x grows from
    // 0.0 to 1.0 to the right and y grows from 0.0 to -1.0 downwards, i.e.
    // [0, 1] x [-1, 0]. the orientation is preserved so that the top of the
    // curve is toward y = 0 and the bottom of the curve toward y = -1.
    //
    // the bounds are computed from the control points only. the point widths
    // are in the normalized model space already (a fraction of the node size)
    // and are left unchanged, so the line thickness, the joints and the end
    // caps can extend outside the unit box.
    //
    // the scaling is uniform, i.e. the aspect ratio of the curve is kept
    // and the longer side of the bounds is fitted to the unit box.
    // the shorter side is centered in the unit box. normalizing each axis
    // separately would stretch a degenerate curve (for example a horizontal
    // line whose bounds have no height) to fill
    // the whole box.
    glm::vec2 min = { std::numeric_limits<float>::max(),    std::numeric_limits<float>::max() };
    glm::vec2 max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    for (const auto& point : curve)
    {
        const auto position = point.GetPosition();
        min.x = std::min(min.x, position.x);
        min.y = std::min(min.y, position.y);
        max.x = std::max(max.x, position.x);
        max.y = std::max(max.y, position.y);
    }
    const auto width  = max.x - min.x;
    const auto height = max.y - min.y;
    const auto scale  = std::max(width, height);
    // a degenerate (zero size) bounds collapses to the
    // center of the box instead of dividing by zero.
    if (!(scale > 0.0f))
    {
        for (auto& point : curve)
            point.SetPosition({0.5f, -0.5f});
        return;
    }
    const auto offset_x = (scale - width)  * 0.5f;
    const auto offset_y = (scale - height) * 0.5f;
    for (auto& point : curve)
    {
        const auto position = point.GetPosition();
        point.SetPosition({(position.x - min.x + offset_x) / scale,
                           (position.y - min.y + offset_y) / scale - 1.0f});
    }
}

void CurvePoint::IntoJson(data::Writer& writer) const
{
    writer.Write("pos-x", mData[0]);
    writer.Write("pos-y", mData[1]);
    writer.Write("data-x", mData[2]);
    writer.Write("data-y", mData[3]);
    writer.Write("width", mData[4]);
}

bool CurvePoint::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("pos-x", &mData[0]);
    ok &= reader.Read("pos-y", &mData[1]);
    ok &= reader.Read("data-x", &mData[2]);
    ok &= reader.Read("data-y", &mData[3]);
    ok &= reader.Read("width", &mData[4]);
    return ok;
}

float GetPointDistance(const CurvePoint& p0, const CurvePoint& p1) noexcept
{
    return glm::length(p1.GetPosition().ToVec2() -
                       p0.GetPosition().ToVec2());
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
    writer.Write("curve-flags", mFlags);

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
    ok &= reader.Read("curve-flags", &mFlags);

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
    return Construct(buffer, base::Float2(1.0f, 1.0f));
}

bool CurveClass::Construct(GeometryBuffer* buffer, const base::Float2& scale) const
{
    const auto points = ScaleCurvePoints(mPoints, glm::vec2(scale.x, scale.y));

    std::string error;
    if (ConstructCurve(mCurveType,
        mJointStyle, mEndCapStyle, points, buffer, &error))
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
void CurveInstance::SetInstanceCurve(CurveArgs&& curve)
{
    if (curve.curve_id.empty())
        ComputeCurveId(curve);

    mInstanceCurve = std::move(curve);
}

bool CurveInstance::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
    Device& device, ProgramState& program, RasterState& raster) const
{
    // the inverse of the isotropic aspect ratio scaling is baked into the model matrix.
    const auto model_matrix = GetCurveModelMatrix(*env.model_matrix, ComputeCurveAspectRatioBucket(env.model_matrix));

    const auto& kModelViewMatrix  = (*env.view_matrix) * model_matrix;
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
    if (mInstanceCurve)
        return GetInstanceGeometry(env, device);

    return GetClassGeometry(env, device);
}
DrawGeometryBuffer CurveInstance::Construct(const Environment& env) const
{
    if (mInstanceCurve)
        return ConstructInstanceBuffer(env);

    return ConstructClassBuffer(env);
}

DrawGeometryHandle CurveInstance::GetInstanceGeometry(const Environment& env, Device& device) const
{
    const auto& curve_args = mInstanceCurve.value();

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
        id = curve_args.curve_id + "/" + std::to_string(ComputeCurveAspectRatioBucket(env.model_matrix));
        name = curve_args.curve_name;
    }

    if (auto geometry = device.FindGeometry(id))
        return geometry;

    auto buffer = Construct(env);
    if (buffer.IsNull())
    {
        if (!curve_args.make_fallback_on_fail)
            return DrawGeometryHandle::Null;

        return DrawGeometryHandle::CreateErrorGeometry(id,
            base::FormatString("Failed to tessellate curve mesh '%1'", curve_args.curve_name),
            base::FormatString("Curve '%1' fallback", curve_args.curve_name),
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

DrawGeometryHandle CurveInstance::GetClassGeometry(const Environment& env, Device& device) const
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
        id = mClass->GetId() + "/" + std::to_string(ComputeCurveAspectRatioBucket(env.model_matrix));
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

DrawGeometryBuffer CurveInstance::ConstructInstanceBuffer(const Environment& env) const
{
    const auto& args = mInstanceCurve.value();

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
        const auto aspect = GetCurveAspectScale(ComputeCurveAspectRatioBucket(env.model_matrix));
        const auto points = ScaleCurvePoints(args.points, aspect);
        if (ConstructCurve(args.curve_type,
                           args.joint_style, args.end_cap_style, points,
                           &buffer, &error))
            return std::move(buffer);

        if (args.make_fallback_on_fail)
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

DrawGeometryBuffer CurveInstance::ConstructClassBuffer(const Environment& env) const
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
        const auto aspect = GetCurveAspectScale(ComputeCurveAspectRatioBucket(env.model_matrix));

        GeometryBuffer buffer;
        if (!mClass->Construct(&buffer, base::Float2(aspect.x, aspect.y)))
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

Curve::Curve(const CurveArgs& args)
    : mArgs(args)
{
    if (mArgs.curve_id.empty())
        ComputeCurveId(mArgs);
}
Curve::Curve(CurveArgs&& args)
    : mArgs(std::move(args))
{
    if (mArgs.curve_id.empty())
        ComputeCurveId(mArgs);
}

bool Curve::ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
        Device& device, ProgramState& program, RasterState& raster) const
{
    // the inverse of the isotropic aspect ratio scaling is baked into the model matrix.
    const auto model_matrix = GetCurveModelMatrix(*env.model_matrix, ComputeCurveAspectRatioBucket(env.model_matrix));

    const auto& kModelViewMatrix  = (*env.view_matrix) * model_matrix;
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
        id = mArgs.curve_id + "/" + std::to_string(ComputeCurveAspectRatioBucket(env.model_matrix));
        name = mArgs.curve_name;
    }

    if (auto geometry = device.FindGeometry(id))
        return geometry;

    auto buffer = Construct(env);
    if (buffer.IsNull())
    {
        if (!mArgs.make_fallback_on_fail)
            return DrawGeometryHandle::Null;

        return DrawGeometryHandle::CreateErrorGeometry(id,
            base::FormatString("Failed to tessellate curve mesh '%1'", mArgs.curve_name),
            base::FormatString("Curve '%1' fallback", mArgs.curve_name),
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
        const auto aspect = GetCurveAspectScale(ComputeCurveAspectRatioBucket(env.model_matrix));
        const auto points = ScaleCurvePoints(mArgs.points, aspect);
        if (ConstructCurve(mArgs.curve_type,
                           mArgs.joint_style, mArgs.end_cap_style, points,
                           &buffer, &error))
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