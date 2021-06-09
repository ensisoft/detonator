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

#include <functional> // for hash
#include <cmath>

#include "base/utility.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/drawable.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/geometry.h"
#include "graphics/resource.h"

namespace gfx
{
namespace detail {

// static
Shader* GeometryBase::GetShader(Device& device)
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
// static
Geometry* ArrowGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

   Geometry* geom = nullptr;

    if (style == Style::Outline)
    {
        geom = device.FindGeometry("ArrowOutline");
        if (!geom)
        {
            const Vertex verts[] = {
                {{0.0f, -0.25f}, {0.0f, 0.25f}},
                {{0.0f, -0.75f}, {0.0f, 0.75f}},
                {{0.7f, -0.75f}, {0.7f, 0.75f}},
                {{0.7f, -1.0f}, {0.7f, 1.0f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{0.7f, -0.0f}, {0.7f, 0.0f}},
                {{0.7f, -0.25f}, {0.7f, 0.25f}},
            };
            geom = device.MakeGeometry("ArrowOutline");
            geom->SetVertexBuffer(verts, 7);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (style == Style::Solid)
    {
        geom = device.FindGeometry("Arrow");
        if (!geom)
        {
            const Vertex verts[] = {
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
            geom = device.MakeGeometry("Arrow");
            geom->SetVertexBuffer(verts, 9);
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        }
    }
    else if (style == Style::Wireframe)
    {
        geom = device.FindGeometry("ArrowWireframe");
        if (!geom)
        {
            const Vertex verts[] = {
                // body
                {{0.0f, -0.25f}, {0.0f, 0.25f}},
                {{0.0f, -0.75f}, {0.0f, 0.75f}},
                {{0.0f, -0.75f}, {0.0f, 0.75f}},
                {{0.7f, -0.25f}, {0.7f, 0.25f}},
                {{0.7f, -0.25f}, {0.7f, 0.25f}},
                {{0.0f, -0.25f}, {0.0f, 0.25f}},

                // body
                {{0.0f, -0.75f}, {0.0f, 0.75f}},
                {{0.7f, -0.75f}, {0.7f, 0.75f}},
                {{0.7f, -0.75f}, {0.7f, 0.75f}},
                {{0.7f, -0.25f}, {0.0f, 0.25f}},

                // arrow head
                {{0.7f, -0.0f}, {0.7f, 0.0f}},
                {{0.7f, -1.0f}, {0.7f, 1.0f}},
                {{0.7f, -1.0f}, {0.7f, 1.0f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{1.0f, -0.5f}, {1.0f, 0.5f}},
                {{0.7f, -0.0f}, {0.7f, 0.0f}},
            };
            geom = device.MakeGeometry("ArrowWireframe");
            geom->SetVertexBuffer(verts, 16);
            geom->AddDrawCmd(Geometry::DrawType::Lines);
        }
    }
    return geom;
}

// static
Geometry* LineGeometry::Generate(const Environment& env, Style style, Device& device)
{
    Geometry* geom = device.FindGeometry("LineSegment");
    if (geom == nullptr)
    {
        // horizontal line.
        const gfx::Vertex verts[2] = {
                {{0.0f,  -0.5f}, {0.0f, 0.5f}},
                {{1.0f,  -0.5f}, {1.0f, 0.5f}}
        };
        geom = device.MakeGeometry("LineSegment");
        geom->SetVertexBuffer(verts, 2);
        geom->AddDrawCmd(Geometry::DrawType::Lines);
    }
    return geom;
}

// static
Geometry* CapsuleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    // todo LOD information
    const auto slices = 50;
    const auto radius = 0.1f;
    const auto max_slice = style == Style::Solid ? slices + 1 : slices;
    const auto angle_increment = math::Pi / slices;

    const auto* name = style == Style::Outline ? "CapsuleOutline" :
                      (style == Style::Wireframe ? "CapsuleWireframe" : "Capsule");

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        geom = device.MakeGeometry(name);

        std::vector<Vertex> vs;
        auto offset = 0;

        // semi-circle at the left end.
        Vertex left_center;
        left_center.aPosition.x =  radius;
        left_center.aPosition.y = -0.5f;
        left_center.aTexCoord.x =  radius;
        left_center.aTexCoord.y =  0.5f;
        if (style == Style::Solid)
            vs.push_back(left_center);

        float left_angle = math::Pi * 0.5;
        for (unsigned i=0; i<max_slice; ++i)
        {
            const auto x = std::cos(left_angle) * radius;
            const auto y = std::sin(left_angle) * radius;
            Vertex v;
            v.aPosition.x =  radius + x;
            v.aPosition.y = -0.5f + y;
            v.aTexCoord.x =  radius + x;
            v.aTexCoord.y =  0.5f - y;
            vs.push_back(v);

            left_angle += angle_increment;

            if (style == Style::Wireframe)
            {
                const auto x = std::cos(left_angle) * radius;
                const auto y = std::sin(left_angle) * radius;
                Vertex v;
                v.aPosition.x =  radius + x;
                v.aPosition.y = -0.5f + y;
                v.aTexCoord.x =  radius + x;
                v.aTexCoord.y =  0.5f - y;
                vs.push_back(v);
                vs.push_back(left_center);
            }
        }
        if (style == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size()-offset);
        else if (style == Style::Wireframe)
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset, vs.size()-offset);

        if (style != Style::Outline)
        {
            // center box.
            const Vertex box[6] = {
                {{0.1f, -0.4f}, {0.1f, 0.4f}},
                {{0.1f, -0.6f}, {0.1f, 0.6f}},
                {{0.9f, -0.6f}, {0.9f, 0.6f}},

                {{0.1f, -0.4f}, {0.1f, 0.4f}},
                {{0.9f, -0.6f}, {0.9f, 0.6f}},
                {{0.9f, -0.4f}, {0.9f, 0.4f}}
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
                geom->AddDrawCmd(Geometry::DrawType::Triangles, offset, 6);
            }
            else
            {
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset + 0, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset + 3, 3);
            }
        }

        offset = vs.size();

        // semi circle at the right end
        Vertex right_center;
        right_center.aPosition.x =  1.0f - radius;
        right_center.aPosition.y = -0.5f;
        right_center.aTexCoord.x =  1.0f - radius;
        right_center.aTexCoord.y =  0.5f;
        if (style == Style::Solid)
            vs.push_back(right_center);

        const float right_angle_increment = math::Pi / slices;
        float right_angle = math::Pi * -0.5;
        for (unsigned i=0; i<max_slice; ++i)
        {
            const auto x = std::cos(right_angle) * radius;
            const auto y = std::sin(right_angle) * radius;
            Vertex v;
            v.aPosition.x =  1.0f - radius + x;
            v.aPosition.y = -0.5f + y;
            v.aTexCoord.x =  1.0f - radius + x;
            v.aTexCoord.y =  0.5f - y;
            vs.push_back(v);

            right_angle += right_angle_increment;

            if (style == Style::Wireframe)
            {
                const auto x = std::cos(right_angle) * radius;
                const auto y = std::sin(right_angle) * radius;
                Vertex v;
                v.aPosition.x =  1.0f - radius + x;
                v.aPosition.y = -0.5f + y;
                v.aTexCoord.x =  1.0f - radius + x;
                v.aTexCoord.y =  0.5f - y;
                vs.push_back(v);
                vs.push_back(right_center);
            }
        }
        if (style == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size()-offset);
        else if (style == Style::Wireframe)
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset, vs.size()-offset);

        if (style == Style::Outline)
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);

