// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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
