// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include <functional> // for hash
#include <cmath>

#include "base/utility.h"
#include "drawable.h"
#include "device.h"
#include "shader.h"
#include "geometry.h"
#include "resource.h"

namespace gfx
{

Shader* Arrow::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Arrow::Upload(Device& device) const
{
    if (mStyle == Style::Points)
        return nullptr;

   Geometry* geom = nullptr;

    if (mStyle == Style::Outline)
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
            geom->Update(verts, 7);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (mStyle == Style::Solid)
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
            geom->Update(verts, 9);
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        }
    }
    else if (mStyle == Style::Wireframe)
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
            geom->Update(verts, 16);
            geom->AddDrawCmd(Geometry::DrawType::Lines);
        }
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void Arrow::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Line::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Line::Upload(Device& device) const
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
        geom->Update(verts, 2);
        geom->AddDrawCmd(Geometry::DrawType::Lines);
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}
void Line::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Circle::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Circle::Upload(Device& device) const
{
    if (mStyle == Style::Points)
        return nullptr;

    // todo: we could use some information here about the
    // eventual transform on the screen and use that to compute
    // some kind of "LOD" value for figuring out how many slices we should have.
    const auto slices = 100;
    const auto* name = mStyle == Style::Outline   ? "CircleOutline" :
                      (mStyle == Style::Wireframe ? "CircleWireframe" : "Circle");

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
        if (mStyle == Style::Solid)
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

            if (mStyle == Style::Wireframe)
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
        geom->Update(&vs[0], vs.size());

    }
    geom->ClearDraws();

    if (mStyle == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::TriangleFan);
    else if (mStyle == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    else if (mStyle == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void Circle::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Rectangle::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* Rectangle::Upload(Device& device) const
{
    if (mStyle == Style::Points)
        return nullptr;

    Geometry* geom = nullptr;

    if (mStyle == Style::Outline)
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
            geom->Update(verts, 4);
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
        }
    }
    else if (mStyle == Style::Solid || mStyle == Style::Wireframe)
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
            geom->Update(verts, 6);
        }
        geom->ClearDraws();
        if (mStyle == Style::Solid)
            geom->AddDrawCmd(Geometry::DrawType::Triangles);
        else if (mStyle == Style::Wireframe)
            geom->AddDrawCmd(Geometry::DrawType::LineLoop);
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void Rectangle::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* RoundRectangle::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* RoundRectangle::Upload(Device& device) const
{
    if (mStyle == Style::Points)
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

    if (mStyle == Style::Outline)
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
            geom->Update(std::move(vs));
            geom->AddDrawCmd(Geometry::DrawType::Lines);
        }
    }
    else if (mStyle == Style::Solid || mStyle == Style::Wireframe)
    {
        geom = device.FindGeometry(mStyle == Style::Solid ? "RoundRect" : "RoundRectWireframe");
        if (geom == nullptr)
        {
            geom = device.MakeGeometry(mStyle == Style::Solid ? "RoundRect" : "RoundRectWireframe");

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

            if (mStyle == Style::Solid)
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

                if (mStyle == Style::Solid)
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
                    if (mStyle == Style::Wireframe)
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
                if (mStyle == Style::Solid)
                {
                    geom->AddDrawCmd(Geometry::DrawType::TriangleFan, offset, vs.size() - offset);
                }
                else if (mStyle == Style::Wireframe)
                {
                    geom->AddDrawCmd(Geometry::DrawType::LineLoop, offset, vs.size() - offset);
                }
            }
            geom->Update(std::move(vs));
        }
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void RoundRectangle::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Triangle::GetShader(Device& device) const
{
    Shader* s = device.FindShader("vertex_array.glsl");
    if (s == nullptr || !s->IsValid())
    {
        if (s == nullptr)
            s = device.MakeShader("vertex_array.glsl");
        if (!s->CompileFile("shaders/es2/vertex_array.glsl"))
            return nullptr;
    }
    return s;
}

Geometry* Triangle::Upload(Device& device) const
{
    if (mStyle == Style::Points)
        return nullptr;

    Geometry* geom = device.FindGeometry("Triangle");
    if (!geom)
    {
        const Vertex verts[3] = {
            { {0.5f,  0.0f}, {0.5f, 0.0f} },
            { {0.0f, -1.0f}, {0.0f, 1.0f} },
            { {1.0f, -1.0f}, {1.0f, 1.0f} }
        };
        geom = device.MakeGeometry("Triangle");
        geom->Update(verts, 3);
    }
    geom->SetLineWidth(mLineWidth);
    geom->ClearDraws();
    if (mStyle == Style::Solid)
        geom->AddDrawCmd(Geometry::DrawType::Triangles);
    else if (mStyle == Style::Outline)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    else if (mStyle == Style::Wireframe)
        geom->AddDrawCmd(Geometry::DrawType::LineLoop); // this is not a mistake.
    return geom;
}

