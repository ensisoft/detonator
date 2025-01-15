// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <vector>

#include "base/utility.h"
#include "editor/app/types.h"

namespace  app
{
    class ResourceMigrationLog
    {
    public:
        struct Migration {
            QString name;
            QString id;
            QString message;
            QString type;
        };
        void WriteLog(const AnyString& id, const AnyString& name, const AnyString& type, const AnyString& message)
        {
            Migration m;
            m.id      = id;
            m.name    = name;
            m.message = message;
            m.type    = type;
            mLog.push_back(std::move(m));
        }
        template<typename Resource>
        void WriteLog(const Resource& res, const AnyString& type, const AnyString& message)
        {
            Migration a;
            a.id      = FromUtf8(res.GetId());
            a.name    = FromUtf8(res.GetName());
            a.message = message;
            a.type    = type;
            mLog.push_back(std::move(a));
        }
        bool IsEmpty() const
        { return mLog.empty(); }
        size_t GetNumMigrations() const
        { return mLog.size(); }
        const Migration& GetMigration(size_t index) const
        { return base::SafeIndex(mLog, index); }
    private:
        std::vector<Migration> mLog;
    };
} // namespace