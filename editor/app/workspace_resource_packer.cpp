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

#include "config.h"

#include "warnpush.h"
#  include <QFile>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/buffer.h"
#include "editor/app/workspace_resource_packer.h"

namespace app
{

WorkspaceResourcePacker::WorkspaceResourcePacker(const QString& package_dir, const QString& workspace_dir)
  : mPackageDir(package_dir)
  , mWorkspaceDir(workspace_dir)
{}

bool WorkspaceResourcePacker::CopyFile(const AnyString& uri, const AnyString& dir)
{
    // sort of hack here, probe the uri and skip the copy of a
    // custom shader .json descriptor. it's not needed in the
    // deployed package.
    if (base::Contains(uri, "shaders/es2") && base::EndsWith(uri, ".json"))
    {
        DEBUG("Skipping copy of shader .json descriptor. [uri='%1']", uri);
        return true;
    }

    // if the target dir for packing is textures/ we skip this because
    // the textures are packed through calls to GfxTexturePacker.
    if (dir == "textures/")
    {
        mUriMapping[uri] = uri;
        return true;
    }

    if (const auto* dupe = base::SafeFind(mUriMapping, uri))
    {
        DEBUG("Skipping duplicate file copy. [file='%1']", uri);
        return true;
    }

    const auto& src_file = MapFileToFilesystem(uri);
    const auto& dst_file = DoCopyFile(src_file, dir);
    if (dst_file.isEmpty())
        return false;

    const auto& dst_uri  = MapFileToPackage(dst_file);
    mUriMapping[uri] = dst_uri;

    // if the font is a .json+.png font then copy the .png file too!
    if (base::Contains(uri, "fonts/") && base::EndsWith(uri, ".json"))
    {
        const auto& png_uri  = ReplaceAll(uri, ".json", ".png");
        const auto& png_file = MapFileToFilesystem(png_uri);
        DoCopyFile(png_file, dir);
    }
    return true;
}
bool WorkspaceResourcePacker::WriteFile(const AnyString& uri, const AnyString& dir, const void* data, size_t len)
{
    if (const auto* dupe = base::SafeFind(mUriMapping, uri))
    {
        DEBUG("Skipping duplicate file replace. [file='%1']", uri);
        return true;
    }
    const auto& src_file = MapFileToFilesystem(uri);
    const auto& dst_file = DoWriteFile(src_file, dir, data, len);
    if (dst_file.isEmpty())
        return false;

    const auto& dst_uri  = MapFileToPackage(dst_file);
    mUriMapping[uri] = dst_uri;
    return true;
}
bool WorkspaceResourcePacker::ReadFile(const AnyString& uri, QByteArray* bytes) const
{
    const auto& file = MapFileToFilesystem(uri);
    return app::detail::LoadArrayBuffer(file, bytes);
}
bool WorkspaceResourcePacker::HasMapping(const AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}
AnyString WorkspaceResourcePacker::MapUri(const AnyString& uri) const
{
    if (const auto* mapping = base::SafeFind(mUriMapping, uri))
        return *mapping;

    return QString("");
}

QString WorkspaceResourcePacker::DoWriteFile(const QString& src_file, const QString& dst_dir, const void* data, size_t len)
{
    if (!MakePath(JoinPath(mPackageDir, dst_dir)))
    {
        ERROR("Failed to create directory. [dir='%1/%2']", mPackageDir, dst_dir);
        return "";
    }
    const auto& dst_file = CreateFileName(src_file, dst_dir, QString(""));
    if (dst_file.isEmpty())
        return "";

    QFile file;
    file.setFileName(dst_file);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (!file.isOpen())
    {
        ERROR("Failed to open file for writing. [file='%1', error='%2']", dst_file, file.errorString());
        return "";
    }
    file.write((const char*)data, len);
    file.close();
    return dst_file;
}
QString WorkspaceResourcePacker::DoCopyFile(const QString& src_file, const QString& dst_dir, const QString& filename)
{
    if (const auto* dupe = base::SafeFind(mFileMap, src_file))
    {
        DEBUG("Skipping duplicate file copy. [file='%1']", src_file);
        return *dupe;
    }
    if (!MakePath(JoinPath(mPackageDir, dst_dir)))
    {
        ERROR("Failed to create directory. [dir='%1/%2']", mPackageDir, dst_dir);
        mNumErrors++;
        return "";
    }
    const auto& dst_file = CreateFileName(src_file, dst_dir, filename);
    if (dst_file.isEmpty())
    {
        ERROR("Failed to create output file name. [src_file='%1']", src_file);
        mNumErrors++;
        return "";
    }

    CopyFileBuffer(src_file, dst_file);
    mFileMap[src_file] = dst_file;
    mFileNames.insert(dst_file);
    return dst_file;
}
QString WorkspaceResourcePacker::CreateFileName(const QString& src_file, const QString& dst_dir, const QString& filename) const
{
    const auto& src_info = QFileInfo(src_file);
    if (!src_info.exists())
    {
        ERROR("Could not find source file. [file='%1']", src_file);
        return "";
    }
    const auto& src_path = src_info.absoluteFilePath();
    const auto& src_name = src_info.fileName();
    const auto& dst_path = JoinPath(mPackageDir, dst_dir);
    QString dst_name = filename.isEmpty() ? src_name : filename;
    QString dst_file = JoinPath(dst_path, dst_name);

    // use a silly race-condition laden loop trying to figure out whether
    // a file with this name already exists or not and if so then generate
    // a different output name for the file
    for (unsigned i=0; i<10000; ++i)
    {
        const QFileInfo dst_info(dst_file);
        if (!dst_info.exists())
            break;
        // if the destination file exists *from before* we're
        // going to overwrite it. The user should have been confirmed
        // for this, and it should be fine at this point.
        // So only try to resolve a name collision if we're trying to
        // write an output file by the same name multiple times
        if (mFileNames.find(dst_file) == mFileNames.end())
            break;
        // generate a new name.
        dst_name = filename.isEmpty()
                   ? QString("%1_%2").arg(src_name).arg(i)
                   : QString("%1_%2").arg(filename).arg(i);
        dst_file = app::JoinPath(dst_path, dst_name);
    }
    return dst_file;
}
QString WorkspaceResourcePacker::MapFileToFilesystem(const AnyString& uri) const
{
    return MapWorkspaceUri(uri, mWorkspaceDir);
}
QString WorkspaceResourcePacker::MapFileToPackage(const QString& file) const
{
    ASSERT(file.startsWith(mPackageDir));
    QString ret = file;
    ret.remove(0, mPackageDir.count());
    if (ret.startsWith("/") || ret.startsWith("\\"))
        ret.remove(0, 1);
    ret = ret.replace("\\", "/");
    return QString("pck://%1").arg(ret);
}

void WorkspaceResourcePacker::CopyFileBuffer(const QString& src, const QString& dst)
{
    // if src equals dst then we can actually skip the copy, no?
    if (src == dst)
    {
        DEBUG("Skipping copy of file onto itself. [src='%1', dst='%2']", src, dst);
        return;
    }
    auto [success, error] = app::CopyFile(src, dst);
    if (!success)
    {
        ERROR("Failed to copy file. [src='%1', dst='%2' error=%3]", src, dst, error);
        mNumErrors++;
        return;
    }
    mNumCopies++;
    DEBUG("File copy done. [src='%1', dst='%2']", src, dst);
}

} // namespace