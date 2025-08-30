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
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <functional>
#include <variant>

#include "base/bitflag.h"
#include "graphics/vertex.h"
#include "graphics/drawable.h"
#include "graphics/enum.h"

#include "base/snafu.h"

namespace gfx
{
    class EffectDrawable : public Drawable
    {
    public:
        enum class EffectType : uint32_t {
            MeshExplosion = 1
        };
        struct MeshExplosionEffectArgs {
            unsigned mesh_subdivision_count = 1;
            float shard_linear_speed = 0.0f;
            float shard_linear_acceleration = 0.0f;
            float shard_rotational_speed = 0.0f;
            float shard_rotational_acceleration = 0.0f;
        };
        using EffectArgs = std::variant<MeshExplosionEffectArgs>;

        explicit EffectDrawable(std::shared_ptr<Drawable> drawable,
            std::string effectId) noexcept;

        bool EnableEffect();
        void DisableEffect();

        auto GetEffectType() const noexcept
        { return mType; }
        void SetEffectType(EffectType type) noexcept
        { mType = type;}
        void SetEffectArgs(EffectArgs args)
        { mArgs = args; }

        bool ApplyDynamicState(const Environment& env, ProgramState& program, RasterState&  state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        bool Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;
        void Update(const Environment& env, float dt) override;
        void Restart(const Environment& env) override;
        size_t GetGeometryHash() const override;
        Usage GetGeometryUsage() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        bool IsAlive() const override;
        Type GetType() const override;
        Usage GetInstanceUsage(const InstancedDraw& draw) const override;
        size_t GetInstanceHash(const InstancedDraw& draw) const override;
        std::string GetInstanceId(const Environment& env, const InstancedDraw& draw) const override;
        void Execute(const Environment& env, const Command& command) override;
        DrawCmd GetDrawCmd() const override;

        static void SetRandomGenerator(std::function<float(float min, float max)> random_function);
    private:
        bool ConstructExplosionMesh(const Environment& env, Geometry::CreateArgs& create) const;
    private:
        std::shared_ptr<Drawable> mDrawable;
        std::string mEffectId;
        EffectType mType = EffectType::MeshExplosion;
        EffectArgs mArgs;

        bool mEnabled = false;
        double mCurrentTime = 0.0f;

        mutable glm::vec3 mShapeCenter = {0.0f, 0.0f, 0.0f};
    };

} // namespace