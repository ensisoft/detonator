// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-20215 Ensisoft http://www.ensisoft.com
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

#include <memory>
#include <string>
#include <vector>

// this header is the interface header between the engine
// and the native (cpp based) game logic, i.e. entity, scene
// scripts etc.

namespace engine
{
    class EntityScript;

    struct EntityScriptRegistration {
        std::string classId;
        std::unique_ptr<EntityScript> script;
    };
    // when implementing game logic in a custom game specific
    // engine this function needs to be implemented by the game.
    void GetEntityScripts(std::vector<EntityScriptRegistration>* out);

} // engine