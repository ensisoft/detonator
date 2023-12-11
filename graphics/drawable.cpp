// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <nlohmann/json.hpp>
#  include <base64/base64.h>
#include "warnpop.h"

#include <functional> // for hash
#include <cmath>
#include <cstdio>

#include "base/logging.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/json.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"
#include "graphics/drawable.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"
#include "graphics/resource.h"
#include "graphics/transform.h"
#include "graphics/loader.h"

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

std::string MakeSimple2DVertexShader(const gfx::Device& device)
{
    // the varyings vParticleRandomValue, vParticleAlpha and vParticleTime
    // are used to support per particle features.
    // This shader doesn't provide that data but writes these varyings
    // nevertheless so that it's possible to use a particle shader enabled
    // material also with this shader.

    // the vertex model space  is defined in the lower right quadrant in
    // NDC (normalized device coordinates) (x grows right to 1.0 and
    // y grows up to 1.0 to the top of the screen).

constexpr auto* src = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;

uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

varying vec2 vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;
varying float vParticleTime;

void VertexShaderMain()
{
    vec4 vertex  = vec4(aPosition.x, aPosition.y * -1.0, 0.0, 1.0);
    vTexCoord    = aTexCoord;
    vParticleRandomValue = 0.0;
    vParticleAlpha       = 1.0;
    vParticleTime        = 0.0;
    gl_Position  = kProjectionMatrix * kModelViewMatrix * vertex;
}
)";
    return src;
}

std::string MakeSimple3DVertexShader(const gfx::Device& device)
{
constexpr const auto* src = R"(
attribute vec3 aPosition;
attribute vec2 aTexCoord;

uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

varying vec2 vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;
varying float vParticleTime;

void VertexShaderMain()
{
    vTexCoord = aTexCoord;
    vParticleRandomValue = 0.0;
    vParticleAlpha       = 1.0;
    vParticleTime        = 0.0;
    gl_Position = kProjectionMatrix * kModelViewMatrix * vec4(aPosition.xyz, 1.0);

}

)";
    return src;
}

} // namespace

namespace gfx {
namespace detail {

void ArrowGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
        geometry.SetVertexBuffer(verts, 9);
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void StaticLineGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
{
    // horizontal line.
    constexpr const gfx::Vertex2D verts[2] = {
        {{0.0f,  -0.5f}, {0.0f, 0.5f}},
        {{1.0f,  -0.5f}, {1.0f, 0.5f}}
    };
    geometry.SetVertexBuffer(verts, 2);
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
}

// static
void CapsuleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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

    geometry.SetVertexBuffer(std::move(vs));
}

// static
void SemiCircleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
    geometry.ClearDraws();

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}

// static
void CircleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
    geometry.ClearDraws();

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}

// static
void RectangleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
{
    if (style == Style::Outline)
    {
        const Vertex2D verts[] = {
            { {0.0f,  0.0f}, {0.0f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} },
            { {1.0f,  0.0f}, {1.0f, 0.0f} }
        };
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
        geometry.SetVertexBuffer(verts, 6);
        geometry.ClearDraws();
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void IsoscelesTriangleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
{
    const Vertex2D verts[3] = {
        { {0.5f,  0.0f}, {0.5f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} }
    };

    geometry.SetVertexBuffer(verts, 3);
    geometry.ClearDraws();
    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
}

// static
void RightTriangleGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
{
    const Vertex2D verts[3] = {
        { {0.0f,  0.0f}, {0.0f, 0.0f} },
        { {0.0f, -1.0f}, {0.0f, 1.0f} },
        { {1.0f, -1.0f}, {1.0f, 1.0f} }
    };
    geometry.SetVertexBuffer(verts, 3);
    geometry.ClearDraws();
    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
}

// static
void TrapezoidGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
        geometry.ClearDraws();
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void ParallelogramGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
        geometry.ClearDraws();
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
}

// static
void SectorGeometry::Generate(const Environment& env, Style style, Geometry& geometry, float fill_percentage)
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
    geometry.ClearDraws();

    if (style == Style::Solid)
        geometry.AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geometry.AddDrawCmd(Geometry::DrawType::LineLoop);
}

// static
void RoundRectGeometry::Generate(const Environment& env, Style style, Geometry& geometry, float corner_radius)
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
    }
}

// static
void ArrowCursorGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
}

// static
void BlockCursorGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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
    geometry.AddDrawCmd(Geometry::DrawType::Triangles);
}

// static
void CubeGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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

    geometry.UploadVertices(vertices, sizeof(vertices), Geometry::Usage::Static);
    geometry.UploadIndices(indices, sizeof(indices), Geometry::IndexType::Index16, Geometry::Usage::Static);
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
void CylinderGeometry::Generate(const Environment& env, Style style, Geometry& geometry, unsigned slices)
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
void ConeGeometry::Generate(const Environment& env, Style style, Geometry& geometry, unsigned int slices)
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
void SphereGeometry::Generate(const Environment& env, Style style, Geometry& geometry, unsigned int slices)
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
void PyramidGeometry::Generate(const Environment& env, Style style, Geometry& geometry)
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

std::string GetSimpleShapeGeometryName(const SimpleShapeArgs& args,
                                       const SimpleShapeEnvironment& env,
                                       SimpleShapeStyle style,
                                       SimpleShapeType type)
{
    if (Is3DShape(type))
        style = Style::Solid;

    std::string name;
    name += base::ToString(type);
    name += base::ToString(style);
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
            name += NameAspectRatio(rect_width, rect_height, HalfRound, "%1.1f:%1.1f");
        else if (type == SimpleShapeType::RoundRect)
            name += NameAspectRatio(rect_width, rect_height, Truncate, "%d:%d");
    }
    return name;
}

void ConstructSimpleShape(const SimpleShapeArgs& args,
                          const SimpleShapeEnvironment& environment,
                          SimpleShapeStyle style,
                          SimpleShapeType type,
                          Geometry& geometry)
{
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
        detail::RightTriangleGeometry::Generate(environment, style, geometry);
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

} // detail

std::size_t SimpleShapeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mShape);
    hash = base::hash_combine(hash, mArgs);
    return hash;
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

void SimpleShapeInstance::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
}
std::string SimpleShapeInstance::GetShader(const Environment& env, const Device& device) const
{
    if (Is3DShape(mClass->GetShapeType()))
        return MakeSimple3DVertexShader(device);

    return MakeSimple2DVertexShader(device);
}
std::string SimpleShapeInstance::GetGeometryName(const Environment& env) const
{
    return detail::GetSimpleShapeGeometryName(mClass->GetShapeArgs(), env, mStyle, mClass->GetShapeType());
}

bool SimpleShapeInstance::Upload(const Environment& env, Geometry& geometry) const
{
    detail::ConstructSimpleShape(mClass->GetShapeArgs(), env, mStyle, mClass->GetShapeType(), geometry);
    return true;
}
std::string SimpleShapeInstance::GetShaderId(const Environment& env) const
{
    if (Is3DShape(mClass->GetShapeType()))
        return "simple-3D-vertex-shader";

    return "simple-2D-vertex-shader";
}

std::string SimpleShapeInstance::GetShaderName(const Environment& env) const
{
    if (Is3DShape(mClass->GetShapeType()))
        return "Simple3DVertexShader";

    return "Simple2DVertexShader";
}

Drawable::Type SimpleShapeInstance::GetType() const
{
    return Type::SimpleShape;
}

Drawable::Primitive SimpleShapeInstance::GetPrimitive() const
{
    if (Is3DShape(mClass->GetShapeType()))
        return Primitive::Triangles;

    if (mStyle == Style::Outline)
        return Primitive::Lines;

    return Primitive::Triangles;
}

void SimpleShape::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
}
std::string SimpleShape::GetShader(const Environment& env, const Device& device) const
{
    if (Is3DShape(mShape))
        return MakeSimple3DVertexShader(device);

    return MakeSimple2DVertexShader(device);
}
std::string SimpleShape::GetShaderId(const Environment& env) const
{
    if (Is3DShape(mShape))
        return "simple-3D-vertex-shader";

    return "simple-2D-vertex-shader";
}

