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
#include "base/logging.h"
#include "engine/main/interface.h"
#include "engine/loader.h"

// helper stuff for dependency management for now.
// see interface.h for more details.

extern "C" {
GAMESTUDIO_EXPORT void Gamestudio_CreateFileLoaders(Gamestudio_Loaders* out)
{
    out->ContentLoader  = engine::JsonFileClassLoader::Create();
    out->ResourceLoader = engine::FileResourceLoader::Create();
}

GAMESTUDIO_EXPORT void Gamestudio_SetGlobalLogger(base::Logger* logger,
    bool debug_log, bool warn_log, bool info_log, bool error_log)
{
    base::SetGlobalLog(logger);
    base::EnableLogEvent(base::LogEvent::Debug, debug_log);
    base::EnableLogEvent(base::LogEvent::Warning, warn_log);
    base::EnableLogEvent(base::LogEvent::Info, info_log);
    base::EnableLogEvent(base::LogEvent::Error, error_log);
}

} // extern "C"