        geom->SetVertexBuffer(std::move(vs));
    }
    return geom;
}

// static
Geometry* SemiCircleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 50;
    const auto* name = style == Style::Outline   ? "SemiCircleOutline" :
                       (style == Style::Wireframe ? "SemiCircleWireframe" : "SemiCircle");

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        std::vector<Vertex> vs;

        // center point for triangle fan.
        Vertex center;
        center.aPosition.x =  0.5;
        center.aPosition.y = -0.5;
        center.aTexCoord.x =  0.5;
        center.aTexCoord.y =  0.5;
        if (style == Style::Solid)
        {
            vs.push_back(center);
        }

        const float angle_increment = (float)math::Pi / slices;
        const auto max_slice = style == Style::Wireframe ? slices : slices + 1;
        float angle = 0.0f;

        for (unsigned i=0; i<max_slice; ++i)
        {
            const auto x = std::cos(angle) * 0.5f;
            const auto y = std::sin(angle) * 0.5f;
            Vertex v;
            v.aPosition.x = x + 0.5f;
            v.aPosition.y = y - 0.5f;
            v.aTexCoord.x = x + 0.5f;
            v.aTexCoord.y = 1.0 - (y + 0.5f);
            vs.push_back(v);

            angle += angle_increment;

            if (style == Style::Wireframe)
            {
                const auto x = std::cos(angle) * 0.5f;
                const auto y = std::sin(angle) * 0.5f;
                Vertex v;
                v.aPosition.x = x + 0.5f;
                v.aPosition.y = y - 0.5f;
                v.aTexCoord.x = x + 0.5f;
                v.aTexCoord.y = 1.0 - (y + 0.5f);
                vs.push_back(v);
                vs.push_back(center);
            }
        }
        geom = device.MakeGeometry(name);
        geom->SetVertexBuffer(&vs[0], vs.size());
    }
    geom->ClearDraws();

    if (style == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    else if (style == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    return geom;

}

// static
Geometry* CircleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 100;
    const auto* name = style == Style::Outline   ? "CircleOutline" :
                      (style == Style::Wireframe ? "CircleWireframe" : "Circle");

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        std::vector<Vertex> vs;

        // center point for triangle fan.
        Vertex center;
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
            Vertex v;
            v.aPosition.x = x + 0.5f;
            v.aPosition.y = y - 0.5f;
            v.aTexCoord.x = x + 0.5f;
            v.aTexCoord.y = 1.0 - (y + 0.5f);
            vs.push_back(v);

            angle += angle_increment;

            if (style == Style::Wireframe)
            {
                const auto x = std::cos(angle) * 0.5f;
                const auto y = std::sin(angle) * 0.5f;
                Vertex v;
                v.aPosition.x = x + 0.5f;
                v.aPosition.y = y - 0.5f;
                v.aTexCoord.x = x + 0.5f;
                v.aTexCoord.y = 1.0 - (y + 0.5f);
                vs.push_back(v);
                vs.push_back(center);
            }
        }
        geom = device.MakeGeometry(name);
        geom->SetVertexBuffer(&vs[0], vs.size());

    }
    geom->ClearDraws();

    if (style == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (style == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    else if (style == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    return geom;
}

// static
Geometry* RectangleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    Geometry* geom = nullptr;

    if (style == Style::Outline)
    {
        geom = device.FindGeometry("RectangleOutline");
        if (geom == nullptr)
        {
            const Vertex verts[] = {
                { {0.0f,  0.0f}, {0.0f, 0.0f} },
                { {0.0f, -1.0f}, {0.0f, 1.0f} },
                { {1.0f, -1.0f}, {1.0f, 1.0f} },
                { {1.0f,  0.0f}, {1.0f, 0.0f} }
            };
            geom = device.MakeGeometry("RectangleOutline");
            geom->SetVertexBuffer(verts, 4);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (style == Style::Solid || style == Style::Wireframe)
    {
        geom = device.FindGeometry("Rectangle");
        if (geom == nullptr)
        {
            const Vertex verts[6] = {
                { {0.0f,  0.0f}, {0.0f, 0.0f} },
                { {0.0f, -1.0f}, {0.0f, 1.0f} },
                { {1.0f, -1.0f}, {1.0f, 1.0f} },

                { {0.0f,  0.0f}, {0.0f, 0.0f} },
                { {1.0f, -1.0f}, {1.0f, 1.0f} },
                { {1.0f,  0.0f}, {1.0f, 0.0f} }
            };
            geom = device.MakeGeometry("Rectangle");
            geom->SetVertexBuffer(verts, 6);
        }
        geom->ClearDraws();
        if (style == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        else if (style == Style::Wireframe)
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    return geom;
}

// static
Geometry* IsoscelesTriangleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    Geometry* geom = device.FindGeometry("IsoscelesTriangle");
    if (!geom)
    {
        const Vertex verts[3] = {
                { {0.5f,  0.0f}, {0.5f, 0.0f} },
                { {0.0f, -1.0f}, {0.0f, 1.0f} },
                { {1.0f, -1.0f}, {1.0f, 1.0f} }
        };
        geom = device.MakeGeometry("IsoscelesTriangle");
        geom->SetVertexBuffer(verts, 3);
    }
    geom->ClearDraws();
    if (style == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    else if (style == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    return geom;
}

// static
Geometry* RightTriangleGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    Geometry* geom = device.FindGeometry("RightTriangle");
    if (!geom)
    {
        const Vertex verts[3] = {
                { {0.0f,  0.0f}, {0.0f, 0.0f} },
                { {0.0f, -1.0f}, {0.0f, 1.0f} },
                { {1.0f, -1.0f}, {1.0f, 1.0f} }
        };
        geom = device.MakeGeometry("RightTriangle");
        geom->SetVertexBuffer(verts, 3);
    }
    geom->ClearDraws();
    if (style == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::Triangles);
    else if (style == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    else if (style == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    return geom;
}

// static
Geometry* TrapezoidGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    Geometry* geom = nullptr;
    if (style == Style::Outline)
    {
        geom = device.FindGeometry("TrapezoidOutline");
        if (!geom)
        {
            const Vertex verts[] = {
                    { {0.2f,  0.0f}, {0.2f, 0.0f} },
                    { {0.0f, -1.0f}, {0.0f, 1.0f} },
                    { {1.0f, -1.0f}, {1.0f, 1.0f} },
                    { {0.8f,  0.0f}, {0.8f, 0.0f} }
            };

            geom = device.MakeGeometry("TrapezoidOutline");
            geom->SetVertexBuffer(verts, 4);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (style == Style::Solid || style == Style::Wireframe)
    {
        geom = device.FindGeometry("Trapezoid");
        if (!geom)
        {
            const Vertex verts[] = {
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
            geom = device.MakeGeometry("Trapezoid");
            geom->SetVertexBuffer(verts, 12);
        }
        geom->ClearDraws();
        if (style == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        else if (style == Style::Wireframe)
        {
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 0, 3);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 3, 3);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 6, 3);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 9, 3);
        }
    }
    return geom;
}

// static
Geometry* ParallelogramGeometry::Generate(const Environment& env, Style style, Device& device)
{
    if (style == Style::Points)
        return nullptr;

    Geometry* geom = nullptr;
    if (style == Style::Outline)
    {
        geom = device.FindGeometry("ParallelogramOutline");
        if (!geom)
        {
            const Vertex verts[] = {
                    { {0.2f,  0.0f}, {0.2f, 0.0f} },
                    { {0.0f, -1.0f}, {0.0f, 1.0f} },
                    { {0.8f, -1.0f}, {0.8f, 1.0f} },
                    { {1.0f,  0.0f}, {1.0f, 0.0f} }
            };
            geom = device.MakeGeometry("ParallelogramOutline");
            geom->SetVertexBuffer(verts, 4);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (style == Style::Solid || style == Style::Wireframe)
    {
        geom = device.FindGeometry("Parallelogram");
        if (!geom)
        {
            const Vertex verts[] = {
                    {{0.2f,  0.0f}, {0.2f, 0.0f}},
                    {{0.0f, -1.0f}, {0.0f, 1.0f}},
                    {{0.8f, -1.0f}, {0.8f, 1.0f}},

                    {{0.8f, -1.0f}, {0.8f, 1.0f}},
                    {{1.0f,  0.0f}, {1.0f, 0.0f}},
                    {{0.2f,  0.0f}, {0.2f, 0.0f}}
            };
            geom = device.MakeGeometry("Parallelogram");
            geom->SetVertexBuffer(verts, 6);
        }
        geom->ClearDraws();
        if (style == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        else if (style == Style::Wireframe)
        {
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 0, 3);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop, 3, 3);
        }
    }
    return geom;
}

} // detail

Shader* RoundRectangleClass::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* RoundRectangleClass::Upload(Drawable::Style style, Device& device) const
{
    using Style = Drawable::Style;

    if (style == Style::Points)
        return nullptr;

    const auto r = mRadius;
    const auto slices    = 20;
    const auto increment = (float)(math::Pi * 0.5  / slices); // each corner is a quarter circle, i.e. half pi rad

    Geometry* geom = nullptr;

    // each corner contains one quadrant of a circle with radius r
    struct CornerOrigin {
        // x, y pos of the origin
        float x, y;
    } corners[4] = {
        {1.0f-r,      -r}, // top right
        {     r,      -r}, // top left
        {     r, -1.0f+r}, // bottom left
        {1.0f-r, -1.0f+r}, // bottom right
    };

    if (style == Style::Outline)
    {
        geom = device.FindGeometry("RoundRectOutline");
        if (geom == nullptr)
        {
            // outline of the box body
            std::vector<Vertex> vs = {
                // left box
                {{0.0f,      -r}, {0.0f,      r}},
                {{0.0f, -1.0f+r}, {0.0f, 1.0f-r}},
                // center box
                {{r,       0.0f}, {r,      0.0f}},
                {{1.0f-r,  0.0f}, {1.0f-r, 0.0f}},
                {{r,      -1.0f}, {r,      1.0f}},
                {{1.0f-r, -1.0f}, {r,      1.0f}},
                // right box
                {{1.0f,      -r}, {1.0f,        r}},
                {{1.0f, -1.0f+r}, {1.0f,   1.0f-r}},
            };

            // generate corners
            for (int i=0; i<4; ++i)
            {
                float angle = math::Pi * 0.5 * i;
                for (unsigned s = 0; s<=slices; ++s)
                {
                    const auto x0 = std::cos(angle) * mRadius;
                    const auto y0 = std::sin(angle) * mRadius;
                    Vertex v0, v1;
                    v0.aPosition.x = corners[i].x + x0;
                    v0.aPosition.y = corners[i].y + y0;
                    v0.aTexCoord.x = corners[i].x + x0;
                    v0.aTexCoord.y = (corners[i].y + y0) * -1.0f;

                    angle += increment;

                    const auto x1 = std::cos(angle) * mRadius;
                    const auto y1 = std::sin(angle) * mRadius;
                    v1.aPosition.x = corners[i].x + x1;
                    v1.aPosition.y = corners[i].y + y1;
                    v1.aTexCoord.x = corners[i].x + x1;
                    v1.aTexCoord.y = (corners[i].y + y1) * -1.0f;

                    vs.push_back(v0);
                    vs.push_back(v1);
                }
            }
            geom = device.MakeGeometry("RoundRectOutline");
            geom->SetVertexBuffer(std::move(vs));
            geom->AddDrawCmd(Geometry::DrawType::Lines);
        }
    }
    else if (style == Style::Solid || style == Style::Wireframe)
    {
        geom = device.FindGeometry(style == Style::Solid ? "RoundRect" : "RoundRectWireframe");
        if (geom == nullptr)
        {
            geom = device.MakeGeometry(style == Style::Solid ? "RoundRect" : "RoundRectWireframe");

            // center body
            std::vector<Vertex> vs = {
                // left box
                {{0.0f,      -r}, {0.0f,      r}},
                {{0.0f, -1.0f+r}, {0.0f, 1.0f-r}},
                {{r,    -1.0f+r}, {r,    1.0f-r}},
                {{r,    -1.0f+r}, {r,    1.0f-r}},
                {{r,         -r}, {r,         r}},
                {{0.0f,      -r}, {0.0f,      r}},

                // center box
                {{r,       0.0f}, {r,      0.0f}},
                {{r,      -1.0f}, {r,      1.0f}},
                {{1.0f-r, -1.0f}, {1.0f-r, 1.0f}},
                {{1.0f-r, -1.0f}, {1.0f-r, 1.0f}},
                {{1.0f-r,  0.0f}, {1.0f-r, 0.0f}},
                {{r,       0.0f}, {r,      0.0f}},

                // right box.
                {{1.0f-r,       -r}, {1.0f-r,      r}},
                {{1.0f-r,  -1.0f+r}, {1.0f-r, 1.0f-r}},
                {{1.0f,    -1.0f+r}, {1.0f,   1.0f-r}},
                {{1.0f,    -1.0f+r}, {1.0f,   1.0f-r}},
                {{1.0f,         -r}, {1.0f,        r}},
                {{1.0f-r,       -r}, {1.0f-r,      r}},
            };

            if (style == Style::Solid)
            {
                geom->AddDrawCmd(Geometry::DrawType::Triangles, 0, 18);
            }
            else
            {
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 0, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 3, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 6, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 9, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 12, 3);
                geom->AddDrawCmd(Geometry::DrawType::LineLoop, 15, 3);
            }

            // generate corners
            for (int i=0; i<4; ++i)
            {
                const auto offset = vs.size();

                Vertex center;
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
                    const auto x = std::cos(angle) * mRadius;
                    const auto y = std::sin(angle) * mRadius;
                    Vertex v;
                    v.aPosition.x = corners[i].x + x;
                    v.aPosition.y = corners[i].y + y;
                    v.aTexCoord.x = corners[i].x + x;
                    v.aTexCoord.y = (corners[i].y + y) * -1.0f;
                    vs.push_back(v);

                    angle += increment;
                    if (style == Style::Wireframe)
                    {
                        const auto x = std::cos(angle) * mRadius;
                        const auto y = std::sin(angle) * mRadius;
                        Vertex v;
                        v.aPosition.x = corners[i].x + x;
                        v.aPosition.y = corners[i].y + y;
                        v.aTexCoord.x = corners[i].x + x;
                        v.aTexCoord.y = (corners[i].y + y)  * -1.0f;
                        vs.push_back(v);
                        vs.push_back(center);
                    }
                }
                if (style == Style::Solid)
                {
                    geom->AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size() - offset);
                }
                else if (style == Style::Wireframe)
                {
                    geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset, vs.size() - offset);
                }
            }
            geom->SetVertexBuffer(std::move(vs));
        }
    }
    return geom;
}

void RoundRectangleClass::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}
void RoundRectangleClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("radius", mRadius);
}

