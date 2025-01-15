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

#define LOGTAG "app"

#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/buffer.h"
#include "editor/app/utility.h"
#include "editor/app/zip_archive_exporter.h"

namespace app
{

ZipArchiveExporter::ZipArchiveExporter(const QString& filename, const QString& workspace_dir)
  : mZipFile(filename)
  , mWorkspaceDir(workspace_dir)
{
    mZip.setAutoClose(true);
    mZip.setFileNameCodec("UTF-8");
    mZip.setUtf8Enabled(true);
    mZip.setZip64Enabled(true);
}


bool ZipArchiveExporter::CopyFile(const AnyString& uri, const AnyString& dir)
{
    // don't package resources that are part of the editor.
    // todo: this would need some kind of versioning in order to
    // make sure that the resources under app:// then match between
    // the exporter and the importer.
    if (base::StartsWith(uri, "app://"))
        return true;

    if (const auto* dupe = base::SafeFind(mUriMapping, uri))
    {
        DEBUG("Skipping duplicate file copy. [file='%1']", uri);
        return true;
    }

    const auto& src_file = MapFileToFilesystem(uri);
    const auto& src_info = QFileInfo(src_file);
    if (!src_info.exists())
    {
        ERROR("Failed to find zip export source file. [file='%1']", src_file);
        return false;
    }
    const auto& src_path = src_info.absoluteFilePath();
    const auto& src_name = src_info.fileName();
    const auto& dst_dir  = dir;

    QString dst_name = src_name;
    QString dst_file = JoinPath(dst_dir, dst_name);
    unsigned rename_attempt = 0;
    while (base::Contains(mFileNames, dst_name))
    {
        dst_name = toString("%1_%2", rename_attempt++, src_name);
        dst_file = JoinPath(dst_dir, dst_name);
    }

    if (!CopyFile(src_file, dst_file))
        return false;

    mFileNames.insert(dst_name);
    mUriMapping[uri] = toString("zip://%1%2", dir, dst_name);

    // hack for now to copy the bitmap font image.
    // this will not work if:
    // - the file extension is not .png
    // - the file name is same as the .json file base name
    if (base::Contains(dir, "fonts/") && base::EndsWith(uri, ".json"))
    {
        const auto& src_png_uri  = app::ReplaceAll(uri, ".json", ".png");
        const auto& src_png_file = MapFileToFilesystem(src_png_uri);
        auto png_name = src_name;
        png_name.replace(".json", ".png");
        CopyFile(src_png_file, app::JoinPath(dst_dir, png_name));
    }
    return true;
}
bool ZipArchiveExporter::WriteFile(const app::AnyString& uri, const AnyString& dir, const void* data, size_t len)
{
    if (const auto* dupe = base::SafeFind(mUriMapping, uri))
    {
        DEBUG("Skipping duplicate file replace. [file='%1']", uri);
        return true;
    }
    const auto& src_file = MapFileToFilesystem(uri);
    const auto& src_info = QFileInfo(src_file);
    if (!src_info.exists())
    {
        ERROR("Failed to find zip export source file. [file='%1']", src_file);
        return false;
    }
    const auto& src_name = src_info.fileName();
    const auto& dst_dir  = app::FromUtf8(dir);
    const auto& dst_name = app::JoinPath(dst_dir, src_name);

    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(dst_name));
    zip_file.write((const char*)data, len);
    zip_file.close();
    ASSERT(base::EndsWith(dir, "/"));
    mUriMapping[uri] = toString("zip://%1%2", dir, src_name);
    DEBUG("Wrote new file into zip archive. [file='%1']", dst_name);
    return true;
}
bool ZipArchiveExporter::ReadFile(const AnyString& uri, QByteArray* bytes) const
{
    const auto& file = MapFileToFilesystem(uri);
    return app::detail::LoadArrayBuffer(file, bytes);
}
bool ZipArchiveExporter::HasMapping(const AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}

AnyString ZipArchiveExporter::MapUri(const AnyString& uri) const
{
    if (base::StartsWith(uri, "app://"))
        return uri;

    const auto* mapping = base::SafeFind(mUriMapping, uri);
    ASSERT(mapping);
    return *mapping;
}

void ZipArchiveExporter::WriteText(const std::string& text, const char* name)
{
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(name));
    zip_file.write(text.c_str(), text.size());
    zip_file.close();
}
void ZipArchiveExporter::WriteBytes(const QByteArray& bytes, const char* name)
{
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(name));
    zip_file.write(bytes.data(), bytes.size());
    zip_file.close();
}

bool ZipArchiveExporter::CopyFile(const QString& src_file, const QString& dst_file)
{
    std::vector<char> buffer;
    if (!ReadBinaryFile(src_file, buffer))
    {
        ERROR("Failed to read file contents. [file='%1']", src_file);
        return false;
    }
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(dst_file));
    zip_file.write(buffer.data(), buffer.size());
    zip_file.close();
    DEBUG("Copied new file into zip archive. [file='%1', size=%2]", src_file, app::Bytes { buffer.size() });
    return true;
}

void ZipArchiveExporter::Close()
{
    mZip.close();
    mFile.flush();
    mFile.close();
}
bool ZipArchiveExporter::Open()
{
    mFile.setFileName(mZipFile);
    if (!mFile.open(QIODevice::ReadWrite))
    {
        ERROR("Failed to open zip file for writing. [file='%1', error='%2']", mZipFile, mFile.errorString());
        return false;
    }
    mZip.setIoDevice(&mFile);
    if (!mZip.open(QuaZip::mdCreate))
    {
        ERROR("QuaZip open failed. [code=%1]", mZip.getZipError());
        return false;
    }
    DEBUG("QuaZip open successful. [file='%1']", mZipFile);
    return true;
}

QString ZipArchiveExporter::MapFileToFilesystem(const app::AnyString& uri) const
{
    return MapWorkspaceUri(uri, mWorkspaceDir);
}


} // app