void Triangle::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Grid::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}

Geometry* Grid::Upload(Device& device) const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mNumVerticalLines);
    hash = base::hash_combine(hash, mNumHorizontalLines);
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
        geom->Update(std::move(verts));
        geom->AddDrawCmd(Geometry::DrawType::Lines);
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void Grid::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

Shader* Polygon::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("vertex_array.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("vertex_array.glsl");
        shader->CompileFile("shaders/es2/vertex_array.glsl");
    }
    return shader;
}
Geometry* Polygon::Upload(Device& device) const
{
    Geometry* geom = nullptr;

    if (mStatic)
    {
        geom = device.FindGeometry(GetName());
        if (!geom)
        {
            geom = device.MakeGeometry(GetName());
            geom->Update(mVertices);
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
        geom->Update(mVertices);
        geom->ClearDraws();
        for (const auto& cmd : mDrawCommands)
        {
            geom->AddDrawCmd(cmd.type, cmd.offset, cmd.count);
        }
    }
    geom->SetLineWidth(mLineWidth);
    return geom;
}

void Polygon::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/vertex_array.glsl");
}

nlohmann::json Polygon::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "static", mStatic);
    for (const auto& v : mVertices)
    {
        nlohmann::json js = {
            {"x", v.aPosition.x},
            {"y", v.aPosition.y},
            {"s", v.aTexCoord.x},
            {"t", v.aTexCoord.y}
        };
        json["vertices"].push_back(std::move(js));
    }
    for (const auto& cmd : mDrawCommands)
    {
        nlohmann::json js;
        base::JsonWrite(js, "type", cmd.type);
        base::JsonWrite(js, "offset", (unsigned)cmd.offset);
        base::JsonWrite(js, "count",  (unsigned)cmd.count);
        json["draws"].push_back(std::move(js));
    }
    return json;
}

// static
std::optional<Polygon> Polygon::FromJson(const nlohmann::json& object)
{
    Polygon ret;
    if (!base::JsonReadSafe(object, "static", &ret.mStatic))
        return std::nullopt;

    if (object.contains("vertices"))
    {
        for (const auto& js : object["vertices"].items())
        {
            const auto& obj = js.value();
            float x, y, s, t;
            if (!base::JsonReadSafe(obj, "x", &x) ||
                !base::JsonReadSafe(obj, "y", &y) ||
                !base::JsonReadSafe(obj, "s", &s) ||
                !base::JsonReadSafe(obj, "t", &t))
                return std::nullopt;
            Vertex vertex;
            vertex.aPosition.x = x;
            vertex.aPosition.y = y;
            vertex.aTexCoord.x = s;
            vertex.aTexCoord.y = t;
            ret.mVertices.push_back(vertex);
        }
    }
    if (object.contains("draws"))
    {
        for (const auto& js : object["draws"].items())
        {
            const auto& obj = js.value();
            unsigned offset = 0;
            unsigned count  = 0;
            DrawCommand cmd;
            if (!base::JsonReadSafe(obj, "type", &cmd.type) ||
                !base::JsonReadSafe(obj, "offset", &offset) ||
                !base::JsonReadSafe(obj, "count",  &count))
                return std::nullopt;
            cmd.offset = offset;
            cmd.count  = count;
            ret.mDrawCommands.push_back(cmd);
        }
    }
    return ret;
}

void Polygon::EraseVertex(size_t index)
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

void Polygon::InsertVertex(const Vertex& vertex, size_t index)
{
    ASSERT(index <= mVertices.size());

    auto it = mVertices.begin();
    std::advance(it, index);
    mVertices.insert(it, vertex);

    for (size_t i=0; i<mDrawCommands.size(); ++i)
    {
        auto& cmd = mDrawCommands[i];
        if (index >= cmd.offset && index <= cmd.offset + cmd.count)
            cmd.count++;
        else if (index < cmd.offset)
            cmd.offset++;
    }
    mName.clear();
}

std::size_t Polygon::GetHash() const
{
    size_t hash = 0;
    for (const auto& vertex : mVertices)
    {
        hash = base::hash_combine(hash, vertex.aPosition.x);
        hash = base::hash_combine(hash, vertex.aPosition.y);
        hash = base::hash_combine(hash, vertex.aTexCoord.x);
        hash = base::hash_combine(hash, vertex.aTexCoord.y);
    }
    for (const auto& cmd : mDrawCommands)
    {
        hash = base::hash_combine(hash, cmd.type);
        hash = base::hash_combine(hash, cmd.offset);
        hash = base::hash_combine(hash, cmd.count);
    }
    return hash;
}

