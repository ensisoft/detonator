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

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/math.h"
#include "base/threadpool.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/particle_engine.h"
#include "graphics/shader_source.h"
#include "graphics/geometry.h"
#include "graphics/program.h"
#include "graphics/transform.h"

namespace  {
    struct EnvironmentCopy {
        explicit EnvironmentCopy(const gfx::DrawableClass::Environment& env) noexcept
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
        gfx::DrawableClass::Environment ToEnv() const
        {
            gfx::DrawableClass::Environment env;
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

    std::function<float (float min, float max)> random_function;
    auto GetRandomGenerator()
    {
        if (!random_function)
            random_function = math::rand<float>;
        return random_function;
    }

} // namespace

namespace gfx
{

std::string ParticleEngineClass::GetShaderId(const Environment& env) const
{
    return env.use_instancing ? "instanced-particle-shader" : "particle-shader";
}

std::string ParticleEngineClass::GetGeometryId(const Environment& env) const
{
    return "particle-buffer";
}

ShaderSource ParticleEngineClass::GetShader(const Environment& env, const Device& device) const
{
    static const char* base_shader = {
#include "shaders/vertex_shader_base.glsl"
    };
    static const char* particle_shader = {
#include "shaders/vertex_2d_particle_shader.glsl"
    };

    ShaderSource source;
    source.SetVersion(gfx::ShaderSource::Version::GLSL_300);
    source.SetType(gfx::ShaderSource::Type::Vertex);
    if (env.use_instancing)
    {
        source.AddPreprocessorDefinition("INSTANCED_DRAW");
    }
    source.LoadRawSource(base_shader);
    source.LoadRawSource(particle_shader);
    source.AddShaderName("2D Particle Shader");
    source.AddShaderSourceUri("shaders/vertex_shader_base.glsl");
    source.AddShaderSourceUri("shaders/vertex_2d_particle_shader.glsl");
    source.AddDebugInfo("Instanced", env.use_instancing ? "YES" : "NO");
    return source;
}

std::string ParticleEngineClass::GetShaderName(const Environment& env) const
{
    return env.use_instancing ? "InstancedParticleShader" : "ParticleShader";
}

bool ParticleEngineClass::Construct(const Drawable::Environment& env,  const InstanceState& state, Geometry::CreateArgs& create) const
{
    // take a lock on the mutex to make sure that we don't have
    // a race condition with the background tasks.
    std::lock_guard<std::mutex> lock(state.mutex);

    // the point rasterization doesn't support non-uniform
    // sizes for the points, i.e. they're always square
    // so therefore we must choose one of the pixel ratio values
    // as the scaler for converting particle sizes to pixel/fragment
    // based sizes
    const auto pixel_scaler = std::min(env.pixel_ratio.x, env.pixel_ratio.y);

#pragma pack(push, 1)
    struct ParticleVertex {
        Vec2 aPosition;
        Vec2 aDirection;
        Vec4 aData;
    };
#pragma pack(pop)
    static const VertexLayout layout(sizeof(ParticleVertex), {
        {"aPosition",  0, 2, 0, offsetof(ParticleVertex, aPosition)},
        {"aDirection", 0, 2, 0, offsetof(ParticleVertex, aDirection)},
        {"aData",      0, 4, 0, offsetof(ParticleVertex, aData)}
    });
    static_assert(std::is_trivially_copyable<ParticleVertex>::value &&
                  std::is_trivially_copy_assignable<ParticleVertex>::value &&
                  std::is_standard_layout<ParticleVertex>::value, "Particle vertex is not supported.");

    auto& geometry = create.buffer;
    ASSERT(geometry.GetNumDrawCmds() == 0);
    create.usage = Geometry::Usage::Stream;
    create.content_name = mName;

    if (mParams->primitive == DrawPrimitive::Point)
    {
        TypedVertexBuffer<ParticleVertex> vertex_buffer;
        vertex_buffer.SetVertexLayout(layout);
        vertex_buffer.Resize(state.particles.size());

        for (size_t i=0; i<state.particles.size(); ++i)
        {
            const auto& particle = state.particles[i];
            // When using local coordinate space the max x/y should
            // be the extents of the simulation in which case the
            // particle x,y become normalized on the [0.0f, 1.0f] range.
            // when using global coordinate space max x/y should be 1.0f
            // and particle coordinates are left in the global space
            auto& vertex = vertex_buffer[i];
            vertex.aPosition.x = particle.position.x / mParams->max_xpos;
            vertex.aPosition.y = particle.position.y / mParams->max_ypos;
            vertex.aDirection = ToVec(particle.direction);
            // copy the per particle data into the data vector for the fragment shader.
            vertex.aData.x = particle.pointsize >= 0.0f ? particle.pointsize * pixel_scaler : 0.0f;
            // abusing texcoord here to provide per particle random value.
            // we can use this to simulate particle rotation for example
            // (if the material supports it)
            vertex.aData.y = particle.randomizer;
            // Use the particle data to pass the per particle alpha.
            vertex.aData.z = particle.alpha;
            // use the particle data to pass the per particle time.
            vertex.aData.w = particle.time / (particle.time_scale * mParams->max_lifetime);
        }

        geometry.SetVertexLayout(layout);
        geometry.SetVertexBuffer(std::move(vertex_buffer));
        geometry.AddDrawCmd(Geometry::DrawType::Points);
    }
    else if (mParams->primitive == DrawPrimitive::FullLine ||
             mParams->primitive == DrawPrimitive::PartialLineBackward ||
             mParams->primitive == DrawPrimitive::PartialLineForward)
    {
        // when the coordinate space is local we must be able to
        // determine what is the length of the line (that is expressed
        // in world units) in local units.
        // so we're going to take the point size (that is used to express
        // the line length), create a vector in global coordinates and map
        // that back to the local coordinate system to see what it maps to
        //
        // then the line geometry generation will take the particle point
        // and create a line that extends through that point half the line
        // length in both directions (forward and backward)

        // there's a bunch of things to improve here.
        //
        // - the speed of the particle is baked into the direction vector
        //   this complicates the line end point computation since we need
        //   just the direction which now requires normalizing the direction
        //   which is expensive.
        //
        // - computing the line length in local coordinate space is expensive
        //   this that requires inverse mapping
        //
        // - the boundary conditions on local coordinate space only consider
        //   the center point of the particle. on large particle sizes this
        //   might make the particle visually ugly since the extents of the
        //   rasterized area will then poke beyond the boundary. This problem
        //   is not just with lines but also with points.

        const auto& model_to_world = *env.model_matrix;
        const auto& world_to_model = mParams->coordinate_space == CoordinateSpace::Local
                                     ? glm::inverse(model_to_world) : glm::mat4(1.0f);

        const auto primitive = mParams->primitive;
        const auto particle_count = state.particles.size();

        TypedVertexBuffer<ParticleVertex> vertex_buffer;
        vertex_buffer.SetVertexLayout(layout);
        vertex_buffer.Resize(particle_count * 2); // for the 2 line end points.

        for (size_t i=0; i<state.particles.size(); ++i)
        {
            const auto& particle = state.particles[i];
            const auto line_length = mParams->coordinate_space == CoordinateSpace::Local
                                     ? glm::length(world_to_model * glm::vec4(particle.pointsize, 0.0f, 0.0f, 0.0f)) : particle.pointsize;

            const auto& pos = particle.position;
            // todo: the velocity is baked in the direction vector
            // so computing the end points for the line based on the
            // position and direction is expensive since need to
            // normalize...
            const auto& dir = glm::normalize(particle.direction);
            glm::vec2 start;
            glm::vec2 end;
            if (primitive == DrawPrimitive::FullLine)
            {
                start = pos + dir * line_length * 0.5f;
                end   = pos - dir * line_length * 0.5f;
            }
            else if (primitive == DrawPrimitive::PartialLineForward)
            {
                start = pos;
                end   = pos + dir * line_length * 0.5f;
            }
            else if (primitive == DrawPrimitive::PartialLineBackward)
            {
                start = pos - dir * line_length * 0.5f;
                end   = pos;
            }

            const auto vertex_index = 2 * i;

            ParticleVertex vertex;
            vertex.aPosition = ToVec(start / glm::vec2(mParams->max_xpos, mParams->max_ypos));
            vertex.aDirection = ToVec(particle.direction);
            // copy the per particle data into the data vector for the fragment shader.
            vertex.aData.x = particle.pointsize >= 0.0f ? particle.pointsize * pixel_scaler : 0.0f;
            // abusing texcoord here to provide per particle random value.
            // we can use this to simulate particle rotation for example
            // (if the material supports it)
            vertex.aData.y = particle.randomizer;
            // Use the particle data to pass the per particle alpha.
            vertex.aData.z = particle.alpha;
            // use the particle data to pass the per particle time.
            vertex.aData.w = particle.time / (particle.time_scale * mParams->max_lifetime);
            vertex_buffer[vertex_index+0] = vertex;

            vertex.aPosition = ToVec(end / glm::vec2(mParams->max_xpos, mParams->max_ypos));
            vertex_buffer[vertex_index+1] = vertex;
        }
        geometry.SetVertexBuffer(std::move(vertex_buffer));
        geometry.SetVertexLayout(layout);
        geometry.AddDrawCmd(Geometry::DrawType::Lines);
    }
    return true;
}

bool ParticleEngineClass::Construct(const Environment& env, const InstanceState& state, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    InstancedDrawBuffer buffer;
    buffer.SetInstanceDataLayout(GetInstanceDataLayout<InstanceAttribute>());
    buffer.Resize(draw.instances.size());

    for (size_t i=0; i<draw.instances.size(); ++i)
    {
        const auto& instance = draw.instances[i];
        InstanceAttribute ia;
        ia.iaModelVectorX = ToVec(instance.model_to_world[0]);
        ia.iaModelVectorY = ToVec(instance.model_to_world[1]);
        ia.iaModelVectorZ = ToVec(instance.model_to_world[2]);
        ia.iaModelVectorW = ToVec(instance.model_to_world[3]);
        buffer.SetInstanceData(ia, i);
    }

    // we're not making any contribution to the instance data here, therefore
    // the hash and usage are exactly what the caller specified.
    args.usage = draw.usage;
    args.content_hash = draw.content_hash;
    args.content_name = draw.content_name;
    args.buffer = std::move(buffer);
    return true;
}

bool ParticleEngineClass::ApplyDynamicState(const Environment& env, ProgramState& program) const
{
    if (mParams->coordinate_space == CoordinateSpace::Global)
    {
        // when the coordinate space is global the particles are spawn directly
        // in the global coordinate space. therefore, no model transformation
        // is needed but only the view transformation.
        const auto& kViewMatrix = *env.view_matrix;
        const auto& kProjectionMatrix = *env.proj_matrix;
        program.SetUniform("kProjectionMatrix", kProjectionMatrix);
        program.SetUniform("kModelViewMatrix", kViewMatrix);
    }
    else if (mParams->coordinate_space == CoordinateSpace::Local)
    {
        const auto& kModelViewMatrix = (*env.view_matrix) * (*env.model_matrix);
        const auto& kProjectionMatrix = *env.proj_matrix;
        program.SetUniform("kProjectionMatrix", kProjectionMatrix);
        program.SetUniform("kModelViewMatrix", kModelViewMatrix);
    }
    return true;
}

// Update the particle simulation.
void ParticleEngineClass::Update(const Environment& env, InstanceStatePtr ptr, float dt) const
{
    // In case particles become heavy on the CPU here are some ways to try
    // to mitigate the issue:
    // - Reduce the number of particles in the content (i.e. use less particles
    //   in animations etc.)
    // - Share complete particle engines between assets, i.e. instead of each
    //   animation (for example space ship) using its own particle engine
    //   instance each kind of ship could share one particle engine.
    // - Parallelize the particle updates, i.e. try to throw more CPU cores
    //   at the issue.
    // - Use the GPU instead of the CPU. On GL ES 2 there are no transform
    //   feedback buffers. But for any simple particle animation such as this
    //   that doesn't use any second degree derivatives it should be possible
    //   to do the simulation on the GPU without transform feedback. I.e. in the
    //   absence of acceleration a numerical integration of particle position
    //   is not needed but a new position can simply be computed with
    //   vec2 pos = initial_pos + time * velocity;
    //   Just that one problem that remains is killing particles at the end
    //   of their lifetime or when their size or alpha value reaches 0.
    //   Possibly a hybrid solution could be used.
    //   It could also be possible to simulate transform feedback through
    //   texture writes. For example here: https://nullprogram.com/webgl-particles/

    auto& state = *ptr;

    const bool has_max_time = mParams->max_time < std::numeric_limits<float>::max();

    // check if we've exceeded maximum lifetime.
    if (has_max_time && state.time >= mParams->max_time)
    {
        state.particles.clear();
        state.time += dt;
        return;
    }

    // with automatic spawn modes (once, maintain, continuous) do first
    // particle emission after initial delay has expired.
    if (mParams->mode != SpawnPolicy::Command)
    {
        if (state.time < state.delay)
        {
            if (state.time + dt > state.delay)
            {
                InitParticles(env, ptr, state.hatching);
                state.hatching = 0;
            }
            state.time += dt;
            return;
        }
    }

    UpdateParticles(env, ptr, dt);

    // Spawn new particles if needed.
    if (mParams->mode == SpawnPolicy::Maintain)
    {
        class MaintainParticlesTask : public base::ThreadTask {
        public:
            MaintainParticlesTask(const Environment& env, InstanceStatePtr state, EngineParamsPtr params)
              : mEnvironment(env)
              , mInstanceState(std::move(state))
              , mEngineParams(std::move(params))
            {}
        protected:
            void DoTask() override
            {
                const auto& env = mEnvironment.ToEnv();
                const auto& params = *mEngineParams;

                const auto num_particles_always = size_t(mEngineParams->num_particles);

                std::lock_guard<std::mutex> buffer_mutex(mInstanceState->buffer_mutex);

                const auto num_particles_now = mInstanceState->task_buffer[0].size();
                if (num_particles_now < num_particles_always)
                {
                    const auto num_particles_needed = num_particles_always - num_particles_now;

                    ParticleEngineClass::InitParticles(mEnvironment.ToEnv(),
                                                       *mEngineParams,
                                                       mInstanceState->task_buffer[0],
                                                       num_particles_needed);

                    // copy the updated contents over to the dst buffer.
                    // we're copying the data to a secondary buffer so that
                    // the part where we have to lock on the particle buffer
                    // that the renderer is using is as short as possible.
                    mInstanceState->task_buffer[1] = mInstanceState->task_buffer[0];

                    // make this change atomically available for rendering
                    {
                        std::lock_guard<std::mutex> lock(mInstanceState->mutex);
                        std::swap(mInstanceState->particles, mInstanceState->task_buffer[1]);
                    }
                }
                mInstanceState->task_count--;
            }
        private:
            const EnvironmentCopy mEnvironment;
            const InstanceStatePtr mInstanceState;
            const EngineParamsPtr  mEngineParams;
        };

        if (auto* pool = base::GetGlobalThreadPool())
        {
            ptr->task_count++;

            auto task = std::make_unique<MaintainParticlesTask>(env, ptr, mParams);
            task->SetTaskName("MaintainParticlesTask");
            pool->SubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);

        }
        else
        {
            const auto num_particles_always = size_t(mParams->num_particles);
            const auto num_particles_now = state.particles.size();
            if (num_particles_now < num_particles_always)
            {
                const auto num_particles_needed = num_particles_always - num_particles_now;

                std::lock_guard<std::mutex> lock(state.mutex);

                ParticleEngineClass::InitParticles(env, *mParams, ptr->particles, num_particles_needed);
            }
        }
    }
    else if (mParams->mode == SpawnPolicy::Continuous)
    {
        // the number of particles is taken as the rate of particles per
        // second. fractionally cumulate particles and then
        // spawn when we have some number non-fractional particles.
        state.hatching += mParams->num_particles * dt;
        const auto num = size_t(state.hatching);
        InitParticles(env, ptr, num);
        state.hatching -= num;
    }
    state.time += dt;
}

void ParticleEngineClass::IntoJson(data::Writer& data) const
{
    data.Write("id",                           mId);
    data.Write("name",                         mName);
    data.Write("primitive",                    mParams->primitive);
    data.Write("direction",                    mParams->direction);
    data.Write("placement",                    mParams->placement);
    data.Write("shape",                        mParams->shape);
    data.Write("coordinate_space",             mParams->coordinate_space);
    data.Write("motion",                       mParams->motion);
    data.Write("mode",                         mParams->mode);
    data.Write("boundary",                     mParams->boundary);
    data.Write("delay",                        mParams->delay);
    data.Write("min_time",                     mParams->min_time);
    data.Write("max_time",                     mParams->max_time);
    data.Write("warmup_time",                  mParams->warmup_time);
    data.Write("num_particles",                mParams->num_particles);
    data.Write("min_lifetime",                 mParams->min_lifetime);
    data.Write("max_lifetime",                 mParams->max_lifetime);
    data.Write("max_xpos",                     mParams->max_xpos);
    data.Write("max_ypos",                     mParams->max_ypos);
    data.Write("init_rect_xpos",               mParams->init_rect_xpos);
    data.Write("init_rect_ypos",               mParams->init_rect_ypos);
    data.Write("init_rect_width",              mParams->init_rect_width);
    data.Write("init_rect_height",             mParams->init_rect_height);
    data.Write("min_velocity",                 mParams->min_velocity);
    data.Write("max_velocity",                 mParams->max_velocity);
    data.Write("direction_sector_start_angle", mParams->direction_sector_start_angle);
    data.Write("direction_sector_size",        mParams->direction_sector_size);
    data.Write("min_point_size",               mParams->min_point_size);
    data.Write("max_point_size",               mParams->max_point_size);
    data.Write("min_alpha",                    mParams->min_alpha);
    data.Write("max_alpha",                    mParams->max_alpha);
    data.Write("growth_over_time",             mParams->rate_of_change_in_size_wrt_time);
    data.Write("growth_over_dist",             mParams->rate_of_change_in_size_wrt_dist);
    data.Write("alpha_over_time",              mParams->rate_of_change_in_alpha_wrt_time);
    data.Write("alpha_over_dist",              mParams->rate_of_change_in_alpha_wrt_dist);
    data.Write("gravity",                      mParams->gravity);
    data.Write("flags",                        mParams->flags);
}


bool ParticleEngineClass::FromJson(const data::Reader& data)
{
    Params params;

    bool ok = true;
    ok &= data.Read("id",                           &mId);
    ok &= data.Read("name",                         &mName);
    ok &= data.Read("primitive",                    &params.primitive);
    ok &= data.Read("direction",                    &params.direction);
    ok &= data.Read("placement",                    &params.placement);
    ok &= data.Read("shape",                        &params.shape);
    ok &= data.Read("coordinate_space",             &params.coordinate_space);
    ok &= data.Read("motion",                       &params.motion);
    ok &= data.Read("mode",                         &params.mode);
    ok &= data.Read("boundary",                     &params.boundary);
    ok &= data.Read("delay",                        &params.delay);
    ok &= data.Read("min_time",                     &params.min_time);
    ok &= data.Read("max_time",                     &params.max_time);
    ok &= data.Read("num_particles",                &params.num_particles);
    ok &= data.Read("min_lifetime",                 &params.min_lifetime) ;
    ok &= data.Read("max_lifetime",                 &params.max_lifetime);
    ok &= data.Read("max_xpos",                     &params.max_xpos);
    ok &= data.Read("max_ypos",                     &params.max_ypos);
    ok &= data.Read("init_rect_xpos",               &params.init_rect_xpos);
    ok &= data.Read("init_rect_ypos",               &params.init_rect_ypos);
    ok &= data.Read("init_rect_width",              &params.init_rect_width);
    ok &= data.Read("init_rect_height",             &params.init_rect_height);
    ok &= data.Read("min_velocity",                 &params.min_velocity);
    ok &= data.Read("max_velocity",                 &params.max_velocity);
    ok &= data.Read("direction_sector_start_angle", &params.direction_sector_start_angle);
    ok &= data.Read("direction_sector_size",        &params.direction_sector_size);
    ok &= data.Read("min_point_size",               &params.min_point_size);
    ok &= data.Read("max_point_size",               &params.max_point_size);
    ok &= data.Read("min_alpha",                    &params.min_alpha);
    ok &= data.Read("max_alpha",                    &params.max_alpha);
    ok &= data.Read("growth_over_time",             &params.rate_of_change_in_size_wrt_time);
    ok &= data.Read("growth_over_dist",             &params.rate_of_change_in_size_wrt_dist);
    ok &= data.Read("alpha_over_time",              &params.rate_of_change_in_alpha_wrt_time);
    ok &= data.Read("alpha_over_dist",              &params.rate_of_change_in_alpha_wrt_dist);
    ok &= data.Read("gravity",                      &params.gravity);

    if (data.HasValue("flags"))
        ok &= data.Read("flags", &params.flags);
    if (data.HasValue("warmup_time"))
        ok &= data.Read("warmup_time", &params.warmup_time);

    SetParams(params);
    return ok;
}

std::unique_ptr<DrawableClass> ParticleEngineClass::Clone() const
{
    auto ret = std::make_unique<ParticleEngineClass>(*this);
    ret->mId = base::RandomString(10);
    return ret;
}
std::unique_ptr<DrawableClass> ParticleEngineClass::Copy() const
{
    return std::make_unique<ParticleEngineClass>(*this);
}

std::size_t ParticleEngineClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, *mParams);
    return hash;
}

