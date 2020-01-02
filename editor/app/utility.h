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
#include "warnpop.h"

#include "base/assert.h"

#include <string>
#include <fstream>
#include <vector>

namespace app
{

QString joinPath(const QString& lhs, const QString& rhs);
QString cleanPath(const QString& path);
bool    makePath(const QString& path);

QString strFromEngine(const std::string& str);
QString fromUtf8(const std::string& str);
QString fromLatin1(const std::string& str);
QString widen(const std::string& str);

std::string narrow(const QString& str);
std::string strToEngine(const QString& str);
std::string toUtf8(const QString& str);
std::string toLatin(const QString& str);

std::string toLocal8Bit(const QString& str);

std::ofstream openBinaryOStream(const QString& file);
std::ifstream openBinaryIStream(const QString& file);

bool writeAsBinary(const QString& file, const void* data, std::size_t bytes);

template<typename T>
bool writeAsBinary(const QString& file, const std::vector<T>& data)
{
    return writeAsBinary(file, (const void*)data.data(),
        sizeof(T) * data.size());
}

template<typename T>
bool readAsBinary(const QString& file, std::vector<T>& data)
{
    std::ifstream in = openBinaryIStream(file);
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

} // namespace
