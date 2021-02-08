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
