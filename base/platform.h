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

// this is the base platform file with universally useful definitions etc.
// the idea is that in your project you can make a file named "config.h"
// which should #include "base/platform.h"
// the config.h can then add additional project/application specific
// definitions and can reside in an application specific folder.
// all the rest of the source files in the whole tree then just #include "config.h"

#pragma once

// assuming some msvc version for windows, gcc or clang for linux,
// emscripten for wasm/web

#if defined(_MSC_VER)
  #define __MSVC__
  #define WINDOWS_OS
  #ifdef _M_AMD64
    #define X86_64
  #endif
  #define COMPILER_NAME    "msvc"
  #define COMPILER_VERSION _MSC_FULL_VER

  #define __PRETTY_FUNCTION__ __FUNCSIG__

  // execlude some stuff from the windows headers we dont need
  #define WIN32_LEAN_AND_MEAN

   // get rid of the stupid MIN MAX macros..
  #define NOMINMAX

  // suppress some useless warnings
  #define _CRT_SECURE_NO_WARNINGS
  #define _SCL_SECURE_NO_WARNINGS

  // acknowledge a fix in vs2015 update 2 regarding atomic object alignment
  // the fix changes the binary layout of objects and breaks interoperability
  // with code that has been compilied/linked with an older toolchain  
  #define _ENABLE_ATOMIC_ALIGNMENT_FIX

  // we're building in unicode
#ifndef UNICODE
  #define UNICODE
#endif

  // msvc wants this for M_PI
  #define _USE_MATH_DEFINES

  #define NORETURN __declspec(noreturn)
#endif

#if defined(__GNUG__) && !defined(__EMSCRIPTEN__)
  #define __GCC__
  #define POSIX_OS
  #ifdef __LP64__
    #define X86_64
  #endif
  #define COMPILER_NAME "GCC"
  #define COMPILER_VERSION __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__
  #define NOTHROW  noexcept
  #define NORETURN [[noreturn]]
#endif

#if defined(__clang__) && !defined(__EMSCRIPTEN__)
  #ifdef __LP64__
    #define X86_64
  #endif
  #define __CLANG__
  #define POSIX_OS
  #ifndef COMPILER_NAME
  #  define COMPILER_NAME    "clang"
  #endif
  #ifndef COMPILER_VERSION
  #  define COMPILER_VERSION __clang_version__
  #endif
  #define NOTHROW  noexcept
  #define NORETURN [[noreturn]]
#endif

#if defined(__EMSCRIPTEN__)
#  define WEBASSEMBLY
#  define WEBGL
#  define COMPILER_NAME "emscripten"
#  define COMPILER_VERSION "xxxx"
// emscripten tries to be posix compatible to some extent
#  define POSIX_OS
#endif

// how to distinguish between Linux and MacOS ?
// https://stackoverflow.com/questions/2166483/which-macro-to-wrap-mac-os-x-specific-code-in-c-c
#ifdef POSIX_OS
#  if defined(__APPLE__)
#    define APPLE_OS
#  elif defined(__EMSCRIPTEN__)
#    define WEB_OS
#  else
#    define LINUX_OS
#  endif
#endif

