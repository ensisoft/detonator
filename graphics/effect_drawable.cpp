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

#include "base/math.h"
#include "base/format.h"
#include "base/logging.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/effect_drawable.h"

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
}

bool EffectDrawable::EnableEffect()
{
    mEnabled = true;
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

    if (mEnabled)
    {
        program.SetUniform("kEffectMeshCenter", mShapeCenter);
        program.SetUniform("kEffectTime", static_cast<float>(mCurrentTime));
        program.SetUniform("kEffectType", static_cast<int>(mType));
        if (mType == EffectType::MeshExplosion)
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
    return mDrawable->ApplyDynamicState(e, device, program, state);
}

ShaderSource EffectDrawable::GetShader(const Environment& env, const Device& device) const
{
    static const char* effect_shader = {
#include "shaders/vertex_2d_effect.glsl"
    };

    Environment e = env;
    e.mesh_type =  mEnabled ? MeshType::ShardedEffectMesh : MeshType::NormalRenderMesh;

    if (mEnabled)
    {
        ShaderSource source;
        source.SetType(ShaderSource::Type::Vertex);
        source.SetVersion(ShaderSource::Version::GLSL_300);
        source.LoadRawSource(effect_shader);
        source.AddPreprocessorDefinition("USE_EFFECTS_MESH");
        source.AddPreprocessorDefinition("MESH_EFFECT_TYPE_EXPLOSION", static_cast<int>(EffectType::MeshExplosion));
        source.AddShaderSourceUri("shaders/vertex_2d_effect.glsl");
        source.AddDebugInfo("Effects", "YES");

        source.Merge(mDrawable->GetShader(e, device));
        return source;
    }
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

    if (mType == EffectType::MeshExplosion)
        return ConstructExplosionMesh(env, device, create);
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

DrawPrimitive EffectDrawable::GetDrawPrimitive() const
{
    if (!mEnabled)
        return mDrawable->GetDrawPrimitive();

    return DrawPrimitive::Triangles;
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

bool EffectDrawable::ConstructExplosionMesh(const Environment& env, Device& device, Geometry::CreateArgs& create) const
{
    Environment e = env;
    e.mesh_type = MeshType::ShardedEffectMesh;

    Geometry::CreateArgs args;
    if (!mDrawable->Construct(env, device, args))
    {
        ERROR("Failed to construct mesh.");
        return false;
    }

    ASSERT(std::holds_alternative<MeshExplosionEffectArgs>(mArgs));
    const auto* effect_args = std::get_if<MeshExplosionEffectArgs>(&mArgs);

    // the triangle mesh computation produces a  mesh that  has the same
    // vertex layout as the original drawables geometry  buffer.
    auto effect_mesh_buffer = std::make_shared<GeometryBuffer>();
    if (!TessellateMesh(args.buffer, *effect_mesh_buffer, TessellationAlgo::LongestEdgeBisection,
            effect_args->mesh_subdivision_count))
    {
        ERROR("Failed to compute triangle mesh.");
        return false;
    }

    glm::vec3 minimums, maximums;
    if (!FindGeometryMinMax(args.buffer, &minimums, &maximums))
    {
        ERROR("Failed to compute mesh bounds.");
        return false;
    }

    const auto vertex_count = effect_mesh_buffer->GetVertexCount();
    const auto original_mesh_layout = effect_mesh_buffer->GetLayout();
    const auto& original_mesh_buffer = effect_mesh_buffer->GetVertexBuffer();
    const auto triangle_count = vertex_count / 3;

    // we're going to amend the vertex layout and add the following
    // per vertex data (vertex attribute)
     const VertexLayout::Attribute per_shard_data = {
        "aEffectShardData", 0, 4, 0, 0
    };

    auto effect_mesh_layout = effect_mesh_buffer->GetLayout();
    effect_mesh_layout.AppendAttribute(per_shard_data);
    effect_mesh_buffer->SetVertexLayout(effect_mesh_layout);
    const auto* effect_shard_data_attribute = effect_mesh_layout.FindAttribute("aEffectShardData");

    const VertexStream vertex_stream(original_mesh_layout, original_mesh_buffer);

    VertexBuffer vertex_buffer_copy(effect_mesh_layout);
    vertex_buffer_copy.Resize(vertex_count);
    for (size_t i=0; i<vertex_count; ++i)
    {
        const auto* src_vertex_ptr =  vertex_stream.GetVertexPtr(i);
        const auto src_vertex_size = original_mesh_layout.vertex_struct_size;
        vertex_buffer_copy.CopyVertex(src_vertex_ptr, src_vertex_size, i);
    }

    const auto* vertex_position_attribute = effect_mesh_layout.FindAttribute("aPosition");
    if (!vertex_position_attribute)
        return false;

    const auto shape_bounds_dimensions = maximums - minimums;
    const auto shape_center = minimums + shape_bounds_dimensions * 0.5f;

    //mTriangleState.resize(triangle_count);
    for (size_t triangle_index=0; triangle_index < triangle_count; ++triangle_index)
    {
        const auto vertex_index = triangle_index * 3;
        auto* v0 = vertex_buffer_copy.GetVertexPtr(vertex_index + 0);
        auto* v1 = vertex_buffer_copy.GetVertexPtr(vertex_index + 1);
        auto* v2 = vertex_buffer_copy.GetVertexPtr(vertex_index + 2);

        if (vertex_position_attribute->num_vector_components == 2)
        {
            const auto& p0 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*vertex_position_attribute, v0));
            const auto& p1 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*vertex_position_attribute, v1));
            const auto& p2 = ToVec(*VertexLayout::GetVertexAttributePtr<Vec2>(*vertex_position_attribute, v2));

            const auto& shard_center = ((p0 + p1 + p2) / 3.0f);
            const auto shard_random_value = random_function(0.0f, 1.0f);

            // use a vec4 to pack shard data that has the shard center (x, y) and
            // and a random value.
            Vec4 shard_data;
            shard_data.x = shard_center.x;
            shard_data.y = shard_center.y;
            shard_data.z = 0.0f;
            shard_data.w = shard_random_value;

            // shove this into each vertex , alternative would be to use something like
            // an uniform buffer, but then we have to know up front how much data
            // we're going to be dealing with, i.e. how many shards we will have.
            auto* t0 = VertexLayout::GetVertexAttributePtr<Vec4>(*effect_shard_data_attribute, v0);
            auto* t1 = VertexLayout::GetVertexAttributePtr<Vec4>(*effect_shard_data_attribute, v1);
            auto* t2 = VertexLayout::GetVertexAttributePtr<Vec4>(*effect_shard_data_attribute, v2);
            *t0 = shard_data;
            *t1 = shard_data;
            *t2 = shard_data;
        }
    }

    effect_mesh_buffer->SetVertexBuffer(vertex_buffer_copy.TransferBuffer());
    effect_mesh_buffer->SetVertexLayout(effect_mesh_layout);

    mShapeCenter = shape_center;
    create.buffer_ptr = std::move(effect_mesh_buffer);
    create.usage = mDrawable->GetGeometryUsage();
    create.content_hash = mDrawable->GetGeometryHash();
    create.content_name = "EffectMesh:" + mEffectId;
    return true;
}


} // namespace