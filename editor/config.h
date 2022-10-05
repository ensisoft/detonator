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

#include "config/config.h"

// Editor build time configuration file.
#if defined(NDEBUG)
#  define APP_TITLE "Gamestudio Editor"
#else
#  define APP_TITLE "Gamestudio Editor DEBUG"
#endif
#define APP_VERSION "21ce035c0fe3\n" __DATE__
#define APP_COPYRIGHT "Copyright (c) 2020-2022\n" \
                      "Sami Väisänen"   \

#define APP_LINKS "http://www.ensisoft.com\n" \
                  "http://www.gamestudio.com\n" \
                  "http://www.github.com/ensisoft/gamestudio"

