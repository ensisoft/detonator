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
#include <functional>

#include "base/json.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"

namespace app
{
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

        template<typename T>
        PropertyKey(const char* key, T value)
          : mKey(toString("%1:%2", key, value))
        {}
        template<typename T>
        PropertyKey(const QString& key, T value)
          : mKey(toString("%1:%2", key, value))
        {}
        template<typename T>
        PropertyKey(const std::string& key, T value)
          : mKey(toString("%1:%2", key, value))
        {}
        operator const QString& () const
        { return mKey; }
        const QString& key() const
        { return mKey; }
    private:
        QString mKey;
    };

    class AnyString
    {
    public:
        AnyString() = default;

        AnyString(const char* str)
          : mStr(str)
        {}
        AnyString(const QString& str)
          : mStr(str)
        {}
        AnyString(const std::string& str)
          : mStr(FromUtf8(str))
        {}
        operator QString () const
        { return mStr; }
        operator QVariant () const
        { return mStr; }
        operator std::string () const
        { return app::ToUtf8(mStr); }

        const QString& GetWide() const
        { return mStr; }
        const std::string GetUtf8() const
        { return app::ToUtf8(mStr); }

        const quint64 GetHash() const
        { return qHash(mStr); }

        AnyString& operator=(const QString& str)
        {
            mStr = str;
            return *this;
        }
        AnyString& operator=(const char* str)
        {
            mStr = str;
            return *this;
        }
        AnyString& operator=(const std::string& str)
        {
            mStr = FromUtf8(str);
            return *this;
        }
    private:
        friend bool operator==(const AnyString&, const AnyString&);
        friend bool operator!=(const AnyString&, const AnyString&);

        friend bool operator==(const AnyString&, const QString&);
        friend bool operator!=(const AnyString&, const QString&);

        friend bool operator==(const QString&, const AnyString&);
        friend bool operator!=(const QString&, const AnyString&);

        friend bool operator<(const AnyString& lhs, const AnyString& rhs);
    private:
        QString mStr;
    };

    inline bool operator==(const AnyString& lhs, const AnyString& rhs)
    { return lhs.mStr == rhs.mStr; }
    inline bool operator!=(const AnyString& lhs, const AnyString& rhs)
    { return lhs.mStr != rhs.mStr; }

    inline bool operator==(const AnyString& lhs, const QString& rhs)
    { return lhs.mStr == rhs; }
    inline bool operator!=(const AnyString& lhs, const QString& rhs)
    { return lhs.mStr != rhs; }

    inline bool operator==(const QString& lhs, const AnyString& rhs)
    { return lhs == rhs.mStr; }
    inline bool operator!=(const QString& lhs, const AnyString& rhs)
    { return lhs != rhs.mStr; }

    inline bool operator<(const AnyString& lhs, const AnyString& rhs)
    { return lhs.mStr < rhs.mStr; }


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

    inline AnyString ReplaceAll(const AnyString& str, const AnyString& dis, const AnyString& dat)
    {
        QString s = str.GetWide();
        s.replace(dis, dat);
        return s;
    }

} // namespace

namespace std {
    template<>
    struct hash<app::AnyString> {
        size_t operator()(const app::AnyString& str) const {
            return str.GetHash();
        }
    };
} // namespace std

namespace base {
    inline void JsonWrite(nlohmann::json& json, const char* name, const app::AnyString& str)
    { base::JsonWrite(json, name, str.GetUtf8()); }
} // base