bool RoundRectangleClass::LoadFromJson(const data::Reader& data)
{
    if (!data.Read("id", &mId) ||
        !data.Read("radius", &mRadius))
        return false;
    return true;
}

Shader* GridClass::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* GridClass::Upload(Device& device) const
{
    // use the content properties to generate a name for the
    // gpu side geometry.
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
    hash = base::hash_combine(hash, mBorderLines);
    const auto& name = std::to_string(hash);

    Geometry* geom = device.FindGeometry(name);
    if (!geom)
    {
        std::vector<Vertex> verts;

        const float yadvance = 1.0f / (mNumHorizontalLines + 1);
        const float xadvance = 1.0f / (mNumVerticalLines + 1);
        for (unsigned i=1; i<=mNumVerticalLines; ++i)
        {
            const float x = i * xadvance;
            const Vertex line[2] = {
                {{x,  0.0f}, {x, 0.0f}},
                {{x, -1.0f}, {x, 1.0f}}
            };
            verts.push_back(line[0]);
            verts.push_back(line[1]);
        }
        for (unsigned i=1; i<=mNumHorizontalLines; ++i)
        {
            const float y = i * yadvance;
            const Vertex line[2] = {
                {{0.0f, y*-1.0f}, {0.0f, y}},
                {{1.0f, y*-1.0f}, {1.0f, y}},
            };
            verts.push_back(line[0]);
            verts.push_back(line[1]);
        }
        if (mBorderLines)
        {
            const Vertex corners[4] = {
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
        geom = device.MakeGeometry(name);
        geom->SetVertexBuffer(std::move(verts));
        geom->AddDrawCmd(Geometry::DrawType::Lines);
    }
    return geom;
}

void GridClass::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

void GridClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("vertical_lines", mNumVerticalLines);
    data.Write("horizontal_lines", mNumHorizontalLines);
    data.Write("border_lines", mBorderLines);
}

bool GridClass::LoadFromJson(const data::Reader& data)
{
    if (!data.Read("id", &mId) ||
        !data.Read("vertical_lines", &mNumVerticalLines) ||
        !data.Read("horizontal_lines", &mNumHorizontalLines) ||
        !data.Read("border_lines", &mBorderLines))
        return false;
    return true;
}

Shader* PolygonClass::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* PolygonClass::Upload(Device& device) const
{
    Geometry* geom = nullptr;

    if (mStatic)
    {
        geom = device.FindGeometry(GetName());
        if (!geom)
        {
            geom = device.MakeGeometry(GetName());
            geom->SetVertexBuffer(mVertices);
            for (const auto& cmd : mDrawCommands)
            {
                geom->AddDrawCmd(cmd.type, cmd.offset, cmd.count);
            }
        }
    }
    else
    {
        geom = device.FindGeometry("DynamicPolygon");
        if (!geom)
        {
            geom = device.MakeGeometry("DynamicPolygon");
        }
        geom->SetVertexBuffer(mVertices);
        geom->ClearDraws();
        for (const auto& cmd : mDrawCommands)
        {
            geom->AddDrawCmd(cmd.type, cmd.offset, cmd.count);
        }
    }
    return geom;
}

void PolygonClass::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

std::size_t PolygonClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStatic);
    for (const auto& vertex : mVertices)
    {
        hash = base::hash_combine(hash, vertex.aTexCoord.x);
        hash = base::hash_combine(hash, vertex.aTexCoord.y);
        hash = base::hash_combine(hash, vertex.aPosition.x);
        hash = base::hash_combine(hash, vertex.aPosition.y);
    }
    for (const auto& draw : mDrawCommands)
    {
        hash = base::hash_combine(hash, draw.type);
        hash = base::hash_combine(hash, draw.count);
        hash = base::hash_combine(hash, draw.offset);
    }
    return hash;
}

void PolygonClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("static", mStatic);
    for (const auto& v : mVertices)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("x", v.aPosition.x);
        chunk->Write("y", v.aPosition.y);
        chunk->Write("s", v.aTexCoord.x);
        chunk->Write("t", v.aTexCoord.y);
        data.AppendChunk("vertices", std::move(chunk));
    }
    for (const auto& cmd : mDrawCommands)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("type", cmd.type);
        chunk->Write("offset", (unsigned)cmd.offset);
        chunk->Write("count",  (unsigned)cmd.count);
        data.AppendChunk("draws", std::move(chunk));
    }
}

bool PolygonClass::LoadFromJson(const data::Reader& data)
{
    auto ret = PolygonClass::FromJson(data);
    if (!ret.has_value())
        return false;
    auto& val = ret.value();
    mId           = std::move(val.mId);
    mVertices     = std::move(val.mVertices);
    mDrawCommands = std::move(val.mDrawCommands);
    mStatic       = val.mStatic;
    return true;
}

// static
std::optional<PolygonClass> PolygonClass::FromJson(const data::Reader& data)
{
    PolygonClass ret;
    if (!data.Read("id", &ret.mId) ||
        !data.Read("static", &ret.mStatic))
        return std::nullopt;

    for (unsigned i=0; i<data.GetNumChunks("vertices"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vertices", i);
        float x, y, s, t;
        if (!chunk->Read("x", &x) ||
            !chunk->Read("y", &y) ||
            !chunk->Read("s", &s) ||
            !chunk->Read("t", &t))
            return std::nullopt;
        Vertex vertex;
        vertex.aPosition.x = x;
        vertex.aPosition.y = y;
        vertex.aTexCoord.x = s;
        vertex.aTexCoord.y = t;
        ret.mVertices.push_back(vertex);
    }
    for (unsigned i=0; i<data.GetNumChunks("draws"); ++i)
    {
        const auto& chunk = data.GetReadChunk("draws", i);
        unsigned offset = 0;
        unsigned count  = 0;
        DrawCommand cmd;
        if (!chunk->Read("type", &cmd.type) ||
            !chunk->Read("offset", &offset) ||
            !chunk->Read("count",  &count))
            return std::nullopt;
        cmd.offset = offset;
        cmd.count  = count;
        ret.mDrawCommands.push_back(cmd);
    }
    return ret;
}

void PolygonClass::EraseVertex(size_t index)
{
    ASSERT(index < mVertices.size());
    auto it = mVertices.begin();
    std::advance(it, index);
    mVertices.erase(it);
    // remove the vertex from the draw commands.
    for (size_t i=0; i<mDrawCommands.size();)
    {
        auto& cmd = mDrawCommands[i];
        if (index >= cmd.offset && index < cmd.offset + cmd.count) {
            if (--cmd.count == 0)
            {
                auto it = mDrawCommands.begin();
                std::advance(it, i);
                mDrawCommands.erase(it);
                continue;
            }
        }
        else if (index < cmd.offset)
            cmd.offset--;
        ++i;
    }
    mName.clear();
}

void PolygonClass::InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index)
{
    ASSERT(cmd_index < mDrawCommands.size());
    ASSERT(index <= mDrawCommands[cmd_index].count);

    // figure out the index where the put the new vertex in the vertex
    // array.
    auto& cmd = mDrawCommands[cmd_index];
    cmd.count = cmd.count + 1;

    const auto vertex_index = cmd.offset + index;
    mVertices.insert(mVertices.begin() + vertex_index, vertex);

    for (size_t i=0; i<mDrawCommands.size(); ++i)
    {
        if (i == cmd_index)
            continue;
        auto& cmd = mDrawCommands[i];
        if (vertex_index <= cmd.offset)
            cmd.offset++;
    }
    mName.clear();
}

