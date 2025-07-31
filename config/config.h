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

// config.h for the helper libs combine several translation
// units together into a static helper libs in order to reduce
// the number of translation unit compilations.
// however using these libs means that the build flags cannot
// be changed. If the build flags need to differ from the build
// flags set here then the target must build the translation
// units.

#include "base/platform.h"

// some of the code uses LINUX_OS
// we expect that POSIX_OS is Linux
#if defined(POSIX_OS) && !defined(__EMSCRIPTEN__)
#  define LINUX_OS
#endif

#define BASE_LOGGING_ENABLE_LOG
#define BASE_TRACING_ENABLE_TRACING
#define BASE_FORMAT_SUPPORT_GLM
#define BASE_FORMAT_SUPPORT_MAGIC_ENUM
#define BASE_TEST_HELP_SUPPORT_GLM
#define BASE_TYPES_SUPPORT_GLM

// This flag controls whether tracing functionality is enabled or not.
#define BASE_TRACING_ENABLE_TRACING

#define MATH_SUPPORT_GLM

// This flag controls whether the engine uses a separate thread to
// perform parallel game state update while rendering a frame.
// If this flag is not defined update and rendering are sequential.
#define ENGINE_USE_UPDATE_THREAD

// This flag controls whether the physics engine can provide
// debug drawing of physics objects or not.
#define ENGINE_ENABLE_PHYSICS_DEBUG

// This flag controls whether the audio engine is enabled or not.
// Useful for troubleshooting issues sometimes, i.e. whole audio
// system can be silently turned off.
#define ENGINE_ENABLE_AUDIO

// This flag controls whether Lua scripting is enabled
// in the engine or not. Currently, this needs to be
// enabled since the native APIs can't yet provide all
// the required functionality to run a game properly.
#define ENGINE_ENABLE_LUA_SCRIPTING

// This flag controls whether Cpp scripting is enabled
// in the engine or not. This is an additional way to
// program game logic.
#define ENGINE_ENABLE_CPP_SCRIPTING

// select the Web audio backend
#if defined(LINUX_OS)
  #define AUDIO_USE_PULSEAUDIO
#elif defined(WINDOWS_OS)
  #define AUDIO_USE_WAVEOUT
#elif defined(WEBASSEMBLY)
  #define AUDIO_USE_SOKOL
  //#define AUDIO_USE_OPENAL
  //#define AUDIO_USE_SDL2
#endif

#if defined(DETONATOR_EDITOR_BUILD)
  // When this flag is defined the audio player in audio/player.h uses a
  // separate std::thread to run the platform specific audio device and
  // to manage the audio playback.
  #define AUDIO_USE_PLAYER_THREAD
#endif

// When AUDIO_USE_PLAYER_THREAD is defined  use this flag to control
// the type of locking around the audio queue inside the audio/player
// When the flag is defined the audio queue is a lock free queue
// otherwise a standard queue with std mutex is used.
//#define AUDIO_LOCK_FREE_QUEUE

// When this flag is defined the OpenAL API calls are checked for errors.
// Currently,turned off for performance reasons. Might cause unexpected
// issues and errors that are not discovered properly.
//
//#define AUDIO_CHECK_OPENAL

#if !defined(WEBGL)
  // use this flag to control whether calls to OpenGL are checked
  // for errors or not. Disabled for WebGL for performance reasons.
  #define GRAPHICS_CHECK_OPENGL
#endif

#define GFX_ENABLE_DEVICE_TRACING

// need these for the correct operation of SOL
// regarding different C++ numeric types and their
// overloading and type resolution.
#define SOL_ALL_SAFETIES_ON 1
#define SOL_SAFE_NUMERICS 1