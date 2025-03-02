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

#include <unordered_map>

#include "editor/app/resource_packer.h"

namespace app
{
    class ZipArchiveImporter : public app::ResourcePacker
    {
    public:
        ZipArchiveImporter(const QString& zip_file, const QString& zip_dir, const QString& workspace_dir, QuaZip& zip);
        bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        bool ReadFile(const AnyString& uri, QByteArray* array) const override;
        bool HasMapping(const AnyString& uri) const override;
        AnyString MapUri(const AnyString& uri) const override;
        Operation GetOp() const override
        { return Operation::Import; }

        bool CopyFile(const QString& src_file, const AnyString& dir, QString*  dst_name);

    private:
        QString MapUriToZipFile(const std::string& uri) const;
        bool FindZipFile(const QString& unix_style_name) const;
        void AddUriMapping(const AnyString& uri, const QString& name);
    private:
        const QString mZipFile;
        const QString mZipDir;
        const QString mWorkspaceDir;
        QuaZip& mZip;
        std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
    };

} // namespace
