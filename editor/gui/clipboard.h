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
#  include <QVariant>
#  include <QVariantMap>
#include "warnpop.h"

#include <string>
#include <any>
#include <memory>

#include "editor/app/utility.h"
#include "editor/app/types.h"

namespace gui
{
    // there's the QClipboard but this clipboard is simpler
    // and only within this application.
    class Clipboard
    {
    public:
        void SetText(const std::string& text)
        { mData = text; }
        void SetText(std::string&& text)
        { mData = std::move(text); }
        void SetType(const std::string& type)
        { mType = type; }
        void SetType(std::string&& type)
        { mType = std::move(type); }
        bool IsEmpty() const
        { return !mData.has_value(); }
        void Clear()
        {
            mData.reset();
            mType.clear();
            mProps.clear();
        }
        const std::string& GetType() const
        { return mType; }
        std::string GetText() const
        { return std::any_cast<std::string>(mData); }
        template<typename T>
        void SetObject(std::shared_ptr<T> object)
        { mData = object; }
        template<typename T>
        void SetObject(std::unique_ptr<T> object)
        { mData = std::shared_ptr<T>(std::move(object)); }
        template<typename T>
        std::shared_ptr<const T> GetObject() const
        { return std::any_cast<std::shared_ptr<T>>(mData); }

        inline void SetProperty(const app::PropertyKey& name, const QByteArray& bytes)
        { SetVariantProperty(name, bytes.toBase64()); }
        inline void SetProperty(const app::PropertyKey& name, const QColor& color)
        { SetVariantProperty(name, color); }
        inline void SetProperty(const app::PropertyKey& name, const QString& value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, const std::string& value)
        { SetVariantProperty(name, app::FromUtf8(value)); }
        inline void SetProperty(const app::PropertyKey& name, quint64 value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, qint64 value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, unsigned value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, int value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, double value)
        { SetVariantProperty(name, value); }
        inline void SetProperty(const app::PropertyKey& name, float value)
        { SetVariantProperty(name, value); }

        std::string GetProperty(const app::PropertyKey& name, const std::string& def) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return def;
            const auto& str = ret.toString();
            return app::ToUtf8(str);
        }

        QByteArray GetProperty(const app::PropertyKey& name, const QByteArray& def) const
        {
            const auto& ret  = GetVariantProperty(name);
            if (ret.isNull())
                return def;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                return QByteArray::fromBase64(str.toLatin1());
            return QByteArray();
        }

        template<typename T>
        T GetProperty(const app::PropertyKey& name, const T& def) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return def;
            return qvariant_cast<T>(ret);
        }

        template<typename T>
        bool GetProperty(const app::PropertyKey& name, T* out) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return false;
            *out = qvariant_cast<T>(ret);
            return true;
        }
        bool GetProperty(const app::PropertyKey& name, QByteArray* out) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return false;
            const auto& str = ret.toString();
            if (!str.isEmpty())
                *out = QByteArray::fromBase64(str.toLatin1());
            return true;
        }
        bool GetProperty(const app::PropertyKey& name, std::string* out) const
        {
            const auto& ret = GetVariantProperty(name);
            if (ret.isNull())
                return false;
            *out = app::ToUtf8(ret.toString());
            return true;
        }
    private:
        void SetVariantProperty(const QString& name, const QVariant& value)
        { mProps[name] = value; }
        QVariant GetVariantProperty(const QString& name) const
        { return mProps[name]; }
    private:
        std::any mData;
        std::string mType;
        QVariantMap mProps;
    };

} //  namespace