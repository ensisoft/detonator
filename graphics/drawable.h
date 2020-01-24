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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "base/math.h"
#include "base/utility.h"
#include "device.h"
#include "shader.h"
#include "geometry.h"
#include "text.h"

namespace gfx
{
    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        virtual ~Drawable() = default;
        // Get the device specific shader object.
        // If the shader does not yet exist on the device it's created
        // and compiled.  On any errors nullptr should be returned.
        virtual Shader* GetShader(Device& device) const = 0;
        // Get the device specific geometry object. If the geometry
        // does not yet exist on the device it's created and the
        // contents from this drawable object are uploaded in some
        // device specific data format.
        virtual Geometry* Upload(Device& device) const = 0;
        // Update the state of the drawable object. dt is the
        // elapsed (delta) time in seconds.
        virtual void Update(float dt) {}
    private:
    };

    // 2D rectangle.
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
        // Returns true if the simulation has particles and is still running.
        virtual bool IsAlive() const = 0;
        // Restart the simulation if no longer alive.
        virtual void Restart() = 0;
        // todo: add more methods as needed.
    private:
    };

    // KinematicsParticleEngine implements particle simulation
    // based on pure motion without reference to the forces
    // or the masses acting upon the particles.
    class KinematicsParticleEngine : public Drawable,
                                     public ParticleEngine
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
            Kill,
            // Reflect the particle at the boundary
            Reflect
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
            // of the simulation. num particles is the number of
            // particles to spawn per second.
            Continuous
        };

        // initial engine configuration params
        struct Params {
            // type of motion for the particles
            Motion motion = Motion::Linear;
            // when to spawn particles.
            SpawnPolicy mode = SpawnPolicy::Maintain;
            // What happens to a particle at the simulation boundary
            BoundaryPolicy boundary = BoundaryPolicy::Clamp;
            // the number of particles this engine shall create
            // the behaviour of this parameter depends on the spawn mode.
            float num_particles = 100;
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
            // rate of change with respect to unit of time.
            float rate_of_change_in_size_wrt_time = 0.0f;
            // rate of change with respect to unit of distance
            // travelled.
            float rate_of_change_in_size_wrt_dist = 0.0f;
        };

        KinematicsParticleEngine(const Params& init) : mParams(init)
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

            geom->Update(std::move(verts));
            geom->SetDrawType(Geometry::DrawType::Points);
            return geom;
        }

        // Update the particle simulation.
        virtual void Update(float dt) override
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
                InitParticles(mParams.num_particles - mParticles.size());
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
        virtual bool IsAlive() const override
        {
            if (mParams.mode == SpawnPolicy::Continuous)
                return true;
            return !mParticles.empty();
        }

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
            mNumParticlesHatching = 0;
        }
        size_t GetNumParticlesAlive() const
        { return mParticles.size(); }
        const Params& GetParams() const
        { return mParams; }

        nlohmann::json ToJson() const
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
        static std::optional<KinematicsParticleEngine> FromJson(const nlohmann::json& object)
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

        bool UpdateParticle(size_t i, float dt)
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

            }
            return true;
        }

    private:
        const Params mParams;
        float mNumParticlesHatching = 0.0f;
        float mTime = 0.0f;
        std::vector<Particle> mParticles;
    };

} // namespace