std::string Polygon::GetName() const
{
    if (!mName.empty())
        return mName;
    mName = std::to_string(GetHash());
    return mName;
}

Shader* KinematicsParticleEngine::GetShader(Device& device) const
{
    Shader* shader = device.FindShader("particles.glsl");
    if (shader == nullptr)
    {
        shader = device.MakeShader("particles.glsl");
        shader->CompileFile("shaders/es2/particles.glsl");
    }
    return shader;
}
// Drawable implementation. Upload particles to the device.
Geometry* KinematicsParticleEngine::Upload(Device& device) const
{
    Geometry* geom = device.FindGeometry("particle-buffer");
    if (!geom)
    {
        geom = device.MakeGeometry("particle-buffer");
    }
    std::vector<Vertex> verts;
    for (const auto& p : mParticles)
    {
        // in order to use vertex_arary.glsl we need to convert the points to
        // lower right quadrant coordinates.
        Vertex v;
        v.aPosition.x = p.position.x / mParams.max_xpos;
        v.aPosition.y = p.position.y / mParams.max_ypos * -1.0f;
        // abusing texcoord here to pass per particle point size
        // to the fragment shader.
        v.aTexCoord.x = p.pointsize >= 0.0f ? p.pointsize : 0.0f;
        // abusing texcoord here to provide per particle random value.
        // we can use this to simulate particle rotation for example
        // (if the material supports it)
        v.aTexCoord.y = p.randomizer;
        verts.push_back(v);
    }

    geom->Update(std::move(verts));
    geom->ClearDraws();
    geom->AddDrawCmd(Geometry::DrawType::Points);
    return geom;
}

void KinematicsParticleEngine::Pack(ResourcePacker* packer) const
{
    packer->PackShader(this, "shaders/es2/particles.glsl");
}

// Update the particle simulation.
void KinematicsParticleEngine::Update(float dt)
{
    mTime += dt;

    // update each particle
    for (size_t i=0; i<mParticles.size();)
    {
        if (UpdateParticle(i, dt))
        {
            ++i;
            continue;
        }
        KillParticle(i);
    }

    // Spawn new particles if needed.
    if (mParams.mode == SpawnPolicy::Maintain)
    {
        const auto num_particles_always = size_t(mParams.num_particles);
        const auto num_particles_now = mParticles.size();
        const auto num_particles_needed = num_particles_always - num_particles_now;
        InitParticles(num_particles_needed);
    }
    else if (mParams.mode == SpawnPolicy::Continuous)
    {
        // the number of particles is taken as the rate of particles per
        // second. fractionally cumulate partciles and then
        // spawn when we have some number non-fractional particles.
        mNumParticlesHatching += mParams.num_particles * dt;
        const auto num = size_t(mNumParticlesHatching);
        InitParticles(num);
        mNumParticlesHatching -= num;
    }
}

// ParticleEngine implementation.
bool KinematicsParticleEngine::IsAlive() const
{
    if (mParams.mode == SpawnPolicy::Continuous)
        return true;
    return !mParticles.empty();
}

// ParticleEngine implementation. Restart the simulation
// with the previous parameters.
void KinematicsParticleEngine::Restart()
{
    mParticles.clear();
    mTime = 0.0f;
    mNumParticlesHatching = 0;

    InitParticles(size_t(mParams.num_particles));
}

nlohmann::json KinematicsParticleEngine::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "motion", mParams.motion);
    base::JsonWrite(json, "mode", mParams.mode);
    base::JsonWrite(json, "boundary", mParams.boundary);
    base::JsonWrite(json, "num_particles", mParams.num_particles);
    base::JsonWrite(json, "min_lifetime", mParams.min_lifetime);
    base::JsonWrite(json, "max_lifetime", mParams.max_lifetime);
    base::JsonWrite(json, "max_xpos", mParams.max_xpos);
    base::JsonWrite(json, "max_ypos", mParams.max_ypos);
    base::JsonWrite(json, "init_rect_xpos", mParams.init_rect_xpos);
    base::JsonWrite(json, "init_rect_ypos", mParams.init_rect_ypos);
    base::JsonWrite(json, "init_rect_width", mParams.init_rect_width);
    base::JsonWrite(json, "init_rect_height", mParams.init_rect_height);
    base::JsonWrite(json, "min_velocity", mParams.min_velocity);
    base::JsonWrite(json, "max_velocity", mParams.max_velocity);
    base::JsonWrite(json, "direction_sector_start_angle", mParams.direction_sector_start_angle);
    base::JsonWrite(json, "direction_sector_size", mParams.direction_sector_size);
    base::JsonWrite(json, "min_point_size", mParams.min_point_size);
    base::JsonWrite(json, "max_point_size", mParams.max_point_size);
    base::JsonWrite(json, "growth_over_time", mParams.rate_of_change_in_size_wrt_time);
    base::JsonWrite(json, "growth_over_dist", mParams.rate_of_change_in_size_wrt_dist);
    return json;
}

