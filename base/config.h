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


// this is the base config file with universally useful definitions etc.

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

// you can put your definitions that pertain to base here.
#define BASE_LOGGING_ENABLE_LOG

#ifdef POSIX_OS
  #define BASE_LOGGING_ENABLE_CURSES
#endif

#define BASE_FORMAT_SUPPORT_QT
