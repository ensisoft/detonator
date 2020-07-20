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

#include "base/math.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;

    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        // Style of the drawable's geometry determines how the geometry
        // is to be rasterized.
        enum class Style {
            // Rasterize the outline of the shape as lines.
            // Only the fragments that are within the line are shaded.
            // Line width setting is applied to determine the width
            // of the lins.
            Outline,
            // Rasterize the individual triangles as lines.
            Wireframe,
            // Rasterize the interior of the drawable. This is the default
            Solid,
            // Rasterizez the shape's vertices as individual points.
            Points
        };

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
        // Request a particular line width to be used when style
        // is either Outline or Wireframe.
        virtual void SetLineWidth(float width) {}
        // Set the style to be used for rasterizing the drawable
        // shapes's fragments.
        // Not all drawables support all Styles.
        virtual void SetStyle(Style style) {}
        // Get the current style.
        virtual Style GetStyle() const = 0;
    private:
    };

    class Arrow : public Drawable
    {
    public:
        Arrow() = default;
        Arrow(Style style) : mStyle(style)
        {}
        Arrow(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class Circle : public Drawable
    {
    public:
        Circle() = default;
        Circle(Style style) : mStyle(style)
        {}
        Circle(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };


    class Rectangle : public Drawable
    {
    public:
        Rectangle() = default;
        Rectangle(Style style) : mStyle(style)
        {}
        Rectangle(Style style, float linewidth)  : mStyle(style), mLineWidth(linewidth)
        {}
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class Triangle : public Drawable
    {
    public:
        Triangle() = default;
        Triangle(Style style) : mStyle(style)
        {}
        Triangle(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    // render a series of intersecting horizontal and vertical lines
    // at some particular interval (gap distance)
    class Grid : public Drawable
    {
    public:
        // the num vertical and horizontal lines is the number of lines
        // *inside* the grid. I.e. not including the enclosing border lines
        Grid(unsigned num_vertical_lines, unsigned num_horizontal_lines, bool border_lines = true)
            : mNumVerticalLines(num_vertical_lines)
            , mNumHorizontalLines(num_horizontal_lines)
            , mBorderLines(border_lines)
        {}
        Grid() = default;

        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(Device& device) const override;
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return Style::Outline; }

        void SetNumVerticalLines(unsigned lines)
        { mNumVerticalLines = lines; }
        void SetNumHorizontalLines(unsigned lines)
        { mNumHorizontalLines = lines; }
    private:
        unsigned mNumVerticalLines = 1;
        unsigned mNumHorizontalLines = 1;
        float mLineWidth = 1.0f;
        bool mBorderLines = false;
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
            float direction_sector_size = math::Pi * 2.0f;
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
            InitParticles(size_t(mParams.num_particles));
        }

        // Drawable implementation. Compile the shader.
        virtual Shader* GetShader(Device& device) const override;
        // Drawable implementation. Upload particles to the device.
        virtual Geometry* Upload(Device& device) const override;
        virtual Style GetStyle() const override
        { return Style::Points; }

        // Update the particle simulation.
        virtual void Update(float dt) override;
        // ParticleEngine implementation.
        virtual bool IsAlive() const override;
        // ParticleEngine implementation. Restart the simulation
        // with the previous parameters if possible to do so.
        virtual void Restart() override;

        size_t GetNumParticlesAlive() const
        { return mParticles.size(); }
        const Params& GetParams() const
        { return mParams; }

        // Serialize into JSON.
        nlohmann::json ToJson() const;
        // Load from JSON
        static std::optional<KinematicsParticleEngine> FromJson(const nlohmann::json& object);

    private:
        void InitParticles(size_t num);
        void KillParticle(size_t i);
        bool UpdateParticle(size_t i, float dt);
    private:
        /* const */ Params mParams;
        float mNumParticlesHatching = 0.0f;
        float mTime = 0.0f;
        std::vector<Particle> mParticles;
    };

} // namespace