std::string SimpleShape::GetShaderName(const Environment& env) const
{
    if (Is3DShape(mShape))
        return "Simple3DVertexShader";

    return "Simple2DVertexShader";
}

std::string SimpleShape::GetGeometryName(const Environment& env) const
{
    return detail::GetSimpleShapeGeometryName(mArgs, env, mStyle, mShape);
}

bool SimpleShape::Upload(const Environment& env, Geometry& geometry) const
{
    detail::ConstructSimpleShape(mArgs, env, mStyle, mShape, geometry);
    return true;
}

Drawable::Type SimpleShape::GetType() const
{
    return Type::SimpleShape;
}

Drawable::Primitive SimpleShape::GetPrimitive() const
{
    if (Is3DShape(mShape))
        return Primitive::Triangles;

    if (mStyle == Style::Outline)
        return Primitive::Lines;

    return Primitive::Triangles;
}

void Grid::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
}

std::string Grid::GetShaderId(const Environment&) const
{
    return "simple-2D-vertex-shader";
}

std::string Grid::GetShader(const Environment&, const Device& device) const
{
    return MakeSimple2DVertexShader(device);
}

std::string Grid::GetShaderName(const Environment& env) const
{
    return "Simple2DVertexShader";
}

std::string Grid::GetGeometryName(const Environment& env) const
{
    // use the content properties to generate a name for the
    // gpu side geometry.
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
    hash = base::hash_combine(hash, mBorderLines);
    return std::to_string(hash);
}

bool Grid::Upload(const Environment&, Geometry& geometry) const
{
    std::vector<Vertex2D> verts;

    const float yadvance = 1.0f / (mNumHorizontalLines + 1);
    const float xadvance = 1.0f / (mNumVerticalLines + 1);
    for (unsigned i=1; i<=mNumVerticalLines; ++i)
    {
        const float x = i * xadvance;
        const Vertex2D line[2] = {
            {{x,  0.0f}, {x, 0.0f}},
            {{x, -1.0f}, {x, 1.0f}}
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    for (unsigned i=1; i<=mNumHorizontalLines; ++i)
    {
        const float y = i * yadvance;
        const Vertex2D line[2] = {
            {{0.0f, y*-1.0f}, {0.0f, y}},
            {{1.0f, y*-1.0f}, {1.0f, y}},
        };
        verts.push_back(line[0]);
        verts.push_back(line[1]);
    }
    if (mBorderLines)
    {
        const Vertex2D corners[4] = {
            // top left
            {{0.0f, 0.0f}, {0.0f, 0.0f}},
            // top right
            {{1.0f, 0.0f}, {1.0f, 0.0f}},

            // bottom left
            {{0.0f, -1.0f}, {0.0f, 1.0f}},
            // bottom right
            {{1.0f, -1.0f}, {1.0f, 1.0f}}
        };
        verts.push_back(corners[0]);
        verts.push_back(corners[1]);
        verts.push_back(corners[2]);
        verts.push_back(corners[3]);
        verts.push_back(corners[0]);
        verts.push_back(corners[2]);
        verts.push_back(corners[1]);
        verts.push_back(corners[3]);
    }
    geometry.SetVertexBuffer(std::move(verts));
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}

void PolygonMeshClass::SetVertexBuffer(std::vector<uint8_t> buffer) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.vertices = std::move(buffer);
}

void PolygonMeshClass::SetVertexLayout(VertexLayout layout) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.layout = std::move(layout);
}

void PolygonMeshClass::SetCommandBuffer(std::vector<Geometry::DrawCommand> cmds) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.cmds = std::move(cmds);
}

void PolygonMeshClass::SetVertexBuffer(VertexBuffer&& buffer) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.vertices = std::move(buffer).GetVertexBuffer();
    data.layout   = std::move(buffer).GetLayout();
}
void PolygonMeshClass::SetVertexBuffer(const VertexBuffer& buffer) noexcept
{
    if (!mData.has_value())
        mData = InlineData {};

    auto& data = mData.value();
    data.vertices = buffer.GetVertexBuffer();
    data.layout   = buffer.GetLayout();
}

const VertexLayout* PolygonMeshClass::GetVertexLayout() const noexcept
{
    if (mData.has_value())
        return &mData.value().layout;

    return nullptr;
}

const void* PolygonMeshClass::GetVertexBufferPtr() const noexcept
{
    if (mData.has_value())
    {
        if (!mData.value().vertices.empty())
            return mData.value().vertices.data();
    }
    return nullptr;
}

size_t PolygonMeshClass::GetNumDrawCmds() const noexcept
{
    if (mData.has_value())
        return mData.value().cmds.size();

    return 0;
}

size_t PolygonMeshClass::GetVertexBufferSize() const noexcept
{
    if (mData.has_value())
        return mData.value().vertices.size();

    return 0;
}

const Geometry::DrawCommand* PolygonMeshClass::GetDrawCmd(size_t index) const noexcept
{
    if (mData.has_value())
    {
        const auto& draws = mData.value().cmds;
        return &draws[index];
    }
    return nullptr;
}

std::string PolygonMeshClass::GetGeometryName(const Environment& env) const
{
    return mId;
}

bool PolygonMeshClass::IsDynamic(const Environment& env) const
{
    // editing mode overrides static
    if (env.editing_mode)
        return true;

    return !mStatic;
}

bool PolygonMeshClass::Upload(const Environment& env, Geometry& geometry) const
{
    if (!geometry.GetDataHash())
    {
        return UploadGeometry(env, geometry);
    }
    else if (IsDynamic(env))
    {
        const auto content_hash  = GetContentHash();
        const auto geometry_hash = geometry.GetDataHash();
        if (content_hash != geometry_hash)
        {
            return UploadGeometry(env, geometry);
        }
    }
    return true;
}

std::size_t PolygonMeshClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStatic);
    hash = base::hash_combine(hash, mContentHash);
    hash = base::hash_combine(hash, mContentUri);

    if (mData.has_value())
    {
        const auto& data = mData.value();
        hash = base::hash_combine(hash, data.layout.GetHash());
        hash = base::hash_combine(hash, data.vertices);

        // BE-AWARE, padding might make this non-deterministic!
        //hash = base::hash_combine(hash, data.cmds);

        for (const auto& cmd : data.cmds)
        {
            hash = base::hash_combine(hash, cmd.type);
            hash = base::hash_combine(hash, cmd.count);
            hash = base::hash_combine(hash, cmd.offset);
        }
    }
    return hash;
}

std::unique_ptr<DrawableClass> PolygonMeshClass::Clone() const
{
    auto ret = std::make_unique<PolygonMeshClass>(*this);
    ret->mId = base::RandomString(10);
    return ret;
}
std::unique_ptr<DrawableClass> PolygonMeshClass::Copy() const
{
    return std::make_unique<PolygonMeshClass>(*this);
}

void PolygonMeshClass::IntoJson(data::Writer& writer) const
{
    writer.Write("id",     mId);
    writer.Write("name",   mName);
    writer.Write("static", mStatic);
    writer.Write("uri",    mContentUri);

    if (mData.has_value())
    {
        auto inline_chunk = writer.NewWriteChunk();

        const auto& data = mData.value();
        data.layout.IntoJson(*inline_chunk);

        const VertexStream vertex_stream(data.layout, data.vertices);
        vertex_stream.IntoJson(*inline_chunk);

        const CommandStream command_stream(data.cmds);
        command_stream.IntoJson(*inline_chunk);

        writer.Write("inline_data", std::move(inline_chunk));
    }

    static_assert(sizeof(unsigned) == 4, "4 bytes required for unsigned.");

    if constexpr (sizeof(mContentHash) == 4)
    {
        writer.Write("content_hash", (unsigned)mContentHash);
    }
    else if (sizeof(mContentHash) == 8)
    {
        const unsigned hi = (mContentHash >> 32) & 0xffffffff;
        const unsigned lo = (mContentHash >> 0 ) & 0xffffffff;
        writer.Write("content_hash_lo", lo);
        writer.Write("content_hash_hi", hi);
    }
}