// ParticleEngine implementation.
bool ParticleEngineClass::IsAlive(const InstanceStatePtr& ptr) const
{
    auto& state = *ptr;

    if (state.time < mParams->delay)
        return true;
    else if (state.time < mParams->min_time)
        return true;
    else if (state.time > mParams->max_time)
        return false;

    if (mParams->mode == SpawnPolicy::Continuous ||
        mParams->mode == SpawnPolicy::Maintain ||
        mParams->mode == SpawnPolicy::Command)
        return true;

    // if we have pending tasks we must be alive still
    if (state.task_count)
        return true;

    return !state.particles.empty();
}

void ParticleEngineClass::Emit(const Environment& env, InstanceStatePtr ptr, int count) const
{
    if (count < 0)
        return;
    if (mParams->mode != SpawnPolicy::Command)
    {
        WARN("Ignoring emit particle command since spawn policy is not set to emit on command. [name='%1', mode='%2']",
            mName, mParams->mode);
        return;
    }

    InitParticles(env, std::move(ptr), static_cast<size_t>(count));
}

// ParticleEngine implementation. Restart the simulation
// with the previous parameters.
void ParticleEngineClass::Restart(const Environment& env, InstanceStatePtr ptr) const
{
    auto& state = *ptr;

    class ClearParticlesTask : public base::ThreadTask {
    public:
        explicit ClearParticlesTask(InstanceStatePtr state) noexcept
          : mInstanceState(std::move(state))
        {}
    protected:
        void DoTask() override
        {
            {
                std::lock_guard<std::mutex> lock(mInstanceState->buffer_mutex);
                mInstanceState->task_buffer[0].clear();
                mInstanceState->task_buffer[1].clear();
            }

            {
                std::lock_guard<std::mutex> lock(mInstanceState->mutex);
                mInstanceState->particles.clear();
                mInstanceState->task_count--;
            }
        }
    private:
        const InstanceStatePtr mInstanceState;
    };

    if (auto* pool = base::GetGlobalThreadPool())
    {
        ptr->task_count++;

        auto task = std::make_unique<ClearParticlesTask>(ptr);
        task->SetTaskName("ClearParticlesTask");
        pool->SubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
    }
    else
    {
        std::lock_guard<std::mutex> lock(state.mutex);

        state.particles.clear();
    }

    state.delay    = mParams->delay;
    state.time     = 0.0f;
    state.hatching = 0.0f;
    // if the spawn policy is continuous the num particles
    // is a rate of particles per second. in order to avoid
    // a massive initial burst of particles skip the init here
    if (mParams->mode == SpawnPolicy::Continuous)
        return;

    // if the spawn mode is on command only we don't spawn anything
    // unless by command.
    if (mParams->mode == SpawnPolicy::Command)
        return;

    if (state.delay != 0.0f)
        state.hatching = mParams->num_particles;
    else InitParticles(env, ptr, size_t(mParams->num_particles));
}

