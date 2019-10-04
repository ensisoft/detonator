// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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


// this is the base platform file with universally useful definitions etc.
// the idea is that in your project you can make a file named "config.h"
// which should #include "base/platform.h"
// the config.h can then add additional project/application specific
// definitions and can reside in an application specific folder.
// all the rest of the source files in the whole tree then just #include "config.h"

#pragma once

// assuming some msvc version for windows, gcc or clang for linux

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

#if defined(__GNUG__)
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

#if defined(__clang__)
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


// how to distinguish between Linux and MacOS ?
// https://stackoverflow.com/questions/2166483/which-macro-to-wrap-mac-os-x-specific-code-in-c-c
#ifdef POSIX_OS
#  ifdef __APPLE__
#    define APPLE_OS
#  else
#    define LINUX_OS
#  endif
#endif

