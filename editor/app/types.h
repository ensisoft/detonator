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
#  include <QModelIndexList>
#include "warnpop.h"

#include <string>
#include <functional>
#include <vector>
#include <cstddef>

#include "base/assert.h"
#include "base/json.h"
#include "base/utility.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"

namespace app
{
    class ModelIndex
    {
    public:
        ModelIndex() = default;
        ModelIndex(const QModelIndex& index)
        {
            ASSERT(index.row() >= 0);
            ASSERT(index.isValid());
            mIndex = static_cast<size_t>(index.row());
        }
        ModelIndex(size_t index)
          : mIndex(index)
        {}
        operator size_t () const noexcept
        { return mIndex; }
    private:
        std::size_t mIndex = 0;
    };

    // Falicate implicit conversion from various ways of expressing Qt's
    // model indices lists into a single unified type.
    class ModelIndexList
    {
    public:
        ModelIndexList() = default;
        ModelIndexList(const QModelIndexList& list)
        {
            for (const auto& index : list)
            {
                ASSERT(index.row() >= 0);
                ASSERT(index.isValid());
                mIndices.push_back(static_cast<size_t>(index.row()));
            }
        }
        ModelIndexList(const std::vector<size_t>& list)
          : mIndices(list)
        {}
        ModelIndexList(std::vector<size_t>&& list) noexcept
          : mIndices(std::move(list))
        {}
        ModelIndexList(size_t index) noexcept
        {
            mIndices.push_back(index);
        }

        inline size_t operator[](size_t index) const noexcept
        { return base::SafeIndex(mIndices, index); }
        inline size_t size() const noexcept
        { return mIndices.size(); }

        inline void push_back(size_t index)
        { mIndices.push_back(index); }

        inline auto begin() { return mIndices.begin(); }
        inline auto end() { return mIndices.end(); }
        inline auto begin() const { return mIndices.begin(); }
        inline auto end() const { return mIndices.end(); }

        inline auto& GetRef() { return mIndices; }
        inline const auto& GetRef() const { return mIndices; }
        inline auto GetData() const { return mIndices; }
        //inline auto GetData() && { return std::move(mIndices); }
    private:
        std::vector<size_t> mIndices;
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

        bool IsEmpty() const
        { return mStr.isEmpty(); }

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
