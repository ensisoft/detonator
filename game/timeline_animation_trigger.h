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

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <cstddef>
#include <string>
#include <memory>
#include <unordered_map>
#include <type_traits>

#include "base/utility.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"

namespace game
{
    class AnimationTriggerClass
    {
    public:
        enum class Type {
            EmitParticlesTrigger,
            RunSpriteCycle,
            PlayAudio
        };
        enum class Flags {
            Enabled
        };

        using AudioStreamType   = game::AnimationAudioTriggerEvent::AudioStream;
        using AudioStreamAction = game::AnimationAudioTriggerEvent::StreamAction;

        AnimationTriggerClass() = default;
        explicit AnimationTriggerClass(Type type) noexcept;
        inline auto GetId() const noexcept
        { return mId; }
        inline auto GetType() const noexcept
        { return mType; }
        inline auto GetTime() const noexcept
        { return mTime; }
        inline void SetTime(float time) noexcept
        { mTime = time; }
        inline auto GetNodeId() const
        { return mTargetNodeId; }
        inline auto GetTimelineId() const
        { return mTimelineId; }
        inline auto GetName() const
        { return mName; }
        inline const auto& GetParameters() const noexcept
        { return mParameters; }
        inline void SetParameter(const std::string& name, const AnimationTriggerParam& param)
        { mParameters[name] = param; }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline void SetNodeId(std::string id) noexcept
        { mTargetNodeId = std::move(id); }
        inline void SetTimelineId(std::string id) noexcept
        { mTimelineId = std::move(id); }

        inline bool HasParameter(const std::string& name) const noexcept
        { return base::Contains(mParameters, name); }

        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        inline bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        inline void Enable(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }

        template<typename T>
        inline void SetParameter(const std::string& name, T value)
        {
            if constexpr (std::is_enum<T>::value)
                SetParameter(name, std::string(magic_enum::enum_name(value)));
            else SetParameter(name, AnimationTriggerParam { value });
        }

        template<typename T>
        inline T* GetParameter(const std::string& name)
        {
            if (auto* ptr = base::SafeFind(mParameters, name))
            {
                return std::get_if<T>(ptr);
            }
            return nullptr;
        }
        template<typename T>
        inline const T* GetParameter(const std::string& name) const
        {
            if (const auto* ptr = base::SafeFind(mParameters, name))
            {
                return std::get_if<T>(ptr);
            }
            return nullptr;
        }

        template<typename T>
        bool GetParameter(const std::string& name, T* out) const
        {
            if constexpr (std::is_enum<T>::value) {
                if (const auto* str = GetParameter<std::string>(name)) {
                    const auto& enum_val = magic_enum::enum_cast<T>(*str);
                    if (enum_val.has_value()) {
                        *out = enum_val.value();
                        return true;
                    }
                }
                return false;
            } else if (const auto* ptr = GetParameter<T>(name)) {
                *out = *ptr;
                return true;
            }
            return false;
        }

        template<typename T>
        bool HasParameter(const std::string& name) const
        {
            T temp;
            return GetParameter(name, &temp);
        }

        std::unique_ptr<AnimationTriggerClass> Copy() const;
        std::unique_ptr<AnimationTriggerClass> Clone() const;

        size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
    private:
        Type mType = Type::EmitParticlesTrigger;
        std::string mId;
        std::string mName;
        std::string mTargetNodeId;
        std::string mTimelineId;
        std::unordered_map<std::string, AnimationTriggerParam> mParameters;
        base::bitflag<Flags> mFlags;
        float mTime = 0.0f;
    };

    class AnimationTrigger
    {
    public:
        using Type  = AnimationTriggerClass::Type;
        using Event = game::AnimationTriggerEvent;

        explicit AnimationTrigger(std::shared_ptr<AnimationTriggerClass> klass)
            : mClass(std::move(klass))
        {}
        explicit AnimationTrigger(const AnimationTriggerClass& klass)
            : mClass(std::make_shared<AnimationTriggerClass>(klass))
        {}

        inline auto GetNodeId() const
        { return mClass->GetNodeId(); }

        inline auto GetTime() const noexcept
        { return mClass->GetTime(); }

        inline auto GetType() const noexcept
        { return mClass->GetType(); }

        inline std::unique_ptr<AnimationTrigger> Copy() const
        { return std::make_unique<AnimationTrigger>(*this); }

        bool Validate(const EntityNode& node) const;

        void Trigger(EntityNode& node, std::vector<Event>* events = nullptr);
    private:
        std::shared_ptr<const AnimationTriggerClass> mClass;
    };

}// namespace