void ParticleEngineClass::UpdateParticles(const Environment& env, InstanceStatePtr state, float dt) const
{
    class UpdateParticlesTask : public base::ThreadTask {
    public:
        UpdateParticlesTask(const Environment& env, InstanceStatePtr state, EngineParamsPtr params, float dt)
          : mEnvironment(env)
          , mInstanceState(std::move(state))
          , mEngineParams(std::move(params))
          , mTimeStep(dt)
        {}
    protected:
        void DoTask() override
        {
            const auto& env = mEnvironment.ToEnv();
            const auto& params = *mEngineParams;

            {
                std::lock_guard<std::mutex> buffer_lock(mInstanceState->buffer_mutex);

                ParticleEngineClass::UpdateParticles(mEnvironment.ToEnv(),
                                                     *mEngineParams,
                                                     mInstanceState->task_buffer[0],
                                                     mTimeStep);

                // copy the updated contents over to the dst buffer.
                // we're copying the data to a secondary buffer so that
                // the part where we have to lock on the particle buffer
                // that the renderer is using is as short as possible.
                mInstanceState->task_buffer[1] = mInstanceState->task_buffer[0];
            }

            // make this change atomically available for rendering
            {
                std::lock_guard<std::mutex> lock(mInstanceState->mutex);
                std::swap(mInstanceState->particles, mInstanceState->task_buffer[1]);
            }
            mInstanceState->task_count--;
        }
    private:
        const EnvironmentCopy mEnvironment;
        const InstanceStatePtr mInstanceState;
        const EngineParamsPtr  mEngineParams;
        const float mTimeStep;
    };

    if (auto* pool = base::GetGlobalThreadPool())
    {
        state->task_count++;

        auto task = std::make_unique<UpdateParticlesTask>(env, std::move(state), mParams, dt);
        task->SetTaskName("UpdateParticles");
        pool->SubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
    }
    else
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        ParticleEngineClass::UpdateParticles(env, *mParams, state->particles, dt);
    }
}