// static
std::optional<KinematicsParticleEngine> KinematicsParticleEngine::FromJson(const nlohmann::json& object)
{
    Params params;
    if (!base::JsonReadSafe(object, "motion", &params.motion) ||
        !base::JsonReadSafe(object, "mode", &params.mode) ||
        !base::JsonReadSafe(object, "boundary", &params.boundary) ||
        !base::JsonReadSafe(object, "num_particles", &params.num_particles) ||
        !base::JsonReadSafe(object, "min_lifetime", &params.min_lifetime) ||
        !base::JsonReadSafe(object, "max_lifetime", &params.max_lifetime) ||
        !base::JsonReadSafe(object, "max_xpos", &params.max_xpos) ||
        !base::JsonReadSafe(object, "max_ypos", &params.max_ypos) ||
        !base::JsonReadSafe(object, "init_rect_xpos", &params.init_rect_xpos) ||
        !base::JsonReadSafe(object, "init_rect_ypos", &params.init_rect_ypos) ||
        !base::JsonReadSafe(object, "init_rect_width", &params.init_rect_width) ||
        !base::JsonReadSafe(object, "init_rect_height", &params.init_rect_height) ||
        !base::JsonReadSafe(object, "min_velocity", &params.min_velocity) ||
        !base::JsonReadSafe(object, "max_velocity", &params.max_velocity) ||
        !base::JsonReadSafe(object, "direction_sector_start_angle", &params.direction_sector_start_angle) ||
        !base::JsonReadSafe(object, "direction_sector_size", &params.direction_sector_size) ||
        !base::JsonReadSafe(object, "min_point_size", &params.min_point_size) ||
        !base::JsonReadSafe(object, "max_point_size", &params.max_point_size) ||
        !base::JsonReadSafe(object, "growth_over_time", &params.rate_of_change_in_size_wrt_time) ||
        !base::JsonReadSafe(object, "growth_over_dist", &params.rate_of_change_in_size_wrt_dist))
            return std::nullopt;
    return KinematicsParticleEngine(params);
}

std::size_t KinematicsParticleEngine::GetHash() const
{
    static_assert(std::is_trivially_copyable<Params>::value);
    size_t hash = 0;

    const auto* ptr = reinterpret_cast<const unsigned char*>(&mParams);
    const auto  len = sizeof(mParams);
    for (size_t i=0; i<len; ++i)
        hash ^= std::hash<unsigned char>()(ptr[i]);

    return hash;
}

void KinematicsParticleEngine::InitParticles(size_t num)
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
        p.position  = glm::vec2(mParams.init_rect_xpos + initx, mParams.init_rect_ypos + inity);
        // note that the velocity vector is baked into the
        // direction vector in order to save space.
        p.direction = glm::vec2(std::cos(angle), std::sin(angle)) * velocity;
        p.randomizer = math::rand(0.0f, 1.0f);
        mParticles.push_back(p);
    }
}
void KinematicsParticleEngine::KillParticle(size_t i)
{
    const auto last = mParticles.size() - 1;
    std::swap(mParticles[i], mParticles[last]);
    mParticles.pop_back();
}

bool KinematicsParticleEngine::UpdateParticle(size_t i, float dt)
{
    auto& p = mParticles[i];

    // countdown to end of lifetime
    p.lifetime -= dt;
    if (p.lifetime <= 0.0f)
        return false;

    const auto p0 = p.position;

    // update change in position
    if (mParams.motion == Motion::Linear)
        p.position += (p.direction * dt);

    const auto& p1 = p.position;
    const auto& dp = p1 - p0;
    const auto  dd = glm::length(dp);

    // update change in size with respect to time.
    p.pointsize += (dt * mParams.rate_of_change_in_size_wrt_time);
    // update change in size with respect to distance
    p.pointsize += (dd * mParams.rate_of_change_in_size_wrt_dist);
    if (p.pointsize <= 0.0f)
        return false;
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


} // namespace
