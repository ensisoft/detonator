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

#pragma once

#include "config.h"

#include "game/fwd.h"

namespace engine
{
    // this is the native "script" interface for implementing
    // entity logic in native code (vs Lua scripts). Keep in mind
    // that when implementing this interface and writing game logic
    // you *MAY NOT* cache any game play object pointers. The objects
    // may become invalidated at any time. Rather, if you need a
    // specific game play object you should search for it in the scene.
    class EntityScript
    {
    public:
        virtual ~EntityScript() = default;

        // Called on every iteration of the game loop. game_time is the current
        // game time so far in seconds not including the next time step dt.
        virtual void Update(game::Entity& entity, double game_time, double dt) {}

        // Called on every low frequency game tick. The tick frequency is
        // determined in the project settings. If you want to perform animation
        // such as move your game objects more smoothly then Update is the place
        // to do it. This function can be used to do thing such as evaluate AI or
        // path finding or other similar "thinking" game logic that needs to run
        // intermittently.
        virtual void Tick(game::Entity& entity, double game_time, double dt) {}

        // Called once when the game play begins for the entity in the scene.
        virtual void BeginPlay(game::Entity& entity, game::Scene* scene, game::Tilemap* map) {}

        // Called once when the game play ends for the entity in the scene.
        virtual void EndPlay(game::Entity& entity, game::Scene* scene, game::Tilemap* map) {}
    };

} // namespace