// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <sstream>
#include <string>

namespace debug
{
    // check if running in debugger
    bool has_debugger();

    [[noreturn]]
    void do_assert(const char* expression, const char* file, const char* func, int line);

    // Force a breakpoint when having a debugger attached or otherwise abort
    [[noreturn]]
    void do_break();

} // debug

// What is an assert and what is an ASSERT?
// assert is the standard C utility which by default
// is elided when a release build is made (_NDEBUG is defined).
// This can be sufficient for cases where the assert is
// only to quickly remind the programmer of having done something
// silly. In other words it's a development time utility.
// However when checking for conditions whose violation could
// have severe consequences for the process (e.g undefined behaviour)
// we have ASSERT that is *always* compiled in.
//
// To throw or to dump core?
// Throwing an exception might seem like a good idea but it has
// few drawbacks. It's might be difficult to know which part of the
// system has failed, hence doing operations such as saving current
// state might cause further assertion failures. Also Keep in mind
// that to throw an exception causes the stack to be unwound
// which will a) potentially execute loads of code b) unwind the callstack
// c) alter the state of the program making it harder to do post-mortem
// diagnosis based on the core-dump.
#define ASSERT(expr) \
    (expr) \
    ? ((void)0) \
    : (debug::has_debugger() ? \
        debug::do_break() : \
        debug::do_assert(#expr, __FILE__, __PRETTY_FUNCTION__, __LINE__))
#define BUG(message)                                                    \
  do {                                                                  \
    debug::has_debugger()                                               \
  ? debug::do_break()                                                   \
  : debug::do_assert(message, __FILE__, __PRETTY_FUNCTION__, __LINE__); \
} while(0)