bool PolygonMeshClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("id",     &mId);
    ok &= reader.Read("name",   &mName);
    ok &= reader.Read("static", &mStatic);
    ok &= reader.Read("uri",    &mContentUri);

    if (const auto& inline_chunk = reader.GetReadChunk("inline_data"))
    {
        InlineData data;
        ok &= data.layout.FromJson(*inline_chunk);

        VertexBuffer vertex_buffer(&data.vertices);
        ok &= vertex_buffer.FromJson(*inline_chunk);

        CommandBuffer command_buffer(&data.cmds);
        ok &= command_buffer.FromJson(*inline_chunk);

        mData = std::move(data);
    }

    // legacy load
    if (reader.HasArray("vertices") && reader.HasArray("draws"))
    {
        VertexBuffer vertexBuffer(gfx::GetVertexLayout<Vertex2D>());

        for (unsigned i=0; i<reader.GetNumChunks("vertices"); ++i)
        {
            const auto& chunk = reader.GetReadChunk("vertices", i);
            float x, y, s, t;
            ok &= chunk->Read("x", &x);
            ok &= chunk->Read("y", &y);
            ok &= chunk->Read("s", &s);
            ok &= chunk->Read("t", &t);

            Vertex2D vertex;
            vertex.aPosition.x = x;
            vertex.aPosition.y = y;
            vertex.aTexCoord.x = s;
            vertex.aTexCoord.y = t;
            vertexBuffer.PushBack(&vertex);
        }

        std::vector<Geometry::DrawCommand> cmds;
        for (unsigned i=0; i<reader.GetNumChunks("draws"); ++i)
        {
            const auto& chunk = reader.GetReadChunk("draws", i);
            unsigned offset = 0;
            unsigned count  = 0;
            Geometry::DrawCommand cmd;
            ok &=  chunk->Read("type",   &cmd.type);
            ok &=  chunk->Read("offset", &offset);
            ok &=  chunk->Read("count",  &count);

            cmd.offset = offset;
            cmd.count  = count;
            cmds.push_back(cmd);
        }
        InlineData data;
        data.vertices = std::move(vertexBuffer).GetVertexBuffer();
        data.cmds     = std::move(cmds);
        data.layout   = gfx::GetVertexLayout<gfx::Vertex2D>();
        mData = std::move(data);
    }

    static_assert(sizeof(unsigned) == 4, "4 bytes required for unsigned.");

    if constexpr(sizeof(mContentHash) == 4)
    {
        unsigned value = 0;
        ok &= reader.Read("content_hash", &value);
        mContentHash = value;
    }
    else if (sizeof(mContentHash) == 8)
    {
        unsigned hi = 0;
        unsigned lo = 0;
        ok &= reader.Read("content_hash_lo", &lo);
        ok &= reader.Read("content_hash_hi", &hi);
        mContentHash = (size_t(hi) << 32) | size_t(lo);
    }
    return ok;
}

bool PolygonMeshClass::UploadGeometry(const Environment& env, Geometry& geometry) const
{
    const auto dynamic = IsDynamic(env);
    const auto usage   = dynamic ? Geometry::Usage::Dynamic
                                 : Geometry::Usage::Static;
    if (mData.has_value())
    {
        const auto& data = mData.value();

        geometry.SetDataHash(GetContentHash());
        geometry.UploadVertices(data.vertices.data(), data.vertices.size(), usage);
        geometry.SetVertexLayout(data.layout);
        geometry.ClearDraws();

        for (const auto& cmd : data.cmds)
        {
            geometry.AddDrawCmd(cmd);
        }
    }

    if (mContentUri.empty())
        return true;

    gfx::Loader::ResourceDesc desc;
    desc.uri  = mContentUri;
    desc.id   = mId;
    desc.type = gfx::Loader::Type::Mesh;
    const auto& buffer = LoadResource(desc);
    if (!buffer)
    {
        ERROR("Failed to load polygon mesh. [uri='%1']", mContentUri);
        return false;
    }

    const char* beg = (const char*)buffer->GetData();
    const char* end = (const char*)buffer->GetData() + buffer->GetByteSize();
    const auto& [success, json, error] = base::JsonParse(beg, end);
    if (!success)
    {
        ERROR("Failed to parse geometry buffer. [uri='%1', error='%2'].", mContentUri, error);
        return false;
    }

    data::JsonObject reader(std::move(json));

    VertexBuffer vertex_buffer;
    if (!vertex_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh vertex buffer. [uri='%1']", mContentUri);
        return false;
    }
    if (!vertex_buffer.Validate())
    {
        ERROR("Polygon mesh vertex buffer is not valid. [uri='%1']", mContentUri);
        return false;
    }

    CommandBuffer command_buffer;
    if (!command_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh command buffer. [uri='%1']", mContentUri);
        return false;
    }

    IndexBuffer index_buffer;
    if (!index_buffer.FromJson(reader))
    {
        ERROR("Failed to load polygon mesh index buffer. [uri='%1']", mContentUri);
        return false;
    }

    geometry.SetVertexLayout(vertex_buffer.GetLayout());
    geometry.UploadVertices(vertex_buffer.GetBufferPtr(), vertex_buffer.GetBufferSize(), usage);
    geometry.UploadIndices(index_buffer.GetBufferPtr(), index_buffer.GetBufferSize(),
                           index_buffer.GetType(), usage);
    geometry.SetDataHash(GetContentHash());
    geometry.ClearDraws();
    geometry.SetDrawCommands(command_buffer.GetCommandBuffer());
    return true;
}

void PolygonMeshInstance::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
    const auto& kProjectionMatrix = *env.proj_matrix;
    program.SetUniform("kProjectionMatrix", kProjectionMatrix);
    program.SetUniform("kModelViewMatrix", kModelViewMatrix);
}
std::string PolygonMeshInstance::GetShader(const Environment& env, const Device& device) const
{
    return MakeSimple2DVertexShader(device);
}

std::string PolygonMeshInstance::GetGeometryName(const Environment& env) const
{
    return mClass->GetGeometryName(env);
}

bool PolygonMeshInstance::Upload(const Environment& env, Geometry& geometry) const
{
    return mClass->Upload(env, geometry);
}
std::string PolygonMeshInstance::GetShaderId(const Environment& env) const
{
    return "simple-2D-vertex-shader";
}

std::string PolygonMeshInstance::GetShaderName(const Environment& env) const
{
    return "Simple2DVertexShader";
}

std::string ParticleEngineClass::GetProgramId(const Environment& env) const
{
    if (mParams.coordinate_space == CoordinateSpace::Local)
        return "local-particle-program";
    else if (mParams.coordinate_space == CoordinateSpace::Global)
        return "global-particle-program";
    else BUG("Unknown particle program coordinate space.");
    return "";
}

std::string ParticleEngineClass::GetGeometryName(const Environment& env) const
{
    return "particle-buffer";
}

std::string ParticleEngineClass::GetShader(const Environment& env, const Device& device) const
{
    // this shader doesn't actually write to vTexCoord because when
    // particle (GL_POINTS) rasterization is done the fragment shader
    // must use gl_PointCoord instead.
    constexpr auto* local_src = R"(
attribute vec2 aPosition;
attribute vec4 aData;

uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

varying vec2  vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;
varying float vParticleTime;

void VertexShaderMain()
{
    vec4 vertex  = vec4(aPosition.x, aPosition.y, 0.0, 1.0);
    gl_PointSize = aData.x;
    vParticleRandomValue = aData.y;
    vParticleAlpha       = aData.z;
    vParticleTime        = aData.w;
    gl_Position  = kProjectionMatrix * kModelViewMatrix * vertex;
}
    )";

    constexpr auto* global_src = R"(
attribute vec2 aPosition;
attribute vec4 aData;

uniform mat4 kProjectionMatrix;
uniform mat4 kViewMatrix;

varying vec2 vTexCoord;
varying float vParticleRandomValue;
varying float vParticleAlpha;
varying float vParticleTime;

