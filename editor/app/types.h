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

#include "warnpush.h"
#  include <QString>
#include "warnpop.h"

#include <string>

#include "editor/app/utility.h"

namespace app
{
    struct Bytes {
        quint64 bytes = 0;
    };

    // Facilitate implicit conversion from different
    // types into a property key string.
    class PropertyKey
    {
    public:
        PropertyKey(const char* key)
          : mKey(key)
        {}
        PropertyKey(const QString& key)
          : mKey(key)
        {}
        PropertyKey(const std::string& key)
          : mKey(FromUtf8(key))
        {}
        PropertyKey(const char* key, size_t index)
          : mKey(QString("%1:%2").arg(key).arg(index))
        {}
        PropertyKey(const QString& key, size_t index)
          : mKey(QString("%1:%2").arg(key).arg(index))
        {}
        PropertyKey(const std::string& key, size_t index)
          : mKey(QString("%1:%2").arg(FromUtf8(key), index))
        {}

        operator const QString& () const
        { return mKey; }
        const QString& key() const
        { return mKey; }
    private:
        QString mKey;
    };

    template<typename Key, typename Value>
    class KeyValueRange
    {
    public:
        KeyValueRange(const QMap<Key, Value>& map)
          : mMap(map)
        {}
        auto begin()
        { return mMap.keyValueBegin(); }
        auto end()
        { return mMap.keyValueEnd(); }
    private:
        const QMap<Key, Value>& mMap;
    };
} // namespace

