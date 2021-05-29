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

// Engine is the top level generic game engine that combines
// various components and subsystems into a single component
// called the "engine".
//
// By default this includes subsystems such as:
//   renderer, physics engine and audio engine.
//
// And components such as:
//   scene, loader, gfx resources/classes.
//
// The engine is built into a shared library and implements
// the main app interface for game host compatibility.. This means
// that under normal operation the game host (GameMain) is the
// executable process which loads the game library which is the engine
// library which has the top level generic game engine implementation
// that manages and orchestrates all the various subsystems. The actual
// game logic is encapsulated behind the Game interface. The game engine
// then in turns calls the Game functions in order to invoke the game
// logic. The provided LuaGame will embed a Lua environment and invoke
// Lua scripts for running the game. (See game.h and lua.cpp)

// Note that the game engine does not deal with Window system integration.
// This is done by the game host since it's very specific to the platform,
// i.e. dependent on whether the running platforms is Windows, Linux, Android
// or even inside the Editor. All window management/rendering context creation
// and input handling happens inside these host executables/processes.

// Regarding this engine implementation it's entirely possible to replace
// this (or modify this) implementation to write a customized engine with
// some different functionality. In that case the project will be more
// complicated requiring the use of a compiler to compile the custom engine.
// Then in workspace/project settings (in the Editor application) one must
// configure the name of the shared library to use as the game's engine.

// This file is currently intentionally empty since everything is in engine.cpp
