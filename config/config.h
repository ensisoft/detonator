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
#define BASE_FORMAT_SUPPORT_GLM
#define GAMESTUDIO_ENABLE_PHYSICS_DEBUG
