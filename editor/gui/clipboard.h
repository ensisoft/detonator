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

#include <string>
#include <any>
#include <memory>

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
    private:
        std::any mData;
        std::string mType;
    };

} //  namespace