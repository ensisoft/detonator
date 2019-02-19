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

#include <string>

#include "device.h"
#include "shader.h"
#include "geometry.h"

namespace invaders
{
    class Drawable
    {
    public:
        virtual ~Drawable() = default;
        virtual Shader* GetShader(GraphicsDevice& device) const = 0;
        virtual void Upload(Geometry& geom) const = 0;
    private:
    };

    class Rect : public Drawable
    {
    public:
        virtual Shader* GetShader(GraphicsDevice& device) const override
        {
            Shader* s = device.FindShader("vertex_array.glsl");
            if (s == nullptr || !s->IsValid())
            {
                if (s == nullptr)
                    s = device.MakeShader("vertex_array.glsl");
                if (!s->CompileFile("vertex_array.glsl"))
                    return nullptr;
            }
            return s;
        }

        virtual void Upload(Geometry& geom) const
        {
            const Vertex verts[6] = {
                { {0,  0}, {0, 1} },
                { {0, -1}, {0, 0} },
                { {1, -1}, {1, 0} },

                { {0,  0}, {0, 1} },
                { {1, -1}, {1, 0} },
                { {1,  0}, {1, 1} }
            };
            geom.Update(verts, 6);
        }
    private:
    };

    class Triangle : public Drawable
    {
    public:
        virtual Shader* GetShader(GraphicsDevice& device) const override
        {
            Shader* s = device.FindShader("vertex_array.glsl");
            if (s == nullptr || !s->IsValid())
            {
                if (s == nullptr)
                    s = device.MakeShader("vertex_array.glsl");
                if (!s->CompileFile("vertex_array.glsl"))
                    return nullptr;
            }
            return s;
        }

        virtual void Upload(Geometry& geom) const
        {
            const Vertex verts[6] = {
                { {0.5,  0.0}, {0.5, 1.0} },
                { {0.0, -1.0}, {0.0, 0.0} },
                { {1.0, -1.0}, {1.0, 0.0} }
            };
            geom.Update(verts, 3);
        }
    private:
    };

} // namespace
