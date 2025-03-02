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
#include <unordered_map>

#include "editor/app/types.h"
#include "editor/app/resource_packer.h"

namespace app
{
    class WorkspaceResourcePacker : public ResourcePacker
    {
    public:
        WorkspaceResourcePacker(const QString& package_dir, const QString& workspace_dir);

        bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        bool ReadFile(const AnyString& uri, QByteArray* bytes) const override;
        bool HasMapping(const AnyString& uri) const override;
        AnyString MapUri(const AnyString& uri) const override;
        Operation GetOp() const override
        { return Operation::Deploy; }

        using app::ResourcePacker::HasMapping;
        using app::ResourcePacker::MapUri;

        QString DoWriteFile(const QString& src_file, const QString& dst_dir, const void* data, size_t len);
        QString DoCopyFile(const QString& src_file, const QString& dst_dir, const QString& filename = QString(""));
        QString CreateFileName(const QString& src_file, const QString& dst_dir, const QString& filename) const;
        QString MapFileToFilesystem(const AnyString& uri) const;
        QString MapFileToPackage(const QString& file) const;

        inline unsigned GetNumErrors() const noexcept
        { return mNumErrors; }
        inline unsigned GetNumFilesCopied() const noexcept
        { return mNumCopies; }
    private:
        void CopyFileBuffer(const QString& src, const QString& dst);

    private:
        const QString mPackageDir;
        const QString mWorkspaceDir;
        unsigned mNumErrors = 0;
        unsigned mNumCopies = 0;
        std::unordered_map<QString, QString> mFileMap;
        std::unordered_set<QString> mFileNames;
        std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
    };

} // namespace