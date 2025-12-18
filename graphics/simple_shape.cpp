// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/shader_code.h"
#include "graphics/simple_shape.h"
#include "graphics/shader_source.h"
#include "graphics/utility.h"
#include "graphics/vertex.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/geometry_algo.h"

namespace {
float HalfRound(float value)
{
    const float fraction  = value - (int)value;
    const float whole = (int)value;
    if (fraction < 0.25)
        return whole;
    else if (fraction < 0.5)
        return whole + 0.5f;
    else if (fraction < 0.75)
        return whole + 0.5f;
    return whole + 1.0f;
}
int Truncate(float value)
{ return (int)value; }

template<typename RoundFunc>
std::string NameAspectRatio(float width, float height, RoundFunc Round, const char* fmt)
{
    std::string ret;
    ret.resize(64);
    if (width > height)
    {
        const auto q = Round(math::clamp(1.0f, 5.0f, width/height));
        ret.resize(std::snprintf(&ret[0], ret.size(), fmt, q, Round(1.0f)));
    }
    else
    {
        const auto q = Round(math::clamp(1.0f, 5.0f, height/width));
        ret.resize(std::snprintf(&ret[0], ret.size(), fmt, Round(1.0f), q));
    }
    return ret;
}

std::string GetShaderName(gfx::SimpleShapeType type, bool use_instancing, gfx::Drawable::MeshType mesh_type)
{
    std::string name;
    name += "Simple";
    if (use_instancing)
        name += "Instanced";

    if (mesh_type == gfx::Drawable::MeshType::NormalRenderMesh)
        name += "RenderMesh";
    else if (mesh_type == gfx::Drawable::MeshType::ShardedEffectMesh)
        name += "ShardEffectMesh";
    else BUG("Unhandled mesh type.");

    name += (Is3DShape(type) ? "3D" : "2D");
    name += "VertexShader";
    return name;
}

std::string GetShaderId(gfx::SimpleShapeType type, bool use_instancing, gfx::Drawable::MeshType mesh_type)
{
    std::string id;
    id += "simple-";
    if (use_instancing)
        id += "instanced-";

    if (mesh_type == gfx::Drawable::MeshType::NormalRenderMesh)
        id += "render-mesh-";
    else if (mesh_type == gfx::Drawable::MeshType::ShardedEffectMesh)
        id += "shard-effect-mesh-";
    else BUG("Unhandled mesh type.");

    id += (Is3DShape(type) ? "3D-" : "2D-");
    id += "vertex-shader";
    return id;
}

} // namespace

namespace gfx {
namespace detail {

void ArrowGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    if (style == Style::Outline)
    {
        const Vertex2D verts[] = {
            {{0.0f, -0.25f}, {0.0f, 0.25f}},
            {{0.0f, -0.75f}, {0.0f, 0.75f}},
            {{0.7f, -0.75f}, {0.7f, 0.75f}},
            {{0.7f, -1.0f},  {0.7f, 1.0f}},
            {{1.0f, -0.5f},  {1.0f, 0.5f}},
            {{0.7f, -0.0f},  {0.7f, 0.0f}},
            {{0.7f, -0.25f}, {0.7f, 0.25f}},
        };
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.SetVertexBuffer(verts, 7);
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    else if (style == Style::Solid)
    {
        const Vertex2D verts[] = {
            // body
            {{0.0f, -0.25f}, {0.0f, 0.25f}},
            {{0.0f, -0.75f}, {0.0f, 0.75f}},
            {{0.7f, -0.25f}, {0.7f, 0.25f}},
            // body
            {{0.7f, -0.25f}, {0.7f, 0.25f}},
            {{0.0f, -0.75f}, {0.0f, 0.75f}},
            {{0.7f, -0.75f}, {0.7f, 0.75f}},

            // arrow head
            {{0.7f, -0.0f}, {0.7f, 0.0f}},
            {{0.7f, -1.0f}, {0.7f, 1.0f}},
            {{1.0f, -0.5f}, {1.0f, 0.5f}},
        };
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.SetVertexBuffer(verts, 9);
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void StaticLineGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    // horizontal line.
    constexpr const gfx::Vertex2D verts[2] = {
        {{0.0f,  -0.5f}, {0.0f, 0.5f}},
        {{1.0f,  -0.5f}, {1.0f, 0.5f}}
    };
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.SetVertexBuffer(verts, 2);
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
}

// static
void CapsuleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    // todo LOD information
    const auto slices = 50;
    const auto radius = 0.25f;
    const auto max_slice = style == Style::Solid ? slices + 1 : slices;
    const auto angle_increment = math::Pi / slices;

    // try to figure out if the model matrix will distort the
    // round rectangle out of it's square shape which would then
    // distort the rounded corners out of the shape too.
    const auto& model_matrix = *env.model_matrix;
    const auto rect_width   = glm::length(model_matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    const auto rect_height  = glm::length(model_matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    const auto aspect_ratio = rect_width / rect_height;
    float w = radius;
    float h = radius;
    if (rect_width > rect_height)
        w = h / (rect_width/rect_height);
    else h = w / (rect_height/rect_width);

    std::vector<Vertex2D> vs;
    auto offset = 0;

    // semi-circle at the left end.
    Vertex2D left_center;
    left_center.aPosition.x =  w;
    left_center.aPosition.y = -0.5f;
    left_center.aTexCoord.x =  w;
    left_center.aTexCoord.y =  0.5f;
    if (style == Style::Solid)
        vs.push_back(left_center);

    float left_angle = math::Pi * 0.5;
    for (unsigned i=0; i<max_slice; ++i)
    {
        const auto x = std::cos(left_angle) * w;
        const auto y = std::sin(left_angle) * h;
        Vertex2D v;
        v.aPosition.x =  w + x;
        v.aPosition.y = -0.5f + y;
        v.aTexCoord.x =  w + x;
        v.aTexCoord.y =  0.5f - y;
        vs.push_back(v);

        left_angle += angle_increment;
    }
    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size()-offset);

    if (style != Style::Outline)
    {
        // center box.
        const Vertex2D box[6] = {
            {{     w, -0.5f+h}, {     w, 0.5f-h}},
            {{     w, -0.5f-h}, {     w, 0.5f+h}},
            {{1.0f-w, -0.5f-h}, {1.0f-w, 0.5f+h}},

            {{     w, -0.5f+h}, {     w, 0.5f-h}},
            {{1.0f-w, -0.5f-h}, {1.0f-w, 0.5f+h}},
            {{1.0f-w, -0.5f+h}, {1.0f-w, 0.5f-h}}
        };
        offset = vs.size();
        vs.push_back(box[0]);
        vs.push_back(box[1]);
        vs.push_back(box[2]);
        vs.push_back(box[3]);
        vs.push_back(box[4]);
        vs.push_back(box[5]);
        if (style == Style::Solid)
        {
            geometry.AddDrawCmd(Geometry::DrawType::Triangles, offset, 6);
        }
        else
        {
            geometry.AddDrawCmd(Geometry::DrawType::LineLoop, offset + 0, 3);
            geometry.AddDrawCmd(Geometry::DrawType::LineLoop, offset + 3, 3);
        }
    }

    offset = vs.size();

    // semi circle at the right end
    Vertex2D right_center;
    right_center.aPosition.x =  1.0f - w;
    right_center.aPosition.y = -0.5f;
    right_center.aTexCoord.x =  1.0f - w;
    right_center.aTexCoord.y =  0.5f;
    if (style == Style::Solid)
        vs.push_back(right_center);

    const float right_angle_increment = math::Pi / slices;
    float right_angle = math::Pi * -0.5;
    for (unsigned i=0; i<max_slice; ++i)
    {
        const auto x = std::cos(right_angle) * w;
        const auto y = std::sin(right_angle) * h;
        Vertex2D v;
        v.aPosition.x =  1.0f - w + x;
        v.aPosition.y = -0.5f + y;
        v.aTexCoord.x =  1.0f - w + x;
        v.aTexCoord.y =  0.5f - y;
        vs.push_back(v);

        right_angle += right_angle_increment;
    }
    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size()-offset);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);

    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.SetVertexBuffer(std::move(vs));
}

// static
void SemiCircleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 50;

    std::vector<Vertex2D> vs;

    // center point for triangle fan.
    Vertex2D center;
    center.aPosition.x =  0.5;
    center.aPosition.y = -0.5;
    center.aTexCoord.x =  0.5;
    center.aTexCoord.y =  0.5;
    if (style == Style::Solid)
    {
        vs.push_back(center);
    }

    const float angle_increment = (float)math::Pi / slices;
    const auto max_slice = slices + 1;
    float angle = 0.0f;

    for (unsigned i=0; i<max_slice; ++i)
    {
        const auto x = std::cos(angle) * 0.5f;
        const auto y = std::sin(angle) * 0.5f;
        Vertex2D v;
        v.aPosition.x = x + 0.5f;
        v.aPosition.y = y - 0.5f;
        v.aTexCoord.x = x + 0.5f;
        v.aTexCoord.y = 1.0 - (y + 0.5f);
        vs.push_back(v);

        angle += angle_increment;
    }
    geometry.SetVertexBuffer(&vs[0], vs.size());
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}

// static
void CircleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 100;

    std::vector<Vertex2D> vs;

    // center point for triangle fan.
    Vertex2D center;
    center.aPosition.x =  0.5;
    center.aPosition.y = -0.5;
    center.aTexCoord.x = 0.5;
    center.aTexCoord.y = 0.5;
    if (style == Style::Solid)
    {
        vs.push_back(center);
    }

    const float angle_increment = (float)(math::Pi * 2.0f) / slices;
    float angle = 0.0f;

    for (unsigned i=0; i<=slices; ++i)
    {
        const auto x = std::cos(angle) * 0.5f;
        const auto y = std::sin(angle) * 0.5f;
        Vertex2D v;
        v.aPosition.x = x + 0.5f;
        v.aPosition.y = y - 0.5f;
        v.aTexCoord.x = x + 0.5f;
        v.aTexCoord.y = 1.0 - (y + 0.5f);
        vs.push_back(v);

        angle += angle_increment;
    }
    geometry.SetVertexBuffer(&vs[0], vs.size());
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}

// static
void RectangleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    if (style == Style::Outline)
    {
        const Vertex2D verts[] = {
            { {0.0f,  0.0f}, {0.0f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} },
            { {1.0f,  0.0f}, {1.0f, 0.0f} }
        };
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.SetVertexBuffer(verts, 4);
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    else if (style == Style::Solid )
    {
        const Vertex2D verts[6] = {
            { {0.0f,  0.0f}, {0.0f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} },

            { {0.0f,  0.0f}, {0.0f, 0.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} },
            { {1.0f,  0.0f}, {1.0f, 0.0f} }
        };
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.SetVertexBuffer(verts, 6);
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void IsoscelesTriangleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    const Vertex2D verts[3] = {
        { {0.5f,  0.0f}, {0.5f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} }
    };

    geometry.SetVertexBuffer(verts, 3);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
}

// static
void RightTriangleGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, const RightTriangleArgs& args)
{
    const Vertex2D bottom_left[3] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} }
    };
    const Vertex2D bottom_right[3] = {
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} },
        { {1.0f,  0.0f}, {1.0f, 0.0f} }
    };
    const Vertex2D top_left[3] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f,  0.0f}, {1.0f, 0.0f} }
    };
    const Vertex2D top_right[3] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} },
        { {1.0f,  0.0f}, {1.0f, 0.0f} }
    };

    if (args.corner == RightTriangleArgs::Corner::BottomLeft)
        geometry.SetVertexBuffer(bottom_left, 3);
    else if (args.corner == RightTriangleArgs::Corner::BottomRight)
        geometry.SetVertexBuffer(bottom_right, 3);
    else if (args.corner == RightTriangleArgs::Corner::TopLeft)
        geometry.SetVertexBuffer(top_left, 3);
    else if (args.corner == RightTriangleArgs::Corner::TopRight)
        geometry.SetVertexBuffer(top_right, 3);
    else BUG("No such right triangle geometry");

    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
}

