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

#pragma once

#include "config.h"

#include <cstddef>
#include <vector>

namespace gfx
{
    // 2 float vector data object. Use glm::vec2 for math.
    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Vertex for 2D drawing on the XY plane.
    struct Vertex {
        // Coordinate / position of the vertex in the model space.
        // Model space is a 2D space on XY plane where X varies from
        // 0.0f to 1.0f (left to right when mapped onto render target)
        // and Y varies from 0.0f to -1.0f ( from top to bottom when
        // mapped onto render target).
        Vec2 aPosition;
        // Texture coordinate for the vertex. Texture coordinates
        // are normalized and can vary beyond 0.0f-1.0f range in
        // which case they either get clamped or wrapped.
        // Texture coordinates on the X axis vary from 0.0f to 1.0f
        // i.e. from left to right on horizontal axis.
        // Texture coordinates on the Y axis vary from 0.0f to 1.0f
        // i.e. from top to bottom on the vertical axis. (0.0f is
        // the *top* row of pixels and 1.0f is the *bottom* row of
        // pixels).
        Vec2 aTexCoord;
        // Arbitrary data that is accompanied by the vertex. Used for
        // example by the particle system to provide particle sizes/alphas
        // to the shader programs.
        Vec2 aData;
    };

    // Encapsulate information about a particular geometry
    // and how how that geometry is to be rendered and
    // rasterized. A geometry object contains a set of vertex
    // data and then multiple draw commands each command
    // addressing some subset of the vertices.
    class Geometry
    {
    public:
        // Define how the geometry is to be rasterized.
        enum class DrawType {
            // Draw the given vertices as triangles, i.e.
            // each 3 vertices make a single triangle.
            Triangles,
            // Draw each given vertex as a separate point
            Points,
            // Draw a series of triangles all connected to the
            // first vertex
            TriangleFan,
            // Draw the vertices as a series of connected lines
            // where each pair of adjacent vertices are connected
            // by a line.
            // In this draw the line width setting applies.
            Lines,
            // Draw a line between the given vertices looping back
            // from the last vertex to the first.
            LineLoop
        };

        virtual ~Geometry() = default;
        // Clear previous draw commands.
        virtual void ClearDraws() = 0;
        // Add a draw command that starts at offset 0 and covers the whole
        // current vertex buffer (i.e. count = num of vertices)
        virtual void AddDrawCmd(DrawType type) = 0;
        // Add a draw command for some particular set of vertices within
        // the current vertex buffer.
        virtual void AddDrawCmd(DrawType type, size_t offset, size_t count) = 0;
        // Set (request) the line width to be using when rasterizing
        // the geometry as a series of lines.
        virtual void SetLineWidth(float width) = 0;
        // Update the geometry object's data buffer contents.
        virtual void Update(const Vertex* vertices, std::size_t count) = 0;
        // Update the geometry objects' data buffer contents
        // with the given vector of data.
        virtual void Update(const std::vector<Vertex>& vertices) = 0;
        // Update the geoemtry object's data buffer contents
        // by moving the contents of verts into geometry object.
        virtual void Update(std::vector<Vertex>&& vertices) = 0;
    private:
    };
} // namespace