void VertexShaderMain()
{
  vec4 vertex = vec4(aPosition.x, aPosition.y, 0.0, 1.0);
  gl_PointSize = aData.x;
  vParticleRandomValue = aData.y;
  vParticleAlpha       = aData.z;
  vParticleTime        = aData.w;
  gl_Position  = kProjectionMatrix * kViewMatrix * vertex;
}
    )";
    if (mParams.coordinate_space == CoordinateSpace::Local)
        return local_src;
    else if (mParams.coordinate_space == CoordinateSpace::Global)
        return global_src;
    else BUG("Missing particle shader simulation space source.");
    return "";
}

std::string ParticleEngineClass::GetShaderName(const Environment& env) const
{
    if (mParams.coordinate_space == CoordinateSpace::Local)
        return "LocalParticleShader";
    else if (mParams.coordinate_space == CoordinateSpace::Global)
        return "GlobalParticleShader";
    else BUG("Missing particle shader name.");
    return "";
}

bool ParticleEngineClass::Upload(const Drawable::Environment& env, const InstanceState& state, Geometry& geometry) const
{
    // the point rasterization doesn't support non-uniform
    // sizes for the points, i.e. they're always square
    // so therefore we must choose one of the pixel ratio values
    // as the scaler for converting particle sizes to pixel/fragment
    // based sizes
    const auto pixel_scaler = std::min(env.pixel_ratio.x, env.pixel_ratio.y);

    struct ParticleVertex {
        Vec2 aPosition;
        Vec4 aData;
    };
    static const VertexLayout layout(sizeof(ParticleVertex), {
        {"aPosition", 0, 2, 0, offsetof(ParticleVertex, aPosition)},
        {"aData",     0, 4, 0, offsetof(ParticleVertex, aData)}
    });

    std::vector<ParticleVertex> verts;
    for (const auto& p : state.particles)
    {
        // When using local coordinate space the max x/y should
        // be the extents of the simulation in which case the
        // particle x,y become normalized on the [0.0f, 1.0f] range.
        // when using global coordinate space max x/y should be 1.0f
        // and particle coordinates are left in the global space
        ParticleVertex v;
        v.aPosition.x = p.position.x / mParams.max_xpos;
        v.aPosition.y = p.position.y / mParams.max_ypos;
        // copy the per particle data into the data vector for the fragment shader.
        v.aData.x = p.pointsize >= 0.0f ? p.pointsize * pixel_scaler : 0.0f;
        // abusing texcoord here to provide per particle random value.
        // we can use this to simulate particle rotation for example
        // (if the material supports it)
        v.aData.y = p.randomizer;
        // Use the particle data to pass the per particle alpha.
        v.aData.z = p.alpha;
        // use the particle data to pass the per particle time.
        v.aData.w = p.time / (p.time_scale * mParams.max_lifetime);
        verts.push_back(v);
    }

    geometry.SetVertexBuffer(std::move(verts), Geometry::Usage::Stream);
    geometry.SetVertexLayout(layout);
    geometry.ClearDraws();
    geometry.AddDrawCmd(Geometry::DrawType::Points);
    return true;
}

void ParticleEngineClass::ApplyDynamicState(const Environment& env, Program& program) const
{
    if (mParams.coordinate_space == CoordinateSpace::Global)
    {
        // when the coordinate space is global the particles are spawn directly
        // in the global coordinate space. therefore, no model transformation
        // is needed but only the view transformation.
        const auto& kViewMatrix = *env.view_matrix;
        const auto& kProjectionMatrix = *env.proj_matrix;
        program.SetUniform("kProjectionMatrix", kProjectionMatrix);
        program.SetUniform("kViewMatrix", kViewMatrix);
    }
    else if (mParams.coordinate_space == CoordinateSpace::Local)
    {
        const auto& kModelViewMatrix = (*env.view_matrix) * (*env.model_matrix);
        const auto& kProjectionMatrix = *env.proj_matrix;
        program.SetUniform("kProjectionMatrix", kProjectionMatrix);
        program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    }
}

// Update the particle simulation.
void ParticleEngineClass::Update(const Environment& env, InstanceState& state, float dt) const
{
    // In case particles become heavy on the CPU here are some ways to try
    // to mitigate the issue:
    // - Reduce the number of particles in the content (i.e. use less particles
    //   in animations etc.)
    // - Share complete particle engines between assets, i.e. instead of each
    //   animation (for example space ship) using its own particle engine
    //   instance each kind of ship could share one particle engine.
    // - Parallelize the particle updates, i.e. try to throw more CPU cores
    //   at the issue.
    // - Use the GPU instead of the CPU. On GL ES 2 there are no transform
    //   feedback buffers. But for any simple particle animation such as this
    //   that doesn't use any second degree derivatives it should be possible
    //   to do the simulation on the GPU without transform feedback. I.e. in the
    //   absence of acceleration a numerical integration of particle position
    //   is not needed but a new position can simply be computed with
    //   vec2 pos = initial_pos + time * velocity;
    //   Just that one problem that remains is killing particles at the end
    //   of their lifetime or when their size or alpha value reaches 0.
    //   Possibly a hybrid solution could be used.
    //   It could also be possible to simulate transform feedback through
    //   texture writes. For example here: https://nullprogram.com/webgl-particles/

    const bool has_max_time = mParams.max_time < std::numeric_limits<float>::max();

    // check if we've exceeded maximum lifetime.
    if (has_max_time && state.time >= mParams.max_time)
    {
        state.particles.clear();
        state.time += dt;
        return;
    }

    // with automatic spawn modes (once, maintain, continuous) do first
    // particle emission after initial delay has expired.
    if (mParams.mode != SpawnPolicy::Command)
    {
        if (state.time < state.delay)
        {
            if (state.time + dt > state.delay)
            {
                InitParticles(env, state, state.hatching);
                state.hatching = 0;
            }
            state.time += dt;
            return;
        }
    }

    // update each current particle
    for (size_t i=0; i<state.particles.size();)
    {
        if (UpdateParticle(env, state, i, dt))
        {
            ++i;
            continue;
        }
        KillParticle(state, i);
    }

    // Spawn new particles if needed.
    if (mParams.mode == SpawnPolicy::Maintain)
    {
        const auto num_particles_always = size_t(mParams.num_particles);
        const auto num_particles_now = state.particles.size();
        if (num_particles_now < num_particles_always)
        {
            const auto num_particles_needed = num_particles_always - num_particles_now;
            InitParticles(env, state, num_particles_needed);
        }
    }
    else if (mParams.mode == SpawnPolicy::Continuous)
    {
        // the number of particles is taken as the rate of particles per
        // second. fractionally cumulate particles and then
        // spawn when we have some number non-fractional particles.
        state.hatching += mParams.num_particles * dt;
        const auto num = size_t(state.hatching);
        InitParticles(env, state, num);
        state.hatching -= num;
    }
    state.time += dt;
}

// ParticleEngine implementation.
bool ParticleEngineClass::IsAlive(const InstanceState& state) const
{
    if (state.time < mParams.delay)
        return true;
    else if (state.time < mParams.min_time)
        return true;
    else if (state.time > mParams.max_time)
        return false;

    if (mParams.mode == SpawnPolicy::Continuous ||
        mParams.mode == SpawnPolicy::Maintain ||
        mParams.mode == SpawnPolicy::Command)
        return true;

    return !state.particles.empty();
}

void ParticleEngineClass::Emit(const Environment& env, InstanceState& state, int count) const
{
    if (count < 0)
        return;

    InitParticles(env, state, size_t(count));
}

// ParticleEngine implementation. Restart the simulation
// with the previous parameters.
void ParticleEngineClass::Restart(const Environment& env, InstanceState& state) const
{
    state.particles.clear();
    state.delay    = mParams.delay;
    state.time     = 0.0f;
    state.hatching = 0.0f;
    // if the spawn policy is continuous the num particles
    // is a rate of particles per second. in order to avoid
    // a massive initial burst of particles skip the init here
    if (mParams.mode == SpawnPolicy::Continuous)
        return;

    // if the spawn mode is on command only we don't spawn anything
    // unless by command.
    if (mParams.mode == SpawnPolicy::Command)
        return;

    if (state.delay != 0.0f)
        state.hatching = mParams.num_particles;
    else InitParticles(env, state, size_t(mParams.num_particles));
}