// static
void ParticleEngineClass::UpdateParticles(const Environment& env, const Params& params, ParticleBuffer& particles, float dt)
{
    ParticleWorld world;
    // transform the gravity vector associated with the particle engine
    // to world space. For example when the rendering system uses dimetric
    // rendering for some shape (we're looking at it at on a xy plane at
    // a certain angle) the gravity vector needs to be transformed so that
    // the local gravity vector makes sense in this dimetric world.
    if (env.world_matrix && params.coordinate_space == CoordinateSpace::Global)
    {
        const auto local_gravity_dir = glm::normalize(params.gravity);
        const auto world_gravity_dir = glm::normalize(*env.world_matrix * glm::vec4 { local_gravity_dir, 0.0f, 0.0f });
        const auto world_gravity = glm::vec2 { world_gravity_dir.x * std::abs(params.gravity.x),
                                               world_gravity_dir.y * std::abs(params.gravity.y) };
        world.world_gravity = world_gravity;
    }

    // update each current particle
    for (size_t i=0; i<particles.size();)
    {
        if (UpdateParticle(env, params, world, particles, i, dt))
        {
            ++i;
            continue;
        }
        KillParticle(particles, i);
    }
}

// static
void ParticleEngineClass::SetRandomGenerator(std::function<float(float min, float max)> rd)
{
    random_function = std::move(rd);
}

