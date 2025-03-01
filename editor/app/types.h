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
#  include <QSize>
#  include <QPoint>
#  include <QByteArray>
#  include <QJsonObject>
#  include <QVariant>
#  include <QVariantMap>
#  include <QMetaType>
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#  include <glm/vec4.hpp>
#include "warnpop.h"

#include <string>
#include <functional>
#include <vector>
#include <cstddef>

#include "base/assert.h"
#include "base/json.h"
#include "base/utility.h"
#include "editor/app/format.h"

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
                : mStr(QString::fromUtf8(str.c_str()))
        {}
        operator QString () const
        { return mStr; }
        operator QVariant () const
        { return mStr; }
        operator std::string () const
        {
            const auto& bytes = mStr.toUtf8();
            return std::string(bytes.data(), bytes.size());
        }

        const QString& GetWide() const
        { return mStr; }
        const std::string GetUtf8() const
        {
            const auto& bytes = mStr.toUtf8();
            return std::string(bytes.data(), bytes.size());
        }

        const quint64 GetHash() const
        { return qHash(mStr); }

        bool IsEmpty() const
        { return mStr.isEmpty(); }

        bool contains(const AnyString& filter, Qt::CaseSensitivity casing) const
        {
            return mStr.contains(filter, casing);
        }
        bool Contains(const AnyString& filter, Qt::CaseSensitivity casing = Qt::CaseSensitive) const
        {
            return mStr.contains(filter, casing);
        }
        bool StartsWith(const AnyString& other) const
        {
            return mStr.startsWith(other);
        }
        bool EndsWith(const AnyString& other, Qt::CaseSensitivity casing = Qt::CaseSensitive) const
        {
            return mStr.endsWith(other);
        }
        QStringList Split(const AnyString& split) const
        {
            return mStr.split(split);
        }

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
            mStr = QString::fromUtf8(str.c_str());
            return *this;
        }
    private:
        friend bool operator==(const AnyString&, const AnyString&);
        friend bool operator!=(const AnyString&, const AnyString&);
        friend bool operator==(const AnyString&, const char*);

        friend bool operator==(const AnyString&, const QString&);
        friend bool operator!=(const AnyString&, const QString&);

        friend bool operator==(const QString&, const AnyString&);
        friend bool operator!=(const QString&, const AnyString&);

        friend bool operator<(const AnyString& lhs, const AnyString& rhs);
    private:
        QString mStr;
    };

    inline bool operator==(const AnyString& lhs, const char* rhs)
    { return lhs.mStr == QString::fromUtf8(rhs); }

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
          : mKey(QString::fromUtf8(key.c_str()))
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
        template<typename T>
        PropertyKey(const app::AnyString& key, T value)
          : mKey(toString("%1:%2", key, value))
        {}
        operator const QString& () const
        { return mKey; }
        const QString& key() const
        { return mKey; }
    private:
        QString mKey;
    };

    bool ValidateQVariantMapJsonSupport(const QVariantMap& map);

    template<typename T>
    bool ValidateQVariantJsonSupport(const T& value)
    {
        const QVariant variant(value);
        // Qt API brainfart (QVariant::Type is deprecated)
        const auto type = (QMetaType::Type)variant.type();
        if (type == QMetaType::Type::QVariantMap)
            return ValidateQVariantMapJsonSupport(qvariant_cast<QVariantMap>(variant));

        // todo:there's more but these should work at least.
        return type == QMetaType::Type::Float ||
               type == QMetaType::Type::Double ||
               type == QMetaType::Type::Int ||
               type == QMetaType::Type::UInt ||
               type == QMetaType::Type::QString ||
               type == QMetaType::Type::QStringList ||
               type == QMetaType::Type::QColor ||
               type == QMetaType::Type::ULongLong ||
               type == QMetaType::Type::LongLong ||
               type == QMetaType::Type::Bool ||
               type == QMetaType::Type::QJsonObject ||
               type == QMetaType::Type::QStringList;
    }
    inline bool ValidateQVariantJsonSupport(const QVariantMap& map)
    {
        return ValidateQVariantMapJsonSupport(map);
    }


    inline bool ValidateQVariantMapJsonSupport(const QVariantMap& map)
    {
        for (const auto& value : map)
        {
            if (!ValidateQVariantJsonSupport(value))
                return false;
        }
        return true;
    }

    class PropertyValue
    {
    public:
        PropertyValue(const QJsonObject& json)
        { SetVariantProperty(json); }
        PropertyValue(const QByteArray& bytes)
        { SetVariantProperty(QString::fromLatin1(bytes.toBase64())); }
        PropertyValue(const QColor& color)
        { SetVariantProperty(color); }
        PropertyValue(const char* str)
        { SetVariantProperty(QString(str)); }
        PropertyValue(const QString& value)
        { SetVariantProperty(value); }
        PropertyValue(const std::string& value)
        { SetVariantProperty(QString::fromUtf8(value.c_str())); }
        PropertyValue(const AnyString& value)
        { SetVariantProperty(value.GetWide()); }
        PropertyValue(quint64 value)
        { SetVariantProperty(value); }
        PropertyValue(qint64 value)
        { SetVariantProperty(value); }
        PropertyValue(unsigned value)
        { SetVariantProperty(value); }
        PropertyValue(int value)
        { SetVariantProperty(value); }
        PropertyValue(double value)
        { SetVariantProperty(value); }
        PropertyValue(float value)
        { SetVariantProperty(value); }
        PropertyValue(bool value)
        { SetVariantProperty(value); }
        PropertyValue(const QSize& value)
        {
            QVariantMap map;
            map["width"]  = value.width();
            map["height"] = value.height();
            SetVariantProperty(map);
        }
        PropertyValue(const QPoint& value)
        {
            QVariantMap map;
            map["x"] = value.x();
            map["y"] = value.y();
            SetVariantProperty(map);
        }
        PropertyValue(const glm::vec2& vector)
        {
            QVariantMap map;
            map["x"] = vector.x;
            map["y"] = vector.y;
            SetVariantProperty(map);
        }
        PropertyValue(const glm::vec3& vector)
        {
            QVariantMap map;
            map["x"] = vector.x;
            map["y"] = vector.y;
            map["z"] = vector.z;
            SetVariantProperty(map);
        }
        PropertyValue(const glm::vec4& vector)
        {
            QVariantMap map;
            map["x"] = vector.x;
            map["y"] = vector.y;
            map["z"] = vector.z;
            map["w"] = vector.w;
            SetVariantProperty(map);
        }

        PropertyValue(const QVariantMap& map)
        { SetVariantProperty(map); }
        PropertyValue(const QStringList& strings)
        { SetVariantProperty(strings); }

        PropertyValue(const QVariant& variant)
            : mVariantValue(variant)
        {
            ASSERT(!mVariantValue.isNull());
        }

        template<typename T>
        void GetValue(T* out) const
        {
            *out = qvariant_cast<T>(mVariantValue);
        }

        void GetValue(std::string* out) const
        {
            const auto& str = mVariantValue.toString();
            *out = std::string(str.toUtf8());
        }
        void GetValue(QByteArray* out) const
        {
            const auto& str = mVariantValue.toString();
            if (!str.isEmpty())
                *out = QByteArray::fromBase64(str.toLatin1());
        }
        void GetValue(QPoint* out) const
        {
            const auto& map = mVariantValue.toMap();
            out->setX(map["x"].toInt());
            out->setY(map["y"].toInt());
        }
        void GetValue(QSize* out) const
        {
            const auto& map = mVariantValue.toMap();
            out->setWidth(map["width"].toInt());
            out->setHeight(map["height"].toInt());
        }
        void GetValue(glm::vec2* out) const
        {
            const auto& map = mVariantValue.toMap();
            out->x = map["x"].toFloat();
            out->y = map["y"].toFloat();
        }
        void GetValue(glm::vec3* out) const
        {
            const auto& map = mVariantValue.toMap();
            out->x = map["x"].toFloat();
            out->y = map["y"].toFloat();
            out->z = map["z"].toFloat();
        }
        void GetValue(glm::vec4* out) const
        {
            const auto& map = mVariantValue.toMap();
            out->x = map["x"].toFloat();
            out->y = map["y"].toFloat();
            out->z = map["z"].toFloat();
            out->w = map["w"].toFloat();
        }

        operator QVariant () const
        { return mVariantValue; }
    private:
        template<typename T>
        void SetVariantProperty(const T& value)
        {
            ASSERT(ValidateQVariantJsonSupport(value));
            mVariantValue = value;
        }

        void SetVariantProperty(const QVariantMap& map)
        {
            ASSERT(ValidateQVariantMapJsonSupport(map));
            mVariantValue = map;
        }
    private:
        QVariant mVariantValue;
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
