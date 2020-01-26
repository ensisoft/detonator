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

#include "config.h"

// some universally useful warning suppressions

// you can make your own application level warnpush.h and warnpop.h
// headers which will then include the base/warnpush and base/warnpop
// (don't have to but can be useful)
// then you'll wrap any third party header inclusions that cause problems
// inside the warnpush.h and warnpop.h inclusions.

// note that GCC and clang don't give the same warnings, hence
// the suppressions are different
#if defined(__GCC__)
  // for boost
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#  pragma GCC diagnostic ignored "-Wlong-long"
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__CLANG__)
  // for Qt
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wuninitialized"
#  pragma clang diagnostic ignored "-Wc++11-long-long"
#  pragma clang diagnostic ignored "-Winconsistent-missing-override"
#  pragma clang diagnostic ignored "-Wunused-local-typedefs"
#elif defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4091) // msvs14 DbgHelp.h
#  pragma warning(disable: 4244) // boost.spirit (conversion from double to float)
#  pragma warning(disable: 4251) // Qt warning about not having a dll-interface
#  pragma warning(disable: 4800) // Qt warning about forcing value to bool
#  pragma warning(disable: 4244) // Qt warning about conversion from double to float
#  pragma warning(disable: 4275) // Qt, no dll-interface class std::exception used as baseclass for QException
#  pragma warning(disable: 4003) // protobuf, not enough actual parameters
#  pragma warning(disable: 4267) // protobuf, conversion from size_t to int
#  pragma warning(disable: 4244) // Qt warning about conversion from int to float
#endif