void ParticleEngineClass::IntoJson(data::Writer& data) const
{
    data.Write("id",                           mId);
    data.Write("name",                         mName);
    data.Write("direction",                    mParams.direction);
    data.Write("placement",                    mParams.placement);
    data.Write("shape",                        mParams.shape);
    data.Write("coordinate_space",             mParams.coordinate_space);
    data.Write("motion",                       mParams.motion);
    data.Write("mode",                         mParams.mode);
    data.Write("boundary",                     mParams.boundary);
    data.Write("delay",                        mParams.delay);
    data.Write("min_time",                     mParams.min_time);
    data.Write("max_time",                     mParams.max_time);
    data.Write("num_particles",                mParams.num_particles);
    data.Write("min_lifetime",                 mParams.min_lifetime);
    data.Write("max_lifetime",                 mParams.max_lifetime);
    data.Write("max_xpos",                     mParams.max_xpos);
    data.Write("max_ypos",                     mParams.max_ypos);
    data.Write("init_rect_xpos",               mParams.init_rect_xpos);
    data.Write("init_rect_ypos",               mParams.init_rect_ypos);
    data.Write("init_rect_width",              mParams.init_rect_width);
    data.Write("init_rect_height",             mParams.init_rect_height);
    data.Write("min_velocity",                 mParams.min_velocity);
    data.Write("max_velocity",                 mParams.max_velocity);
    data.Write("direction_sector_start_angle", mParams.direction_sector_start_angle);
    data.Write("direction_sector_size",        mParams.direction_sector_size);
    data.Write("min_point_size",               mParams.min_point_size);
    data.Write("max_point_size",               mParams.max_point_size);
    data.Write("min_alpha",                    mParams.min_alpha);
    data.Write("max_alpha",                    mParams.max_alpha);
    data.Write("growth_over_time",             mParams.rate_of_change_in_size_wrt_time);
    data.Write("growth_over_dist",             mParams.rate_of_change_in_size_wrt_dist);
    data.Write("alpha_over_time",              mParams.rate_of_change_in_alpha_wrt_time);
    data.Write("alpha_over_dist",              mParams.rate_of_change_in_alpha_wrt_dist);
    data.Write("gravity",                      mParams.gravity);
}

bool ParticleEngineClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                           &mId);
    ok &= data.Read("name",                         &mName);
    ok &= data.Read("direction",                    &mParams.direction);
    ok &= data.Read("placement",                    &mParams.placement);
    ok &= data.Read("shape",                        &mParams.shape);
    ok &= data.Read("coordinate_space",             &mParams.coordinate_space);
    ok &= data.Read("motion",                       &mParams.motion);
    ok &= data.Read("mode",                         &mParams.mode);
    ok &= data.Read("boundary",                     &mParams.boundary);
    ok &= data.Read("delay",                        &mParams.delay);
    ok &= data.Read("min_time",                     &mParams.min_time);
    ok &= data.Read("max_time",                     &mParams.max_time);
    ok &= data.Read("num_particles",                &mParams.num_particles);
    ok &= data.Read("min_lifetime",                 &mParams.min_lifetime) ;
    ok &= data.Read("max_lifetime",                 &mParams.max_lifetime);
    ok &= data.Read("max_xpos",                     &mParams.max_xpos);
    ok &= data.Read("max_ypos",                     &mParams.max_ypos);
    ok &= data.Read("init_rect_xpos",               &mParams.init_rect_xpos);
    ok &= data.Read("init_rect_ypos",               &mParams.init_rect_ypos);
    ok &= data.Read("init_rect_width",              &mParams.init_rect_width);
    ok &= data.Read("init_rect_height",             &mParams.init_rect_height);
    ok &= data.Read("min_velocity",                 &mParams.min_velocity);
    ok &= data.Read("max_velocity",                 &mParams.max_velocity);
    ok &= data.Read("direction_sector_start_angle", &mParams.direction_sector_start_angle);
    ok &= data.Read("direction_sector_size",        &mParams.direction_sector_size);
    ok &= data.Read("min_point_size",               &mParams.min_point_size);
    ok &= data.Read("max_point_size",               &mParams.max_point_size);
    ok &= data.Read("min_alpha",                    &mParams.min_alpha);
    ok &= data.Read("max_alpha",                    &mParams.max_alpha);
    ok &= data.Read("growth_over_time",             &mParams.rate_of_change_in_size_wrt_time);
    ok &= data.Read("growth_over_dist",             &mParams.rate_of_change_in_size_wrt_dist);
    ok &= data.Read("alpha_over_time",              &mParams.rate_of_change_in_alpha_wrt_time);
    ok &= data.Read("alpha_over_dist",              &mParams.rate_of_change_in_alpha_wrt_dist);
    ok &= data.Read("gravity",                      &mParams.gravity);
    return ok;
}

std::unique_ptr<DrawableClass> ParticleEngineClass::Clone() const
{
    auto ret = std::make_unique<ParticleEngineClass>(*this);
    ret->mId = base::RandomString(10);
    return ret;
}
std::unique_ptr<DrawableClass> ParticleEngineClass::Copy() const
{
    return std::make_unique<ParticleEngineClass>(*this);
}

std::size_t ParticleEngineClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mParams);
    return hash;
}

