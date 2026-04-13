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

#pragma once

#include "config.h"

#include <string>
#include <variant>
#include <functional>

#include "graphics/enum.h"
#include "graphics/drawable_geometry.h"

namespace gfx
{
    class Device;
    class GeometryBuffer;
    class ProgramState;

    class DrawableEffect
    {
    public:
        using EffectType = MeshEffectType;
        using EffectRand = std::function<float(float min, float max)>;

        enum class EffectMeshType {
            ShardMesh
        };

        struct ShardMeshArgs {
            unsigned mesh_subdivision_count = 1;
            bool discard_skinny_slivers = true;
        };

        struct MeshExplosion {
            unsigned mesh_subdivision_count = 1;
            float shard_linear_speed = 0.0f;
            float shard_linear_acceleration = 0.0f;
            float shard_rotational_speed = 0.0f;
            float shard_rotational_acceleration = 0.0f;
        };

        struct GeometryInfo {
            BufferUsage usage = BufferUsage::Static;
            std::string content_id;
            std::string content_name;
            std::size_t content_hash = 0;
        };

        explicit DrawableEffect(const MeshExplosion& args)
            : mEffect(args)
        {}
        DrawableEffect() = default;

        EffectType GetEffectType() const noexcept;
        EffectMeshType GetEffectMshType() const noexcept;
        ShardMeshArgs GetShardMeshArgs() const noexcept;

        void SetState(const DrawGeometryHandle& handle, ProgramState& program) const;
        void Update(float dt) noexcept;

        static DrawGeometryHandle GetShardGeometry(const GeometryInfo& info, DrawGeometryBuffer buffer, Device& device);
        static void SetRandomGenerator(EffectRand rd);
        static float GetRandomValue(float min, float max);
        static bool ConstructShardEffectMesh(GeometryBuffer buffer,
            GeometryBuffer* shard_geometry, TextureBuffer* shard_data,
            unsigned mesh_subdivision_count, bool discard_skinny_slivers);
    private:
        using Effect = std::variant<std::monostate, MeshExplosion>;
        Effect mEffect;

    private:
        double mCurrentTime = 0.0;
    };
} // namespace