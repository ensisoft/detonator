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

#pragma once

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include <string>

namespace misc
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
