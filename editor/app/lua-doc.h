// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#include "warnpop.h"

#include <string>
#include <vector>

namespace app
{
    struct LuaMethodArg {
        std::string name;
        std::string type;
    };
    enum class LuaMemberType {
        TableProperty,
        ObjectProperty,
        Function,
        Method,
        MetaMethod
    };

    struct LuaMemberDoc {
        LuaMemberType type = LuaMemberType::Function;
        std::string table;
        std::string name;
        std::string desc;
        std::string ret;
        std::vector<LuaMethodArg> args;
    };

    void InitLuaDoc();

    std::size_t GetNumLuaMethodDocs();
    const LuaMemberDoc& GetLuaMethodDoc(size_t index);

    QString ParseLuaDocTypeString(const QString& str);
    QString ParseLuaDocTypeString(const std::string& str);

} // namespace
