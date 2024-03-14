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

#define LOGTAG "app"

#include "base/utility.h"
#include "editor/app/packer.h"
#include "editor/app/eventlog.h"
#include "editor/app/buffer.h"
#include "editor/app/utility.h"

namespace app
{

WorkspaceResourcePacker::WorkspaceResourcePacker(const QString& package_dir, const QString& workspace_dir)
  : mPackageDir(package_dir)
  , mWorkspaceDir(workspace_dir)
{}

bool WorkspaceResourcePacker::CopyFile(const app::AnyString& uri, const std::string& dir)
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
    const auto& dst_file = CopyFile(src_file, app::FromUtf8(dir));
    if (dst_file.isEmpty())
        return false;

    const auto& dst_uri  = MapFileToPackage(dst_file);
    mUriMapping[uri] = dst_uri;

    // if the font is a .json+.png font then copy the .png file too!
    if (base::Contains(uri, "fonts/") && base::EndsWith(uri, ".json"))
    {
        const auto& png_uri = app::ReplaceAll(uri, ".json", ".png");
        const auto& png_file = MapFileToFilesystem(png_uri);
        CopyFile(png_file, app::FromUtf8(dir));
    }
    return true;
}
bool WorkspaceResourcePacker::WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len)
{
    if (const auto* dupe = base::SafeFind(mUriMapping, uri))
    {
        DEBUG("Skipping duplicate file replace. [file='%1']", uri);
        return true;
    }
    const auto& src_file = MapFileToFilesystem(uri);
    const auto& dst_file = WriteFile(src_file, app::FromUtf8(dir), data, len);
    if (dst_file.isEmpty())
        return false;

    const auto& dst_uri  = MapFileToPackage(dst_file);
    mUriMapping[uri] = dst_uri;
    return true;
}
bool WorkspaceResourcePacker::ReadFile(const app::AnyString& uri, QByteArray* bytes) const
{
    const auto& file = MapFileToFilesystem(uri);
    return app::detail::LoadArrayBuffer(file, bytes);
}
bool WorkspaceResourcePacker::HasMapping(const app::AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}
app::AnyString WorkspaceResourcePacker::MapUri(const app::AnyString& uri) const
{
    if (const auto* mapping = base::SafeFind(mUriMapping, uri))
        return *mapping;

    return QString("");
}

QString WorkspaceResourcePacker::WriteFile(const QString& src_file, const QString& dst_dir, const void* data, size_t len)
{
    if (!app::MakePath(app::JoinPath(mPackageDir, dst_dir)))
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
QString WorkspaceResourcePacker::CopyFile(const QString& src_file, const QString& dst_dir, const QString& filename)
{
    if (const auto* dupe = base::SafeFind(mFileMap, src_file))
    {
        DEBUG("Skipping duplicate file copy. [file='%1']", src_file);
        return *dupe;
    }
    if (!app::MakePath(app::JoinPath(mPackageDir, dst_dir)))
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
    const auto& dst_path = app::JoinPath(mPackageDir, dst_dir);
    QString dst_name = filename.isEmpty() ? src_name : filename;
    QString dst_file = app::JoinPath(dst_path, dst_name);

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
QString WorkspaceResourcePacker::MapFileToFilesystem(const app::AnyString& uri) const
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

ZipArchiveImporter::ZipArchiveImporter(const QString& zip_file, const QString& zip_dir, const QString& workspace_dir, QuaZip& zip)
  : mZipFile(zip_file)
  , mZipDir(zip_dir)
  , mWorkspaceDir(workspace_dir)
  , mZip(zip)
{}

bool ZipArchiveImporter::CopyFile(const app::AnyString& uri, const std::string& dir)
{
    // Skip resources that are part of the editor itself.
    if (base::StartsWith(uri, "app://"))
        return true;

    const auto& src_file = MapUriToZipFile(uri);

    QString dst_name;

    if (CopyFile(src_file, dir, &dst_name))
    {
        auto mapping = app::toString("ws://%1/%2", mZipDir, dst_name);
        DEBUG("New zip URI mapping. [uri='%1', mapping='%2']", uri, mapping);
        mUriMapping[uri] = mapping;
    }

    // hack for now to copy the bitmap font image.
    // this will not work if:
    // - the file extension is not .png
    // - the file name is same as the .json file base name
    if (base::Contains(dir, "fonts/") && base::EndsWith(uri, ".json"))
    {
        const auto& src_png_uri  = app::ReplaceAll(uri, ".json", ".png");
        const auto& src_png_file = MapUriToZipFile(src_png_uri);

        QString dst_png_name;
        CopyFile(src_png_file, dir, &dst_png_name);
    }
    return true;
}
bool ZipArchiveImporter::WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len)
{
    // write the file contents into the workspace directory.

    const auto& src_file = MapUriToZipFile(uri);
    if (!FindZipFile(src_file))
        return false;

    QuaZipFileInfo info;
    mZip.getCurrentFileInfo(&info);

    // the dir part of the filepath should already have been baked in the zip
    // when exporting and the filename already contains the directory/path
    const auto& dst_dir  = app::JoinPath({mWorkspaceDir, mZipDir, app::FromUtf8(dir)});
    const auto& dst_file = app::JoinPath({mWorkspaceDir, mZipDir, info.name});

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
    auto mapping = base::FormatString("ws://%1/%2", app::ToUtf8(mZipDir), app::ToUtf8(info.name));
    DEBUG("New zip URI mapping. [uri='%1', mapping='%2']", uri, mapping);
    mUriMapping[uri] = std::move(mapping);
    return true;
}
bool ZipArchiveImporter::ReadFile(const app::AnyString& uri, QByteArray* array) const
{
    const auto& src_file = MapUriToZipFile(uri);
    if (!FindZipFile(src_file))
        return false;
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::ReadOnly);
    *array = zip_file.readAll();
    zip_file.close();
    return true;
}
bool ZipArchiveImporter::HasMapping(const app::AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}
app::AnyString ZipArchiveImporter::MapUri(const app::AnyString& uri) const
{
    if (base::StartsWith(uri, "app://"))
        return uri;

    const auto* mapping = base::SafeFind(mUriMapping, uri);
    ASSERT(mapping);
    return *mapping;
}