// static
void TrapezoidGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    if (style == Style::Outline)
    {
        const Vertex2D verts[] = {
            { {0.2f,  0.0f}, {0.2f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} },
            { {0.8f,  0.0f}, {0.8f, 0.0f} }
        };
        geometry.SetVertexBuffer(verts, 4);
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    else if (style == Style::Solid)
    {
        const Vertex2D verts[] = {
            {{0.2f,  0.0f}, {0.2f, 0.0f}},
            {{0.0f, -1.0f}, {0.0f, 1.0f}},
            {{0.2f, -1.0f}, {0.2f, 1.0f}},

            {{0.2f,  0.0f}, {0.2f, 0.0f}},
            {{0.2f, -1.0f}, {0.2f, 1.0f}},
            {{0.8f, -1.0f}, {0.8f, 1.0f}},

            {{0.8f, -1.0f}, {0.8f, 1.0f}},
            {{0.8f,  0.0f}, {0.8f, 0.0f}},
            {{0.2f,  0.0f}, {0.2f, 0.0f}},

            {{0.8f,  0.0f}, {0.8f, 0.0f}},
            {{0.8f, -1.0f}, {0.8f, 1.0f}},
            {{1.0f, -1.0f}, {1.0f, 1.0f}}
        };
        geometry.SetVertexBuffer(verts, 12);
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void ParallelogramGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    if (style == Style::Outline)
    {
        const Vertex2D verts[] = {
            { {0.2f,  0.0f}, {0.2f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {0.8f, -1.0f}, {0.8f, 1.0f} },
            { {1.0f,  0.0f}, {1.0f, 0.0f} }
        };
        geometry.SetVertexBuffer(verts, 4);
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    else if (style == Style::Solid)
    {
        const Vertex2D verts[] = {
            {{0.2f,  0.0f}, {0.2f, 0.0f}},
            {{0.0f, -1.0f}, {0.0f, 1.0f}},
            {{0.8f, -1.0f}, {0.8f, 1.0f}},

            {{0.8f, -1.0f}, {0.8f, 1.0f}},
            {{1.0f,  0.0f}, {1.0f, 0.0f}},
            {{0.2f,  0.0f}, {0.2f, 0.0f}}
        };
        geometry.SetVertexBuffer(verts, 6);
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void SectorGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, float fill_percentage)
{
    std::vector<Vertex2D> vs;

    // center point for triangle fan.
    Vertex2D center;
    center.aPosition.x =  0.5;
    center.aPosition.y = -0.5;
    center.aTexCoord.x =  0.5;
    center.aTexCoord.y =  0.5;
    if (style == Style::Solid || style == Style::Outline)
    {
        vs.push_back(center);
    }
    const auto slices    = 100 * fill_percentage;
    const auto angle_max = math::Pi * 2.0 * fill_percentage;
    const auto angle_inc = angle_max / slices;
    const auto max_slice = slices + 1;

    float angle = 0.0f;
    for (unsigned i=0; i<max_slice; ++i)
    {
        const auto x = std::cos(angle) * 0.5f;
        const auto y = std::sin(angle) * 0.5f;
        Vertex2D v;
        v.aPosition.x = x + 0.5f;
        v.aPosition.y = y - 0.5f;
        v.aTexCoord.x = x + 0.5f;
        v.aTexCoord.y = 1.0 - (y + 0.5f);
        vs.push_back(v);

        angle += angle_inc;
    }
    geometry.SetVertexBuffer(&vs[0], vs.size());
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}


// static
void RoundRectGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, float corner_radius)
{

    // try to figure out if the model matrix will distort the
    // round rectangle out of it's square shape which would then
    // distort the rounded corners out of the shape too.
    const auto& model_matrix = *env.model_matrix;
    const auto rect_width  = glm::length(model_matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    const auto rect_height = glm::length(model_matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    const auto aspect_ratio = rect_width / rect_height;
    float w = corner_radius;
    float h = corner_radius;
    if (rect_width > rect_height)
        w = h / (rect_width/rect_height);
    else h = w / (rect_height/rect_width);

    const auto slices    = 20;
    const auto increment = (float)(math::Pi * 0.5  / slices); // each corner is a quarter circle, i.e. half pi rad

    // each corner contains one quadrant of a circle with radius r
    struct CornerOrigin {
        // x, y pos of the origin
        float x, y;
    } corners[4] = {
        {1.0f-w,      -h}, // top right
        {     w,      -h}, // top left
        {     w, -1.0f+h}, // bottom left
        {1.0f-w, -1.0f+h}, // bottom right
    };

    if (style == Style::Outline)
    {

        // outline of the box body
        std::vector<Vertex2D> vs = {
            // left box
            {{0.0f,      -h}, {0.0f,      h}},
            {{0.0f, -1.0f+h}, {0.0f, 1.0f-h}},
            // center box
            {{w,       0.0f}, {w,      0.0f}},
            {{1.0f-w,  0.0f}, {1.0f-w, 0.0f}},
            {{w,      -1.0f}, {w,      1.0f}},
            {{1.0f-w, -1.0f}, {w,      1.0f}},
            // right box
            {{1.0f,      -h}, {1.0f,        h}},
            {{1.0f, -1.0f+h}, {1.0f,   1.0f-h}},
        };

        // generate corners
        for (int i=0; i<4; ++i)
        {
            float angle = math::Pi * 0.5 * i;
            for (unsigned s = 0; s<=slices; ++s)
            {
                const auto x0 = std::cos(angle) * w;
                const auto y0 = std::sin(angle) * h;
                Vertex2D v0, v1;
                v0.aPosition.x = corners[i].x + x0;
                v0.aPosition.y = corners[i].y + y0;
                v0.aTexCoord.x = corners[i].x + x0;
                v0.aTexCoord.y = (corners[i].y + y0) * -1.0f;

                angle += increment;

                const auto x1 = std::cos(angle) * w;
                const auto y1 = std::sin(angle) * h;
                v1.aPosition.x = corners[i].x + x1;
                v1.aPosition.y = corners[i].y + y1;
                v1.aTexCoord.x = corners[i].x + x1;
                v1.aTexCoord.y = (corners[i].y + y1) * -1.0f;

                vs.push_back(v0);
                vs.push_back(v1);
            }
        }
        geometry.SetVertexBuffer(std::move(vs));
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
        geometry.AddDrawCmd(Geometry::DrawType::Lines);
    }
    else if (style == Style::Solid)
    {
        // center body
        std::vector<Vertex2D> vs = {
            // left box
            {{0.0f,      -h}, {0.0f,      h}},
            {{0.0f, -1.0f+h}, {0.0f, 1.0f-h}},
            {{w,    -1.0f+h}, {w,    1.0f-h}},
            {{w,    -1.0f+h}, {w,    1.0f-h}},
            {{w,         -h}, {w,         h}},
            {{0.0f,      -h}, {0.0f,      h}},

            // center box
            {{w,       0.0f}, {w,      0.0f}},
            {{w,      -1.0f}, {w,      1.0f}},
            {{1.0f-w, -1.0f}, {1.0f-w, 1.0f}},
            {{1.0f-w, -1.0f}, {1.0f-w, 1.0f}},
            {{1.0f-w,  0.0f}, {1.0f-w, 0.0f}},
            {{w,       0.0f}, {w,      0.0f}},

            // right box.
            {{1.0f-w,       -h}, {1.0f-w,      h}},
            {{1.0f-w,  -1.0f+h}, {1.0f-w, 1.0f-h}},
            {{1.0f,    -1.0f+h}, {1.0f,   1.0f-h}},
            {{1.0f,    -1.0f+h}, {1.0f,   1.0f-h}},
            {{1.0f,         -h}, {1.0f,        h}},
            {{1.0f-w,       -h}, {1.0f-w,      h}},
        };

        geometry.AddDrawCmd(Geometry::DrawType::Triangles, 0, 18); // body

        // generate corners
        for (int i=0; i<4; ++i)
        {
            const auto offset = vs.size();

            Vertex2D center;
            center.aPosition.x = corners[i].x;
            center.aPosition.y = corners[i].y;
            center.aTexCoord.x = corners[i].x;
            center.aTexCoord.y = corners[i].y * -1.0f;

            if (style == Style::Solid)
            {
                // triangle fan center point
                vs.push_back(center);
            }

            float angle = math::Pi * 0.5 * i;
            for (unsigned s = 0; s<=slices; ++s)
            {
                const auto x = std::cos(angle) * w;
                const auto y = std::sin(angle) * h;
                Vertex2D v;
                v.aPosition.x = corners[i].x + x;
                v.aPosition.y = corners[i].y + y;
                v.aTexCoord.x = corners[i].x + x;
                v.aTexCoord.y = (corners[i].y + y) * -1.0f;
                vs.push_back(v);

                angle += increment;
            }
            geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size() - offset); // corners
        }
        geometry.SetVertexBuffer(std::move(vs));
        geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    }
}


// static
void ArrowCursorGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    const Vertex2D verts[] = {
        {{0.0f,  0.0f}, {0.0f, 0.0f}},
        {{0.0f, -0.6f}, {0.0f, 0.6f}},
        {{0.6f, 0.0f}, {0.6f, 0.0f}},

        {{0.3f, 0.0f}, {0.3f, 0.0f}},
        {{0.0f, -0.3f}, {0.0f, 0.3f}},
        {{0.7f, -1.0f}, {0.7f, 1.0f}},

        {{0.3f, 0.0f}, {0.3f, 0.0f}},
        {{0.7f, -1.0f}, {0.7f, 1.0f}},
        {{1.0f, -0.7f}, {1.0f, 0.7f}}
    };
    geometry.SetVertexBuffer(verts, 9);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
}

// static
void BlockCursorGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    const Vertex2D verts[] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} },

        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} },
        { {1.0f,  0.0f}, {1.0f, 0.0f} }
    };
    geometry.SetVertexBuffer(verts, 6);
    geometry.SetVertexLayout(GetVertexLayout<Vertex2D>());
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
}

