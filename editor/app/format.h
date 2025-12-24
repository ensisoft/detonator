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
#  include <QString>
#  include <QByteArray>
#  include <QDir>
#  include <QFile>
#  include <QPoint>
#  include <QSize>
#  include <QProcess>
#  include <neargye/magic_enum.hpp>
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <cstring>
#include <type_traits>

#include "audio/format.h"
#include "base/format.h"
#include "base/types.h"
#include "graphics/vertex.h"

namespace app
{
    namespace detail {
        template<typename BaseType>
        auto ToString(const BaseType& type)
        {
            const auto str = base::ToString(type);
            return QString::fromUtf8(str.c_str());
        }
    }

    QString toString(QFile::FileError error);
    QString toString(QProcess::ProcessState state);
    QString toString(QProcess::ProcessError error);
    QString toString(const audio::Format& format);
    QString toString(const base::Color4f& color);

#ifdef DETONATOR_EDITOR_BUILD
    QString toString(const gfx::Vec2& vec);
    QString toString(const gfx::Vec3& vec);
    QString toString(const gfx::Vec4& vec);
#endif
    inline QString toString(const base::FRect& rect)
    { return detail::ToString(rect); }
    inline QString toString(const base::FPoint& point)
    { return detail::ToString(point); }
    inline QString toString(const base::FDegrees& degrees)
    { return detail::ToString(degrees); }
    inline QString toString(const base::FRadians& radians)
    { return detail::ToString(radians); }
    inline QString toString(const std::string& s)
    { return QString::fromUtf8(s.c_str());  }
    inline QString toString(const char* str)
    { return QString::fromUtf8(str); }
    inline QString toString(const QString& str)
    { return str; }
    inline QString toString(const QSize& size)
    { return QString("%1,%2").arg(size.width()).arg(size.height()); }
    inline QString toString(const QPoint& point)
    { return QString("%1,%2").arg(point.x()).arg(point.y()); }
    inline QString toString(const QPointF& point)
    { return QString("%1,%2").arg(point.x()).arg(point.y()); }
    inline QString toString(bool value)
    { return (value ? "True" : "False"); }
    inline QString toString(const glm::vec2& vec)
    { return QString("%1,%2").arg(vec.x).arg(vec.y); }
    inline QString toString(const glm::vec4& vec)
    { return QString("%1,%2,%3,%4").arg(vec.x).arg(vec.y).arg(vec.z).arg(vec.w); }

    struct Bytes {
        quint64 bytes = 0;
    };

    QString toString(const Bytes& b);

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