// static
void ParticleEngineClass::InitParticles(const Environment& env, InstanceStatePtr state, size_t num) const
{
    class InitParticlesTask : public base::ThreadTask {
    public:
        InitParticlesTask(const Environment& env, InstanceStatePtr state, EngineParamsPtr params, size_t count)
          : mEnvironment(env)
          , mInstanceState(std::move(state))
          , mEngineParams(std::move(params))
          , mInitCount(count)
        {}
    protected:
        void DoTask() override
        {
            const auto& env = mEnvironment.ToEnv();
            const auto& params = *mEngineParams;

            {
                std::lock_guard<std::mutex> buffer_lock(mInstanceState->buffer_mutex);

                ParticleEngineClass::InitParticles(mEnvironment.ToEnv(),
                                                   *mEngineParams,
                                                   mInstanceState->task_buffer[0],
                                                   mInitCount);

                // copy the updated contents over to the dst buffer.
                // we're copying the data to a secondary buffer so that
                // the part where we have to lock on the particle buffer
                // that the renderer is using is as short as possible.
                mInstanceState->task_buffer[1] = mInstanceState->task_buffer[0];
            }

            // make this change atomically available for rendering
            {
                std::lock_guard<std::mutex> lock(mInstanceState->mutex);
                std::swap(mInstanceState->particles, mInstanceState->task_buffer[1]);
            }
            mInstanceState->task_count--;
        }
    private:
        const EnvironmentCopy mEnvironment;
        const InstanceStatePtr mInstanceState;
        const EngineParamsPtr  mEngineParams;
        const size_t mInitCount = 0;
    };

    if (auto* pool = base::GetGlobalThreadPool())
    {
        state->task_count++;

        auto task = std::make_unique<InitParticlesTask>(env, std::move(state), mParams, num);
        task->SetTaskName("InitParticlesTask");
        pool->SubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
    }
    else
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        ParticleEngineClass::InitParticles(env, *mParams, state->particles, num);
    }
}