// static
void CubeGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    // all corners of the cube.
    constexpr const Vec3 FrontTopLeft =  {-0.5f,  0.5f,  0.5f};
    constexpr const Vec3 FrontBotLeft =  {-0.5f, -0.5f,  0.5f};
    constexpr const Vec3 FrontBotRight = { 0.5f, -0.5f,  0.5f};
    constexpr const Vec3 FrontTopRight = { 0.5f,  0.5f,  0.5f};
    constexpr const Vec3 BackTopLeft   = {-0.5f,  0.5f, -0.5f};
    constexpr const Vec3 BackBotLeft   = {-0.5f, -0.5f, -0.5f};
    constexpr const Vec3 BackBotRight  = { 0.5f, -0.5f, -0.5f};
    constexpr const Vec3 BackTopRight  = { 0.5f,  0.5f, -0.5f};


    Vertex3D vertices[4 * 6];
    Index16 indices[6 * 6];

    // front face
    MakeFace(0, &indices[0], &vertices[0], FrontTopLeft, FrontBotLeft, FrontBotRight, FrontTopRight, Vec3 {0.0, 0.0, 1.0} );
    // left face
    MakeFace(4, &indices[6], &vertices[4], BackTopLeft, BackBotLeft, FrontBotLeft, FrontTopLeft, Vec3 {-1.0, 0.0, 0.0});
    // right face
    MakeFace(8, &indices[12], &vertices[8], FrontTopRight, FrontBotRight, BackBotRight, BackTopRight, Vec3 {1.0, 0.0, 0.0});
    // top face
    MakeFace(12, &indices[18], &vertices[12], BackTopLeft, FrontTopLeft, FrontTopRight, BackTopRight, Vec3 {0.0, 1.0, 0.0});
    // bottom face
    MakeFace(16, &indices[24], &vertices[16], FrontBotLeft, BackBotLeft, BackBotRight, FrontBotRight, Vec3{0.0, -1.0, 0.0});
    // back face
    MakeFace(20, &indices[30], &vertices[20], BackTopRight, BackBotRight, BackBotLeft, BackTopLeft, Vec3{0.0, 0.0, -1.0});

    geometry.UploadVertices(vertices, sizeof(vertices));
    geometry.UploadIndices(indices, sizeof(indices), Geometry::IndexType::Index16);
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);

}

// static
void CubeGeometry::MakeFace(size_t vertex_offset, Index16* indices, Vertex3D* vertices,
                            const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
                            const Vec3& normal)
{
    constexpr const Vec2 TexBotLeft  = {0.0, 1.0};
    constexpr const Vec2 TexTopLeft  = {0.0, 0.0};
    constexpr const Vec2 TexTopRight = {1.0, 0.0};
    constexpr const Vec2 TexBotRight = {1.0, 1.0};

    vertices[0].aPosition = v0;
    vertices[1].aPosition = v1;
    vertices[2].aPosition = v2;
    vertices[3].aPosition = v3;
    vertices[0].aTexCoord = TexTopLeft;
    vertices[1].aTexCoord = TexBotLeft;
    vertices[2].aTexCoord = TexBotRight;
    vertices[3].aTexCoord = TexTopRight;
    vertices[0].aNormal   = normal;
    vertices[1].aNormal   = normal;
    vertices[2].aNormal   = normal;
    vertices[3].aNormal   = normal;

    indices[0] = gfx::Index16(vertex_offset + 0);
    indices[1] = gfx::Index16(vertex_offset + 1);
    indices[2] = gfx::Index16(vertex_offset + 2);
    indices[3] = gfx::Index16(vertex_offset + 2);
    indices[4] = gfx::Index16(vertex_offset + 3);
    indices[5] = gfx::Index16(vertex_offset + 0);
}

// static
void CubeGeometry::AddLine(const Vec3& v0, const Vec3& v1, std::vector<Vertex3D>& vertex)
{
    Vertex3D a;
    a.aPosition = v0;

    Vertex3D b;
    b.aPosition = v1;

    vertex.push_back(a);
    vertex.push_back(b);
}

