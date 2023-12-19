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

#include <variant>
#include <string>

#include "engine/classlib.h"
#include "engine/event.h"
#include "engine/types.h"
#include "engine/color.h"

namespace engine
{
    // Open a new UI window and place it on top of the stack.
    // The top of the stack UI (if any) will be given the chance
    // to process the user input coming from mouse/keyboard.
    struct OpenUIAction {
        // window instance. this is a shared_ptr because of the
        // variant but should be a unique_ptr
        std::shared_ptr<uik::Window> ui;
    };
    // Close the topmost UI and pop it off of the UI stack.
    struct CloseUIAction {
        int result = 0;
        std::string action_id;
        std::string window_name;
    };

    // Action to start playing the given scene.
    // When the engine processes this action request
    // it will create an instance of the SceneClass and
    // call BeginPlay. The engine will retain the ownership
    // of the Scene instance that is created.
    struct PlayAction {
        // handle of the scene class object for the scene instance
        // creation. This may not be nullptr.
        std::unique_ptr<game::Scene> scene;
    };

    // Suspend the game play. Suspending keeps the current scene
    // loaded but time accumulation and updates stop.
    // Play, Suspend, Resume, Stop, Quit
    struct SuspendAction {};
    struct ResumeAction {};
    struct EndPlayAction {};
    struct QuitAction {
        int exit_code = 0;
    };

    // Delay the game state action processing by some
    // amount of time. This can be used to create delayed
    // transitions when going from one game state to another.
    struct DelayAction {
        float seconds = 0.0;
    };
    struct DebugPrintAction {
        std::string message;
        bool clear = false;
    };
    struct DebugClearAction {
    };
    struct ShowDebugAction {
        bool show = true;
    };
    struct GrabMouseAction {
        bool grab = true;
    };
    struct ShowMouseAction {
        bool show = true;
    };
    struct BlockKeyboardAction {
        bool block = true;
    };
    struct BlockMouseAction {
        bool block = true;
    };
    struct RequestFullScreenAction {
        bool full_screen = true;
    };
    struct PostEventAction {
        GameEvent event;
    };
    struct ShowDeveloperUIAction {
        bool show = true;
    };
    struct DebugDrawLine {
        FPoint a;
        FPoint b;
        Color4f color;
        float width = 1.0f;
    };
    struct DebugDrawRect {
        FPoint top_left;
        FPoint bottom_right;
        Color4f color;
        float width = 1.0f;
    };
    struct DebugDrawCircle {
        FPoint center;
        float radius;
        Color4f color;
        float width = 1.0f;
    };
    using DebugDraw = std::variant<
            DebugDrawLine,
            DebugDrawCircle,
            DebugDrawRect>;

    struct DebugPauseAction {
        bool pause = true;
    };

    struct EnableEffectAction {
        std::string name;
        bool value = false;
    };

    // Actions express some want the game wants to take
    // such as opening a menu, playing a scene and so on.
    using Action = std::variant<PlayAction,
            SuspendAction,
            ResumeAction,
            EndPlayAction,
            QuitAction,
            OpenUIAction,
            CloseUIAction,
            DebugPrintAction,
            DebugClearAction,
            DebugPauseAction,
            DelayAction,
            ShowDebugAction,
            ShowMouseAction,
            BlockKeyboardAction,
            BlockMouseAction,
            GrabMouseAction,
            RequestFullScreenAction,
            PostEventAction,
            ShowDeveloperUIAction,
            EnableEffectAction>;

} // namespace