const size_t PolygonClass::FindDrawCommand(size_t vertex_index) const
{
    for (size_t i=0; i<mDrawCommands.size(); ++i)
    {
        const auto& cmd = mDrawCommands[i];
        // first and last index in the vertex buffer
        // for this draw command.
        const auto vertex_index_first = cmd.offset;
        const auto vertex_index_last  = cmd.offset + cmd.count;
        if (vertex_index >= vertex_index_first && vertex_index < vertex_index_last)
            return i;
    }
    BUG("no draw command found.");
}

std::string PolygonClass::GetName() const
{
    if (!mName.empty())
        return mName;
    mName = std::to_string(GetHash());
    return mName;
}

Shader* KinematicsParticleEngineClass::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("particles.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("particles.glsl");
        shader->CompileFile("shaders/es2/particles.glsl");
    }
    return shader;
}

Geometry* KinematicsParticleEngineClass::Upload(const Drawable::Environment& env, const InstanceState& state, Device& device) const
{
    Geometry* geom = device.FindGeometry("particle-buffer");
    if (!geom)
    {
        geom = device.MakeGeometry("particle-buffer");
    }
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
    static VertexLayout layout(sizeof(ParticleVertex), {
        {"aPosition", 0, 2, 0, offsetof(ParticleVertex, aPosition)},
        {"aData",     0, 4, 0, offsetof(ParticleVertex, aData)}
    });

    std::vector<ParticleVertex> verts;
    for (const auto& p : state.particles)
    {
        // Convert the particle coordinates to lower right
        // quadrant coordinates.
        ParticleVertex v;
        v.aPosition.x = p.position.x / mParams.max_xpos;
        v.aPosition.y = p.position.y / mParams.max_ypos * -1.0f;
        // copy the per particle data into the data vector for the fragment shader.
        v.aData.x = p.pointsize >= 0.0f ? p.pointsize * pixel_scaler : 0.0f;
        // abusing texcoord here to provide per particle random value.
        // we can use this to simulate particle rotation for example
        // (if the material supports it)
        v.aData.y = p.randomizer;
        // Use the particle data to pass the per particle alpha.
        v.aData.z = p.alpha;
        verts.push_back(v);
    }

    geom->SetVertexBuffer(std::move(verts));
    geom->SetVertexLayout(layout);
    geom->ClearDraws();
    geom->AddDrawCmd(Geometry::DrawType::Points);
    return geom;
}

