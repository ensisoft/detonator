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
#include <vector>
#include <limits>
#include <cmath>

#include "base/math.h"
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
        virtual Geometry* Upload(GraphicsDevice& device) const = 0;
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

        virtual Geometry* Upload(GraphicsDevice& device) const override
        {
            Geometry* geom = device.FindGeometry("rect");
            if (!geom)
            {
                const Vertex verts[6] = {
                    { {0,  0}, {0, 1} },
                    { {0, -1}, {0, 0} },
                    { {1, -1}, {1, 0} },

                    { {0,  0}, {0, 1} },
                    { {1, -1}, {1, 0} },
                    { {1,  0}, {1, 1} }
                };
                geom = device.MakeGeometry("rect");
                geom->Update(verts, 6);
            }
            return geom;
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

        virtual Geometry* Upload(GraphicsDevice& device) const override
        {
            Geometry* geom = device.FindGeometry("triangle");
            if (!geom)
            {
                const Vertex verts[6] = {
                    { {0.5,  0.0}, {0.5, 1.0} },
                    { {0.0, -1.0}, {0.0, 0.0} },
                    { {1.0, -1.0}, {1.0, 0.0} }
                };
                geom = device.MakeGeometry("triangle");
                geom->Update(verts, 3);
            }
            return geom;
        }
    private:
    };

    class ParticleEngine : public Drawable
    {
    public:
        // what to do when the particle reaches the maximum
        // distance in either x or y axis
        enum class BoundaryFunction {
            // Wrap over to 0
            Wrap,
            // Clamp to the boundary
            Clamp,
            // Kill the particle
            Kill
        };

        struct Params {
            // the number of particles this engine shall create
            std::size_t num_particles = 100;
            // maximum particle lifetime
            float min_lifetime = 0.0f;
            float max_lifetime = std::numeric_limits<float>::max();
            // the maximum bounds of travel for each particle.
            // at the bounds the particles either die, wrap or clamp
            float max_xpos = 1.0f;
            float max_ypos = 1.0f;
            // the starting rectangle for each particle. each particle
            // begins life at a random location within this rectangle
            float max_init_xpos = 1.0f;
            float max_init_ypos = 1.0f;
            // each particle has an initial velocity between min and max
            float min_velocity = 1.0f;
            float max_velocity = 1.0f;
            // each particle has a direction vector within a sector
            // defined by start angle and the size of the sectore
            // expressed by angle
            float direction_sector_start_angle = 0.0f;
            float direction_sector_size = math::Pi * 2;
            // min max points sizes.
            float min_point_size = 1.0f;
            float max_point_size = 1.0f;
            // what to do at the boundary
            BoundaryFunction boundary = BoundaryFunction::Wrap;
        };

        ParticleEngine(const std::string& name, const Params& init)
          : mEngineName(name)
          , mMaxX(init.max_xpos)
          , mMaxY(init.max_ypos)
          , mBoundary(init.boundary)
        {
            for (size_t i=0; i<init.num_particles; ++i)
            {
                const auto v = math::rand(init.min_velocity, init.max_velocity);
                const auto x = math::rand(0.0f, init.max_init_xpos);
                const auto y = math::rand(0.0f, init.max_init_ypos);
                const auto a = math::rand(0.0f, init.direction_sector_size) +
                   init.direction_sector_start_angle;

                Particle p;
                p.lifetime  = math::rand(init.min_lifetime, init.max_lifetime);
                p.pointsize = math::rand(init.min_point_size, init.max_point_size);
                p.pos = math::Vector2D(x, y);
                p.dir = math::Vector2D(std::cos(a), std::sin(a));
                p.dir *= v; // baking the velocity in the direction vector.
                mParticles.push_back(p);
            }
        }
        virtual Shader* GetShader(GraphicsDevice& device) const override
        {
            Shader* shader = device.FindShader("vertex_array.glsl");
            if (shader == nullptr || !shader->IsValid())
            {
                if (shader == nullptr)
                    shader = device.MakeShader("vertex_array.glsl");
                if (!shader->CompileFile("vertex_array.glsl"))
                    return nullptr;
            }
            return shader;

        }
        virtual Geometry* Upload(GraphicsDevice& device) const override
        {
            Geometry* geom = device.FindGeometry(mEngineName);
            if (!geom)
            {
                geom = device.MakeGeometry(mEngineName);
            }
            std::vector<Vertex> verts;
            for (const auto& p : mParticles)
            {
                // in order to use vertex_arary.glsl we need to convert the points to
                // lower right quadrant coordinates.
                Vertex v;
                v.aPosition.x = p.pos.X() / mMaxX;
                v.aPosition.y = p.pos.Y() / mMaxY * -1.0;
                v.aTexCoord.x = p.pointsize;
                verts.push_back(v);
            }

            geom->Update(&verts[0], verts.size());
            geom->SetDrawType(Geometry::DrawType::Points);
            return geom;
        }
        void Update(float dt /*seconds */)
        {
            mTime += dt;

            // update each particle
            for (size_t i=0; i<mParticles.size();)
            {
                auto& p = mParticles[i];
                const auto last = mParticles.size() - 1;
                const auto pos = p.pos + p.dir * dt;

                auto x = pos.X();
                auto y = pos.Y();
                if (mBoundary == BoundaryFunction::Wrap)
                {
                    x = math::wrap(mMaxX, 0.0f, x);
                    y = math::wrap(mMaxY, 0.0f, y);
                }
                else if (mBoundary == BoundaryFunction::Clamp)
                {
                    x = math::clamp(0.0f, mMaxX, x);
                    y = math::clamp(0.0f, mMaxY, y);
                }

                // see if it should be killed, happens if the pos
                // has exceeded max bounds (no clamp/wrap) or lifetime
                // exceeds max lifetime
                if (x > mMaxX || y > mMaxY || mTime > p.lifetime)
                {
                    std::swap(mParticles[i], mParticles[last]);
                    mParticles.pop_back();
                    continue;
                }
                p.pos = math::Vector2D(x, y);
                ++i;
            }
        }
        bool HasParticles() const
        { return !mParticles.empty(); }

        size_t NumParticlesAlive() const
        { return mParticles.size(); }
    private:
        struct Particle {
            math::Vector2D pos;
            math::Vector2D dir;
            float pointsize = 1.0f;
            float lifetime  = 0.0f;
         };
    private:
        const std::string mEngineName;
        const float mMaxX = 0.0f;
        const float mMaxY = 0.0f;
        const BoundaryFunction mBoundary = BoundaryFunction::Wrap;
        float mTime = 0.0f;
        std::vector<Particle> mParticles;
    };

} // namespace
