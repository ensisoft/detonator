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
#include "game/types.h"
#include "game/fwd.h"
#include "engine/action.h"
#include "engine/types.h"
#include "uikit/window.h"
#include "uikit/types.h"

namespace engine
{
    class PhysicsEngine;
    class AudioEngine;
    class KeyValueStore;
    struct ContactEvent;
    struct AudioEvent;
    struct MouseEvent;
    struct GameEvent;

    // This is the main interface for the game engine to interface
    // with the actual game logic. I.e. implementations of this
    // interface implement game specific functionality by for example
    // reacting to the engine callbacks or by handling the keyboard/mouse
    // input coming from the player.
    class Game
    {
    public:
        using Action = engine::Action;

        virtual ~Game() = default;
        // Set the default store/scratch pad that the game can
        // use for sharing data between game and entity scripting
        // states.
        virtual void SetStateStore(KeyValueStore* store) = 0;
        // Set physics engine instance.
        virtual void SetPhysicsEngine(const PhysicsEngine* engine) = 0;
        // Set audio engine instance.
        virtual void SetAudioEngine(const AudioEngine* engine) = 0;
        // Load the game data. This is called once by the engine after the
        // main application has started. In the implementation you should
        // load whatever initial game state that is needed. It's possible to
        // fail (indicated by returning false) and this will make the host
        // application exit early.
        virtual bool LoadGame(const ClassLibrary* loader) = 0;
        // Start the actual game after all required initial content has been
        // loaded. At this point all the engine subsystems are available
        // including rendering, physics and audio.
        // The game should enter whatever initial state such as opening
        // main screen/menu.
        virtual void StartGame() = 0;
        // BeginPlay is called as a response to PlayAction. When the
        // action is processed the engine creates an instance of the scene
        // and then calls BeginPlay. The Engine will maintain the ownership
        // of the scene for the duration of the game play.
        virtual void BeginPlay(game::Scene* scene) = 0;
        // Tick is called intermittently in order to perform some low frequency
        // game activity. The actual frequency is specified in the game configuration
        // in config.json.
        // game_time is the current total accumulated game time measured
        // in seconds and updated in dt steps with each step being equal to
        // 1.0/ticks_per_second seconds. On every call game_time already includes
        // the time step dt.
        virtual void Tick(double game_time, double dt) = 0;
        // Update is the main game update callback. It is called (normally)
        // at much higher frequency (for example @ 60 Hz) than Tick. The actual
        // frequency is specified in the game configuration in config.json.
        // game_time is the current total accumulated game time measured
        // in seconds and updated in dt steps with each step being equal to
        // 1.0/updates_per_second seconds. On every call game_time already includes
        // the time step dt.
        virtual void Update(double game_time,  double dt) = 0;
        // todo:
        virtual void PausePlay()
        {}
        virtual void ResumePlay()
        {}
        // Called after StopAction has taken place.
        virtual void EndPlay(game::Scene* scene) = 0;
        // todo:
        virtual void SaveGame() = 0;
        // todo:
        virtual void StopGame() = 0;
        // Get the next action from the game's action queue. The game engine
        // will process all the game actions once per game update loop iteration.
        // If there was no next action returns false, otherwise true is returned
        // and the action should be copied into out.
        virtual bool GetNextAction(Action* out)
        { return false; }
        // Get the game's logical viewport into the game world.
        // The viewport is defined in the same units as the game itself
        // and has no direct relation to pixels or to the graphics device
        // viewport. Instead it's completely game related and is managed by
        // the game. The engine will then use the viewport information to
        // render the contents within the game's viewport into some area
        // in some rendering surface such as a window. If your game returns
        // an empty viewport (width and height are 0) *nothing* will be shown.
        virtual FRect GetViewport() const = 0;

        // Event listeners.
        virtual void OnUIOpen(uik::Window* ui)
        {}
        virtual void OnUIClose(uik::Window* ui, int result)
        {}
        virtual void OnUIAction(uik::Window* ui, const uik::Window::WidgetAction& action)
        {}

        // Act on a contact event when 2 physics bodies have come into
        // contact or have come out of contact.
        virtual void OnContactEvent(const ContactEvent& contact) = 0;

        // Act on audio playback event.
        virtual void OnAudioEvent(const AudioEvent& event) = 0;

        // Act on a game event posted through PostEvent
        virtual void OnGameEvent(const GameEvent& event) = 0;

        // action/input handlers for some interesting windowing events.
        virtual void OnKeyDown(const wdk::WindowEventKeyDown& key) {}
        virtual void OnKeyUp(const wdk::WindowEventKeyUp& key) {}
        virtual void OnChar(const wdk::WindowEventChar& text) {}
        virtual void OnMouseMove(const MouseEvent& mouse) {}
        virtual void OnMousePress(const MouseEvent& mouse) {}
        virtual void OnMouseRelease(const MouseEvent& mouse) {}
    private:
    };

} // namespace
