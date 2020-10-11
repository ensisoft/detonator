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
#  include <QByteArray>
#  include <QDir>
#  include <QFile>
#  include <QPoint>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <cstring>
#include <type_traits>

#include "utility.h"

namespace app
{
    QString toString(QFile::FileError error);

    inline QString toString(const std::string& s)
    { return FromUtf8(s); }
    inline QString toString(const char* str)
    { return FromUtf8(str); }
    inline QString toString(const QString& str)
    { return str; }
    inline QString toString(const QPoint& point)
    { return QString("%1,%2").arg(point.x()).arg(point.y()); }
    inline QString toString(bool value)
    { return (value ? "True" : "False"); }

    template<typename T>
    QString toString(const T& val)
    {
        if constexpr (std::is_enum<T>::value)
        {
            const std::string_view& str(magic_enum::enum_name(val));
            return QString::fromLocal8Bit(str.data(), str.size());
        }
        else
        {
            return QString("%1").arg(val);
        }
    }

    template<typename T, typename... Args>
    QString toString(QString fmt, const T& value, const Args&... args)
    {
        fmt = fmt.arg(toString(value));
        return toString(fmt, args...);
    }



} // app