// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <glm/glm.hpp>
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"

#include <atomic>
#include <algorithm>
#include <string>
#include <vector>
#include <limits>
#include <optional>
#include <variant>
#include <unordered_map>
#include <mutex>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/geometry.h"
#include "graphics/device.h"
#include "graphics/program.h"
#include "graphics/types.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;
    class CommandBuffer;
    class ShaderSource;

    // DrawableClass defines a new type of drawable.
    class DrawableClass
    {
    public:
        using Culling = Device::State::Culling;

        using Usage = Geometry::Usage;

        // Type of the drawable (and its instances)
        enum class Type {
            ParticleEngine,
            Polygon,
            TileBatch,
            SimpleShape,
            Undefined
        };
        // Style of the drawable's geometry determines how the geometry
        // is to be rasterized.
        using Primitive = DrawPrimitive;

        struct Environment {
            // true if running in an "editor mode", which means that even
            // content marked static might have changed and should be checked
            // in case it has been modified and should be re-uploaded.
            bool editing_mode = false;
            // how many render surface units (pixels, texels if rendering to a texture)
            // to a game unit.
            glm::vec2 pixel_ratio = {1.0f, 1.0f};
            // the current projection matrix that will be used to project the
            // vertices from the view space into Normalized Device Coordinates.
            const glm::mat4* proj_matrix = nullptr;
            // The current view matrix that will be used to transform the
            // vertices from the world space to the camera/view space.
            const glm::mat4* view_matrix = nullptr;
            // the current model matrix that will be used to transform the
            // vertices from the local space to the world space.
            const glm::mat4* model_matrix = nullptr;
            // the current world matrix that will be used to transform
            // vectors, such as gravity vector, to world space.
            const glm::mat4* world_matrix = nullptr;
        };

        struct DrawCmd {
            size_t draw_cmd_start = 0;
            size_t draw_cmd_count = std::numeric_limits<size_t>::max();
        };

        virtual ~DrawableClass() = default;
        // Get the type of the drawable.
        virtual Type GetType() const = 0;
        // Get the class ID.
        virtual std::string GetId() const = 0;
        // Get the human-readable class name.
        virtual std::string GetName() const = 0;
        // Set the human-readable class name.
        virtual void SetName(const std::string& name) = 0;
        // Create a copy of this drawable class object but with unique id.
        virtual std::unique_ptr<DrawableClass> Clone() const = 0;
        // Create an exact copy of this drawable object.
        virtual std::unique_ptr<DrawableClass> Copy() const = 0;
        // Get the hash of the drawable class object  based on its properties.
        virtual std::size_t GetHash() const = 0;
        // Serialize into JSON
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
    private:
    };

    namespace detail {
        struct EnvironmentCopy {
            explicit EnvironmentCopy(const DrawableClass::Environment& env) noexcept
                    : editing_mode(env.editing_mode)
                    , pixel_ratio(env.pixel_ratio)
            {
                if (env.proj_matrix)
                    proj_matrix = *env.proj_matrix;
                if (env.view_matrix)
                    view_matrix = *env.view_matrix;
                if (env.model_matrix)
                    model_matrix = *env.model_matrix;
                if (env.world_matrix)
                    world_matrix = *env.world_matrix;
            }
            DrawableClass::Environment ToEnv() const
            {
                DrawableClass::Environment env;
                env.editing_mode = editing_mode;
                env.pixel_ratio  = pixel_ratio;
                if (proj_matrix.has_value())
                    env.proj_matrix = &proj_matrix.value();
                if (view_matrix.has_value())
                    env.view_matrix = &view_matrix.value();
                if (model_matrix.has_value())
                    env.model_matrix = &model_matrix.value();
                if (world_matrix.has_value())
                    env.world_matrix = &world_matrix.value();
                return env;
            }

            bool editing_mode;
            glm::vec2 pixel_ratio;
            std::optional<glm::mat4> proj_matrix;
            std::optional<glm::mat4> view_matrix;
            std::optional<glm::mat4> model_matrix;
            std::optional<glm::mat4> world_matrix;
        };
    } // detail


    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        using Primitive = DrawableClass::Primitive;
        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points.
        using Culling = DrawableClass::Culling;
        // The environment that possibly affects the geometry and drawable
        // generation and update in some way.
        using Environment = DrawableClass::Environment;

        using Type = DrawableClass::Type;

        using Usage = DrawableClass::Usage;

        using DrawCmd = DrawableClass::DrawCmd;

        // Rasterizer state that the geometry can manipulate.
        struct RasterState {
            // rasterizer setting for line width when the geometry
            // contains lines.
            float line_width = 1.0f;
            // Culling state for discarding back/front facing fragments.
            // Culling state only applies to polygon's not to points or lines.
            Culling culling = Culling::Back;
        };

        using CommandArg = std::variant<float, int, std::string>;
        struct Command {
            std::string name;
            std::unordered_map<std::string, CommandArg> args;
        };
        using CommandList = std::vector<Command>;

        virtual ~Drawable() = default;
        // Apply the drawable's state (if any) on the program and set the rasterizer state.
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const = 0;
        // Get the device specific shader source applicable for this drawable, its state
        // and the given environment in which it should execute.
        // Should return an empty string on any error.
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const = 0;
        // Get the shader ID applicable for this drawable, its state and the given
        // environment in which it should execute.
        virtual std::string GetShaderId(const Environment& env) const = 0;
        // Get the human readable debug name that should be associated with the
        // shader object generated from this drawable.
        virtual std::string GetShaderName(const Environment& env) const = 0;
        // Get the geometry name that will be used to identify the
        // drawable geometry on the device.
        virtual std::string GetGeometryId(const Environment& env) const = 0;
        // Construct geometry object create args.
        // Returns true if successful or false if geometry is unavailable.
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const = 0;
        // Update the state of the drawable object. dt is the
        // elapsed (delta) time in seconds.
        virtual void Update(const Environment& env, float dt) {}
        // Get the expected type of primitive used to rasterize the
        // geometry produced by the drawable. This is essentially the
        // "summary" of the draw commands in the geometry object.
        virtual Primitive GetPrimitive() const
        { return Primitive::Triangles; }
        // Get the drawable type.
        virtual Type GetType() const
        { return Type::Undefined; }
        // Get the intended usage of the geometry generated by the drawable.
        virtual Usage GetUsage() const
        { return Usage::Static; }
        // Returns true if the drawable is still considered to be alive.
        // For example a particle simulation still has live particles.
        virtual bool IsAlive() const
        { return true; }

        virtual size_t GetContentHash() const
        { return 0; }

        // Restart the drawable, if applicable.
        virtual void Restart(const Environment& env) {}

        // Execute drawable commands coming from the scripting environment.
        // The commands can be used to change the drawable, alter its parameters
        // or trigger its function such as particle emission.
        virtual void Execute(const Environment& env, const Command& command)
        {}

        virtual DrawCmd  GetDrawCmd() const
        {
            // return the defaults which will then draw every draw
            // low level command associated with the geometry itself.
            return {};
        }

        // Get the drawable class instance if any. Warning, this may be null for
        // drawable objects that aren't based on any drawable clas!
        virtual const DrawableClass* GetClass() const { return nullptr; }
    private:
    };

    // render a series of intersecting horizontal and vertical lines
    // at some particular interval (gap distance)
    class Grid : public Drawable
    {
    public:
        // the num vertical and horizontal lines is the number of lines
        // *inside* the grid. I.e. not including the enclosing border lines
        Grid(unsigned num_vertical_lines, unsigned num_horizontal_lines, bool border_lines = true) noexcept
          : mNumVerticalLines(num_vertical_lines)
          , mNumHorizontalLines(num_horizontal_lines)
          , mBorderLines(border_lines)
        {}
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;

        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
        virtual Usage GetUsage() const override
        { return Usage::Static; }
    private:
        unsigned mNumVerticalLines = 1;
        unsigned mNumHorizontalLines = 1;
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
        // Todo: add particle engine specific methods if needed.
    private:
    };

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

        // initial engine configuration params
        struct Params {
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
            float min_time = 0;
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
        ShaderSource GetShader(const Environment& env, const Device& device) const;
        std::string GetProgramId(const Environment& env) const;
        std::string GetShaderName(const Environment& env) const;
        std::string GetGeometryId(const Environment& env) const;

        void ApplyDynamicState(const Environment& env, ProgramState& program) const;
        void Update(const Environment& env, InstanceStatePtr state, float dt) const;
        void Restart(const Environment& env, InstanceStatePtr state) const;
        bool IsAlive(const InstanceStatePtr& state) const;

        void Emit(const Environment& env, InstanceStatePtr state, int count) const;

        // Get the params.
        inline const Params& GetParams() const noexcept
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

        virtual Type GetType() const override
        { return DrawableClass::Type::ParticleEngine; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; };
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::unique_ptr<DrawableClass> Clone() const override;
        virtual std::unique_ptr<DrawableClass> Copy() const override;
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
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment&  env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual void Update(const Environment& env, float dt) override;
        virtual bool IsAlive() const override;
        virtual void Restart(const Environment& env) override;
        virtual void Execute(const Environment& env, const Command& cmd) override;
        virtual Primitive  GetPrimitive() const override;

        virtual Type GetType() const override
        { return Type::ParticleEngine; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }
        virtual const DrawableClass* GetClass() const override
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


    class TileBatch : public Drawable
    {
    public:
        enum class TileShape {
            Automatic, Square, Rectangle
        };
        enum class Projection {
            Dimetric, AxisAligned
        };

        struct Tile {
            Vec3 pos;
            // x = material palette index (for using tile material)
            // y = arbitrary data from the tile map
            Vec2 data;
        };
        TileBatch() = default;
        explicit TileBatch(const std::vector<Tile>& tiles)
          : mTiles(tiles)
        {}
        explicit TileBatch(std::vector<Tile>&& tiles) noexcept
          : mTiles(std::move(tiles))
        {}

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& raster) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override;
        virtual Type GetType() const override
        { return Type::TileBatch; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }

        inline void AddTile(const Tile& tile)
        { mTiles.push_back(tile); }
        inline void ClearTiles() noexcept
        { mTiles.clear(); }
        inline size_t GetNumTiles() const noexcept
        { return mTiles.size(); }
        inline const Tile& GetTile(size_t index) const noexcept
        { return mTiles[index]; }
        inline Tile& GetTile(size_t index) noexcept
        { return mTiles[index]; }
        inline glm::vec3 GetTileWorldSize() const noexcept
        { return mTileWorldSize; }
        inline glm::vec2 GetTileRenderSize() const noexcept
        { return mTileRenderSize; }

        // Set the tile width in tile world units.
        inline void SetTileWorldWidth(float width) noexcept
        { mTileWorldSize.x = width; }
        inline void SetTileWorldHeight(float height) noexcept
        { mTileWorldSize.y = height; }
        inline void SetTileWorldDepth(float depth) noexcept
        { mTileWorldSize.z = depth; }
        inline void SetTileWorldSize(const glm::vec3& size) noexcept
        { mTileWorldSize = size; }
        inline void SetTileRenderWidth(float width) noexcept
        { mTileRenderSize.x = width; }
        inline void SetTileRenderHeight(float height) noexcept
        { mTileRenderSize.y = height; }
        inline void SetTileRenderSize(const glm::vec2& size) noexcept
        { mTileRenderSize = size; }
        inline void SetProjection(Projection projection) noexcept
        { mProjection = projection; }

        TileShape ResolveTileShape() const noexcept
        {
            if (mShape == TileShape::Automatic) {
                if (math::equals(mTileRenderSize.x, mTileRenderSize.y))
                    return TileShape::Square;
                else return TileShape::Rectangle;
            }
            return mShape;
        }
        inline TileShape GetTileShape() const noexcept
        { return mShape; }
        inline void SetTileShape(TileShape shape) noexcept
        { mShape = shape; }
    private:
        Projection mProjection = Projection::AxisAligned;
        TileShape mShape = TileShape::Automatic;
        std::vector<Tile> mTiles;
        glm::vec3 mTileWorldSize = {0.0f, 0.0f, 0.0f};
        glm::vec2 mTileRenderSize = {0.0f, 0.0f};
    };

    class LineBatch2D : public Drawable
    {
    public:
        struct Line {
            glm::vec2 start = {0.0f, 0.0};
            glm::vec2 end   = {0.0f, 0.0f};
        };
        LineBatch2D() = default;
        LineBatch2D(const glm::vec2& start, glm::vec2& end)
        {
            Line line;
            line.start = start;
            line.end   = end;
            mLines.push_back(line);
        }
        explicit LineBatch2D(std::vector<Line> lines) noexcept
          : mLines(std::move(lines))
        {}
        inline void AddLine(Line line)
        { mLines.push_back(line); }
        inline void AddLine(const glm::vec2& start, const glm::vec2& end)
        {
            Line line;
            line.start = start,
            line.end   = end;
            mLines.push_back(line);
        }
        inline void AddLine(float x0, float y0, float x1, float y1)
        {
            Line line;
            line.start = {x0, y0};
            line.end   = {x1, y1};
            mLines.push_back(line);
        }

        virtual void ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const override;
        virtual ShaderSource GetShader(const Environment& environment, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& environment) const override;
        virtual std::string GetShaderName(const Environment& environment) const override;
        virtual std::string GetGeometryId(const Environment& environment) const override;
        virtual bool Construct(const Environment& environment, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }
    private:
        std::vector<Line> mLines;
    };

    class LineBatch3D : public Drawable
    {
    public:
        struct Line {
            glm::vec3 start = {0.0f, 0.0f, 0.0f};
            glm::vec3 end   = {0.0f, 0.0f, 0.0f};
        };

        LineBatch3D() = default;
        LineBatch3D(const glm::vec3& start, const glm::vec3& end)
        {
            Line line;
            line.start = start;
            line.end   = end;
            mLines.push_back(line);
        }
        explicit LineBatch3D(std::vector<Line> lines) noexcept
          : mLines(std::move(lines))
        {}
        inline void AddLine(Line line)
        { mLines.push_back(std::move(line)); }

        virtual void ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const override;
        virtual ShaderSource GetShader(const Environment& environment, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& environment) const override;
        virtual std::string GetShaderName(const Environment& environment) const override;
        virtual std::string GetGeometryId(const Environment& environment) const override;
        virtual bool Construct(const Environment& environment, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }
    private:
        std::vector<Line> mLines;
    };


    class DebugDrawableBase : public Drawable
    {
    public:
        enum class Feature {
            Normals,
            Wireframe
        };

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState&  state) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual Usage GetUsage() const override;
        virtual size_t GetContentHash() const override;
        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
    protected:
        DebugDrawableBase(const Drawable* drawable, Feature feature)
          : mDrawable(drawable)
          , mFeature(feature)
        {}
        DebugDrawableBase() = default;

        const Drawable* mDrawable = nullptr;
        Feature mFeature;
    };

    class DebugDrawableInstance : public DebugDrawableBase
    {
    public:
        DebugDrawableInstance(const std::shared_ptr<const Drawable> drawable, Feature feature)
          : DebugDrawableBase(drawable.get(), feature)
        {
            mSharedDrawable = drawable;
        }
    private:
        std::shared_ptr<const Drawable> mSharedDrawable;
    };

    class WireframeInstance : public DebugDrawableInstance
    {
    public:
        WireframeInstance(const std::shared_ptr<const Drawable>& drawable)
          : DebugDrawableInstance(drawable, Feature::Wireframe)
        {}
    };

    template<typename T>
    class DebugDrawable : public DebugDrawableBase
    {
    public:
        using Feature = DebugDrawableBase::Feature;

        template<typename... Args>
        DebugDrawable(Feature feature, Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = feature;
        }
    private:
        T mObject;
    };
    template<typename T>
    class Wireframe : public DebugDrawableBase
    {
    public:
        template<typename... Args>
        Wireframe(Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = Feature::Wireframe;
        }
    private:
        T mObject;
    };


    bool Is3DShape(const Drawable& drawable) noexcept;
    bool Is3DShape(const DrawableClass& klass) noexcept;

    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

} // namespace
