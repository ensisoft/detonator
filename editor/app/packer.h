// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <quazip/quazip.h>
#  include <quazip/quazipfile.h>
#include "warnpop.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "editor/app/types.h"

namespace app
{
    class ResourcePacker
    {
    public:
        virtual ~ResourcePacker() = default;
        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) = 0;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) = 0;
        virtual bool ReadFile(const AnyString& uri, QByteArray* bytes) const = 0;
        virtual bool HasMapping(const AnyString& uri) const = 0;
        virtual AnyString MapUri(const AnyString& uri) const = 0;
    private:
    };


    class WorkspaceResourcePacker : public ResourcePacker
    {
    public:
        WorkspaceResourcePacker(const QString& package_dir, const QString& workspace_dir);

        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        virtual bool ReadFile(const AnyString& uri, QByteArray* bytes) const override;
        virtual bool HasMapping(const AnyString& uri) const override;
        virtual AnyString MapUri(const AnyString& uri) const override;

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

    class ZipArchiveImporter : public app::ResourcePacker
    {
    public:
        ZipArchiveImporter(const QString& zip_file, const QString& zip_dir, const QString& workspace_dir, QuaZip& zip);
        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        virtual bool ReadFile(const AnyString& uri, QByteArray* array) const override;
        virtual bool HasMapping(const AnyString& uri) const override;
        virtual AnyString MapUri(const AnyString& uri) const override;
        bool CopyFile(const QString& src_file, const AnyString& dir, QString*  dst_name);
    private:
        QString MapUriToZipFile(const std::string& uri) const;
        bool FindZipFile(const QString& unix_style_name) const;
    private:
        const QString mZipFile;
        const QString mZipDir;
        const QString mWorkspaceDir;
        QuaZip& mZip;
        std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
    };


    class ZipArchiveExporter : public app::ResourcePacker
    {
    public:
        ZipArchiveExporter(const QString& filename, const QString& workspace_dir);
        virtual bool CopyFile(const AnyString& uri, const AnyString& dir) override;
        virtual bool WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len) override;
        virtual bool ReadFile(const AnyString& uri, QByteArray* array) const override;
        virtual bool HasMapping(const AnyString& uri) const override;
        virtual AnyString MapUri(const AnyString& uri) const override;

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