void KinematicsParticleEngineClass::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/particles.glsl");
}

// Update the particle simulation.
void KinematicsParticleEngineClass::Update(InstanceState& state, float dt) const
{
    // In case particles become heavy on the CPU here are some ways to try
    // to mitigate the issue:
    // - Reduce the number of particles in the content (i.e. use less particles
    //   in animations etc.)
    // - Share complete particle engines between assets, i.e. instead of each
    //   animation (for example space ship) using it's own particle engine
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

    // update each particle
    for (size_t i=0; i<state.particles.size();)
    {
        if (UpdateParticle(state, i, dt))
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
        const auto num_particles_needed = num_particles_always - num_particles_now;
        InitParticles(state, num_particles_needed);
    }
    else if (mParams.mode == SpawnPolicy::Continuous)
    {
        // the number of particles is taken as the rate of particles per
        // second. fractionally cumulate partciles and then
        // spawn when we have some number non-fractional particles.
        state.hatching += mParams.num_particles * dt;
        const auto num = size_t(state.hatching);
        InitParticles(state, num);
        state.hatching -= num;
    }
}

// ParticleEngine implementation.
bool KinematicsParticleEngineClass::IsAlive(const InstanceState& state) const
{
    if (mParams.mode == SpawnPolicy::Continuous)
        return true;
    return !state.particles.empty();
}