void ParticleEngineClass::InitParticles(const Environment& env, InstanceState& state, size_t num) const
{
    if (mParams.coordinate_space == CoordinateSpace::Global)
    {
        Transform transform(*env.model_matrix);
        transform.Push();
        transform.Scale(mParams.init_rect_width, mParams.init_rect_height);
        transform.Translate(mParams.init_rect_xpos, mParams.init_rect_ypos);
        const auto& particle_to_world = transform.GetAsMatrix();
        const auto emitter_radius = 0.5f;
        const auto emitter_center = glm::vec2(0.5f, 0.5f);

        for (size_t i=0; i<num; ++i)
        {
            const auto velocity = math::rand(mParams.min_velocity, mParams.max_velocity);

            glm::vec2 position;
            glm::vec2 direction;
            if (mParams.shape == EmitterShape::Rectangle)
            {
                if (mParams.placement == Placement::Inside)
                    position = glm::vec2(math::rand(0.0f, 1.0f), math::rand(0.0f, 1.0f));
                else if (mParams.placement == Placement::Center)
                    position = glm::vec2(0.5f, 0.5f);
                else if (mParams.placement == Placement::Edge)
                {
                    const auto edge = math::rand(0, 3);
                    if (edge == 0 || edge == 1)
                    {
                        position.x = edge == 0 ? 0.0f : 1.0f;
                        position.y = math::rand(0.0f, 1.0f);
                    }
                    else
                    {
                        position.x = math::rand(0.0f, 1.0f);
                        position.y = edge == 2 ? 0.0f : 1.0f;
                    }
                }
            }
            else if (mParams.shape == EmitterShape::Circle)
            {
                if (mParams.placement == Placement::Center)
                    position = glm::vec2(0.5f, 0.5f);
                else if (mParams.placement == Placement::Inside)
                {
                    const auto x = math::rand(-emitter_radius, emitter_radius);
                    const auto y = math::rand(-emitter_radius, emitter_radius);
                    const auto r = math::rand(0.0f, 1.0f);
                    position = glm::normalize(glm::vec2(x, y)) * emitter_radius * r + emitter_center;
                }
                else if (mParams.placement == Placement::Edge)
                {
                    const auto x = math::rand(-emitter_radius, emitter_radius);
                    const auto y = math::rand(-emitter_radius, emitter_radius);
                    position = glm::normalize(glm::vec2(x, y)) * emitter_radius + emitter_center;
                }
            }

            if (mParams.direction == Direction::Sector)
            {
                Transform local_transform;
                local_transform.RotateAroundZ(mParams.direction_sector_start_angle +
                                              math::rand(0.0f, mParams.direction_sector_size));

                const auto local_direction = local_transform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                const auto world_direction = glm::normalize(particle_to_world * local_direction);
                const auto world_angle_cos = glm::dot(world_direction, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                const auto world_angle = std::atan2(world_direction.y, world_direction.x);
                direction = glm::vec2(std::cos(world_angle), std::sin(world_angle));
            }
            else if (mParams.placement == Placement::Center)
            {
                direction = glm::normalize(glm::vec2(math::rand(-1.0f, 1.0f),
                                                     math::rand(-1.0f, 1.0f)));
            }
            else if (mParams.direction == Direction::Inwards)
                direction = glm::normalize(emitter_center - position);
            else if (mParams.direction == Direction::Outwards)
                direction = glm::normalize(position - emitter_center);

            const auto world = particle_to_world * glm::vec4(position, 0.0f, 1.0f);
            // note that the velocity vector is baked into the
            // direction vector in order to save space.
            Particle p;
            p.time       = 0.0f;
            p.time_scale = math::rand(mParams.min_lifetime, mParams.max_lifetime) / mParams.max_lifetime;
            p.pointsize  = math::rand(mParams.min_point_size, mParams.max_point_size);
            p.alpha      = math::rand(mParams.min_alpha, mParams.max_alpha);
            p.position   = glm::vec2(world.x, world.y);
            p.direction  = direction * velocity;
            p.randomizer = math::rand(0.0f, 1.0f);
            state.particles.push_back(p);
        }
    }
    else if (mParams.coordinate_space == CoordinateSpace::Local)
    {
        // the emitter box uses normalized coordinates
        const auto sim_width  = mParams.max_xpos;
        const auto sim_height = mParams.max_ypos;
        const auto emitter_width  = mParams.init_rect_width * sim_width;
        const auto emitter_height = mParams.init_rect_height * sim_height;
        const auto emitter_xpos   = mParams.init_rect_xpos * sim_width;
        const auto emitter_ypos   = mParams.init_rect_ypos * sim_height;
        const auto emitter_radius = std::min(emitter_width, emitter_height) * 0.5f;
        const auto emitter_center = glm::vec2(emitter_xpos + emitter_width * 0.5f,
                                              emitter_ypos + emitter_height * 0.5f);
        const auto emitter_size  = glm::vec2(emitter_width, emitter_height);
        const auto emitter_pos   = glm::vec2(emitter_xpos, emitter_ypos);
        const auto emitter_left  = emitter_xpos;
        const auto emitter_right = emitter_xpos + emitter_width;
        const auto emitter_top   = emitter_ypos;
        const auto emitter_bot   = emitter_ypos + emitter_height;

        for (size_t i = 0; i < num; ++i)
        {
            const auto velocity = math::rand(mParams.min_velocity, mParams.max_velocity);
            glm::vec2 position;
            glm::vec2 direction;
            if (mParams.shape == EmitterShape::Rectangle)
            {
                if (mParams.placement == Placement::Inside)
                    position = emitter_pos + glm::vec2(math::rand(0.0f, emitter_width),
                                                       math::rand(0.0f, emitter_height));
                else if (mParams.placement == Placement::Center)
                    position = emitter_center;
                else if (mParams.placement == Placement::Edge)
                {
                    const auto edge = math::rand(0, 3);
                    if (edge == 0 || edge == 1)
                    {
                        position.x = edge == 0 ? emitter_left : emitter_right;
                        position.y = math::rand(emitter_top, emitter_bot);
                    }
                    else
                    {
                        position.x = math::rand(emitter_left, emitter_right);
                        position.y = edge == 2 ? emitter_top : emitter_bot;
                    }
                }
                else if (mParams.placement == Placement::Outside)
                {
                    position.x = math::rand(0.0f, sim_width);
                    position.y = math::rand(0.0f, sim_height);
                    if (position.y >= emitter_top && position.y <= emitter_bot)
                    {
                        if (position.x < emitter_center.x)
                            position.x = math::clamp(0.0f, emitter_left, position.x);
                        else position.x = math::clamp(emitter_right, sim_width, position.x);
                    }
                }
            }
            else if (mParams.shape == EmitterShape::Circle)
            {
                if (mParams.placement == Placement::Center)
                    position  = emitter_center;
                else if (mParams.placement == Placement::Inside)
                {
                    const auto x = math::rand(-1.0f, 1.0f);
                    const auto y = math::rand(-1.0f, 1.0f);
                    const auto r = math::rand(0.0f, 1.0f);
                    const auto p = glm::normalize(glm::vec2(x, y)) * emitter_radius * r;
                    position = p + emitter_pos + emitter_size * 0.5f;
                }
                else if (mParams.placement == Placement::Edge)
                {
                    const auto x = math::rand(-1.0f, 1.0f);
                    const auto y = math::rand(-1.0f, 1.0f);
                    const auto p = glm::normalize(glm::vec2(x, y)) * emitter_radius;
                    position = p + emitter_pos + emitter_size * 0.5f;
                }
                else if (mParams.placement == Placement::Outside)
                {
                    auto p = glm::vec2(math::rand(0.0f, sim_width),
                                       math::rand(0.0f, sim_height));
                    auto v = p - emitter_center;
                    if (glm::length(v) < emitter_radius)
                        p = glm::normalize(v) * emitter_radius + emitter_center;

                    position = p;
                }
            }

            if (mParams.direction == Direction::Sector)
            {
                const auto angle = math::rand(0.0f, mParams.direction_sector_size) + mParams.direction_sector_start_angle;
                direction = glm::vec2(std::cos(angle), std::sin(angle));
            }
            else if (mParams.placement == Placement::Center)
            {
                direction = glm::normalize(glm::vec2(math::rand(-1.0f, 1.0f),
                                                     math::rand(-1.0f, 1.0f)));
            }
            else if (mParams.direction == Direction::Inwards)
                direction = glm::normalize(emitter_center - position);
            else if (mParams.direction == Direction::Outwards)
                direction = glm::normalize(position - emitter_center);

            // note that the velocity vector is baked into the
            // direction vector in order to save space.
            Particle p;
            p.time       = 0.0f;
            p.time_scale = math::rand(mParams.min_lifetime, mParams.max_lifetime) / mParams.max_lifetime;
            p.pointsize  = math::rand(mParams.min_point_size, mParams.max_point_size);
            p.alpha      = math::rand(mParams.min_alpha, mParams.max_alpha);
            p.position   = position;
            p.direction  = direction *  velocity;
            p.randomizer = math::rand(0.0f, 1.0f);
            state.particles.push_back(p);
        }
    } else BUG("Unhandled particle system coordinate space.");
}
void ParticleEngineClass::KillParticle(InstanceState& state, size_t i) const
{
    const auto last = state.particles.size() - 1;
    std::swap(state.particles[i], state.particles[last]);
    state.particles.pop_back();
}

bool ParticleEngineClass::UpdateParticle(const Environment& env, InstanceState& state, size_t i, float dt) const
{
    auto& p = state.particles[i];

    p.time += dt;
    if (p.time > p.time_scale * mParams.max_lifetime)
        return false;

    const auto p0 = p.position;

    // update change in position
    if (mParams.motion == Motion::Linear)
        p.position += (p.direction * dt);
    else if (mParams.motion == Motion::Projectile)
    {
        glm::vec2 gravity = mParams.gravity;

        // transform the gravity vector associated with the particle engine
        // to world space. For example when the rendering system uses dimetric
        // rendering for some shape (we're looking at it at on a xy plane at
        // a certain angle) the gravity vector needs to be transformed so that
        // the local gravity vector makes sense in this dimetric world.
        if (env.world_matrix && mParams.coordinate_space == CoordinateSpace::Global)
        {
            if (env.editing_mode || !state.cached_world_gravity.has_value())
            {
                const auto local_gravity_dir = glm::normalize(mParams.gravity);
                const auto world_gravity_dir = glm::normalize(*env.world_matrix * glm::vec4 { local_gravity_dir, 0.0f, 0.0f });
                const auto world_gravity = glm::vec2 { world_gravity_dir.x * std::abs(mParams.gravity.x),
                                                       world_gravity_dir.y * std::abs(mParams.gravity.y) };
                state.cached_world_gravity = world_gravity;
            }
            gravity = state.cached_world_gravity.value();
        }

        p.position += (p.direction * dt);
        p.direction += (dt * gravity);
    }

    const auto& p1 = p.position;
    const auto& dp = p1 - p0;
    const auto  dd = glm::length(dp);

    // Update particle size with respect to time and distance
    p.pointsize += (dt * mParams.rate_of_change_in_size_wrt_time * p.time_scale);
    p.pointsize += (dd * mParams.rate_of_change_in_size_wrt_dist);
    if (p.pointsize <= 0.0f)
        return false;

    // update particle alpha value with respect to time and distance.
    p.alpha += (dt * mParams.rate_of_change_in_alpha_wrt_time * p.time_scale);
    p.alpha += (dt * mParams.rate_of_change_in_alpha_wrt_dist);
    if (p.alpha <= 0.0f)
        return false;
    p.alpha = math::clamp(0.0f, 1.0f, p.alpha);

    // accumulate distance approximation
    p.distance += dd;

    // todo:
    if (mParams.coordinate_space == CoordinateSpace::Global)
        return true;

    // boundary conditions.
    if (mParams.boundary == BoundaryPolicy::Wrap)
    {
        p.position.x = math::wrap(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::wrap(0.0f, mParams.max_ypos, p.position.y);
    }
    else if (mParams.boundary == BoundaryPolicy::Clamp)
    {
        p.position.x = math::clamp(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, mParams.max_ypos, p.position.y);
    }
    else if (mParams.boundary == BoundaryPolicy::Kill)
    {
        if (p.position.x < 0.0f || p.position.x > mParams.max_xpos)
            return false;
        else if (p.position.y < 0.0f || p.position.y > mParams.max_ypos)
            return false;
    }
    else if (mParams.boundary == BoundaryPolicy::Reflect)
    {
        glm::vec2 n;
        if (p.position.x <= 0.0f)
            n = glm::vec2(1.0f, 0.0f);
        else if (p.position.x >= mParams.max_xpos)
            n = glm::vec2(-1.0f, 0.0f);
        else if (p.position.y <= 0.0f)
            n = glm::vec2(0, 1.0f);
        else if (p.position.y >= mParams.max_ypos)
            n = glm::vec2(0, -1.0f);
        else return true;
        // compute new direction vector given the normal vector of the boundary
        // and then bake the velocity in the new direction
        const auto& d = glm::normalize(p.direction);
        const float v = glm::length(p.direction);
        p.direction = (d - 2 * glm::dot(d, n) * n) * v;

        // clamp the position in order to eliminate the situation
        // where the object has moved beyond the boundaries of the simulation
        // and is stuck there alternating it's direction vector
        p.position.x = math::clamp(0.0f, mParams.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, mParams.max_ypos, p.position.y);
    }
    return true;
}

void ParticleEngineInstance::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    state.line_width = 1.0;
    state.culling    = Culling::None;
    mClass->ApplyDynamicState(env, program);
}

std::string ParticleEngineInstance::GetShader(const Environment& env, const Device& device) const
{
    return mClass->GetShader(env, device);
}
std::string ParticleEngineInstance::GetShaderId(const Environment&  env) const
{
    return mClass->GetProgramId(env);
}
std::string ParticleEngineInstance::GetShaderName(const Environment& env) const
{
    return mClass->GetShaderName(env);
}
std::string ParticleEngineInstance::GetGeometryName(const Environment& env) const
{
    return mClass->GetGeometryName(env);
}

bool ParticleEngineInstance::Upload(const Environment& env, Geometry& geometry) const
{
    return mClass->Upload(env, mState, geometry);
}

void ParticleEngineInstance::Update(const Environment& env, float dt)
{
    mClass->Update(env, mState, dt);
}

bool ParticleEngineInstance::IsAlive() const
{
    return mClass->IsAlive(mState);
}

bool ParticleEngineInstance::IsDynamic(const Environment& env) const
{
    return true;
}

void ParticleEngineInstance::Restart(const Environment& env)
{
    mClass->Restart(env, mState);
}

void ParticleEngineInstance::Execute(const Environment& env, const Command& cmd)
{
    if (cmd.name == "EmitParticles")
    {
        if (const auto* count = base::SafeFind(cmd.args, std::string("count")))
        {
            if (const auto* val = std::get_if<int>(count))
                mClass->Emit(env, mState, *val);
        }
        else
        {
            const auto& params = mClass->GetParams();
            mClass->Emit(env, mState, (int)params.num_particles);
        }
    }
    else WARN("No such particle engine command. [cmd='%1']", cmd.name);
}

void TileBatch::ApplyDynamicState(const Environment& env, Program& program, RasterState& raster) const
{
    const auto pixel_scale = std::min(env.pixel_ratio.x, env.pixel_ratio.y);

    const auto shape = ResolveTileShape();

    // Choose a point on the tile for projecting the tile onto the
    // rendering surface.

    // if the tile shape is square we're rendering point sprites which
    // are always centered around the vertex when rasterized by OpenGL.
    // This means that the projection plays a role when choosing the vertex
    // around which to rasterize the point when using point sprites.
    //
    //  a) square + dimetric
    //    In this case the tile's top left corner maps directly to the
    //    center of the square tile when rendered.
    //
    //  b) square + axis aligned.
    //    In this case the center of the tile yields the center of the
    //    square when projected.
    //
    glm::vec3 tile_point_offset = {0.0f, 0.0f, 0.0f};
    if (mProjection == Projection::AxisAligned && shape == TileShape::Square)
        tile_point_offset = mTileWorldSize * glm::vec3{0.5f, 0.5f, 0.0f};
    else if (mProjection == Projection::Dimetric && shape == TileShape::Rectangle)
        tile_point_offset = mTileWorldSize * glm::vec3{1.0f, 1.0f, 0.0f};  // bottom right corner is the basis for the billboard
    else if (mProjection == Projection::AxisAligned && shape == TileShape::Rectangle)
        tile_point_offset = mTileWorldSize * glm::vec3{0.5f, 1.0f, 0.0f}; // middle of the bottom edge

    glm::vec2 tile_render_size = mTileRenderSize;
    if (shape == TileShape::Square)
        tile_render_size *= pixel_scale;

    program.SetUniform("kTileWorldSize", mTileWorldSize);
    // This is the offset in units to add to the top left tile corner (row, col)
    // for projecting the tile into the render surface coordinates.
    program.SetUniform("kTilePointOffset", tile_point_offset);
    program.SetUniform("kTileRenderSize", tile_render_size);
    program.SetUniform("kTileTransform", *env.proj_matrix * *env.view_matrix);
    program.SetUniform("kTileCoordinateSpaceTransform", *env.model_matrix);
}

std::string TileBatch::GetShader(const Environment& env, const Device& device) const
{
    // the shader uses dummy varyings vParticleAlpha, vParticleRandomValue
    // and vTexCoord. Even though we're now rendering GL_POINTS this isn't
    // a particle vertex shader. However, if a material shader refers to those
    // varyings we might get GLSL program build errors on some platforms.

    const auto shape = ResolveTileShape();

    constexpr const auto*  square_tile_source = R"(
attribute vec3 aTilePosition;

uniform mat4 kTileTransform;
uniform mat4 kTileCoordinateSpaceTransform;

uniform vec3 kTileWorldSize;
uniform vec3 kTilePointOffset;
uniform vec2 kTileRenderSize;

varying float vParticleAlpha;
varying float vParticleRandomValue;
varying vec2 vTexCoord;

void VertexShaderMain()
{
  // transform tile row,col index into a tile position in units in the x,y plane,
  vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

  vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

  gl_Position = kTileTransform * vertex;
  gl_Position.z = 0.0;

  gl_PointSize = kTileRenderSize.x;

  // dummy out.
  vParticleAlpha = 1.0;
  vParticleRandomValue = 1.0;
}
)";


    constexpr const auto* rectangle_tile_source = R"(
attribute vec3 aTilePosition;
attribute vec2 aTileCorner;

uniform mat4 kTileTransform;
uniform mat4 kTileCoordinateSpaceTransform;

uniform vec3 kTileWorldSize;
uniform vec3 kTilePointOffset;
uniform vec2 kTileRenderSize;

varying float vParticleAlpha;
varying float vParticleRandomValue;
varying vec2 vTexCoord;

void VertexShaderMain()
{
  // transform tile col,row index into a tile position in tile world units in the tile x,y plane
  vec3 tile = aTilePosition * kTileWorldSize + kTilePointOffset;

  // transform the tile from tile space to rendering space
  vec4 vertex = kTileCoordinateSpaceTransform * vec4(tile.xyz, 1.0);

  // pull the corner vertices apart by adding a corner offset
  // for each vertex towards some corner/direction away from the
  // center point
  vertex.xy += (aTileCorner * kTileRenderSize);

  gl_Position = kTileTransform * vertex;
  gl_Position.z = 0.0;

  vTexCoord = aTileCorner + vec2(0.5, 1.0);

  // dummy out
  vParticleAlpha = 1.0;
  vParticleRandomValue = 1.0;
}
)";
    if (shape == TileShape::Square)
        return square_tile_source;
    else if (shape == TileShape::Rectangle)
        return rectangle_tile_source;
    else BUG("Missing tile batch shader source.");
    return "";
}