// static
void ParticleEngineClass::InitParticles(const Environment& env, const Params& params, ParticleBuffer& particles, size_t num)
{
    // basic sanity to avoid division by zero.
    if (params.max_lifetime <= 0.0f || params.max_lifetime < params.min_lifetime)
        return;

    const bool can_expire = params.flags.test(Flags::ParticlesCanExpire);

    const auto count = particles.size();
    particles.resize(count + num);

    auto GenRandomNumber = GetRandomGenerator();

    if (params.coordinate_space == CoordinateSpace::Global)
    {
        gfx::Transform transform(*env.model_matrix);
        transform.Push();
            transform.Scale(params.init_rect_width, params.init_rect_height);
            transform.Translate(params.init_rect_xpos, params.init_rect_ypos);
        const auto& particle_to_world = transform.GetAsMatrix();
        const auto emitter_radius = 0.5f;
        const auto emitter_center = glm::vec2(0.5f, 0.5f);

        for (size_t i=0; i<num; ++i)
        {
            const auto velocity = GenRandomNumber(params.min_velocity, params.max_velocity);

            glm::vec2 position;
            glm::vec2 direction;
            if (params.shape == EmitterShape::Rectangle)
            {
                if (params.placement == Placement::Inside)
                    position = glm::vec2(GenRandomNumber(0.0f, 1.0f), GenRandomNumber(0.0f, 1.0f));
                else if (params.placement == Placement::Center)
                    position = glm::vec2(0.5f, 0.5f);
                else if (params.placement == Placement::Edge)
                {
                    const auto value = static_cast<int>(GenRandomNumber(0.0f, 1.0f) * 100.0f);
                    const auto edge = value % 4;
                    if (edge == 0 || edge == 1)
                    {
                        position.x = edge == 0 ? 0.0f : 1.0f;
                        position.y = GenRandomNumber(0.0f, 1.0f);
                    }
                    else
                    {
                        position.x = GenRandomNumber(0.0f, 1.0f);
                        position.y = edge == 2 ? 0.0f : 1.0f;
                    }
                }
            }
            else if (params.shape == EmitterShape::Circle)
            {
                if (params.placement == Placement::Center)
                    position = glm::vec2(0.5f, 0.5f);
                else if (params.placement == Placement::Inside)
                {
                    const auto x = GenRandomNumber(-emitter_radius, emitter_radius);
                    const auto y = GenRandomNumber(-emitter_radius, emitter_radius);
                    const auto r = GenRandomNumber(0.0f, 1.0f);
                    position = glm::normalize(glm::vec2(x, y)) * emitter_radius * r + emitter_center;
                }
                else if (params.placement == Placement::Edge)
                {
                    const auto x = GenRandomNumber(-emitter_radius, emitter_radius);
                    const auto y = GenRandomNumber(-emitter_radius, emitter_radius);
                    position = glm::normalize(glm::vec2(x, y)) * emitter_radius + emitter_center;
                }
            }

            if (params.direction == Direction::Sector)
            {
                const auto direction_angle = params.direction_sector_start_angle +
                                                 GenRandomNumber(0.0f, params.direction_sector_size);

                const float model_to_world_rotation = math::GetRotationFromMatrix(*env.model_matrix);
                const auto world_direction =  math::RotateVectorAroundZ(glm::vec2(1.0f, 0.0f), model_to_world_rotation + direction_angle);
                direction = world_direction;
            }
            else if (params.placement == Placement::Center)
            {
                direction = glm::normalize(glm::vec2(GenRandomNumber(-1.0f, 1.0f),
                                                     GenRandomNumber(-1.0f, 1.0f)));
            }
            else if (params.direction == Direction::Inwards)
                direction = glm::normalize(emitter_center - position);
            else if (params.direction == Direction::Outwards)
                direction = glm::normalize(position - emitter_center);

            const auto world = particle_to_world * glm::vec4(position, 0.0f, 1.0f);
            // note that the velocity vector is baked into the
            // direction vector in order to save space.
            auto& particle = particles[count+i];
            particle.time       = 0.0f;
            particle.time_scale = can_expire ? GenRandomNumber(params.min_lifetime, params.max_lifetime) / params.max_lifetime : 1.0f;
            particle.pointsize  = GenRandomNumber(params.min_point_size, params.max_point_size);
            particle.alpha      = GenRandomNumber(params.min_alpha, params.max_alpha);
            particle.position   = glm::vec2(world.x, world.y);
            particle.direction  = direction * velocity;
            particle.randomizer = GenRandomNumber(0.0f, 1.0f);
        }
    }
    else if (params.coordinate_space == CoordinateSpace::Local)
    {
        // the emitter box uses normalized coordinates
        const auto sim_width  = params.max_xpos;
        const auto sim_height = params.max_ypos;
        const auto emitter_width  = params.init_rect_width * sim_width;
        const auto emitter_height = params.init_rect_height * sim_height;
        const auto emitter_xpos   = params.init_rect_xpos * sim_width;
        const auto emitter_ypos   = params.init_rect_ypos * sim_height;
        const auto emitter_radius = std::min(emitter_width, emitter_height) * 0.5f;
        const auto emitter_center = glm::vec2(emitter_xpos + emitter_width * 0.5f,
                                              emitter_ypos + emitter_height * 0.5f);
        const auto emitter_size  = glm::vec2(emitter_width, emitter_height);
        const auto emitter_pos   = glm::vec2(emitter_xpos, emitter_ypos);
        const auto emitter_left  = emitter_xpos;
        const auto emitter_right = emitter_xpos + emitter_width;
        const auto emitter_top   = emitter_ypos;
        const auto emitter_bot   = emitter_ypos + emitter_height;

        for (size_t i = 0; i < num; ++i)
        {
            const auto velocity = GenRandomNumber(params.min_velocity, params.max_velocity);
            glm::vec2 position;
            glm::vec2 direction;
            if (params.shape == EmitterShape::Rectangle)
            {
                if (params.placement == Placement::Inside)
                    position = emitter_pos + glm::vec2(GenRandomNumber(0.0f, emitter_width),
                                                       GenRandomNumber(0.0f, emitter_height));
                else if (params.placement == Placement::Center)
                    position = emitter_center;
                else if (params.placement == Placement::Edge)
                {
                    const auto value = static_cast<int>(GenRandomNumber(0.0f, 1.0f) * 100.0f);
                    const auto edge = value % 4;
                    if (edge == 0 || edge == 1)
                    {
                        position.x = edge == 0 ? emitter_left : emitter_right;
                        position.y = GenRandomNumber(emitter_top, emitter_bot);
                    }
                    else
                    {
                        position.x = GenRandomNumber(emitter_left, emitter_right);
                        position.y = edge == 2 ? emitter_top : emitter_bot;
                    }
                }
                else if (params.placement == Placement::Outside)
                {
                    position.x = GenRandomNumber(0.0f, sim_width);
                    position.y = GenRandomNumber(0.0f, sim_height);
                    if (position.y >= emitter_top && position.y <= emitter_bot)
                    {
                        if (position.x < emitter_center.x)
                            position.x = math::clamp(0.0f, emitter_left, position.x);
                        else position.x = math::clamp(emitter_right, sim_width, position.x);
                    }
                }
            }
            else if (params.shape == EmitterShape::Circle)
            {
                if (params.placement == Placement::Center)
                    position  = emitter_center;
                else if (params.placement == Placement::Inside)
                {
                    const auto x = GenRandomNumber(-1.0f, 1.0f);
                    const auto y = GenRandomNumber(-1.0f, 1.0f);
                    const auto r = GenRandomNumber(0.0f, 1.0f);
                    const auto p = glm::normalize(glm::vec2(x, y)) * emitter_radius * r;
                    position = p + emitter_pos + emitter_size * 0.5f;
                }
                else if (params.placement == Placement::Edge)
                {
                    const auto x = GenRandomNumber(-1.0f, 1.0f);
                    const auto y = GenRandomNumber(-1.0f, 1.0f);
                    const auto p = glm::normalize(glm::vec2(x, y)) * emitter_radius;
                    position = p + emitter_pos + emitter_size * 0.5f;
                }
                else if (params.placement == Placement::Outside)
                {
                    auto p = glm::vec2(GenRandomNumber(0.0f, sim_width),
                                       GenRandomNumber(0.0f, sim_height));
                    auto v = p - emitter_center;
                    if (glm::length(v) < emitter_radius)
                        p = glm::normalize(v) * emitter_radius + emitter_center;

                    position = p;
                }
            }

            if (params.direction == Direction::Sector)
            {
                const auto angle = GenRandomNumber(0.0f, params.direction_sector_size) + params.direction_sector_start_angle;
                direction = glm::vec2(std::cos(angle), std::sin(angle));
            }
            else if (params.placement == Placement::Center)
            {
                direction = glm::normalize(glm::vec2(GenRandomNumber(-1.0f, 1.0f),
                                                     GenRandomNumber(-1.0f, 1.0f)));
            }
            else if (params.direction == Direction::Inwards)
                direction = glm::normalize(emitter_center - position);
            else if (params.direction == Direction::Outwards)
                direction = glm::normalize(position - emitter_center);

            // note that the velocity vector is baked into the
            // direction vector in order to save space.
            auto& particle      = particles[count+i];
            particle.time       = 0.0f;
            particle.time_scale = can_expire ? GenRandomNumber(params.min_lifetime, params.max_lifetime) / params.max_lifetime : 1.0f;
            particle.pointsize  = GenRandomNumber(params.min_point_size, params.max_point_size);
            particle.alpha      = GenRandomNumber(params.min_alpha, params.max_alpha);
            particle.position   = position;
            particle.direction  = direction *  velocity;
            particle.randomizer = GenRandomNumber(0.0f, 1.0f);
        }
    } else BUG("Unhandled particle system coordinate space.");
}

