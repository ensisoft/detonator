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
#include "editor/app/zip_archive_importer.h"

namespace app
{

ZipArchiveImporter::ZipArchiveImporter(const QString& zip_file, const QString& zip_dir, const QString& workspace_dir, QuaZip& zip)
  : mZipFile(zip_file)
  , mZipDir(zip_dir)
  , mWorkspaceDir(workspace_dir)
  , mZip(zip)
{}

bool ZipArchiveImporter::CopyFile(const AnyString& uri, const AnyString& dir)
{
    // Skip resources that are part of the editor itself.
    if (base::StartsWith(uri, "app://"))
        return true;

    const auto& src_file = MapUriToZipFile(uri);

    QString dst_name;

    if (CopyFile(src_file, dir, &dst_name))
    {
        auto mapping = toString("ws://%1/%2", mZipDir, dst_name);
        DEBUG("New zip URI mapping. [uri='%1', mapping='%2']", uri, mapping);
        mUriMapping[uri] = mapping;
    }

    // hack for now to copy the bitmap font image.
    // this will not work if:
    // - the file extension is not .png
    // - the file name is same as the .json file base name
    if (IsBitmapFontJsonUri(uri))
    {
        const auto& font_bitmap_uri = FontBitmapUriFromJsonUri(uri);
        const auto& font_bitmap_file = MapUriToZipFile(font_bitmap_uri);

        QString dst_png_name;
        CopyFile(font_bitmap_file, dir, &dst_png_name);
    }
    return true;
}
bool ZipArchiveImporter::WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len)
{
    if (base::StartsWith(uri, "app://"))
    {
        DEBUG("Skip re-writing application resource on resource pack (zip) import. [uri='%1']", uri);
        return true;
    }

    // write the file contents into the workspace directory.

    const auto& src_file = MapUriToZipFile(uri);
    if (!FindZipFile(src_file))
        return false;

    QuaZipFileInfo info;
    mZip.getCurrentFileInfo(&info);

    // the dir part of the filepath should already have been baked in the zip
    // when exporting and the filename already contains the directory/path
    const auto& dst_dir  = JoinPath({mWorkspaceDir, mZipDir, dir});
    const auto& dst_file = JoinPath({mWorkspaceDir, mZipDir, info.name});

    if (!app::MakePath(dst_dir))
    {
        ERROR("Failed to create directory. [dir='%1']", dst_dir);
        return false;
    }

    QFile file;
    file.setFileName(dst_file);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        ERROR("Failed to open file for writing. [file='%1', error='%2']", dst_file, file.errorString());
        return false;
    }
    file.write((const char*)data, len);
    file.close();
    auto mapping = toString("ws://%1/%2", mZipDir, info.name);
    DEBUG("New zip URI mapping. [uri='%1', mapping='%2']", uri, mapping);
    mUriMapping[uri] = std::move(mapping);
    return true;
}
bool ZipArchiveImporter::ReadFile(const AnyString& uri, QByteArray* array) const
{
    // this is a hack in order to support dependant script resolution
    // the packing code iterates and recurses the dependant lua scripts.
    // a game script can refer to a script under the editor and then
    // the packing code (mentioned above) would try to process the app://
    // scripts which isn't packed in the .zip file!
    // we're going to lie here and return an empty buffer so the iteration
    // stops
    if (base::StartsWith(uri, "app://"))
    {
        DEBUG("Skip reading application resource on resource pack (zip) import. [uri='%1']", uri);
        return true;
    }

    const auto& src_file = MapUriToZipFile(uri);
    if (!FindZipFile(src_file))
        return false;
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::ReadOnly);
    *array = zip_file.readAll();
    zip_file.close();
    return true;
}
bool ZipArchiveImporter::HasMapping(const AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}
app::AnyString ZipArchiveImporter::MapUri(const AnyString& uri) const
{
    if (base::StartsWith(uri, "app://"))
        return uri;

    const auto* mapping = base::SafeFind(mUriMapping, uri);
    ASSERT(mapping);
    return *mapping;
}

bool ZipArchiveImporter::CopyFile(const QString& src_file, const AnyString& dir, QString*  dst_name)
{
    // copy file from the zip into the workspace directory.
    if (!FindZipFile(src_file))
        return false;

    QuaZipFileInfo info;
    QByteArray bytes;

    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::ReadOnly);
    zip_file.getFileInfo(&info);
    bytes = zip_file.readAll();

    // the dir part of the filepath should already have been baked in the zip
    // when exporting and the filename already contains the directory/path
    const auto& dst_dir  = JoinPath({mWorkspaceDir, mZipDir, dir});
    const auto& dst_file = JoinPath({mWorkspaceDir, mZipDir, info.name});

    if (!MakePath(dst_dir))
    {
        ERROR("Failed to create directory. [dir='%1']", dst_dir);
        return false;
    }
    QFile file;
    file.setFileName(dst_file);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        ERROR("Failed to open file for writing. [file='%1', error='%2']]", dst_file, file.errorString());
        return false;
    }
    file.write(bytes);
    file.close();
    *dst_name = info.name;
    DEBUG("Copied file from zip archive. [src='%1', dst='%2']", src_file, dst_file);
    return true;
}

QString ZipArchiveImporter::MapUriToZipFile(const std::string& uri) const
{
    ASSERT(base::StartsWith(uri, "zip://"));
    const auto& rest = uri.substr(6);
    return app::FromUtf8(rest);
}

bool ZipArchiveImporter::FindZipFile(const QString& unix_style_name) const
{
    if (!mZip.goToFirstFile())
        return false;
    // on windows the zip file paths are also windows style. (of course)
    QString windows_style_name = unix_style_name;
    windows_style_name.replace("/", "\\");
    do
    {
        QuaZipFileInfo info;
        if (!mZip.getCurrentFileInfo(&info))
            return false;
        if ((info.name == unix_style_name) ||
            (info.name == windows_style_name))
            return true;
    }
    while (mZip.goToNextFile());
    ERROR("Failed to find file in zip. [file='%1']", unix_style_name);
    return false;
}

} // namespace