// ParticleEngine implementation. Restart the simulation
// with the previous parameters.
void KinematicsParticleEngineClass::Restart(InstanceState& state) const
{
    state.particles.clear();
    state.time = 0.0f;
    state.hatching = 0.0f;
    InitParticles(state, size_t(mParams.num_particles));
}

void KinematicsParticleEngineClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("motion", mParams.motion);
    data.Write("mode", mParams.mode);
    data.Write("boundary", mParams.boundary);
    data.Write("num_particles", mParams.num_particles);
    data.Write("min_lifetime", mParams.min_lifetime);
    data.Write("max_lifetime", mParams.max_lifetime);
    data.Write("max_xpos", mParams.max_xpos);
    data.Write("max_ypos", mParams.max_ypos);
    data.Write("init_rect_xpos", mParams.init_rect_xpos);
    data.Write("init_rect_ypos", mParams.init_rect_ypos);
    data.Write("init_rect_width", mParams.init_rect_width);
    data.Write("init_rect_height", mParams.init_rect_height);
    data.Write("min_velocity", mParams.min_velocity);
    data.Write("max_velocity", mParams.max_velocity);
    data.Write("direction_sector_start_angle", mParams.direction_sector_start_angle);
    data.Write("direction_sector_size", mParams.direction_sector_size);
    data.Write("min_point_size", mParams.min_point_size);
    data.Write("max_point_size", mParams.max_point_size);
    data.Write("min_alpha", mParams.min_alpha);
    data.Write("max_alpha", mParams.max_alpha);
    data.Write("growth_over_time", mParams.rate_of_change_in_size_wrt_time);
    data.Write("growth_over_dist", mParams.rate_of_change_in_size_wrt_dist);
    data.Write("alpha_over_time", mParams.rate_of_change_in_alpha_wrt_time);
    data.Write("alpha_over_dist", mParams.rate_of_change_in_alpha_wrt_dist);
    data.Write("gravity", mParams.gravity);
}

bool KinematicsParticleEngineClass::LoadFromJson(const data::Reader& data)
{
    auto ret = FromJson(data);
    if (!ret.has_value())
        return false;
    const auto& val = ret.value();
    mParams = val.mParams;
    return true;
}

// static
std::optional<KinematicsParticleEngineClass> KinematicsParticleEngineClass::FromJson(const data::Reader& data)
{
    std::string id;
    Params params;
    if (!data.Read("id", &id) ||
        !data.Read("motion", &params.motion) ||
        !data.Read("mode", &params.mode) ||
        !data.Read("boundary", &params.boundary) ||
        !data.Read("num_particles", &params.num_particles) ||
        !data.Read("min_lifetime", &params.min_lifetime) ||
        !data.Read("max_lifetime", &params.max_lifetime) ||
        !data.Read("max_xpos", &params.max_xpos) ||
        !data.Read("max_ypos", &params.max_ypos) ||
        !data.Read("init_rect_xpos", &params.init_rect_xpos) ||
        !data.Read("init_rect_ypos", &params.init_rect_ypos) ||
        !data.Read("init_rect_width", &params.init_rect_width) ||
        !data.Read("init_rect_height", &params.init_rect_height) ||
        !data.Read("min_velocity", &params.min_velocity) ||
        !data.Read("max_velocity", &params.max_velocity) ||
        !data.Read("direction_sector_start_angle", &params.direction_sector_start_angle) ||
        !data.Read("direction_sector_size", &params.direction_sector_size) ||
        !data.Read("min_point_size", &params.min_point_size) ||
        !data.Read("max_point_size", &params.max_point_size) ||
        !data.Read("min_alpha", &params.min_alpha) ||
        !data.Read("max_alpha", &params.max_alpha) ||
        !data.Read("growth_over_time", &params.rate_of_change_in_size_wrt_time) ||
        !data.Read("growth_over_dist", &params.rate_of_change_in_size_wrt_dist) ||
        !data.Read("alpha_over_time", &params.rate_of_change_in_alpha_wrt_time) ||
        !data.Read("alpha_over_dist", &params.rate_of_change_in_alpha_wrt_dist) ||
        !data.Read("gravity", &params.gravity))
            return std::nullopt;
    KinematicsParticleEngineClass ret;
    ret.mId = std::move(id);
    ret.SetParams(params);
    return ret;
}

