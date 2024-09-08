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

#include "editor/app/workspace.h"
#include "engine/loader.h"

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