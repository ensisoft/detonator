// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "base/math.h"
#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/geometry_algo.h"
#include "graphics/device.h"
#include "graphics/effect_drawable.h"
#include "graphics/texture.h"
#include "graphics/utility.h"

namespace {
std::function<float (float min, float max)> random_function;
} // namespace

namespace gfx
{
EffectDrawable::EffectDrawable(std::shared_ptr<Drawable> drawable, std::string effectId) noexcept
  : mDrawable(std::move(drawable))
  , mEffectId(std::move(effectId))
{
    if (!random_function)
        random_function = math::rand<float>;

    mSourceDrawable = mDrawable;
}

bool EffectDrawable::EnableEffect()
{
    mEnabled = true;
    if (mEffectDrawable)
        mDrawable = mEffectDrawable;

    return true;
}

void EffectDrawable::DisableEffect()
{
    mEnabled = false;
}

bool EffectDrawable::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState&  state) const
{
    Environment e = env;
    e.mesh_type =  mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;

    if (!mDrawable->ApplyDynamicState(e, device, program, state))
        return false;

    if (mEnabled)
    {
        auto* texture = device.FindTexture(mEffectId);
        if (!texture)
            return false;

        const auto texture_count = program.GetSamplerCount();

        program.SetUniform("kEffectMeshCenter", mShapeCenter);
        program.SetUniform("kEffectTime", static_cast<float>(mCurrentTime));
        program.SetUniform("kEffectType", static_cast<int>(mType));
        program.SetTexture("kShardDataTexture", texture_count, *texture);
        program.SetTextureCount(texture_count + 1);

        if (mType == EffectType::ShardedMeshExplosion)
        {
            ASSERT(std::holds_alternative<MeshExplosionEffectArgs>(mArgs));
            const auto* effect_args = std::get_if<MeshExplosionEffectArgs>(&mArgs);
            glm::vec4 args;
            args.x = effect_args->shard_linear_speed;
            args.y = effect_args->shard_linear_acceleration;
            args.z = effect_args->shard_rotational_speed;
            args.w = effect_args->shard_rotational_acceleration;
            program.SetUniform("kEffectArgs", args);
        }
    }
    return true;
}