// static
void ParticleEngineClass::KillParticle(ParticleBuffer& particles, size_t i)
{
    const auto last = particles.size() - 1;
    std::swap(particles[i], particles[last]);
    particles.pop_back();
}


// static
bool ParticleEngineClass::UpdateParticle(const Environment& env, const Params& params, const ParticleWorld& world,
                                         ParticleBuffer& particles, size_t i, float dt)
{
    auto& p = particles[i];

    p.time += dt;

    if (params.flags.test(Flags::ParticlesCanExpire))
    {
        if (p.time > p.time_scale * params.max_lifetime)
            return false;
    }

    const auto p0 = p.position;

    // update change in position
    if (params.motion == Motion::Linear)
        p.position += (p.direction * dt);
    else if (params.motion == Motion::Projectile)
    {
        glm::vec2 gravity = params.gravity;
        if (world.world_gravity.has_value())
            gravity = world.world_gravity.value();

        p.position += (p.direction * dt);
        p.direction += (dt * gravity);
    }

    const auto& p1 = p.position;
    const auto& dp = p1 - p0;
    const auto  dd = glm::length(dp);

    // Update particle size with respect to time and distance
    p.pointsize += (dt * params.rate_of_change_in_size_wrt_time * p.time_scale);
    p.pointsize += (dd * params.rate_of_change_in_size_wrt_dist);
    if (p.pointsize <= 0.0f)
        return false;

    // update particle alpha value with respect to time and distance.
    p.alpha += (dt * params.rate_of_change_in_alpha_wrt_time * p.time_scale);
    p.alpha += (dt * params.rate_of_change_in_alpha_wrt_dist);
    if (p.alpha <= 0.0f)
        return false;
    p.alpha = math::clamp(0.0f, 1.0f, p.alpha);

    // accumulate distance approximation
    p.distance += dd;

    // todo:
    if (params.coordinate_space == CoordinateSpace::Global)
        return true;

    // boundary conditions.
    if (params.boundary == BoundaryPolicy::Wrap)
    {
        p.position.x = math::wrap(0.0f, params.max_xpos, p.position.x);
        p.position.y = math::wrap(0.0f, params.max_ypos, p.position.y);
    }
    else if (params.boundary == BoundaryPolicy::Clamp)
    {
        p.position.x = math::clamp(0.0f, params.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, params.max_ypos, p.position.y);
    }
    else if (params.boundary == BoundaryPolicy::Kill)
    {
        if (p.position.x < 0.0f || p.position.x > params.max_xpos)
            return false;
        else if (p.position.y < 0.0f || p.position.y > params.max_ypos)
            return false;
    }
    else if (params.boundary == BoundaryPolicy::Reflect)
    {
        glm::vec2 n;
        if (p.position.x <= 0.0f)
            n = glm::vec2(1.0f, 0.0f);
        else if (p.position.x >= params.max_xpos)
            n = glm::vec2(-1.0f, 0.0f);
        else if (p.position.y <= 0.0f)
            n = glm::vec2(0, 1.0f);
        else if (p.position.y >= params.max_ypos)
            n = glm::vec2(0, -1.0f);
        else return true;
        // compute new direction vector given the normal vector of the boundary
        // and then bake the velocity in the new direction
        const auto& d = glm::normalize(p.direction);
        const float v = glm::length(p.direction);
        p.direction = (d - 2 * glm::dot(d, n) * n) * v;

        // clamp the position in order to eliminate the situation
        // where the object has moved beyond the boundaries of the simulation
        // and is stuck there alternating it's direction vector
        p.position.x = math::clamp(0.0f, params.max_xpos, p.position.x);
        p.position.y = math::clamp(0.0f, params.max_ypos, p.position.y);
    }
    return true;
}

