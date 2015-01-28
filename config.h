// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft 
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

// assuming msvc for windows, gcc and clang for linux

#if defined(_MSC_VER)
#  define __MSVC__
#  define WINDOWS_OS

   // execlude some stuff from the windows headers we dont need
#  define WIN32_LEAN_AND_MEAN
   
   // get rid of the stupid MIN MAX macros..
#  define NOMINMAX

   // suppress some useless warnings
#  define _CRT_SECURE_NO_WARNINGS
#  define _SCL_SECURE_NO_WARNINGS

   // we're building in unicode
#  define UNICODE
   
   // msvc wants this for M_PI
#  define _USE_MATH_DEFINES
#endif

#if defined(__GNUG__)
#  define __GCC__
#  define LINUX_OS

#  define ENABLE_AUDIO
#endif

#if defined(__clang__)
#  define __CLANG__
#  if defined(SUBLIME_CLANG_WORKAROUNDS)
    // clang has a problem with gcc 4.9.0 stdlib and it complains
    // about max_align_t in cstddef (pulls it into std:: from global scope)
    // however the problem is only affecting sublimeclang in SublimeText
    // 
    // https://bugs.archlinux.org/task/40229
    // http://reviews.llvm.org/rL201729
    //
    // As a workaround we define some type with a matching typename
    // in the global namespace. 
    // the macro is enabled in 'pime.sublime-project'    
    typedef int max_align_t;
#  endif
#  define LINUX_OS

#  define ENABLE_AUDIO
#endif