// static
void CylinderGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned slices)
{
    const auto vertex_count = slices + 1;

    std::vector<Vertex3D> vertices;
    std::vector<Index16> indices;

    for (unsigned i=0; i<slices; ++i)
    {
        const float increment = math::Circle / slices;
        const float angle = i * increment;

        // multiplying by a negative number has winding order significance
        const auto x = std::cos(angle) *  0.5f;
        const auto z = std::sin(angle) * -0.5f;
        const auto normal = glm::normalize(glm::vec3(x, 0.0f, z));

        const auto dist = angle / math::Circle;

        Vertex3D top;
        top.aPosition.x = x;
        top.aPosition.y = 0.5f;
        top.aPosition.z = z;
        top.aNormal.x = normal.x;
        top.aNormal.y = normal.y;
        top.aNormal.z = normal.z;
        top.aTexCoord.x = dist;
        top.aTexCoord.y = 0.0f;

        Vertex3D bottom;
        bottom.aPosition.x = x;
        bottom.aPosition.y = -0.5f;
        bottom.aPosition.z = z;
        bottom.aNormal.x = normal.x;
        bottom.aNormal.y = normal.y;
        bottom.aNormal.z = normal.z;
        bottom.aTexCoord.x = dist;
        bottom.aTexCoord.y = 1.0f;

        vertices.push_back(top);
        vertices.push_back(bottom);
    }

    const auto body_start = indices.size();

    // note the -1 here so that we don't go out of bounds on the vertex vector.
    for (unsigned i=0; i<slices-1; ++i)
    {
        const auto slice = i * 2;
        ASSERT(slice+3 < vertices.size());

        indices.push_back(slice+0);
        indices.push_back(slice+1);
        indices.push_back(slice+2);

        indices.push_back(slice+2);
        indices.push_back(slice+1);
        indices.push_back(slice+3);
    }
    //the last slice wraps over
    indices.push_back((slices-1)*2 + 0);
    indices.push_back((slices-1)*2 + 1);
    indices.push_back(0);
    indices.push_back(0);
    indices.push_back((slices-1)*2 + 1);
    indices.push_back(1);

    const auto body_count = indices.size() - body_start;
    const auto top_start = indices.size();
    {

        Vertex3D top;
        top.aPosition = Vec3 { 0.0f, 0.5f, 0.0f };
        top.aNormal   = Vec3 { 0.0f, 1.0f, 0.0f };
        top.aTexCoord = Vec2 {0.5f, 0.5f};
        vertices.push_back(top);
        indices.push_back(vertices.size()-1);

        for (unsigned i = 0; i < vertex_count; ++i)
        {
            const float increment = math::Circle / slices;
            const float vertex_angle = i * increment;
            const float texture_angle = i * increment; // + math::Pi;

            // multiplying by a negative number has winding order significance
            const float x = std::cos(vertex_angle) *  0.5f;
            const float z = std::sin(vertex_angle) * -0.5f;

            const float tx = std::cos(texture_angle) *  0.5f;
            const float ty = std::sin(texture_angle) * -0.5f;

            Vertex3D vertex;
            vertex.aPosition = Vec3 { x, 0.5f, z };
            vertex.aNormal   = Vec3 { 0.0f, 1.0f, 0.0f };
            vertex.aTexCoord = Vec2 { 0.5f + tx, 0.5f + ty };
            vertices.push_back(vertex);
            indices.push_back(vertices.size()-1);
        }
    }
    const auto top_count = indices.size() - top_start;
    const auto bot_start = indices.size();
    {
        Vertex3D bottom;
        bottom.aPosition = Vec3 { 0.0f, -0.5f, 0.0f };
        bottom.aNormal   = Vec3 { 0.0f, -1.0f, 0.0f };
        bottom.aTexCoord = Vec2 {0.5f, 0.5f};
        vertices.push_back(bottom);
        indices.push_back(vertices.size()-1);

        for (unsigned i = 0; i < vertex_count; ++i)
        {
            const float increment = math::Circle / slices;
            const float vertex_angle = i * increment;
            const float texture_angle = i * increment; // + math::Pi;

            // multiplying by a negative number has winding order significance
            const float x = std::cos(vertex_angle) *  0.5f;
            const float z = std::sin(vertex_angle) *  0.5f; // -0.5f
            const auto normal = glm::normalize(glm::vec3{x, 0.0f, z});

            const float tx = std::cos(texture_angle) *  0.5f;
            const float ty = std::sin(texture_angle) *  0.5f; // -0.5f;

            Vertex3D vertex;
            vertex.aPosition = Vec3 { x, -0.5f, z };
            vertex.aNormal   = Vec3 { 0.0f, -1.0f, 0.0f };
            vertex.aTexCoord = Vec2 { 0.5f + tx, 0.5f + ty };
            vertices.push_back(vertex);
            indices.push_back(vertices.size()-1);
        }
    }
    const auto bot_count = indices.size() - bot_start;

    geometry.SetVertexBuffer(std::move(vertices));
    geometry.SetIndexBuffer(std::move(indices));
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::Triangles, body_start, body_count);
    geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, top_start, top_count);
    geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, bot_start, bot_count);
}

// static
void ConeGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned int slices)
{
    const auto vertex_count = slices + 1;

    std::vector<Vertex3D> vertices;

    Vertex3D apex;
    apex.aPosition = Vec3{0.0f, 0.5f, 0.0f};
    apex.aNormal   = Vec3{0.0f, 1.0f, 0.0f};
    apex.aTexCoord = Vec2{0.5f, 0.5f};
    vertices.push_back(apex);

    for (unsigned i=0; i<vertex_count; ++i)
    {
        const auto angle_increment = math::Circle / slices;
        const auto vertex_start_angle = angle_increment * -0.5f;
        const auto vertex_angle = vertex_start_angle + angle_increment * i;
        const auto texture_angle = angle_increment * i;

        // multiplying by a negative number has winding order significance
        const float x = std::cos(vertex_angle) *  0.5f;
        const float z = std::sin(vertex_angle) * -0.5f;

        const auto position = glm::vec3 { std::cos(vertex_angle) *  0.5f, -0.5f,
                                          std::sin(vertex_angle) * -0.5f };

        const auto next = glm::vec3 { std::cos(vertex_angle + angle_increment) *  0.5f, -0.5f,
                                      std::sin(vertex_angle + angle_increment) * -0.5f };
        const auto apex = glm::vec3 {0.0f, 0.5f, 0.0f };

        const auto to_apex = glm::normalize(apex - position);
        const auto to_next = glm::normalize(next - position);
        const auto normal = glm::normalize(glm::cross(to_next, to_apex));

        const float tx = std::cos(texture_angle) *  0.5f;
        const float ty = std::sin(texture_angle) * -0.5f;

        Vertex3D vertex;
        vertex.aPosition = ToVec(position);
        vertex.aNormal   = ToVec(normal);
        vertex.aTexCoord = Vec2{ 0.5f + tx, 0.5f + ty };
        vertices.push_back(vertex);
    }

    const auto cone_start   = 0;
    const auto cone_count   = vertices.size();
    const auto bottom_start = vertices.size();

    Vertex3D bottom;
    bottom.aPosition = Vec3{0.0f, -0.5f, 0.0f};
    bottom.aNormal   = Vec3{0.0f, -1.0f, 0.0f};
    bottom.aTexCoord = Vec2{0.5f, 0.5f};
    vertices.push_back(bottom);

    for (unsigned i=0; i<vertex_count; ++i)
    {
        const auto angle_increment = math::Circle / slices;
        const auto vertex_start_angle = angle_increment * -0.5f;
        const auto vertex_angle = vertex_start_angle + angle_increment * i;
        const auto texture_angle = angle_increment * i;

        // multiplying by a negative number has winding order significance
        const float x = std::cos(vertex_angle) * 0.5f;
        const float z = std::sin(vertex_angle) * 0.5f; //-0.5f;

        const float tx = std::cos(texture_angle) * 0.5f;
        const float ty = std::sin(texture_angle) * 0.5f; //-0.5f;

        Vertex3D vertex;
        vertex.aPosition = Vec3 { x, -0.5f, z };
        vertex.aNormal   = Vec3 { 0.0f, -1.0f, 0.0f };
        vertex.aTexCoord = Vec2 { 0.5f + tx, 0.5f + ty };
        vertices.push_back(vertex);
    }

    const auto bottom_count = vertices.size() - bottom_start;

    geometry.SetVertexBuffer(std::move(vertices));
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, cone_start, cone_count);
    geometry.AddDrawCmd(Geometry::DrawType::TriangleFan, bottom_start, bottom_count);
}

