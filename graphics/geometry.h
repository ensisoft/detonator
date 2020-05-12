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
    struct Vec2 {
        float x;// = 0.0f;
        float y;// = 0.0f;
    };

    struct Vertex {
        Vec2 aPosition;
        Vec2 aTexCoord;
    };

    // Encapsulate information about a particular geometry
    // and how how that geometry is to be rendered and
    // rasterized.
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
            // Draw the vertices as a series of conneced lines
            // where each pair of adjacent vertices are connected
            // by a line.
            // In this draw the line width setting applies.
            Lines,
            // Draw a line between the given vertices looping back
            // from the last vertex to the first.
            LineLoop
        };

        virtual ~Geometry() = default;
        // Get the current draw type
        virtual DrawType GetDrawType() const = 0;
        // Set the expected type for the geometry when drawn.
        virtual void SetDrawType(DrawType type) = 0;
        // Set (request) the line width to be using when rasterizing
        // the geometry as a series of lines.
        virtual void SetLineWidth(float width) = 0;
        // Update the geometry object's data buffer contents.
        virtual void Update(const Vertex* verts, std::size_t count) = 0;
        // Update the geometry objects' data buffer contents
        // with the given vector of data.
        virtual void Update(const std::vector<Vertex>& verts) = 0;
        // Update the geoemtry object's data buffer contents
        // by moving the contents of verts into geometry object.
        virtual void Update(std::vector<Vertex>&& verts) = 0;
    private:
    };
} // namespace