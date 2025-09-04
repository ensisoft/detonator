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
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <string>
#include <variant>
#include <unordered_map>

#include "base/box.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/rotator.h"
#include "game/enum.h"

namespace game
{
    // type aliases for base types
    using FBox      = base::FBox;
    using FRect     = base::FRect;
    using IRect     = base::IRect;
    using URect     = base::URect;
    using USize     = base::USize;
    using IPoint    = base::IPoint;
    using FPoint    = base::FPoint;
    using FSize     = base::FSize;
    using ISize     = base::ISize;
    using Color4f   = base::Color4f;
    using Rotator   = base::Rotator;
    using FRadians  = base::FRadians;
    using FDegrees  = base::FDegrees;
    using Color     = base::Color;
    using FVector2D = base::FVector2D;
    using Float2    = base::Float2;

    using LightParam = std::variant<float,
            glm::vec2, glm::vec3, glm::vec4,
            Color4f>;
    using LightParamMap = std::unordered_map<std::string, LightParam>;

    using AnimationTriggerParam = std::variant<float, int, std::string>;
    using AnimationTriggerParamMap = std::unordered_map<std::string, AnimationTriggerParam>;

    struct AnimationAudioTriggerEvent {
        enum class AudioStream {
            Effect, Music
        };
        enum class StreamAction {
            Play //, Pause, Kill
        };
        AudioStream stream  = AudioStream::Music;
        StreamAction action = StreamAction::Play;
        std::string audio_graph_id;
        // for debug
        std::string trigger_name;
    };
    struct AnimationSpawnEntityTriggerEvent {
        std::string source_node_id;
        std::string entity_class_id;
        int render_layer = 0;
        // for debug
        std::string trigger_name;
    };

    using AnimationTriggerEvent = std::variant<AnimationAudioTriggerEvent,
        AnimationSpawnEntityTriggerEvent>;

    struct AnimationEvent {
        std::variant<AnimationAudioTriggerEvent,
            AnimationSpawnEntityTriggerEvent> event;
        // for debug.
        std::string animation_name;
    };

    struct EntityTimerEvent {
        std::string name;
        float jitter = 0.0f;
    };

    using EntityPostedEventValue = std::variant<bool, int, float,
        std::string,
        glm::vec2, glm::vec3, glm::vec4>;

    // an event posted in the entity's event queue by the queue
    // via  call to PostEvent
    struct EntityPostedEvent {
        std::string message;
        std::string sender;
        EntityPostedEventValue value;
    };

} // namespace