// static
void SphereGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned int slices)
{
    const float radius     = 0.5f;
    const int numSlices    = slices;
    const int numParallels = slices / 2;
    const int numVertices  = (numParallels + 1) * (slices + 1);
    const int numIndices   = numParallels * slices * 6;
    const float angleStep  = math::Circle / slices;

    std::vector<Vertex3D> vertices;
    std::vector<Index16> indices;

    for (int i=0; i<numParallels+1; ++i)
    {
        for (int j=0; j<numSlices+1; ++j)
        {
            glm::vec3 position;
            position.x = radius * sinf (angleStep * (float)i) *
                         sinf (angleStep * (float)j);
            position.y = radius * cosf (angleStep * (float)i);
            position.z = radius * sinf (angleStep * (float)i) *
                         cosf (angleStep * (float)j);

            glm::vec3 normal;
            normal = position / radius;

            glm::vec2 texcoord;
            texcoord.x = (float)j / (float)numSlices;
            texcoord.y = (float)i / (float)(numParallels - 1);

            Vertex3D vertex;
            vertex.aPosition = ToVec(position);
            vertex.aNormal   = ToVec(normal);
            vertex.aTexCoord = ToVec(texcoord);
            vertices.push_back(std::move(vertex));
        }
    }

    // generate indices
    for (int i=0; i<numParallels; ++i)
    {
        for (int j=0; j<numSlices; ++j)
        {
            indices.push_back( (i + 0) * (numSlices + 1) + (j + 0) );
            indices.push_back( (i + 1) * (numSlices + 1) + (j + 0) );
            indices.push_back( (i + 1) * (numSlices + 1) + (j + 1) );

            indices.push_back( (i + 0) * (numSlices + 1 ) + (j + 0) );
            indices.push_back( (i + 1) * (numSlices + 1 ) + (j + 1) );
            indices.push_back( (i + 0) * (numSlices + 1 ) + (j + 1) );
        }
    }

    geometry.SetVertexBuffer(std::move(vertices));
    geometry.SetIndexBuffer(std::move(indices));
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
}

// static
void PyramidGeometry::Generate(const Environment& env, Style style, GeometryBuffer& geometry)
{
    Vertex3D apex;
    apex.aPosition = Vec3 {0.0f, 0.5f, 0.0f};
    apex.aNormal   = Vec3 {0.0f, 1.0f, 0.0f};
    apex.aTexCoord = Vec2 {0.5f, 0.5f};

    Vertex3D base[4];
    base[0].aPosition = Vec3{-0.5f, -0.5f,  0.5f};
    base[0].aTexCoord = Vec2{0.0f, 1.0f};
    base[1].aPosition = Vec3{ 0.5f, -0.5f,  0.5f};
    base[1].aTexCoord = Vec2{1.0f, 1.0f};
    base[2].aPosition = Vec3{ 0.5f, -0.5f, -0.5f};
    base[2].aTexCoord = Vec2{1.0f, 0.0f};
    base[3].aPosition = Vec3{-0.5f, -0.5f, -0.5f};
    base[3].aTexCoord = Vec2{0.0f, 0.0f};

    std::vector<Vertex3D> verts;
    MakeFace(verts, apex, base[0], base[1]);
    MakeFace(verts, apex, base[1], base[2]);
    MakeFace(verts, apex, base[2], base[3]);
    MakeFace(verts, apex, base[3], base[0]);
    MakeFace(verts, base[0], base[3], base[2]);
    MakeFace(verts, base[0], base[2], base[1]);

    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    geometry.SetVertexBuffer(std::move(verts));
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
}

void PyramidGeometry::MakeFace(std::vector<Vertex3D>& vertices, const Vertex3D& apex, const Vertex3D& base0,
                               const Vertex3D& base1)
{
    const auto this_position = ToVec(base0.aPosition);
    const auto next_position = ToVec(base1.aPosition);
    const auto apex_position = ToVec(apex.aPosition);
    const auto to_apex = glm::normalize(apex_position - this_position);
    const auto to_next = glm::normalize(next_position - this_position);
    const auto normal = glm::normalize(glm::cross(to_next, to_apex));

    vertices.push_back(apex);
    vertices.back().aNormal = ToVec(normal);

    vertices.push_back(base0);
    vertices.back().aNormal = ToVec(normal);

    vertices.push_back(base1);
    vertices.back().aNormal = ToVec(normal);
}

