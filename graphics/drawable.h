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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "base/math.h"
#include "device.h"
#include "shader.h"
#include "geometry.h"
#include "text.h"

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

    class Rectangle : public Drawable
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

    // Particle engine interface. Particle engines implement some kind of
    // "particle" / n-body simulation where a variable number of small objects 
    // are simulated or animated in some particular way.
    class ParticleEngine 
    {
    public: 
        // Destructor
        virtual ~ParticleEngine() = default;
        // Update the particle simulation by one step with the current
        // delta timestep in seconds.
        virtual void Update(float dt) = 0;
        // Returns true if the simulation has particles and is still running.
        virtual bool IsAlive() const = 0;
        // Restart the simulation if no longer alive.
        virtual void Restart() = 0;
        // todo: add more methods as needed. 
    private:
    };

    // Runtime configurable kinematic particle engine
    // update policy class for updating the particles
    // based on the configured parameters of the policy.
    class DefaultKinematicsParticleUpdatePolicy
    {
    public: 
        // Define the motion of the particle.
        enum class Motion {
            // Follow a linear path. 
            Linear
        };
            // Control what happens to the particle when it reaches
        // the boundary of the simulation.
        enum class BoundaryPolicy {
            // Clamp boundary to the boundary value. 
            // Use case example: Bursts of particles that blow up
            // and then remain stationary once they land. 
            Clamp, 
            // Wraps around the boundary. 
            // Use case example: never ending star field animation 
            Wrap, 
            // Kill the particle at the boundary.
            Kill
        };

        inline void BeginIteration(float dt, float time) const
        { /* intentionally empty */ }
        inline void EndIteration() const 
        { /* intentionally empty */ }

        // If the particle is still considered to be alive then
        // update the properties of the particle and return false.
        // Otherwise return false to indicate that the particle has
        // expired and should be expunged.
        template<typename Particle> inline
        bool Update(Particle& p, float dt, float time, const glm::vec2& bounds) const 
        {
            const auto p0 = p.position;
            
            // update change in position
            if (mMotion == Motion::Linear) 
                p.position += (p.direction * dt);

            const auto& p1 = p.position;
            const auto& dp = p1 - p0;
            const auto  dd = glm::length(dp);

            // update change in size with respect to time.
            p.pointsize += (dt * dSdT);
            // update change in size with respect to distance 
            p.pointsize += (dd * dSdD);
            if (p.pointsize <= 0.0f)
                return false;
            // accumulate distance approximation
            p.distance += dd;

            // boundary conditions.
            if (mBoundaryPolicy == BoundaryPolicy::Wrap) 
            {
                p.position.x = math::wrap(0.0f, bounds.x, p.position.x);
                p.position.y = math::wrap(0.0f, bounds.y, p.position.y);
            }
            else if (mBoundaryPolicy == BoundaryPolicy::Clamp)
            {
                p.position.x = math::clamp(0.0f, bounds.x, p.position.x);
                p.position.y = math::clamp(0.0f, bounds.y, p.position.y);
            }
            else if (mBoundaryPolicy == BoundaryPolicy::Kill)
            {
                if (p.position.x < 0.0f || p.position.x > bounds.x)
                    return false;
                else if (p.position.y < 0.0f || p.position.y > bounds.y)
                    return false;
            }
            return true;
        }
        
        void SetGrowthWithRespectToTime(float rate)
        { dSdT = rate; }
        void SetGrowthWithRespectToDistance(float rate)
        { dSdD = rate; }

        void SetMotion(Motion m) 
        { mMotion = m; }
        void SetBoundaryPolicy(BoundaryPolicy p)
        { mBoundaryPolicy = p; }

    protected:
        // no virtual destruction dynamically through this type
        ~DefaultKinematicsParticleUpdatePolicy() {}
    private:
        Motion mMotion = Motion::Linear;
        BoundaryPolicy mBoundaryPolicy = BoundaryPolicy::Wrap;
        // rate of change with respect to unit of time.
        float dSdT = 0.0f;
        // rate of change with respect to unit of distance 
        // travelled.
        float dSdD = 0.0f;
    };

    // KinematicsParticleEngine implements particle simulation
    // based on pure motion without reference to the forces
    // or the masses acting upon the particles.
    template<typename ParticleUpdatePolicy = DefaultKinematicsParticleUpdatePolicy>
    class TKinematicsParticleEngine : public Drawable,
                                      public ParticleEngine,
                                      public ParticleUpdatePolicy
    {
    public:
        struct Particle {
            // The current particle position in simulation space.
            glm::vec2 position;
            // The current direction of travel in simulation space
            // times velocity. 
            glm::vec2 direction;
            // The current particle point size.
            float pointsize = 1.0f;
            // The expected lifetime of the particle.
            float lifetime  = 0.0f;
            // The current distance travelled in simulation units.
            float distance  = 0.0f;
        };

        // Control when to spawn particles.
        enum SpawnPolicy {
            // Spawn only once the initial number of particles
            // and then no more particles.
            Once,
            // Maintain a fixed number of particles in the
            // simulation spawning new particles as old ones die.
            Maintain, 
            // Continuously spawn new particles on every update
            // of the simulation. Todo: maybe this should be rate limited
            // i.e. num of particles per second. 
            Continuous
        };

        // initial engine configuration params
        struct Params {
            SpawnPolicy mode = SpawnPolicy::Maintain;
            // the number of particles this engine shall create
            // the behaviour of this parameter depends on the spawn mode.
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
        };

        TKinematicsParticleEngine(const Params& init) : mParams(init)
        {
            InitParticles(mParams.num_particles);
        }

        // Drawable implementation. Compile the shader.
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
        // Drawable implementation. Upload particles to the device.
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
                v.aPosition.x = p.position.x / mParams.max_xpos;
                v.aPosition.y = p.position.y / mParams.max_ypos * -1.0;
                v.aTexCoord.x = p.pointsize >= 0.0f ? p.pointsize : 0.0f;
                verts.push_back(v);
            }

            geom->Update(&verts[0], verts.size());
            geom->SetDrawType(Geometry::DrawType::Points);
            return geom;
        }

        // ParticleEngine implementation. Update the particle simulation.
        virtual void Update(float dt) override
        {
            mTime += dt;

            const glm::vec2 bounds(mParams.max_xpos, mParams.max_ypos);
            
            ParticleUpdatePolicy::BeginIteration(dt, mTime);

            // update each particle
            for (size_t i=0; i<mParticles.size();)
            {
                // countdown to end of lifetime
                mParticles[i].lifetime -= dt;

                if (mParticles[i].lifetime > 0.0f &&
                    ParticleUpdatePolicy::Update(mParticles[i], dt, mTime, bounds))
                {
                    ++i;
                    continue;
                }
                KillParticle(i);
            }

            ParticleUpdatePolicy::EndIteration();

            // Spawn new particles if needed.
            if (mParams.mode == SpawnPolicy::Maintain)
                InitParticles(mParams.num_particles - mParticles.size());
            else if (mParams.mode == SpawnPolicy::Continuous) 
                InitParticles(mParams.num_particles);
        }

        // ParticleEngine implementation. 
        virtual bool IsAlive() const override
        { return !mParticles.empty(); }

        // ParticleEngine implementation. Restart the simulation
        // with the previous parameters if possible to do so.
        virtual void Restart() override
        {
            if (!mParticles.empty())
                return;
            if (mParams.mode != SpawnPolicy::Once)
                return;
            InitParticles(mParams.num_particles);
            mTime = 0.0f;
        }

    private:
        void InitParticles(size_t num)
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
                mParticles.push_back(p);
            }
        }
        void KillParticle(size_t i)
        {
            const auto last = mParticles.size() - 1;
            std::swap(mParticles[i], mParticles[last]);
            mParticles.pop_back();            
        }
    private:
        const Params mParams;

        float mTime = 0.0f;
        std::vector<Particle> mParticles;
    };

    using KinematicsParticleEngine = TKinematicsParticleEngine<>;

} // namespace
