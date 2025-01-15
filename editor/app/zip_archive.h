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
#  include <quazip/quazip.h>
#  include <quazip/quazipfile.h>
#include "warnpop.h"

#include <set>
#include <vector>

#include "editor/app/resource.h"

namespace app
{
    class ZipArchive
    {
    public:
        ZipArchive();

        bool Open(const QString& zip_file);

        bool ReadFile(const QString& file, QByteArray* array) const;

        void SetImportSubFolderName(const QString& name)
        { mSubFolderName = name; }
        void SetResourceNamePrefix(const QString& prefix)
        { mNamePrefix = prefix; }
        QString GetImportSubFolderName() const
        { return mSubFolderName; }
        QString GetResourceNamePrefix() const
        { return mNamePrefix; }

        size_t GetNumResources() const
        { return mResources.size(); }
        const Resource& GetResource(size_t index) const
        { return *base::SafeIndex(mResources, index); }
        void IgnoreResource(size_t index)
        { mIgnoreSet.insert(index); }
        bool IsIndexIgnored(size_t index) const
        { return base::Contains(mIgnoreSet, index); }
    private:
        bool FindZipFile(const QString& unix_style_name) const;
    private:
        friend class Workspace;
        QString mZipFile;
        QString mSubFolderName;
        QString mNamePrefix;
        QFile mFile;
        mutable QuaZip mZip;
        std::vector<std::unique_ptr<Resource>> mResources;
        std::set<size_t> mIgnoreSet;
    };


} // app