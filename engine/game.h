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

#include <memory>
#include <variant>
#include <string>

#include "wdk/events.h"
#include "engine/classlib.h"
#include "engine/types.h"

namespace game
{
    class Scene;
    class SceneClass;
    class PhysicsEngine;
    struct ContactEvent;

    // This is the main interface for the game engine to interface
    // with the actual game logic. I.e. implementations of this
    // interface implement some gam/application logic by for example
    // reacting to the callbacks or keyboard/mouse input from the player.
    class Game
    {
    public:
        struct OpenMenuAction {

        };
        // Action to start playing the given scene.
        // When the engine processes this action request
        // it will create an instance of the SceneClass and
        // call BeginPlay. The engine will retain the ownership
        // of the Scene instance that is created.
        struct PlaySceneAction {
            // handle of the scene class object for the scene instance
            // creation. This may not be nullptr.
            ClassHandle<SceneClass> klass;
        };
        struct EndPlay {
            // todo: any params ?
        };
        struct PrintDebugStrAction {
            std::string message;
        };
        // Actions express some want the game wants to take
        // such as opening a menu, playing a scene and so on.
        using Action = std::variant<
                PlaySceneAction,
                PrintDebugStrAction>;

        virtual ~Game() = default;
        // Set physics engine instance.
        virtual void SetPhysicsEngine(const PhysicsEngine* engine) = 0;
        // Load the game. This is called once by the engine after the
        // application has started. In the implementation you should
        // start with some initial game state and possibly request some
        // action to take place such as loading the main menu.
        virtual void LoadGame(const ClassLibrary* loader) = 0;
        // BeginPlay is called as a response to PlaySceneAction. When the
        // action is processed the engine creates an instance of the scene
        // and then calls BeginPlay. The Engine will maintain the ownership
        // of the scene for the duration of the game play.
        virtual void BeginPlay(Scene* scene) = 0;
        // Tick is called intermittently in order to perform some low frequency
        // game activity. The actual frequency is specified in the game configuration
        // in config.json.
        // wall_time is the current continuously increasing time since application started.
        // tick-time is the current total accumulated application/game/ tick time measured
        // in seconds and updated in dt steps with each step being equal to
        // 1.0/ticks_per_second seconds. it only updates when the game is actually
        // running, so in case game is paused by the host runner this counter will
        // not increase.
        virtual void Tick(double wall_time, double tick_time, double dt) = 0;
        // Update is the main game update callback. It is called (normally)
        // at much higher frequency (for example @ 60 Hz) than Tick. The actual
        // frequency is specified in the game configuration in config.json.
        // wall_time is the current continuously increasing time since application started.
        // game_time is the current total accumulated application/game time measured
        // in seconds and updated in dt steps with each step being equal to
        // 1.0/updates_per_second seconds. it only updates when the game is
        // actually running, so in case game is paused by the host runner
        // this counter will not increase.
        virtual void Update(double wall_time, double game_time,  double dt) = 0;
        // EndPlay is called after EndPlay action has taken place.
        virtual void EndPlay() = 0;
        // todo:
        virtual void SaveGame() = 0;
        // Get the next action from the game's action queue. The game engine
        // will process all the game actions once per game update loop iteration.
        // If there was no next action returns false, otherwise true is returned
        // and the action should be copied into out.
        virtual bool GetNextAction(Action* out)
        { return false; }

        // Get the game's logical viewport into the game world.
        // The viewport is defined in the same units as the game itself
        // and has no direct relation to pixels or to a graphics device
        // viewport. Instead it's completely logical and is managed by
        // the game. The engine will then use the viewport information to
        // render the contents within the game's viewport into some area
        // in some rendering surface such as a window. If your game returns
        // an empty viewport (width and height are 0) *nothing* will be shown.
        virtual FRect GetViewport() const = 0;

        // Act on a contact event when 2 physics bodies have come into
        // contact or have come out of contact.
        virtual void OnContactEvent(const ContactEvent& contact) = 0;

        // action/input handlers for some interesting windowing events.
        virtual void OnKeyDown(const wdk::WindowEventKeydown& key) {}
        virtual void OnKeyUp(const wdk::WindowEventKeyup& key) {}
        virtual void OnChar(const wdk::WindowEventChar& text) {}
        virtual void OnMouseMove(const wdk::WindowEventMouseMove& mouse) {}
        virtual void OnMousePress(const wdk::WindowEventMousePress& mouse) {}
        virtual void OnMouseRelease(const wdk::WindowEventMouseRelease& mouse) {}
    private:
    };

} // namespace
