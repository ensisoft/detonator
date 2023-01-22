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
#  define APP_TITLE "DETONATOR 2D"
#else
#  define APP_TITLE "DETONATOR 2D Debug"
#endif
// modify the app version before cranking out a release build
#define APP_VERSION "rel-???"
#define APP_COPYRIGHT "Copyright (c) 2020-2023\n" \
                      "Sami Väisänen"   \


