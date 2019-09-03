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

namespace gfx
{
    class Drawable
    {
    public:
        virtual ~Drawable() = default;
        virtual Shader* GetShader(Device& device) const = 0;
        virtual Geometry* Upload(Device& device) const = 0;
    private:
    };

    class Rect : public Drawable
    {
    public:
        virtual Shader* GetShader(Device& device) const override
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

        virtual Geometry* Upload(Device& device) const override
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
        virtual Shader* GetShader(Device& device) const override
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

        virtual Geometry* Upload(Device& device) const override
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

    struct Particle {
        math::Vector2D pos;
        math::Vector2D dir;
        float pointsize = 1.0f;
        float lifetime  = 0.0f;
    };


    // simulate simple linear particle movement into the direction
    // of the particle's direction vector
    struct LinearParticleMovement {
        void BeginIteration(float dt, float time) const
        {}
        bool Update(Particle& p, float dt, float time) const
        {
            p.pos = p.pos + p.dir * dt;
            return true;
        }
        void EndIteration() const
        {}
    };

    // wrap particle position around its bounds.
    struct WrapParticleAtBounds {
        bool Update(Particle& p, const math::Vector2D& bounds) const
        {
            const auto pos = p.pos;
            const auto x = math::wrap(bounds.X(), 0.0f, pos.X());
            const auto y = math::wrap(bounds.Y(), 0.0f, pos.Y());
            p.pos = math::Vector2D(x, y);
            return true;
        }
    };

    // clamp particle position to maximum bounds
    struct ClampParticleAtBounds {
        bool Update(Particle& p, const math::Vector2D& bounds) const
        {
            const auto pos = p.pos;
            const auto x = math::clamp(0.0f, bounds.X(), pos.X());
            const auto y = math::clamp(0.0f, bounds.Y(), pos.Y());
            p.pos = math::Vector2D(x, y);
            return true;
        }

    };

    // advice to kill the particle if the current particle position
    // exceeds some defined bounds.
    struct KillParticleAtBounds {
        bool Update(Particle& p, const math::Vector2D& bounds) const
        {
            const auto pos = p.pos;
            const auto x = pos.X();
            const auto y = pos.Y();
            if (x < 0 || x > bounds.X())
                return false;
            if (y < 0 || y > bounds.Y())
                return false;
            return true;
        }
    };

    // kill particle if its lifetime exceeds the current engine time.
    struct KillParticleAtLifetime {
        bool Update(Particle& p, float dt, float time) const
        {
            if (time >= p.lifetime)
                return false;
            return true;
        }
    };

    // no change in particle state.
    struct ParticleIdentity {
        void BeginIteration(float dt, float time, const math::Vector2D& bounds) const
        {}
        bool Update(Particle& p, float dt, float time, const math::Vector2D& bounds) const
        { return true; }
        void EndIteration() const
        {}
    };

    template<typename ParticleMotionPolicy   = LinearParticleMovement,
             typename ParticleBoundsPolicy   = WrapParticleAtBounds,
             typename ParticleUpdatePolicy   = ParticleIdentity,
             typename ParticleLifetimePolicy = KillParticleAtLifetime>
    class TParticleEngine : public Drawable,
                            public ParticleMotionPolicy,
                            public ParticleBoundsPolicy,
                            public ParticleUpdatePolicy,
                            public ParticleLifetimePolicy
    {
    public:
        // initial engine configuration params
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
            float init_rect_xpos   = 0.0f;
            float init_rect_ypos   = 0.0f;
            float init_rect_width  = 1.0f;
            float init_rect_height = 1.0f;
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
            // whether to spawn new particles to replace the ones killed.
            bool respawn = true;
        };

        TParticleEngine(const Params& init) : mParams(init)
        {
            InitParticles(mParams.num_particles);
        }
        virtual Shader* GetShader(Device& device) const override
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
        virtual Geometry* Upload(Device& device) const override
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
                v.aPosition.x = p.pos.X() / mParams.max_xpos;
                v.aPosition.y = p.pos.Y() / mParams.max_ypos * -1.0;
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

            const math::Vector2D bounds(mParams.max_xpos, mParams.max_ypos);
            ParticleMotionPolicy::BeginIteration(dt, mTime);
            ParticleUpdatePolicy::BeginIteration(dt, mTime, bounds);

            // update each particle
            for (size_t i=0; i<mParticles.size();)
            {
                auto& p = mParticles[i];

                // if any particle policy returns false we kill the particle.
                if (!ParticleMotionPolicy::Update(p, dt, mTime) ||
                    !ParticleUpdatePolicy::Update(p, dt, mTime, bounds) ||
                    !ParticleBoundsPolicy::Update(p, bounds) ||
                    !ParticleLifetimePolicy::Update(p, dt, mTime))
                {
                    const auto last = mParticles.size() - 1;
                    std::swap(mParticles[i], mParticles[last]);
                    mParticles.pop_back();
                    continue;
                }
                ++i;
            }
            ParticleMotionPolicy::EndIteration();
            ParticleUpdatePolicy::EndIteration();

            if (mParams.respawn)
            {
                const auto num_should_have = mParams.num_particles;
                const auto num_have = mParticles.size();
                const auto num_needed = num_should_have - num_have;
                InitParticles(num_needed);
            }
        }
        bool HasParticles() const
        { return !mParticles.empty(); }

        size_t NumParticlesAlive() const
        { return mParticles.size(); }

    private:
        void InitParticles(size_t num)
        {
            for (size_t i=0; i<num; ++i)
            {
                const auto v = math::rand(mParams.min_velocity, mParams.max_velocity);
                const auto x = math::rand(0.0f, mParams.init_rect_width);
                const auto y = math::rand(0.0f, mParams.init_rect_height);
                const auto a = math::rand(0.0f, mParams.direction_sector_size) +
                   mParams.direction_sector_start_angle;

                Particle p;
                p.lifetime  = math::rand(mParams.min_lifetime, mParams.max_lifetime);
                p.pointsize = math::rand(mParams.min_point_size, mParams.max_point_size);
                p.pos = math::Vector2D(mParams.init_rect_xpos + x, mParams.init_rect_ypos + y);
                p.dir = math::Vector2D(std::cos(a), std::sin(a));
                p.dir *= v; // baking the velocity in the direction vector.
                mParticles.push_back(p);
            }
        }
    private:
        const Params mParams;

        float mTime = 0.0f;
        std::vector<Particle> mParticles;
    };

    using ParticleEngine = TParticleEngine<>;

} // namespace