std::size_t KinematicsParticleEngineClass::GetHash() const
{
    static_assert(std::is_trivially_copyable<Params>::value);
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);

    const auto* ptr = reinterpret_cast<const unsigned char*>(&mParams);
    const auto  len = sizeof(mParams);
    for (size_t i=0; i<len; ++i)
        hash = base::hash_combine(hash, ptr[i]);

    return hash;
}

void KinematicsParticleEngineClass::InitParticles(InstanceState& state, size_t num) const
{
    for (size_t i=0; i<num; ++i)
    {
        const auto velocity = math::rand(mParams.min_velocity, mParams.max_velocity);
        const auto initx = math::rand(0.0f, mParams.init_rect_width);
        const auto inity = math::rand(0.0f, mParams.init_rect_height);
        const auto angle = math::rand(0.0f, mParams.direction_sector_size) +
            mParams.direction_sector_start_angle;

        Particle p;
        p.lifetime  = math::rand(mParams.min_lifetime, mParams.max_lifetime);
        p.pointsize = math::rand(mParams.min_point_size, mParams.max_point_size);
        p.alpha     = math::rand(mParams.min_alpha, mParams.max_alpha);
        p.position  = glm::vec2(mParams.init_rect_xpos + initx, mParams.init_rect_ypos + inity);
        // note that the velocity vector is baked into the
        // direction vector in order to save space.
        p.direction = glm::vec2(std::cos(angle), std::sin(angle)) * velocity;
        p.randomizer = math::rand(0.0f, 1.0f);
        state.particles.push_back(p);
    }
}
void KinematicsParticleEngineClass::KillParticle(InstanceState& state, size_t i) const
{
    const auto last = state.particles.size() - 1;
    std::swap(state.particles[i], state.particles[last]);
    state.particles.pop_back();
}

bool KinematicsParticleEngineClass::UpdateParticle(InstanceState& state, size_t i, float dt) const
{
    auto& p = state.particles[i];

    // countdown to end of lifetime
    p.lifetime -= dt;
    if (p.lifetime <= 0.0f)
        return false;

    const auto p0 = p.position;

    // update change in position
    if (mParams.motion == Motion::Linear)
        p.position += (p.direction * dt);
    else if (mParams.motion == Motion::Projectile)
    {
        p.position += (p.direction * dt);
        p.direction += (dt * mParams.gravity);
    }

    const auto& p1 = p.position;
    const auto& dp = p1 - p0;
    const auto  dd = glm::length(dp);

    // Update particle size with respect to time and distance
    p.pointsize += (dt * mParams.rate_of_change_in_size_wrt_time);
    p.pointsize += (dd * mParams.rate_of_change_in_size_wrt_dist);
    if (p.pointsize <= 0.0f)
        return false;

    // update particle alpha value with respect to time and distance.
    p.alpha += (dt * mParams.rate_of_change_in_alpha_wrt_time);
    p.alpha += (dt * mParams.rate_of_change_in_alpha_wrt_dist);
    if (p.alpha <= 0.0f)
        return false;
    p.alpha = math::clamp(0.0f, 1.0f, p.alpha);

    // accumulate distance approximation
    p.distance += dd;

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

std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass)
{
    // factory function based on type switching.
    // Alternative way would be to have a virtual function in the DrawableClass
    // but this would have 2 problems: creating based shared_ptr of the drawable
    // class would require shared_from_this which I consider to be bad practice and
    // it'd cause some problems at some point.
    // secondly it'd create a circular dependency between class and the instance types
    // which is going to cause some problems at some point.

    switch (klass->GetType())
    {
        case DrawableClass::Type::Arrow:
            return std::make_unique<Arrow>();
        case DrawableClass::Type::Line:
            return std::make_unique<Line>();
        case DrawableClass::Type::Capsule:
            return std::make_unique<Capsule>();
        case DrawableClass::Type::Circle:
            return std::make_unique<Circle>();
        case DrawableClass::Type::SemiCircle:
            return std::make_unique<SemiCircle>();
        case DrawableClass::Type::Rectangle:
            return std::make_unique<Rectangle>();
        case DrawableClass::Type::RoundRectangle:
            return std::make_unique<RoundRectangle>(std::static_pointer_cast<const RoundRectangleClass>(klass));
        case DrawableClass::Type::IsoscelesTriangle:
            return std::make_unique<IsoscelesTriangle>();
        case DrawableClass::Type::RightTriangle:
            return std::make_unique<RightTriangle>();
        case DrawableClass::Type::Trapezoid:
            return std::make_unique<Trapezoid>();
        case DrawableClass::Type::Parallelogram:
            return std::make_unique<Parallelogram>();
        case DrawableClass::Type::Grid:
            return std::make_unique<Grid>(std::static_pointer_cast<const GridClass>(klass));
        case DrawableClass::Type::KinematicsParticleEngine:
            return std::make_unique<KinematicsParticleEngine>(std::static_pointer_cast<const KinematicsParticleEngineClass>(klass));
        case DrawableClass::Type::Polygon:
            return std::make_unique<Polygon>(std::static_pointer_cast<const PolygonClass>(klass));
    }
    BUG("Unhandled drawable class type");
    return {};
}

} // namespace