std::string TileBatch::GetShaderId(const Environment& env) const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
        return "square-tile-batch-program";
    else if (shape == TileShape::Rectangle)
        return "rectangle-tile-batch-program";
    else BUG("Missing tile batch shader id.");
    return "";
}

std::string TileBatch::GetShaderName(const Environment& env) const
{
    const auto shape = ResolveTileShape();

    if (shape == TileShape::Square)
        return "SquareTileBatchShader";
    else if (shape == TileShape::Rectangle)
        return "RectangleTileBatchShader";
    else BUG("Missing tile batch shader name.");
    return "";
}

std::string TileBatch::GetGeometryName(const Environment& env) const
{
    return "tile-buffer";
}

bool TileBatch::Upload(const Environment& env, Geometry& geometry) const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
    {
        using TileVertex = Tile;
        static const VertexLayout layout(sizeof(TileVertex), {
            {"aTilePosition", 0, 3, 0, offsetof(TileVertex, pos)},
        });

        geometry.SetVertexBuffer(mTiles, Geometry::Usage::Stream);
        geometry.SetVertexLayout(layout);
        geometry.ClearDraws();
        geometry.AddDrawCmd(Geometry::DrawType::Points);
    }
    else if (shape == TileShape::Rectangle)
    {
        struct TileVertex {
            Vec3 position;
            Vec2 corner;
        };
        static const VertexLayout layout(sizeof(TileVertex), {
            {"aTilePosition", 0, 3, 0, offsetof(TileVertex, position)},
            {"aTileCorner",   0, 2, 0, offsetof(TileVertex, corner)}
        });
        std::vector<TileVertex> vertices;
        vertices.reserve(6 * mTiles.size());
        for (const auto& tile : mTiles)
        {
            const TileVertex top_left  = {tile.pos, {-0.5f, -1.0f}};
            const TileVertex top_right = {tile.pos, { 0.5f, -1.0f}};
            const TileVertex bot_left  = {tile.pos, {-0.5f,  0.0f}};
            const TileVertex bot_right = {tile.pos, { 0.5f,  0.0f}};
            vertices.push_back(top_left);
            vertices.push_back(bot_left);
            vertices.push_back(bot_right);

            vertices.push_back(top_left);
            vertices.push_back(bot_right);
            vertices.push_back(top_right);
        }
        geometry.ClearDraws();
        geometry.SetVertexBuffer(vertices, Geometry::Usage::Stream);
        geometry.SetVertexLayout(layout);
        geometry.AddDrawCmd(Geometry::DrawType::Triangles);
    }
    else BUG("Unknown tile shape!");
    return true;
}

