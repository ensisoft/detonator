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

#include <string>

#include "base/math.h"
#include "audio/format.h"
#include "audio/loader.h"
#include "editor/app/workspace.h"
#include "engine/loader.h"
#include "graphics/drawable.h"
#include "game/animator.h"
#include "game/property_animator.h"
#include "game/kinematic_animator.h"
#include "game/entity_state_controller.h"

// TranslateEnum overloads must be in the same namespace as the enum
// types they overload on in order for the overload resolution to
// select the right overload.h

namespace app {
    std::string TranslateEnum(Workspace::ProjectSettings::CanvasPresentationMode);
    std::string TranslateEnum(Workspace::ProjectSettings::CanvasFullScreenStrategy);
    std::string TranslateEnum(Workspace::ProjectSettings::PowerPreference power);
    std::string TranslateEnum(Workspace::ProjectSettings::WindowMode wm);
} // namespace

namespace engine {
    std::string TranslateEnum(FileResourceLoader::DefaultAudioIOStrategy strategy);
} // namespace

namespace game {
    std::string TranslateEnum(AnimatorClass::Type type);
    std::string TranslateEnum(PropertyAnimatorClass::PropertyName name);
    std::string TranslateEnum(BooleanPropertyAnimatorClass::PropertyName name);
    std::string TranslateEnum(KinematicAnimatorClass::Target target);
    std::string TranslateEnum(EntityStateControllerClass::StateTransitionMode mode);
} // namespace

namespace math {
    std::string TranslateEnum(Interpolation);
}

namespace gfx {
    std::string TranslateEnum(gfx::ParticleEngineClass::CoordinateSpace space);
    std::string TranslateEnum(gfx::ParticleEngineClass::Motion motion);
    std::string TranslateEnum(gfx::ParticleEngineClass::SpawnPolicy spawn);
}

namespace audio {
    std::string TranslateEnum(audio::SampleType sampletype);
    std::string TranslateEnum(audio::IOStrategy io);
}