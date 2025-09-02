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

#pragma once

#include "config.h"

#include <memory>
#include <variant>

#include "data/fwd.h"
#include "game/types.h"

namespace game
{
    class MeshEffectClass
    {
    public:
        enum class EffectType {
            MeshExplosion
        };
        struct MeshExplosionEffectArgs {
            unsigned mesh_subdivision_count = 1;
            float shard_linear_speed = 0.0f;
            float shard_linear_acceleration = 0.0f;
            float shard_rotational_speed = 0.0f;
            float shard_rotational_acceleration = 0.0f;
        };
        using EffectArgs = std::variant<MeshExplosionEffectArgs>;

        void SetEffectType(EffectType effect) noexcept
        { mEffectType = effect; }
        auto GetEffectType() const noexcept
        { return mEffectType; }

        void SetEffectArgs(EffectArgs args)
        { mEffectArgs = args; }
        auto GetEffectArgs() const noexcept
        { return mEffectArgs; }

        template<typename T>
        auto* GetEffectArgs() noexcept
        {
            return std::get_if<T>(&mEffectArgs);
        }
        template<typename T>
        const auto* GetEffectArgs() const noexcept
        {
            return std::get_if<T>(&mEffectArgs);
        }

        const auto* GetMeshExplosionEffectArgs() const noexcept
        {
            return GetEffectArgs<MeshExplosionEffectArgs>();
        }
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
        size_t GetHash() const;
    private:
        EffectType mEffectType = EffectType::MeshExplosion;
        EffectArgs mEffectArgs;
    };

    class MeshEffect
    {
    public:
        using EffectType = MeshEffectClass::EffectType;
        using EffectArgs = MeshEffectClass::EffectArgs;
        using MeshExplosionEffectArgs = MeshEffectClass::MeshExplosionEffectArgs;

        explicit MeshEffect(std::shared_ptr<const MeshEffectClass> klass) noexcept
            : mClass(std::move(klass))
        {}

        auto GetEffectType() const noexcept
        { return mClass->GetEffectType(); }

        template<typename T>
        const auto* GetEffectArgs() const noexcept
        {
            return mClass->template GetEffectArgs<T>();
        }

        const auto* GetMeshExplosionEffectArgs() const noexcept
        {
            return GetEffectArgs<MeshExplosionEffectArgs>();
        }

        auto GetEffectArgs() const noexcept
        { return mClass->GetEffectArgs(); }

        const auto& GetClass() const noexcept
        { return *mClass; }

        const auto* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const MeshEffectClass> mClass;
    };

} // namespace