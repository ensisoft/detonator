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
    class ZipArchiveExporter : public app::ResourcePacker
    {
    public:
        ZipArchiveExporter(const QString& filename, const QString& workspace_dir);
        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        virtual bool ReadFile(const AnyString& uri, QByteArray* array) const override;
        virtual bool HasMapping(const AnyString& uri) const override;
        virtual AnyString MapUri(const AnyString& uri) const override;
        virtual bool IsReleasePackage() const override
        { return false; }

        void WriteText(const std::string& text, const char* name);
        void WriteBytes(const QByteArray& bytes, const char* name);
        bool CopyFile(const QString& src_file, const QString& dst_file);
        void Close();
        bool Open();
    private:
        QString MapFileToFilesystem(const app::AnyString& uri) const;
    private:
        const QString mZipFile;
        const QString mWorkspaceDir;
        std::unordered_set<QString> mFileNames;
        std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
        QFile mFile;
        QuaZip mZip;
    };

} // namespace