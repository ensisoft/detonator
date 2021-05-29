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

#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

#include "base/assert.h"
#include "base/logging.h"
#include "engine/main/interface.h"
#include "engine/loader.h"
#include "graphics/resource.h"

// helper stuff for dependency management for now.
// see interface.h for more details.

extern "C" {
GAMESTUDIO_EXPORT void CreateDefaultEnvironment(game::ClassLibrary** classlib)
{
    auto* ret = new game::ContentLoader();
    *classlib = ret;
    gfx::SetResourceLoader(ret);
}
GAMESTUDIO_EXPORT void DestroyDefaultEnvironment(game::ClassLibrary* classlib)
{
    gfx::SetResourceLoader(nullptr);
    delete classlib;
}

GAMESTUDIO_EXPORT void SetResourceLoader(gfx::ResourceLoader* loader)
{
    gfx::SetResourceLoader(loader);
}

GAMESTUDIO_EXPORT void SetGlobalLogger(base::Logger* logger)
{
    base::SetGlobalLog(logger);
}

} // extern "C"
