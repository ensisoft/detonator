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
#  pragma warning(disable: 4996) // Qt spits deprecated warnings.
#endif
