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


#include "config.h"

#include "base/platform.h"
#if defined(POSIX_OS)
#  include <unistd.h>
#  include <sys/types.h>
#  include <pwd.h>
#elif defined(WINDOWS_OS)
#  include <Shlobj.h>
#endif

#include <stdexcept>
#include <filesystem>

#include "base/utility.h"
#include "homedir.h"

namespace game
{

// static
void HomeDir::Initialize(const std::string& application)
{
#if defined(POSIX_OS)
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw == nullptr || pw->pw_dir == nullptr)
        throw std::runtime_error("user's home directory location not found");

    const std::string home = pw->pw_dir;

    std::filesystem::path p(home);
    p /= application;

    std::filesystem::create_directory(p);

    mApplicationPath = p.generic_u8string();
    mApplicationName = application;

#elif defined(WINDOWS_OS)
    WCHAR path[MAX_PATH] = {};
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path)))
        throw std::runtime_error("user's home directory location not found");

    const std::string home = base::ToUtf8(std::wstring(path));

    std::filesystem::path p(home);
    p /= application;

    std::filesystem::create_directory(p);

    mApplicationPath = p.generic_u8string();
    mApplicationName = application;
#endif
}

std::string HomeDir::MapFile(const std::string& filename)
{
    if (filename.empty())
        return "";

    std::filesystem::path p(mApplicationPath);
    p /= filename;
    return p.generic_u8string(); // std::string until c++20 (u8string)
}

// static
std::string HomeDir::mApplicationName;
// static
std::string HomeDir::mApplicationPath;
// static
std::string HomeDir::mUserHomeDir;

} // misc
