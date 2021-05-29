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

#include "warnpush.h"

#include "warnpop.h"

#include <string>

namespace game
{
    // Application home directory in the user's home directory.
    class HomeDir
    {
    public:
        // You should initialize once only.
        // Folder is name for our application specific folder
        // in the user's real home. for example /home/roger/ on
        // a linux system and "c:\Documents and Settings\roger\"
        // on a windows system, so we get
        // "home/roger/folder" and "c:\documents and settings\roger\folder".
        // Application name should be UTF-8 encoded.
        static void Initialize(const std::string& application);

        // Map a file in the home dir to a complete filename path.
        // Note that this only *maps* the filename and doesn't make
        // any assumptions whether the file actually exists or
        // is accessible etc.
        // The returned string is in UTF-8 encoding.
        // If the filename is empty then an empty string is returned.
        static std::string MapFile(const std::string& filename);

        // Various getters. All return strings are UTF-8 encoded.
        static const std::string& GetApplicationName()
        { return mApplicationName; }
        static const std::string& GetApplicationPath()
        { return mApplicationPath; }
        static const std::string& GetUserHomeDirectory()
        { return mUserHomeDir; }
    private:
        static std::string mApplicationName;
        static std::string mApplicationPath;
        static std::string mUserHomeDir;
    };

} // namespace