bool ParticleEngineInstance::ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const
{
    // state.line_width = 1.0; // don't change the line width
    state.culling    = Culling::None;
    return mClass->ApplyDynamicState(env, program);
}

ShaderSource ParticleEngineInstance::GetShader(const Environment& env, const Device& device) const
{
    return mClass->GetShader(env, device);
}
std::string ParticleEngineInstance::GetShaderId(const Environment&  env) const
{
    return mClass->GetShaderId(env);
}
std::string ParticleEngineInstance::GetShaderName(const Environment& env) const
{
    return mClass->GetShaderName(env);
}
std::string ParticleEngineInstance::GetGeometryId(const Environment& env) const
{
    return mClass->GetGeometryId(env);
}

bool ParticleEngineInstance::Construct(const Environment& env, Geometry::CreateArgs& create) const
{
    return mClass->Construct(env, *mState, create);
}

bool ParticleEngineInstance::Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    return mClass->Construct(env, *mState, draw, args);
}

void ParticleEngineInstance::Update(const Environment& env, float dt)
{
    mClass->Update(env, mState, dt);
}

bool ParticleEngineInstance::IsAlive() const
{
    return mClass->IsAlive(mState);
}

void ParticleEngineInstance::Restart(const Environment& env)
{
    mClass->Restart(env, mState);

    // consume the initial warmup time (if any) by doing successive
    // updates which then simulate the particle system to a certain
    // approximate initial state. This is useful to help get the
    // particles to a visually more desirable certain state that only
    // appears after some time has passed.

    float warmup_time = mClass->GetParams().warmup_time;

    while (warmup_time > 0.0f)
    {
        const auto dt = 1.0f / 60.0f;
        mClass->Update(env, mState, dt);
        warmup_time -= dt;
    }
}

void ParticleEngineInstance::Execute(const Environment& env, const Command& cmd)
{
    const auto& params = mClass->GetParams();
    const auto default_emit_count = static_cast<int>(params.num_particles);
    if (cmd.name == "EmitParticles" && params.mode == ParticleEngineClass::SpawnPolicy::Command)
    {
        if (const auto* count = base::SafeFind(cmd.args, std::string("count")))
        {
            if (const auto* val = std::get_if<int>(count))
            {
                if (*val > 0)
                    mClass->Emit(env, mState, *val);
                else mClass->Emit(env, mState, default_emit_count);
            }
            else
            {
                WARN("Particle engine 'EmitParticles' command argument 'count' has wrong type. Expected 'int'.");
                mClass->Emit(env, mState, default_emit_count);
            }
        }
        else
        {
            mClass->Emit(env, mState, default_emit_count);
        }
    }
    else WARN("No such particle engine command. [cmd='%1']", cmd.name);
}

Drawable::DrawPrimitive ParticleEngineInstance::GetDrawPrimitive() const
{
    const auto p = mClass->GetParams().primitive;
    if (p == ParticleEngineClass::DrawPrimitive::Point)
        return DrawPrimitive::Points;
    else if (p == ParticleEngineClass::DrawPrimitive::FullLine ||
             p == ParticleEngineClass::DrawPrimitive::PartialLineBackward ||
             p == ParticleEngineClass::DrawPrimitive::PartialLineForward)
        return DrawPrimitive ::Lines;
    else BUG("Bug on particle engine draw primitive.");
    return DrawPrimitive::Points;
}

} // namespace
