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
// By default this includes the following subsystems:
// renderer, physics engine, audio engine and scripting.
// And the following components:
// scene, animation, loader, gfx resources/classes.
// Your game logic should be written in the scripting (Lua code).
//
// By default the engine is built into a shared library and implements
// the main game host process interface. This means that under normal
// operation the game host (GameMain) is the executable process which
// loads the game library which is the engine library which has the
// top level generic game engine implementation orchestrating all the
// various subsystems and running the scripts which contain the actual
// game logic. The game host takes care of dealing with the windowing
// context creation and keyboard/mouse input.
//
//
// It's entirely possible to replace this (or modify this) implementation
// to write a customized engine with some different type of behaviour.
// In that case the project will be more complicated requiring the use
// of a compiler to compile the custom engine. Then in workspace/project
// settings (in the Editor application) one can configure the name of the
// shared which contains the game implementation.

// this header is currently empty since everything is in engine.cpp