// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "base/format.h"
#include "base/random.h"
#include "graphics/device.h"
#include "graphics/utility.h"
#include "graphics/texture.h"
#include "graphics/program.h"
#include "graphics/drawable_effect.h"
#include "graphics/geometry_algo.h"
#include "graphics/geometry_buffer.h"
#include "graphics/texture_buffer.h"

namespace {
    std::function<float (float min, float max)> random_function;
} // namespace

namespace gfx
{

DrawableEffect::EffectType DrawableEffect::GetEffectType() const noexcept
{
    if (std::holds_alternative<MeshExplosion>(mEffect))
        return EffectType::ShardedMeshExplosion;
    else BUG("No such drawable effect.");
    return EffectType::ShardedMeshExplosion;
}

DrawableEffect::EffectMeshType DrawableEffect::GetEffectMshType() const noexcept
{
    if (std::holds_alternative<MeshExplosion>(mEffect))
        return EffectMeshType::ShardMesh;
    else BUG("No such effect mesh type.");
    return EffectMeshType::ShardMesh;
}

DrawableEffect::ShardMeshArgs DrawableEffect::GetShardMeshArgs() const noexcept
{
    ShardMeshArgs args;
    if (const auto* ptr = std::get_if<MeshExplosion>(&mEffect))
    {
        args.mesh_subdivision_count = ptr->mesh_subdivision_count;
    }
    return args;
}

void DrawableEffect::SetState(const DrawGeometryHandle& draw, ProgramState& program) const
{
    const auto effect_type = GetEffectType();
    program.SetUniform("kEffectTime", static_cast<float>(mCurrentTime));
    program.SetUniform("kEffectType", static_cast<int>(effect_type));

    if (const auto* effect_args = std::get_if<MeshExplosion>(&mEffect))
    {
        const auto shard_geometry = draw.GetGeometry();
        const auto shard_texture = draw.GetTexture();
        ASSERT(shard_geometry);
        ASSERT(shard_texture);

        const auto texture_count = program.GetSamplerCount();
        program.SetUniform("kEffectMeshCenter", *shard_geometry->GetProperty<glm::vec3>("shape-center"));
        program.SetTexture("kShardDataTexture", texture_count, *shard_texture);
        program.SetTextureCount(texture_count + 1);

        glm::vec4 args;
        args.x = effect_args->shard_linear_speed;
        args.y = effect_args->shard_linear_acceleration;
        args.z = effect_args->shard_rotational_speed;
        args.w = effect_args->shard_rotational_acceleration;
        program.SetUniform("kEffectArgs", args);
    } else BUG("No such drawable effect.");
}

void DrawableEffect::Update(float dt) noexcept
{
    mCurrentTime += dt;
}

// static
DrawGeometryHandle DrawableEffect::GetShardGeometry(const GeometryInfo& info, DrawGeometryBuffer buffer, Device& device)
{
    glm::vec3 minimums = {0.0f, 0.0f, 0.0f};
    glm::vec3 maximums = {0.0f, 0.0f, 0.0f};
    if (!FindGeometryMinMax(buffer.GetGeometryBuffer(), &minimums, &maximums))
        return DrawGeometryHandle::Null;

    const auto shape_bounds = maximums - minimums;
    const auto shape_center = minimums + shape_bounds * 0.5f;

    Geometry::CreateArgs geometry_args;
    geometry_args.buffer       = buffer.TransferGeometryBuffer();
    geometry_args.usage        = BufferUsage::Static;
    geometry_args.content_name = info.content_name;
    geometry_args.content_hash = info.content_hash;
    geometry_args.properties["shape-bounds"] = shape_bounds;
    geometry_args.properties["shape-center"] = shape_center;
    auto geometry = device.CreateGeometry(info.content_id, std::move(geometry_args));

    Texture::CreateArgs texture_args;
    texture_args.buffer        = buffer.TransferTextureBuffer();
    texture_args.min_filter    = Texture::MinFilter::Nearest;
    texture_args.mag_filter    = Texture::MagFilter::Nearest;
    texture_args.x_wrap        = Texture::Wrapping::Clamp;
    texture_args.y_wrap        = Texture::Wrapping::Clamp;
    texture_args.texture_hash  = 0;
    texture_args.generate_mips = false;
    texture_args.flags.set(Texture::Flags::GarbageCollect, false);
    auto texture = device.MakeTexture(info.content_id, std::move(texture_args));

    return DrawGeometryHandle(std::move(geometry), texture);
}

// static
bool DrawableEffect::ConstructShardEffectMesh(GeometryBuffer buffer,
    GeometryBuffer* shard_geometry_buffer_out, TextureBuffer* shard_data_buffer_out,
    unsigned mesh_subdivision_count, bool discard_skinny_slivers)
{
    GeometryBuffer shard_geometry_buffer;
    if (buffer.GetLayout() == GetVertexLayout<Vertex2D>())
    {
        if (!CreateShardMesh(buffer, &shard_geometry_buffer, mesh_subdivision_count, discard_skinny_slivers))
            return false;
    }
    else if (buffer.GetLayout() == GetVertexLayout<ShardVertex2D>())
    {
        shard_geometry_buffer = std::move(buffer);
    }
    else return false;

    const VertexStream vertex_stream(shard_geometry_buffer.GetLayout(),
                                     shard_geometry_buffer.GetVertexBuffer());
    const auto vertex_count = vertex_stream.GetCount();

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

    TypedDataTextureBuffer<ShardData> shard_data_buffer;
    shard_data_buffer.Resize(shard_temp_data.size());

    for (size_t i=0; i<shard_temp_data.size(); ++i)
    {
        // compute the arithmetic center (centroid of vertices)
        const auto& shard_center = shard_temp_data[i].aPosition / float(shard_temp_data[i].vertex_count);
        const auto shard_random_value = GetRandomValue(0.0f, 1.0f);
        ShardData shard_data;
        shard_data.data[0].x = shard_center.x;
        shard_data.data[0].y = shard_center.y;
        shard_data.data[0].z = 0.0f; // reserved
        shard_data.data[0].w = shard_random_value;
        shard_data_buffer.SetAt(i, shard_data);
    }

    *shard_data_buffer_out = PackDataTexture(std::move(shard_data_buffer));
    *shard_geometry_buffer_out = std::move(shard_geometry_buffer);
    return true;
}

// static
void DrawableEffect::SetRandomGenerator(EffectRand rd)
{
    random_function = std::move(rd);
}
// static
float DrawableEffect::GetRandomValue(float min, float max)
{
    if (!random_function)
        return base::rand<float>(min, max);

    return random_function(min, max);
}

} // namespace