std::string GetSimpleShapeGeometryId(const SimpleShapeArgs& args,
                                     const SimpleShapeEnvironment& env,
                                     SimpleShapeStyle style,
                                     SimpleShapeType type)
{
    if (Is3DShape(type))
        style = Style::Solid;

    std::string id;
    id += base::ToString(type);
    id += base::ToString(style);
    if (type == SimpleShapeType::Capsule || type == SimpleShapeType::RoundRect)
    {
        // try to figure out if the model matrix will distort the
        // round rectangle out of it's square shape which would then
        // distort the rounded corners out of the shape too.
        const auto& model_matrix = *env.model_matrix;
        const auto rect_width    = glm::length(model_matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        const auto rect_height   = glm::length(model_matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
        const auto aspect_ratio  = rect_width / rect_height;
        if (type == SimpleShapeType::Capsule)
            id += NameAspectRatio(rect_width, rect_height, HalfRound, "%1.1f:%1.1f");
        else if (type == SimpleShapeType::RoundRect)
            id += NameAspectRatio(rect_width, rect_height, Truncate, "%d:%d");
    }
    return id;
}

void ConstructSimpleShape(const SimpleShapeArgs& args,
                          const SimpleShapeEnvironment& environment,
                          SimpleShapeStyle style,
                          SimpleShapeType type,
                          Geometry::CreateArgs& create)
{

    create.usage = Geometry::Usage::Static;
    create.content_name = base::ToString(type);
    auto& geometry = create.buffer;

    if (type == SimpleShapeType::Arrow)
        detail::ArrowGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::ArrowCursor)
        detail::ArrowCursorGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::BlockCursor)
        detail::BlockCursorGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Capsule)
        detail::CapsuleGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Circle)
        detail::CircleGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Cube)
        detail::CubeGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Cone)
        detail::ConeGeometry::Generate(environment, style, geometry, std::get<ConeShapeArgs>(args).slices);
    else if (type == SimpleShapeType::Cylinder)
        detail::CylinderGeometry::Generate(environment, style, geometry, std::get<CylinderShapeArgs>(args).slices);
    else if (type == SimpleShapeType::IsoscelesTriangle)
        detail::IsoscelesTriangleGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Parallelogram)
        detail::ParallelogramGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Pyramid)
        detail::PyramidGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Rectangle)
        detail::RectangleGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::RightTriangle)
        detail::RightTriangleGeometry::Generate(environment, style, geometry, std::get<RightTriangleArgs>(args));
    else if (type == SimpleShapeType::RoundRect)
        detail::RoundRectGeometry::Generate(environment, style, geometry, std::get<RoundRectShapeArgs>(args).corner_radius);
    else if (type == SimpleShapeType::SemiCircle)
        detail::SemiCircleGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Sector)
        detail::SectorGeometry::Generate(environment, style, geometry, std::get<SectorShapeArgs>(args).fill_percentage);
    else if (type == SimpleShapeType::Sphere)
        detail::SphereGeometry::Generate(environment, style, geometry, std::get<SphereShapeArgs>(args).slices);
    else if (type == SimpleShapeType::StaticLine)
        detail::StaticLineGeometry::Generate(environment, style, geometry);
    else if (type == SimpleShapeType::Trapezoid)
        detail::TrapezoidGeometry::Generate(environment, style, geometry);
    else BUG("Missing geometry.");
}

} // namespace detail

SpatialMode GetSimpleShapeSpatialMode(SimpleShapeType shape)
{
    // 2D shapes
    switch (shape)
    {
        case SimpleShapeType::Arrow:             return SpatialMode::Flat2D;
        case SimpleShapeType::ArrowCursor:       return SpatialMode::Flat2D;
        case SimpleShapeType::BlockCursor:       return SpatialMode::Flat2D;
        case SimpleShapeType::Capsule:           return SpatialMode::Flat2D;
        case SimpleShapeType::Circle:            return SpatialMode::Flat2D;
        case SimpleShapeType::IsoscelesTriangle: return SpatialMode::Flat2D;
        case SimpleShapeType::Parallelogram:     return SpatialMode::Flat2D;
        case SimpleShapeType::Rectangle:         return SpatialMode::Flat2D;
        case SimpleShapeType::RightTriangle:     return SpatialMode::Flat2D;
        case SimpleShapeType::RoundRect:         return SpatialMode::Flat2D;
        case SimpleShapeType::Sector:            return SpatialMode::Flat2D;
        case SimpleShapeType::SemiCircle:        return SpatialMode::Flat2D;
        case SimpleShapeType::StaticLine:        return SpatialMode::Flat2D;
        case SimpleShapeType::Trapezoid:         return SpatialMode::Flat2D;
        case SimpleShapeType::Triangle:          return SpatialMode::Flat2D;
        default: break;
    }

    // 3D shapes
    switch (shape)
    {
        case SimpleShapeType::Cone:              return SpatialMode::True3D;
        case SimpleShapeType::Cube:              return SpatialMode::True3D;
        case SimpleShapeType::Cylinder:          return SpatialMode::True3D;
        case SimpleShapeType::Pyramid:           return SpatialMode::True3D;
        case SimpleShapeType::Sphere:            return SpatialMode::True3D;
        default: break;
    }

    // 4D shapes
    // ... just kidding there are no 4D shapes.

    BUG("Missing simple shape spatial mode mapping.");
    return SpatialMode::Flat2D;
}

std::size_t SimpleShapeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mShape);
    hash = base::hash_combine(hash, mArgs);
    return hash;
}

SimpleShapeClass::SpatialMode SimpleShapeClass::GetSpatialMode() const
{
    return GetSimpleShapeSpatialMode(mShape);
}

void SimpleShapeClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("shape", mShape);
}
bool SimpleShapeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id", &mId);
    ok &= data.Read("name", &mName);
    ok &= data.Read("shape", &mShape);
    return ok;
}

bool SimpleShapeInstance::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const
{
    unsigned flags = 0;
    if (env.flip_uv_horizontally)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Horizontally);
    if (env.flip_uv_vertically)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Vertically);

    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kDrawableFlags", flags);
    return true;
}
ShaderSource SimpleShapeInstance::GetShader(const Environment& env, const Device& device) const
{
    if (Is3DShape(mClass->GetShapeType()))
        return MakeSimple3DVertexShader(device, env.use_instancing);

    const bool enable_effect = env.mesh_type == MeshType::ShardedEffectMesh;

    return MakeSimple2DVertexShader(device, env.use_instancing, enable_effect);
}
std::string SimpleShapeInstance::GetGeometryId(const Environment& env) const
{
    return detail::GetSimpleShapeGeometryId(mClass->GetShapeArgs(), env, mStyle, mClass->GetShapeType());
}

bool SimpleShapeInstance::Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const
{
    if (env.mesh_type == MeshType::ShardedEffectMesh)
    {
        if (!Is2DShape(mClass->GetShapeType()))
            return false;
        if (mStyle != Style::Solid)
            return false;

        const auto& args = std::get<ShardedEffectMeshArgs>(env.mesh_args);
        return ConstructShardMesh(env, device, geometry, args.mesh_subdivision_count);
    }

    detail::ConstructSimpleShape(mClass->GetShapeArgs(), env, mStyle, mClass->GetShapeType(), geometry);

    if (Is3DShape(mClass->GetShapeType()))
        ASSERT(ComputeTangents(geometry.buffer));
    return true;
}

bool SimpleShapeInstance::Construct(const Environment& env, Device&, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    InstancedDrawBuffer buffer;
    buffer.SetInstanceDataLayout(GetInstanceDataLayout<InstanceAttribute>());
    buffer.Resize(draw.instances.size());

    for (size_t i=0; i<draw.instances.size(); ++i)
    {
        const auto& instance = draw.instances[i];
        InstanceAttribute ia;
        ia.iaModelVectorX = ToVec(instance.model_to_world[0]);
        ia.iaModelVectorY = ToVec(instance.model_to_world[1]);
        ia.iaModelVectorZ = ToVec(instance.model_to_world[2]);
        ia.iaModelVectorW = ToVec(instance.model_to_world[3]);
        buffer.SetInstanceData(ia, i);
    }

    // we're not making any contribution to the instance data here, therefore
    // the hash and usage are exactly what the caller specified
    args.usage = draw.usage;
    args.content_name = draw.content_name;
    args.content_hash = draw.content_hash;
    args.buffer = std::move(buffer);
    return true;
}

