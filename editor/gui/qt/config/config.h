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


// this is the config for the Qt5 editor plugin for the custom widgets.
// there's nothing here yet.

#include "base/platform.h"

// needed for Win32 build for the right dllexport/dllimport directives.
// but only when building the Qt plugin for the designer.
#define QDESIGNER_EXPORT_WIDGETS

#define DESIGNER_PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT