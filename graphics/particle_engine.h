// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <atomic>
#include <string>
#include <optional>
#include <memory>
#include <mutex>
#include <functional>

#include "base/bitflag.h"
#include "base/utility.h"
#include "graphics/drawable.h"

namespace gfx
{

    // ParticleEngineClass holds data for some type of
    // particle engine. The data and the class implementation together
    // are used to define a "type" / class for particle engines.
    // For example the user might have defined a particle engine called
    // "smoke" with some particular set of parameters. Once instance
    // (a c++ object) of ParticleEngineClass is then used to
    // represent this particle engine type. ParticleEngineInstance
    // objects are then instances of some engine type and point to the
    // class for class specific behaviour while containing their
    // instance specific data. (I.e. one instance of "smoke" can have
    // particles in different stages as some other instance of "smoke".
    class ParticleEngineClass : public DrawableClass
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
            // The particle time accumulator
            float time  = 0.0f;
            // Scaler for expressing the lifetime of the particle
            // as a fraction of the maximum lifetime.
            float time_scale = 1.0f;
            // The current distance travelled in simulation units.
            float distance  = 0.0f;
            // random float value between 0.0f and 1.0f
            float randomizer = 0.0f;
            // alpha value between 0.0f and 1.0f
            // 0.0f = particle is fully transparent.
            // 1.0f = particle is fully opaque.
            float alpha = 1.0f;
        };
        using ParticleBuffer = std::vector<Particle>;

        // Define the motion of the particle.
        enum class Motion {
            // Follow a linear path.
            Linear,
            // Particles follow a curved path where gravity applies
            // to the vertical component of the particles' velocity
            // vector.
            Projectile
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
        enum class SpawnPolicy {
            // Spawn only once the initial number of particles
            // and then no more particles.
            Once,
            // Maintain a fixed number of particles in the
            // simulation spawning new particles as old ones die.
            Maintain,
            // Continuously spawn new particles on every update
            // of the simulation. num particles is the number of
            // particles to spawn per second.
            Continuous,
            // Spawn only on emission command
            Command
        };

        // Control the simulation space for the particles.
        enum class CoordinateSpace {
            // Particles are simulated inside a local coordinate space
            // relative to a local origin which is then transformed
            // with the model transform to the world (and view) space.
            Local,
            // Particles are simulated in the global/world coordinate space
            // directly and only transformed to view space.
            Global
        };

        // The shape of the area in which the particles are spawned.
        enum class EmitterShape {
            // uses the rectangular area derived either from the local
            // emitter size and position relative to the local simulation
            // origin or the size and position of the rectangle derived from
            // the position and size of the drawable in world space.
            Rectangle,
            // Uses a circular emitter with the diameter equalling the shortest
            // side of either the local emitter rectangle or the rectangle
            // containing the drawable in world space.
            Circle
        };

        // Control where the particles are spawned.
        enum class Placement {
            // Inside the emitter shape
            Inside,
            // On the edge of the emitter shape only
            Edge,
            // Outside the emitter shape
            Outside,
            // Center of the emitter shape
            Center
        };

        // Control what is the initial direction of the particles
        enum class Direction {
            // Traveling outwards from the center of the emitter
            // through the initial particle position
            Outwards,
            // Traveling inwards from the initial position towards
            // the center of the emitter
            Inwards,
            // Traveling direction according to the direction sector.
            Sector
        };

        enum class DrawPrimitive {
            Point, FullLine, PartialLineBackward, PartialLineForward
        };

        enum class Flags {
            ParticlesCanExpire
        };

        // initial engine configuration params
        struct Params {
            base::bitflag<Flags> flags = GetDefaultFlags();

            DrawPrimitive primitive = DrawPrimitive::Point;

            Direction direction = Direction::Sector;
            // Placement of particles wrt. the emitter shape
            Placement placement = Placement::Inside;
            // the shape of the area inside which the particles are spawned
            EmitterShape shape = EmitterShape::Rectangle;
            // The coordinate space in which the emitters are spawned
            CoordinateSpace coordinate_space = CoordinateSpace::Local;
            // type of motion for the particles
            Motion motion = Motion::Linear;
            // when to spawn particles.
            SpawnPolicy mode = SpawnPolicy::Maintain;
            // What happens to a particle at the simulation boundary
            BoundaryPolicy boundary = BoundaryPolicy::Clamp;
            // delay until the particles are spawned after start.
            float delay = 0.0f;
            // the maximum time the simulation will ever be alive
            // regardless of whether there are particles or not.
            float max_time = std::numeric_limits<float>::max();
            // the minimum time the simulation will be alive even if
            // there are no particles.
            float min_time = 0.0f;
            // The time to "dry run" the simulation before rendering it.
            // Useful for getting the simulation into some "Primed" state
            // before visually showing them to the user.
            float warmup_time = 0.0f;
            // the number of particles this engine shall create
            // the behaviour of this parameter depends on the spawn mode.
            float num_particles = 100;
            // minimum and maximum particle lifetime.
            // any particle will have randomly selected lifetime between
            // the min and max value.
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
            // defined by start angle and the size of the sector
            // expressed by angle
            float direction_sector_start_angle = 0.0f;
            float direction_sector_size = math::Pi * 2.0f;
            // min max points sizes.
            float min_point_size = 1.0f;
            float max_point_size = 1.0f;
            // min max alpha values.
            float min_alpha = 1.0f;
            float max_alpha = 1.0f;
            // rate of change with respect to unit of time.
            float rate_of_change_in_size_wrt_time = 0.0f;
            // rate of change with respect to unit of distance.
            float rate_of_change_in_size_wrt_dist = 0.0f;
            // rate of change in alpha value with respect to unit of time.
            float rate_of_change_in_alpha_wrt_time = 0.0f;
            // rate of change in alpha value with respect to unit of distance.
            float rate_of_change_in_alpha_wrt_dist = 0.0f;
            // the gravity that applies when using projectile particles.
            glm::vec2 gravity = {0.0f, 0.3f};

            static base::bitflag<Flags> GetDefaultFlags()
            {
                base::bitflag<Flags> f;
                f.set(Flags::ParticlesCanExpire, true);
                return f;
            }
        };
        using EngineParamsPtr = std::shared_ptr<Params>;

        // State of any instance of ParticleEngineInstance.
        struct InstanceState {
            // exclusion device on the particles buffer below.
            mutable std::mutex mutex;
            // the simulation particles.
            ParticleBuffer particles;
            // delay until the particles are first initially emitted.
            float delay = 0.0f;
            // simulation time.
            float time = 0.0f;
            // fractional count of new particles being hatched.
            float hatching = 0.0f;
            // these back / task buffers let us run the particle
            // engine tasks (init/update) on the background on their
            // own buffers while the rendering state is in the
            // original 'particles' buffers.
            mutable std::mutex buffer_mutex;
            ParticleBuffer task_buffer[2];
            std::atomic<size_t> task_count = 0;
        };
        using InstanceStatePtr = std::shared_ptr<InstanceState>;

        explicit ParticleEngineClass(const Params& init, std::string id = base::RandomString(10), std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
          , mParams(std::make_shared<Params>(init))
        {}
        explicit ParticleEngineClass(std::string id = base::RandomString(10), std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
          , mParams(std::make_shared<Params>())
        {}

        bool Construct(const Environment& env, const InstanceState& state, Geometry::CreateArgs& create) const;
        bool Construct(const Environment& env, const InstanceState& state, const InstancedDraw& draw,
                       gfx::InstancedDraw::CreateArgs& args) const;
        ShaderSource GetShader(const Environment& env, const Device& device) const;
        std::string GetShaderId(const Environment& env) const;
        std::string GetShaderName(const Environment& env) const;
        std::string GetGeometryId(const Environment& env) const;

        bool ApplyDynamicState(const Environment& env, ProgramState& program) const;
        void Update(const Environment& env, InstanceStatePtr state, float dt) const;
        void Restart(const Environment& env, InstanceStatePtr state) const;
        bool IsAlive(const InstanceStatePtr& state) const;

        void Emit(const Environment& env, InstanceStatePtr state, int count) const;

        // Get the params.
        inline const Params& GetParams() const noexcept
        { return *mParams; }
        inline Params& GetParams() noexcept
        { return *mParams; }
        // Set the params.
        inline void SetParams(const Params& params) noexcept
        {
            // create a fresh copy so that should this ever be called
            // outside design time, if we have pending tasks to update
            // the particle engine instance states we avoid having a race
            // condition on the parameters
            mParams = std::make_shared<Params>(params);
        }

        void SetName(const std::string& name) override;

        Type GetType() const override;
        SpatialMode GetSpatialMode() const override;

        std::string GetId() const override;
        std::string GetName() const override;
        std::size_t GetHash() const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
        std::unique_ptr<DrawableClass> Clone() const override;
        std::unique_ptr<DrawableClass> Copy() const override;

        static void SetRandomGenerator(std::function<float(float min, float max)> random_function);
    private:
        void InitParticles(const Environment& env, InstanceStatePtr state, size_t num) const;
        void UpdateParticles(const Environment& env, InstanceStatePtr state, float dt) const;

        static void InitParticles(const Environment& env, const Params& params, ParticleBuffer& particles, size_t num);
        static void UpdateParticles(const Environment& env, const Params& params, ParticleBuffer& particles, float dt);
        static void KillParticle(ParticleBuffer& particles, size_t i);

        struct ParticleWorld {
            std::optional<glm::vec2> world_gravity;
        };

        static bool UpdateParticle(const Environment& env, const Params& params, const ParticleWorld& world,
                                   ParticleBuffer& particles, size_t i, float dt);
    private:
        std::string mId;
        std::string mName;
        std::shared_ptr<Params> mParams;
    };

    // Particle engine interface. Particle engines implement some kind of
    // "particle" / n-body simulation where a variable number of small objects
    // are simulated or animated in some particular way.
    class ParticleEngine
    {
    public:
        // Destructor
        virtual ~ParticleEngine() = default;
        // Todo: add particle engine specific methods if needed.
    private:
    };


    // ParticleEngineInstance implements particle simulation
    // based on pure motion without reference to the forces
    // or the masses acting upon the particles.
    // It represents an instance of a some type of ParticleEngineClass,
    // which is the "type definition" i.e. class for some particle engine.
    class ParticleEngineInstance : public Drawable,
                                   public ParticleEngine
    {
    public:
        using Params = ParticleEngineClass::Params;

        // Create a new particle engine based on an existing particle engine
        // class definition.
        explicit ParticleEngineInstance(std::shared_ptr<const ParticleEngineClass> klass) noexcept
          : mClass(std::move(klass))
          , mState(std::make_shared<ParticleEngineClass::InstanceState>())
        {}
        explicit ParticleEngineInstance(const ParticleEngineClass& klass)
          : mClass(std::make_shared<ParticleEngineClass>(klass))
          , mState(std::make_shared<ParticleEngineClass::InstanceState>())
        {}
        explicit ParticleEngineInstance(const ParticleEngineClass::Params& params)
          : mClass(std::make_shared<ParticleEngineClass>(params))
          , mState(std::make_shared<ParticleEngineClass::InstanceState>())
        {}
        bool ApplyDynamicState(const Environment& env, Device&, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment&  env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Device&, Geometry::CreateArgs& create) const override;
        bool Construct(const Environment& env, Device&, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;
        void Update(const Environment& env, float dt) override;
        bool IsAlive() const override;
        void Restart(const Environment& env) override;
        void Execute(const Environment& env, const Command& cmd) override;

        DrawPrimitive  GetDrawPrimitive() const override;
        SpatialMode GetSpatialMode() const override;;
        Type GetType() const override;
        Usage GetGeometryUsage() const override;

        const DrawableClass* GetClass() const override
        { return mClass.get(); }

        // Get the current number of alive particles.
        inline size_t GetNumParticlesAlive() const noexcept
        { return mState->particles.size(); }

        inline const Params& GetParams() const noexcept
        { return mClass->GetParams(); }

    private:
        // this is the "class" object for this particle engine type.
        std::shared_ptr<const ParticleEngineClass> mClass;
        // this is this particle engine's state.
        // using a shared_ptr here so that we can share the state
        // with some background thread tasks to update the
        // particle engine state
        std::shared_ptr<ParticleEngineClass::InstanceState> mState;
    };

} // namespace