std::string SimpleShapeInstance::GetShaderId(const Environment& env) const
{
    return ::GetShaderId(mClass->GetShapeType(), env.use_instancing, env.mesh_type);
}

std::string SimpleShapeInstance::GetShaderName(const Environment& env) const
{
    return ::GetShaderName(mClass->GetShapeType(), env.use_instancing, env.mesh_type);
}

Drawable::Type SimpleShapeInstance::GetType() const
{
    return Type::SimpleShape;
}

Drawable::DrawPrimitive SimpleShapeInstance::GetDrawPrimitive() const
{
    if (Is3DShape(mClass->GetShapeType()))
        return DrawPrimitive::Triangles;

    if (mStyle == Style::Outline)
        return DrawPrimitive::Lines;

    return DrawPrimitive::Triangles;
}

Drawable::Usage SimpleShapeInstance::GetGeometryUsage() const
{
    return Usage::Static;
}

SpatialMode SimpleShapeInstance::GetSpatialMode() const
{
    return mClass->GetSpatialMode();
}

bool SimpleShapeInstance::ConstructShardMesh(const Environment& env, Device& device, Geometry::CreateArgs& create,
    unsigned mesh_subdivision_count) const
{
    Geometry::CreateArgs args;
    detail::ConstructSimpleShape(mClass->GetShapeArgs(), env, mStyle, mClass->GetShapeType(), args);

    // the triangle mesh computation produces a  mesh that  has the same
    // vertex layout as the original drawables geometry  buffer.
    GeometryBuffer geometry_buffer;
    if (!TessellateMesh(args.buffer, geometry_buffer, TessellationAlgo::LongestEdgeBisection, mesh_subdivision_count))
    {
        ERROR("Failed to compute triangle mesh.");
        return false;
    }
    ASSERT(geometry_buffer.GetLayout() == GetVertexLayout<Vertex2D>());
    ASSERT(geometry_buffer.HasIndexData() == false);

    const VertexStream vertex_stream(geometry_buffer.GetLayout(),
                                      geometry_buffer.GetVertexBuffer());
    const auto vertex_count = vertex_stream.GetCount();

    // change the vertex format to ShardVertex2D and compute shard indices
    // for each vertex using this vertex buffer.
    VertexBuffer vertex_buffer;
    vertex_buffer.SetVertexLayout(GetVertexLayout<ShardVertex2D>());
    vertex_buffer.Resize(vertex_count);

    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        const auto triangle_index = vertex_index / 3;
        const auto* src_vertex = vertex_stream.GetVertex<Vertex2D>(vertex_index);

        ShardVertex2D vertex;
        vertex.aPosition   = src_vertex->aPosition;
        vertex.aTexCoord   = src_vertex->aTexCoord;
        vertex.aShardIndex = triangle_index;
        vertex_buffer.SetVertex(vertex, vertex_index);
    }
    // change the layout and the vertex data.
    // the draw commands remain unchanged.
    geometry_buffer.SetVertexLayout(GetVertexLayout<ShardVertex2D>());
    geometry_buffer.SetVertexBuffer(vertex_buffer.TransferBuffer());

    create.buffer       = std::move(geometry_buffer);
    create.usage        = args.usage;
    create.content_hash = args.content_hash;
    create.content_name = args.content_name;
    return true;
}

bool SimpleShape::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const
{
    unsigned flags = 0;
    if (env.flip_uv_horizontally)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Horizontally);
    if (env.flip_uv_vertically)
        flags |= static_cast<unsigned>(DrawableFlags::Flip_UV_Vertically);

    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    program.SetUniform("kDrawableFlags", flags);
    return true;
}
ShaderSource SimpleShape::GetShader(const Environment& env, const Device& device) const
{
    if (Is3DShape(mShape))
        return MakeSimple3DVertexShader(device, env.use_instancing);

    // not supporting the effect mesh operation in this render path right now
    // since it's not needed.
    ASSERT(env.mesh_type == MeshType::NormalRenderMesh);
    constexpr auto enable_effect = false;
    return MakeSimple2DVertexShader(device, env.use_instancing, enable_effect);
}
std::string SimpleShape::GetShaderId(const Environment& env) const
{
    return ::GetShaderId(mShape, env.use_instancing, env.mesh_type);
}

std::string SimpleShape::GetShaderName(const Environment& env) const
{
    return ::GetShaderName(mShape, env.use_instancing, env.mesh_type);
}

std::string SimpleShape::GetGeometryId(const Environment& env) const
{
    return detail::GetSimpleShapeGeometryId(mArgs, env, mStyle, mShape);
}

bool SimpleShape::Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const
{
    detail::ConstructSimpleShape(mArgs, env, mStyle, mShape, geometry);

    if (Is3DShape(mShape))
        ASSERT(ComputeTangents(geometry.buffer));

    return true;
}

bool SimpleShape::Construct(const Environment& env, Device& device, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    InstancedDrawBuffer buffer;
    buffer.SetInstanceDataLayout(GetInstanceDataLayout<InstanceAttribute>());
    buffer.Resize(draw.instances.size());

    for (size_t i=0; i<draw.instances.size(); ++i)
    {
        const auto& instance = draw.instances[i];
        InstanceAttribute ia;
        ia.iaModelVectorX = ToVec(instance.model_to_world[0]);
        ia.iaModelVectorY = ToVec(instance.model_to_world[1]);
        ia.iaModelVectorZ = ToVec(instance.model_to_world[2]);
        ia.iaModelVectorW = ToVec(instance.model_to_world[3]);
        buffer.SetInstanceData(ia, i);
    }

    // we're not making any contribution to the instance data here, therefore
    // the hash and usage are exactly what the caller specified.
    args.usage = draw.usage;
    args.content_hash = draw.content_hash;
    args.content_name = draw.content_name;
    args.buffer = std::move(buffer);
    return true;
}

Drawable::Type SimpleShape::GetType() const
{
    return Type::SimpleShape;
}

Drawable::DrawPrimitive SimpleShape::GetDrawPrimitive() const
{
    if (Is3DShape(mShape))
        return DrawPrimitive::Triangles;

    if (mStyle == Style::Outline)
        return DrawPrimitive::Lines;

    return DrawPrimitive::Triangles;
}

Drawable::Usage SimpleShape::GetGeometryUsage() const
{
    return Usage::Static;
}

SpatialMode SimpleShape::GetSpatialMode() const
{
    return GetSimpleShapeSpatialMode(mShape);
}


} // namespace gfx
