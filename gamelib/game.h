// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include <memory>
#include <variant>
#include <string>

#include "wdk/events.h"
#include "gamelib/classlib.h"
#include "gamelib/types.h"

namespace game
{
    class Scene;
    class SceneClass;
    class PhysicsEngine;

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
        struct LoadBackgroundAction {
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
                LoadBackgroundAction,
                PrintDebugStrAction>;

        virtual ~Game() = default;
        // Set physics engine instance.
        virtual void SetPhysicsEngine(const PhysicsEngine* engine) = 0;
        // Load the game. This is called once by the engine after the
        // application has started. In the implementation you should
        // start with some initial game state and possibly request some
        // action to take place such as loading the main menu.
        virtual void LoadGame(const ClassLibrary* loader) = 0;
        // LoadBackgroundDone is called as a response to LoadBackgroundAction.
        // When the action is processed the engine creates and instance of the
        // background scene and then calls LoadBackgroundDone. The engine will
        // maintain the ownership of the scene.
        virtual void LoadBackgroundDone(Scene* background) = 0;
        // BeginPlay is called as a response to PlaySceneAction. When the
        // action is processed the engine creates an instance of the scene
        // and then calls BeginPlay. The Engine will maintain the ownership
        // of the scene for the duration of the game play.
        virtual void BeginPlay(Scene* scene) = 0;
        // Tick is called intermittently in order to perform some low frequency
        // game activity. The actual frequency is specified in the game configuration
        // in config.json.
        // Current time is the current total accumulated application time,
        // measured in seconds since the application was started.
        virtual void Tick(double current_time) = 0;
        // Update is the main game update callback. It is called (normally)
        // at much higher frequency (for example @ 60 Hz) than Tick. The actual
        // frequency is specified in the game configuration in config.json.
        // Current time is the current total accumulated application time,
        // measured in seconds since the application was started.
        virtual void Update(double current_time,  double dt) = 0;
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