Drawable::Primitive TileBatch::GetPrimitive() const
{
    const auto shape = ResolveTileShape();
    if (shape == TileShape::Square)
        return Primitive::Points;
    else if (shape == TileShape::Rectangle)
        return Primitive::Triangles;
    else BUG("Unknown tile batch tile shape");
    return Primitive::Points;
}

void DynamicLine3D::ApplyDynamicState(const Environment& environment, Program& program, RasterState& state) const
{
    program.SetUniform("kProjectionMatrix",  *environment.proj_matrix);
    program.SetUniform("kModelViewMatrix", *environment.view_matrix * *environment.model_matrix);
}

std::string DynamicLine3D::GetShader(const Environment& environment, const Device& device) const
{
    return MakeSimple3DVertexShader(device);
}

std::string DynamicLine3D::GetShaderId(const Environment& environment) const
{
    return "simple-3D-vertex-shader";
}

std::string DynamicLine3D::GetShaderName(const Environment& environment) const
{
    return "Simple3DVertexShader";
}

std::string DynamicLine3D::GetGeometryName(const Environment& environment) const
{
    return "line-buffer";
}

bool DynamicLine3D::Upload(const Environment& environment, Geometry& geometry) const
{
    // it's also possible to draw without generating geometry by simply having
    // the two line end points as uniforms in the vertex shader and then using
    // gl_VertexID (which is not available in GL ES2) to distinguish the vertex
    // invocation and use that ID to choose the right vertex end point.
    std::vector<Vertex3D> vertices;
    Vertex3D a;
    a.aPosition = Vec3 { mPointA.x, mPointA.y, mPointA.z };
    Vertex3D b;
    b.aPosition = Vec3 { mPointB.x, mPointB.y, mPointB.z };

    vertices.push_back(a);
    vertices.push_back(b);

    geometry.SetVertexBuffer(vertices, Geometry::Usage::Stream);
    geometry.SetVertexLayout(GetVertexLayout<Vertex3D>());
    geometry.ClearDraws();
    geometry.AddDrawCmd(Geometry::DrawType::Lines);
    return true;
}

void DebugDrawableBase::ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const
{
    mDrawable->ApplyDynamicState(env, program, state);
}

std::string DebugDrawableBase::GetShader(const Environment& env, const Device& device) const
{
    return mDrawable->GetShader(env, device);
}
std::string DebugDrawableBase::GetShaderId(const Environment& env) const
{
    return mDrawable->GetShaderId(env);
}
std::string DebugDrawableBase::GetShaderName(const Environment& env) const
{
    return mDrawable->GetShaderName(env);
}
std::string DebugDrawableBase::GetGeometryName(const Environment& env) const
{
    std::string name;
    name += mDrawable->GetGeometryName(env);
    name += base::ToString(mFeature);
    return name;
}

bool DebugDrawableBase::Upload(const Environment& env, Geometry& geom) const
{
    if (mDrawable->GetPrimitive() != Primitive::Triangles)
    {
        return mDrawable->Upload(env, geom);
    }

    if (mFeature == Feature::Wireframe)
    {
        GeometryBuffer buffer;
        buffer.SetDataHash(geom.GetDataHash());

        if (!mDrawable->Upload(env, buffer))
            return false;

        if (buffer.HasData())
        {
            GeometryBuffer wireframe;
            CreateWireframe(buffer, wireframe);

            wireframe.Transfer(geom);
            geom.SetDataHash(buffer.GetDataHash());
        }
    }
    return true;
}

bool DebugDrawableBase::IsDynamic(const Environment& env) const
{
    return mDrawable->IsDynamic(env);
}

std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass)
{
    // factory function based on type switching.
    // Alternative way would be to have a virtual function in the DrawableClass
    // but this would have 2 problems: creating based shared_ptr of the drawable
    // class would require shared_from_this which I consider to be bad practice and
    // it'd cause some problems at some point.
    // secondly it'd create a circular dependency between class and the instance types
    // which is going to cause some problems at some point.
    const auto type = klass->GetType();

    if (type == DrawableClass::Type::SimpleShape)
        return std::make_unique<SimpleShapeInstance>(std::static_pointer_cast<const SimpleShapeClass>(klass));
    else if (type == DrawableClass::Type::ParticleEngine)
        return std::make_unique<ParticleEngineInstance>(std::static_pointer_cast<const ParticleEngineClass>(klass));
    else if (type == DrawableClass::Type::Polygon)
        return std::make_unique<PolygonMeshInstance>(std::static_pointer_cast<const PolygonMeshClass>(klass));
    else BUG("Unhandled drawable class type");
    return nullptr;
}

} // namespace