bool ZipArchiveImporter::CopyFile(const QString& src_file, const std::string& dir, QString*  dst_name)
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
    const auto& dst_dir  = app::JoinPath({mWorkspaceDir, mZipDir, app::FromUtf8(dir)});
    const auto& dst_file = app::JoinPath({mWorkspaceDir, mZipDir, info.name});

    if (!app::MakePath(dst_dir))
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

ZipArchiveExporter::ZipArchiveExporter(const QString& filename, const QString& workspace_dir)
  : mZipFile(filename)
  , mWorkspaceDir(workspace_dir)
{
    mZip.setAutoClose(true);
    mZip.setFileNameCodec("UTF-8");
    mZip.setUtf8Enabled(true);
    mZip.setZip64Enabled(true);
}


bool ZipArchiveExporter::CopyFile(const app::AnyString& uri, const std::string& dir)
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
    const auto& dst_dir  = app::FromUtf8(dir);

    QString dst_name = src_name;
    QString dst_file = app::JoinPath(dst_dir, dst_name);
    unsigned rename_attempt = 0;
    while (base::Contains(mFileNames, dst_name))
    {
        dst_name = app::toString("%1_%2", rename_attempt++, src_name);
        dst_file = app::JoinPath(dst_dir, dst_name);
    }

    if (!CopyFile(src_file, dst_file))
        return false;

    mFileNames.insert(dst_name);
    mUriMapping[uri] = base::FormatString("zip://%1%2", dir, app::ToUtf8(dst_name));

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
bool ZipArchiveExporter::WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len)
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
    mUriMapping[uri] = base::FormatString("zip://%1%2", dir, app::ToUtf8(src_name));
    DEBUG("Wrote new file into zip archive. [file='%1']", dst_name);
    return true;
}
bool ZipArchiveExporter::ReadFile(const app::AnyString& uri, QByteArray* bytes) const
{
    const auto& file = MapFileToFilesystem(uri);
    return app::detail::LoadArrayBuffer(file, bytes);
}
bool ZipArchiveExporter::HasMapping(const app::AnyString& uri) const
{
    if (mUriMapping.find(uri) != mUriMapping.end())
        return true;
    return false;
}

app::AnyString ZipArchiveExporter::MapUri(const app::AnyString& uri) const
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
    if (!app::ReadBinaryFile(src_file, buffer))
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


} // namespace