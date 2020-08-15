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
#  include <QString>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

// general dumping ground for utility type of functionality
// preferably not specific to any GUI or application.
// inherited from previous projects.

namespace app
{

// Compute a file hash based on the contents of the file.
// Simply returns 0 if the file could not be read.
std::uint64_t GetFileHash(const QString& file);

// Concatenate two strings as file system path.
// Returns the path formatted with native separators.
QString JoinPath(const QString& lhs, const QString& rhs);

// Clean the path string. Returns with native separators.
QString CleanPath(const QString& path);

// Make path. returns true if succesful, otherwise false.
bool MakePath(const QString& path);

// Convert the UTF-8 encoded string to a QString.
QString FromUtf8(const std::string& str);
// Convert the ASCII encoded string to a QString.
QString FromLatin(const std::string& str);
// Convert the 8-bit encoded string to a QString.
QString FromLocal8Bit(const std::string& str);

// Convert and encode the QString into UTF-8 encoded std::string.
std::string ToUtf8(const QString& str);

// Convert the QString into ASCII only std::string.
// This conversion may result in loss of data if the
// string contains characters that are not representable
// in ASCII.
// Useful for converting strings that are not natural language/text.
std::string ToLatin(const QString& str);

// Convert QString to a narrow std::string in
// local (computer specific) encoding.
// The output is either UTF-8 OR some local 8-bit encoding.
std::string ToLocal8Bit(const QString& str);

// Open a binary std::ofstream for writing to a file.
std::ofstream OpenBinaryOStream(const QString& file);
// Open a binary std::ifstream for read from file.
std::ifstream OpenBinaryIStream(const QString& file);

// Simple helper function to quickly dump binary data to a file.
// Returns true if succesful, otherwise false.
bool WriteAsBinary(const QString& file, const void* data, std::size_t bytes);

// Simple helper function to write the contents of vector<T>
// in binary to a file.
template<typename T>
bool WriteAsBinary(const QString& file, const std::vector<T>& data)
{
    return WriteAsBinary(file, (const void*)data.data(),
        sizeof(T) * data.size());
}

// Read the contents of a file into a vector of some generic type T
template<typename T>
bool ReadAsBinary(const QString& file, std::vector<T>& data)
{
    std::ifstream in = OpenBinaryIStream(file);
    if (!in.is_open())
        return false;

    in.seekg(0, std::ios::end);
    const auto size = (size_t)in.tellg();
    in.seekg(0, std::ios::beg);

    if (size % sizeof(T))
        return false;

    data.resize(size / sizeof(T));
    in.read((char*)&data[0], size);
    return true;
}

// Generate a random string.
QString RandomString();

// Initialize the home directory for the application
// in the user's home directory.
void InitializeAppHome(const QString& appname);

// Get an absolute path to a file/folder in the application
// home directory.
QString GetAppFilePath(const QString& name);

template<typename Enum>
Enum EnumFromString(const QString& str, Enum backup, bool* success = nullptr)
{
    const auto& enum_val = magic_enum::enum_cast<Enum>(ToUtf8(str));
    if (!enum_val.has_value()) {
        if (success) *success = false;
        return backup;
    }
    return enum_val.value();
}

} // namespace