ShaderSource EffectDrawable::GetShader(const Environment& env, const Device& device) const
{
    Environment e = env;
    e.mesh_type =  mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;
    return mDrawable->GetShader(e, device);
}
std::string EffectDrawable::GetShaderId(const Environment& env) const
{
    Environment e = env;
    e.mesh_type = mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;
    return mDrawable->GetShaderId(e);
}
std::string EffectDrawable::GetShaderName(const Environment& env) const
{
    Environment e = env;
    e.mesh_type = mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;
    return mDrawable->GetShaderName(e);
}
std::string EffectDrawable::GetGeometryId(const Environment& env) const
{
    if (!mEnabled)
        return mDrawable->GetGeometryId(env);

    // A note about the geometry ID. Since we're now using static geometry
    // for the mesh effect we essentially need to generate a new unique mesh
    // ID for each effect.
    // For example if the source drawable geometry is a "rectangle" we'd map
    // to a static Geometry ID which would mean that any mesh effect geometry
    // derived from the static rectangle geometry would end up referring
    // to the same geometry data on the GPU.
    // I.e. every spaceship explosion would be the same.

    // So to fix this problem we either make the mesh dynamic and update
    // the geometry data individually per each effect or make sure that
    // each effect maps to a different GPU geometry.

    Environment e = env;
    e.mesh_type = mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;

    std::string id;
    id += mDrawable->GetGeometryId(e);
    id += "Effect:";
    id += mEffectId;
    return id;
}
bool EffectDrawable::Construct(const Environment& env, Device& device, Geometry::CreateArgs& create) const
{
    if (!mEnabled)
        return mDrawable->Construct(env, device, create);

    const auto source_draw_primitive = mDrawable->GetDrawPrimitive();
    if (source_draw_primitive != DrawPrimitive::Triangles)
    {
        ERROR("Effects mesh can only be constructed with triangle mesh topology. [top='%1']",
            source_draw_primitive);
        return false;
    }

    if (mType == EffectType::ShardedMeshExplosion)
    {
        ASSERT(std::holds_alternative<MeshExplosionEffectArgs>(mArgs));
        const auto* effect_args = std::get_if<MeshExplosionEffectArgs>(&mArgs);
        return ConstructShardMesh(env, device, create, effect_args->mesh_subdivision_count);
    }
    else BUG("Unhandled effect drawable type.");

    return false;
}
bool EffectDrawable::Construct(const Environment& env, Device& device, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const
{
    return mDrawable->Construct(env, device, draw, args);
}

void EffectDrawable::Update(const Environment &env, float dt)
{
    if (!mEnabled)
        return mDrawable->Update(env, dt);

    mCurrentTime += dt;
}

void EffectDrawable::Restart(const Environment& env)
{
    mDrawable->Restart(env);
}

size_t EffectDrawable::GetGeometryHash() const
{
    // we don't return content / geometry hash here, because it'd be expensive
    // to compute since it depends on the geometry that the drawable we're
    // wrapping produces. Instead we can say our geometry hash changes
    // whenever the drawable geometry hash changes.
    // At runtime this would likely produce a stupid result if the geometry
    // is dynamic or stream. But this should work well fro design time
    // and static content so we're able to reflect the changes done to
    // the underlying geometry when visualizing this effect mesh.
    return mDrawable->GetGeometryHash();
}

Drawable::Usage EffectDrawable::GetGeometryUsage() const
{
    // same comment here , see GetGeometryHash
    return mDrawable->GetGeometryUsage();
}

Drawable::DrawPrimitive EffectDrawable::GetDrawPrimitive() const
{
    if (!mEnabled)
        return mDrawable->GetDrawPrimitive();

    return DrawPrimitive::Triangles;
}

SpatialMode EffectDrawable::GetSpatialMode() const
{
    return mDrawable->GetSpatialMode();
}

bool EffectDrawable::IsAlive() const
{
    if (!mEnabled)
        return mDrawable->IsAlive();

    return true;
}
Drawable::Type EffectDrawable::GetType() const
{
    return Type::EffectsDrawable;
}

Drawable::Usage EffectDrawable::GetInstanceUsage(const InstancedDraw& draw) const
{
    return mDrawable->GetInstanceUsage(draw);
}
size_t EffectDrawable::GetInstanceHash(const InstancedDraw& draw) const
{
    return mDrawable->GetInstanceHash(draw);
}
std::string EffectDrawable::GetInstanceId(const Environment& env, const InstancedDraw& draw) const
{
    return mDrawable->GetInstanceId(env, draw);
}
void EffectDrawable::Execute(const Environment& env, const Command& command)
{
    if (command.name == "EnableMeshEffect")
    {
        DEBUG("Received mesh effect command. [cmd='%1']", command.name);
        if (const auto* ptr = base::SafeFind(command.args, std::string("state")))
        {
            if (const auto* state = std::get_if<std::string>(ptr))
            {
                if (*state == "toggle")
                    mEnabled = !mEnabled;
                else if (*state == "on")
                    mEnabled = true;
                else if (*state == "off")
                    mEnabled = false;
                else WARN("Ignoring enable mesh effect command with unexpected state parameter. [state='%1']", state);
                if (mEnabled)
                {
                    if (mEffectDrawable)
                        mDrawable = mEffectDrawable;
                }
                else
                {
                    mDrawable = mSourceDrawable;
                }
            }
            else
            {
                WARN("Ignoring enable mesh effect command with unexpected 'state' parameter type. Expected 'string'");
            }
        } else WARN("Ignoring enable mesh effect command without 'state' parameter.");
    }
    mDrawable->Execute(env, command);
}
Drawable::DrawCmd EffectDrawable::GetDrawCmd() const
{
    return mDrawable->GetDrawCmd();
}

// static
void EffectDrawable::SetRandomGenerator(std::function<float(float min, float max)> rf)
{
    random_function = rf;
}

bool EffectDrawable::ConstructShardMesh(const Environment& env, Device& device, Geometry::CreateArgs& create,
    unsigned mesh_subdivision_count) const
{
    Environment e = env;
    e.mesh_type = MeshType::ShardedEffectMesh;
    e.mesh_args = ShardedEffectMeshArgs { mesh_subdivision_count };

    Geometry::CreateArgs args;
    if (!mDrawable->Construct(e, device, args))
    {
        ERROR("Failed to construct mesh.");
        return false;
    }
    ASSERT(args.buffer.GetLayout() == GetVertexLayout<ShardVertex2D>());
    const VertexStream vertex_stream(args.buffer.GetLayout(),
                                 args.buffer.GetVertexBuffer());
    const auto vertex_count = vertex_stream.GetCount();

    glm::vec3 minimums = {0.0f, 0.0f, 0.0f};
    glm::vec3 maximums = {0.0f, 0.0f, 0.0f};
    if (!FindGeometryMinMax(args.buffer, &minimums, &maximums))
    {
        ERROR("Failed to compute mesh bounds.");
        return false;
    }
    const auto shape_bounds_dimensions = maximums - minimums;
    mShapeCenter = minimums + shape_bounds_dimensions * 0.5f;

    // hmm, is this adequate?
    if (vertex_count == 0)
        return true;

    struct ShardTempData {
        glm::vec2 aPosition = {0.0f, 0.0f};
        unsigned vertex_count = 0;
    };
    std::vector<ShardTempData> shard_temp_data;

    for (size_t i=0; i<vertex_count; ++i)
    {
        const auto* vertex = vertex_stream.GetVertex<ShardVertex2D>(i);
        const auto shard_index = vertex->aShardIndex;
        if (shard_index >= shard_temp_data.size())
            shard_temp_data.resize(shard_index + 1);
        shard_temp_data[shard_index].aPosition.x += vertex->aPosition.x;
        shard_temp_data[shard_index].aPosition.y += vertex->aPosition.y;
        shard_temp_data[shard_index].vertex_count++;
    }

    struct ShardData {
        Vec4 data[1];
    };
    std::vector<ShardData> shard_data_buffer;
    shard_data_buffer.resize(shard_temp_data.size());
    for (size_t i=0; i<shard_data_buffer.size(); ++i)
    {
        // compute the arithmetic center (centroid of vertices)
        const auto& shard_center = shard_temp_data[i].aPosition / float(shard_temp_data[i].vertex_count);
        const auto shard_random_value = random_function(0.0f, 1.0f);
        shard_data_buffer[i].data[0].x = shard_center.x;
        shard_data_buffer[i].data[0].y = shard_center.y;
        shard_data_buffer[i].data[0].z = 0.0f; // reserved
        shard_data_buffer[i].data[0].w = shard_random_value;
    }

    // ES 3.0 (which is the basis of WebGL 2.0) does not have Shared Storage
    // Buffer Object ((SSBO). The standard workaround is to use a float32
    // texture for packing the data into and then read the data via texelFetch
    // in the shader and manually unpack from the texels.
    // The number of shards can be arbitrary and we really only have 3 choices.
    // 1. Bake the per shard data in each vertex. That's currently 2 * 4 * 4
    //    bytes of overhead per each triangle shard
    // 2. Use Uniform Buffer Object (UBO) and shader uniform block. Works but
    //    requires knowing the maximum buffer sizes up front since.
    // 3. Use float texture workaround.

    // Note that GL ES 3.1 does have SSBO BUT that's not part of WebGL. Rather
    // WebGL 2.0 Compute has SSBO but that's just a completely different WebGL
    // context (compute context vs. rendering context) and is not supported by
    // Emscripten either.
    if (!PackDataTexture(mEffectId, "Shard data texture", &shard_data_buffer[0], shard_data_buffer.size(), device))
    {
        ERROR("Shard data exceeds available data texture size. [shards=%1]", shard_data_buffer.size());
        return false;
    }

    create.buffer = std::move(args.buffer);
    create.usage  = args.usage;
    create.content_hash = args.content_hash;
    create.content_name = args.content_name;
    return true;
}


} // namespace
