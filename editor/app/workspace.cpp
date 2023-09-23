// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <quazip/quazip.h>
#  include <quazip/quazipfile.h>
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#  include <private/qfsfileengine_p.h> // private in Qt5
#  include <QCoreApplication>
#  include <QtAlgorithms>
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QByteArray>
#  include <QFile>
#  include <QFileInfo>
#  include <QIcon>
#  include <QPainter>
#  include <QImage>
#  include <QImageWriter>
#  include <QPixmap>
#  include <QDir>
#  include <QStringList>
#include "warnpop.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <set>
#include <stack>
#include <functional>

#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/packing.h"
#include "editor/app/buffer.h"
#include "editor/app/process.h"
#include "graphics/resource.h"
#include "graphics/color4f.h"
#include "engine/ui.h"
#include "engine/data.h"
#include "data/json.h"
#include "base/json.h"

namespace {

gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}

QString GetAppDir()
{
    static const auto& dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
#if defined(POSIX_OS)
    return dir + "/";
#elif defined(WINDOWS_OS)
    return dir + "\\";
#else
# error unimplemented function
#endif
    return dir;
}

QString FixWorkspacePath(QString path)
{
    path = app::CleanPath(path);
#if defined(POSIX_OS)
    if (!path.endsWith("/"))
        path.append("/");
#elif defined(WINDOWS_OS)
    if (!path.endsWith("\\"))
        path.append("\\");
#endif
    return path;
}
app::AnyString MapWorkspaceUri(const app::AnyString& uri, const QString& workspace)
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.
    QString ret = uri;
    if (ret.startsWith("ws://"))
        ret = app::CleanPath(ret.replace("ws://", workspace));
    else if (ret.startsWith("app://"))
        ret = app::CleanPath(ret.replace("app://", GetAppDir()));
    else if (ret.startsWith("fs://"))
        ret.remove(0, 5);
    // return as is
    return ret;
}

template<typename ClassType>
bool LoadResources(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<app::Resource>>& vector,
                   app::MigrationLog* log = nullptr)
{
    DEBUG("Loading resources. [type='%1']", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        auto chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        chunk = app::detail::MigrateResourceDataChunk<ClassType>(std::move(chunk), log);

        ClassType ret;
        if (!ret.FromJson(*chunk))
        {
            WARN("Incomplete resource load from JSON. [type='%1', name='%2']", type, name);
            success = false;
        }

        vector.push_back(std::make_unique<app::GameResource<ClassType>>(std::move(ret), name));
        DEBUG("Loaded workspace resource. [name='%1']", name);
    }
    return success;
}

template<typename ClassType>
bool LoadMaterials(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<app::Resource>>& vector,
                   app::MigrationLog* log = nullptr)
{
    DEBUG("Loading resources. [type='%1']", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        auto chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        chunk = app::detail::MigrateResourceDataChunk<ClassType>(std::move(chunk), log);

        auto ret = ClassType::ClassFromJson(*chunk);
        if (!ret)
        {
            WARN("Incomplete resource load from JSON. [type='%1', name='%2']", type, name);
            success = false;
            continue;
        }

        vector.push_back(std::make_unique<app::MaterialResource>(std::move(ret), name));
        DEBUG("Loaded workspace resource. [name='%1']", name);
    }
    return success;
}


class ZipArchiveImporter : public app::ResourcePacker
{
public:
    ZipArchiveImporter(const QString& zip_file, const QString& zip_dir, const QString& workspace_dir, QuaZip& zip)
       : mZipFile(zip_file)
       , mZipDir(zip_dir)
       , mWorkspaceDir(workspace_dir)
       , mZip(zip)
    {}
    virtual void CopyFile(const app::AnyString& uri, const std::string& dir) override
    {
        if (base::StartsWith(uri, "app://"))
            return;

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
    }
    virtual void WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len) override
    {
        // write the file contents into the workspace directory.

        const auto& src_file = MapUriToZipFile(uri);
        if (!FindZipFile(src_file))
            return;

        QuaZipFileInfo info;
        mZip.getCurrentFileInfo(&info);

        // the dir part of the filepath should already have been baked in the zip
        // when exporting and the filename already contains the directory/path
        const auto& dst_dir  = app::JoinPath({mWorkspaceDir, mZipDir, app::FromUtf8(dir)});
        const auto& dst_file = app::JoinPath({mWorkspaceDir, mZipDir, info.name});

        if (!app::MakePath(dst_dir))
        {
            ERROR("Failed to create directory. [dir='%1']", dst_dir);
            return;
        }

        QFile file;
        file.setFileName(dst_file);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            ERROR("Failed to open file for writing. [file='%1', error='%2']", dst_file, file.errorString());
            return;
        }
        file.write((const char*)data, len);
        file.close();
        auto mapping = base::FormatString("ws://%1/%2", app::ToUtf8(mZipDir), app::ToUtf8(info.name));
        DEBUG("New zip URI mapping. [uri='%1', mapping='%2']", uri, mapping);
        mUriMapping[uri] = std::move(mapping);
    }

    virtual bool ReadFile(const app::AnyString& uri, QByteArray* array) const override
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
    virtual app::AnyString MapUri(const app::AnyString& uri) const override
    {
        if (base::StartsWith(uri, "app://"))
            return uri;

        const auto* mapping = base::SafeFind(mUriMapping, uri);
        ASSERT(mapping);
        return *mapping;
    }
    virtual bool HasMapping(const app::AnyString& uri) const override
    {
        if (mUriMapping.find(uri) != mUriMapping.end())
            return true;
        return false;
    }

    bool CopyFile(const QString& src_file, const std::string& dir, QString*  dst_name)
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

private:
    QString MapUriToZipFile(const std::string& uri) const
    {
        ASSERT(base::StartsWith(uri, "zip://"));
        const auto& rest = uri.substr(6);
        return app::FromUtf8(rest);
    }

    bool FindZipFile(const QString& unix_style_name) const
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
    ZipArchiveExporter(const QString& filename, const QString& workspace_dir)
      : mZipFile(filename)
      , mWorkspaceDir(workspace_dir)
    {
        mZip.setAutoClose(true);
        mZip.setFileNameCodec("UTF-8");
        mZip.setUtf8Enabled(true);
        mZip.setZip64Enabled(true);
    }
    virtual void CopyFile(const app::AnyString& uri, const std::string& dir) override
    {
        // don't package resources that are part of the editor.
        // todo: this would need some kind of versioning in order to
        // make sure that the resources under app:// then match between
        // the exporter and the importer.
        if (base::StartsWith(uri, "app://"))
            return;

        if (const auto* dupe = base::SafeFind(mUriMapping, uri))
        {
            DEBUG("Skipping duplicate file copy. [file='%1']", uri);
            return;
        }

        const auto& src_file = MapFileToFilesystem(uri);
        const auto& src_info = QFileInfo(src_file);
        if (!src_info.exists())
        {
            ERROR("Failed to find zip export source file. [file='%1']", src_file);
            return;
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
            return;

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
    }
    virtual void WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len) override
    {
        if (const auto* dupe = base::SafeFind(mUriMapping, uri))
        {
            DEBUG("Skipping duplicate file replace. [file='%1']", uri);
            return;
        }
        const auto& src_file = MapFileToFilesystem(uri);
        const auto& src_info = QFileInfo(src_file);
        if (!src_info.exists())
        {
            ERROR("Failed to find zip export source file. [file='%1']", src_file);
            return;
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
    }

    virtual bool ReadFile(const app::AnyString& uri, QByteArray* bytes) const override
    {
        const auto& file = MapFileToFilesystem(uri);
        return app::detail::LoadArrayBuffer(file, bytes);
    }
    virtual app::AnyString MapUri(const app::AnyString& uri) const override
    {
        if (base::StartsWith(uri, "app://"))
            return uri;

        const auto* mapping = base::SafeFind(mUriMapping, uri);
        ASSERT(mapping);
        return *mapping;
    }
    virtual bool HasMapping(const app::AnyString& uri) const override
    {
        if (mUriMapping.find(uri) != mUriMapping.end())
            return true;
        return false;
    }

    void WriteText(const std::string& text, const char* name)
    {
        QuaZipFile zip_file(&mZip);
        zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(name));
        zip_file.write(text.c_str(), text.size());
        zip_file.close();
    }
    void WriteBytes(const QByteArray& bytes, const char* name)
    {
        QuaZipFile zip_file(&mZip);
        zip_file.open(QIODevice::WriteOnly, QuaZipNewInfo(name));
        zip_file.write(bytes.data(), bytes.size());
        zip_file.close();
    }

    bool CopyFile(const QString& src_file, const QString& dst_file)
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

    void Close()
    {
        mZip.close();
        mFile.flush();
        mFile.close();
    }
    bool Open()
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
private:
    QString MapFileToFilesystem(const app::AnyString& uri) const
    {
        return MapWorkspaceUri(uri, mWorkspaceDir);
    }
private:
    const QString mZipFile;
    const QString mWorkspaceDir;
    std::unordered_set<QString> mFileNames;
    std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
    QFile mFile;
    QuaZip mZip;
};

class MyResourcePacker : public app::ResourcePacker
{
public:
    MyResourcePacker(const QString& package_dir, const QString& workspace_dir)
      : mPackageDir(package_dir)
      , mWorkspaceDir(workspace_dir)
    {}
    virtual void CopyFile(const app::AnyString& uri, const std::string& dir) override
    {
        // sort of hack here, probe the uri and skip the copy of a
        // custom shader .json descriptor. it's not needed in the
        // deployed package.
        if (base::Contains(uri, "shaders/es2") && base::EndsWith(uri, ".json"))
        {
            DEBUG("Skipping copy of shader .json descriptor. [uri='%1']", uri);
            return;
        }

        // if the target dir for packing is textures/ we skip this because
        // the textures are packed through calls to GfxTexturePacker.
        if (dir == "textures/")
        {
            mUriMapping[uri] = uri;
            return;
        }

        if (const auto* dupe = base::SafeFind(mUriMapping, uri))
        {
            DEBUG("Skipping duplicate file copy. [file='%1']", uri);
            return;
        }

        const auto& src_file = MapFileToFilesystem(uri);
        const auto& dst_file = CopyFile(src_file, app::FromUtf8(dir));
        if (dst_file.isEmpty())
            return;

        const auto& dst_uri  = MapFileToPackage(dst_file);
        mUriMapping[uri] = dst_uri;

        // if the font is a .json+.png font then copy the .png file too!
        if (base::Contains(uri, "fonts/") && base::EndsWith(uri, ".json"))
        {
            const auto& png_uri = app::ReplaceAll(uri, ".json", ".png");
            const auto& png_file = MapFileToFilesystem(png_uri);
            CopyFile(png_file, app::FromUtf8(dir));
        }
    }
    virtual void WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len) override
    {
        if (const auto* dupe = base::SafeFind(mUriMapping, uri))
        {
            DEBUG("Skipping duplicate file replace. [file='%1']", uri);
            return;
        }
        const auto& src_file = MapFileToFilesystem(uri);
        const auto& dst_file = WriteFile(src_file, app::FromUtf8(dir), data, len);
        const auto& dst_uri  = MapFileToPackage(dst_file);
        mUriMapping[uri] = dst_uri;
    }
    virtual bool ReadFile(const app::AnyString& uri, QByteArray* bytes) const override
    {
        const auto& file = MapFileToFilesystem(uri);
        return app::detail::LoadArrayBuffer(file, bytes);
    }
    virtual app::AnyString MapUri(const app::AnyString& uri) const override
    {
        if (const auto* mapping = base::SafeFind(mUriMapping, uri))
            return *mapping;

        return QString("");
    }
    virtual bool HasMapping(const app::AnyString& uri) const override
    {
        if (mUriMapping.find(uri) != mUriMapping.end())
            return true;
        return false;
    }
    using app::ResourcePacker::HasMapping;
    using app::ResourcePacker::MapUri;

    QString WriteFile(const QString& src_file, const QString& dst_dir, const void* data, size_t len)
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

    QString CopyFile(const QString& src_file, const QString& dst_dir, const QString& filename = QString(""))
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
    QString CreateFileName(const QString& src_file, const QString& dst_dir, const QString& filename) const
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
    QString MapFileToFilesystem(const app::AnyString& uri) const
    {
        return MapWorkspaceUri(uri, mWorkspaceDir);
    }
    QString MapFileToPackage(const QString& file) const
    {
        ASSERT(file.startsWith(mPackageDir));
        QString ret = file;
        ret.remove(0, mPackageDir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("pck://%1").arg(ret);
    }
    unsigned GetNumErrors() const
    { return mNumErrors; }
    unsigned GetNumFilesCopied() const
    { return mNumCopies; }
private:
    void CopyFileBuffer(const QString& src, const QString& dst)
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
private:
    const QString mPackageDir;
    const QString mWorkspaceDir;
    unsigned mNumErrors = 0;
    unsigned mNumCopies = 0;
    std::unordered_map<QString, QString> mFileMap;
    std::unordered_set<QString> mFileNames;
    std::unordered_map<app::AnyString, app::AnyString> mUriMapping;
};

class GfxTexturePacker : public gfx::TexturePacker
{
public:
    using ObjectHandle = gfx::TexturePacker::ObjectHandle;

    GfxTexturePacker(const QString& outdir,
                     unsigned max_width, unsigned max_height,
                     unsigned pack_width, unsigned pack_height, unsigned padding,
                     bool resize_large, bool pack_small)
        : kOutDir(outdir)
        , kMaxTextureWidth(max_width)
        , kMaxTextureHeight(max_height)
        , kTexturePackWidth(pack_width)
        , kTexturePackHeight(pack_height)
        , kTexturePadding(padding)
        , kResizeLargeTextures(resize_large)
        , kPackSmallTextures(pack_small)
    {}
   ~GfxTexturePacker()
    {
        for (const auto& temp : mTempFiles)
        {
            QFile::remove(temp);
        }
    }
    virtual void PackTexture(ObjectHandle instance, const std::string& file) override
    {
        mTextureMap[instance].file = file;
    }
    virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) override
    {
        mTextureMap[instance].rect = box;
    }
    virtual void SetTextureFlag(ObjectHandle instance, gfx::TexturePacker::TextureFlags flags, bool on_off) override
    {
        if (flags == gfx::TexturePacker::TextureFlags::CanCombine)
            mTextureMap[instance].can_be_combined = on_off;
        else if (flags == gfx::TexturePacker::TextureFlags::AllowedToPack)
            mTextureMap[instance].allowed_to_combine = on_off;
        else if (flags == gfx::TexturePacker::TextureFlags::AllowedToResize)
            mTextureMap[instance].allowed_to_resize = on_off;
        else BUG("Unhandled texture packing flag.");
    }
    virtual std::string GetPackedTextureId(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.file;
    }
    virtual gfx::FRect GetPackedTextureBox(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.rect;
    }

    using TexturePackingProgressCallback = std::function<void (std::string, int, int)>;

    void PackTextures(TexturePackingProgressCallback  progress, MyResourcePacker& packer)
    {
        if (mTextureMap.empty())
            return;

        if (!app::MakePath(app::JoinPath(kOutDir, "textures")))
        {
            ERROR("Failed to create texture directory. [dir='%1/%2']", kOutDir, "textures");
            mNumErrors++;
            return;
        }

        // OpenGL ES 2. defines the minimum required supported texture size to be
        // only 64x64 px which is not much. Anything bigger than that
        // is implementation specific. :p
        // for maximum portability we then just pretty much skip the whole packing.

        using PackList = std::vector<app::PackingRectangle>;

        struct TextureCategory {
            QImage::Format format;
            PackList sources;
        };

        // the source list of rectangles (images to pack)

        TextureCategory rgba_textures;
        TextureCategory rgb_textures;
        TextureCategory grayscale_textures;
        rgba_textures.format = QImage::Format::Format_RGBA8888;
        rgb_textures.format  = QImage::Format::Format_RGB888;
        grayscale_textures.format = QImage::Format::Format_Grayscale8;

        TextureCategory* all_textures[] = {
            &rgba_textures, &rgb_textures, &grayscale_textures
        };

        struct GeneratedTextureEntry {
            QString uri;
            // box of the texture that was packed
            // now inside the texture_file
            float xpos   = 0;
            float ypos   = 0;
            float width  = 0;
            float height = 0;
        };

        // map original file handle to a new generated texture entry
        // which defines either a box inside a generated texture pack
        // (combination of multiple textures) or a downscaled (originally large) texture.
        std::unordered_map<std::string, GeneratedTextureEntry> relocation_map;
        // map image URIs to URIs. If the image has been resampled
        // the source URI maps to a file in the /tmp. Otherwise, it maps to itself.
        std::unordered_map<std::string, QString> image_map;

        // 1. go over the list of textures, ignore duplicates
        // 2. if the texture is larger than max texture size resize it
        // 3. if the texture can be combined pick it for combining otherwise
        //    generate a texture entry and copy into output
        // then:
        // 4. combine the textures that have been selected for combining
        //    into atlas/atlasses.
        // -- composite the actual image files.
        // 5. copy the src image contents into the container image.
        // 6. write the container/packed image into the package folder
        // 7. update the textures whose source images were packaged
        //    - the file handle/URI needs to be remapped
        //    - and the rectangle box needs to be remapped

        int cur_step = 0;
        int max_step = static_cast<int>(mTextureMap.size());

        for (auto it = mTextureMap.begin(); it != mTextureMap.end(); ++it)
        {
            progress("Copying textures...", cur_step++, max_step);

            const TextureSource& tex = it->second;
            if (tex.file.empty())
                continue;
            else if (auto it = image_map.find(tex.file) != image_map.end())
                continue;

            const QFileInfo info(app::FromUtf8(tex.file));
            const QString src_file = info.absoluteFilePath();
            //const QImage src_pix(src_file);
            // QImage seems to lie about something or then the test pngs are produced
            // somehow wrong but an image that should have 24 bits for depth gets
            // reported as 32bit when QImage loads it.
            std::vector<char> img_data;
            if (!app::ReadBinaryFile(src_file, img_data))
            {
                ERROR("Failed to open image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }

            gfx::Image img;
            if (!img.Load(&img_data[0], img_data.size()))
            {
                ERROR("Failed to decompress image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }
            const auto width  = img.GetWidth();
            const auto height = img.GetHeight();
            const auto* data = img.GetData();
            QImage src_pix;
            if (img.GetDepthBits() == 8)
                src_pix = QImage((const uchar*)data, width, height, width*1, QImage::Format_Grayscale8);
            else if (img.GetDepthBits() == 24)
                src_pix = QImage((const uchar*)data, width, height, width*3, QImage::Format_RGB888);
            else if (img.GetDepthBits() == 32)
                src_pix = QImage((const uchar*)data, width, height, width*4, QImage::Format_RGBA8888);

            if (src_pix.isNull())
            {
                ERROR("Failed to load image file. [file='%1']", src_file);
                mNumErrors++;
                continue;
            }
            QString img_file = src_file;
            QString img_name = info.fileName();
            auto img_width  = src_pix.width();
            auto img_height = src_pix.height();
            auto img_depth  = src_pix.depth();
            DEBUG("Loading image file. [file='%1', width=%2, height=%3, depth=%4]", src_file, img_width, img_height, img_depth);

            if (!(img_depth == 32 || img_depth == 24 || img_depth == 8))
            {
                ERROR("Unsupported image format and depth. [file='%1', depth=%2]", src_file, img_depth);
                mNumErrors++;
                continue;
            }

            const bool too_large = img_width > kMaxTextureWidth ||
                                   img_height > kMaxTextureHeight;
            const bool can_resize = kResizeLargeTextures && tex.allowed_to_resize;
            const bool needs_resampling = too_large && can_resize;

            // first check if the source image needs to be resampled. if so
            // resample in and output into /tmp
            if (needs_resampling)
            {
                const auto scale = std::min(kMaxTextureWidth / (float)img_width,
                                            kMaxTextureHeight / (float)img_height);
                const auto dst_width  = img_width * scale;
                const auto dst_height = img_height * scale;
                QImage::Format format = QImage::Format::Format_Invalid;
                if (img_depth == 32)
                    format = QImage::Format::Format_RGBA8888;
                else if (img_depth == 24)
                    format = QImage::Format::Format_RGB888;
                else if (img_depth == 8)
                    format = QImage::Format::Format_Grayscale8;
                else BUG("Missed image bit depth support check.");

                QImage buffer(dst_width, dst_height, format);
                buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));
                QPainter painter(&buffer);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true); // bi-linear filtering
                const QRectF dst_rect(0, 0, dst_width, dst_height);
                const QRectF src_rect(0, 0, img_width, img_height);
                painter.drawImage(dst_rect, src_pix, src_rect);

                // create a scratch file into which write the re-sampled image file
                const QString& name = app::RandomString() + ".png";
                const QString temp  = app::JoinPath(QDir::tempPath(), name);
                QImageWriter writer;
                writer.setFormat("PNG");
                writer.setQuality(100);
                writer.setFileName(temp);
                if (!writer.write(buffer))
                {
                    ERROR("Failed to write temp image. [file='%1']", temp);
                    mNumErrors++;
                    continue;
                }
                DEBUG("Image was resampled and resized. [src='%1', dst='%2', width=%3, height=%4]", src_file, temp, img_width, img_height);
                img_width  = dst_width;
                img_height = dst_height;
                img_file   = temp;
                img_name   = info.baseName() + ".png";
                // map the input image to an image in /tmp/
                image_map[tex.file] = temp;
                mTempFiles.push_back(temp);
            }
            else
            {
                // the input image maps to itself since there's no
                // scratch image that is needed.
                image_map[tex.file] = app::FromUtf8(tex.file);
            }

            // check if the texture can be combined.
            if (kPackSmallTextures && tex.can_be_combined && tex.allowed_to_combine && img_width < kTexturePackWidth && img_height < kTexturePackHeight)
            {
                // add as a source for texture packing
                app::PackingRectangle rc;
                rc.width  = img_width + kTexturePadding * 2;
                rc.height = img_height + kTexturePadding * 2;
                rc.cookie = tex.file; // this is just used as an ID here.
                if (img_depth == 32)
                    rgba_textures.sources.push_back(rc);
                else if (img_depth == 24)
                    rgb_textures.sources.push_back(rc);
                else if (img_depth == 8)
                    grayscale_textures.sources.push_back(rc);
                else BUG("Missed image bit depth support check.");
            }
            else
            {
                // Generate a texture entry.
                GeneratedTextureEntry self;
                self.width  = 1.0f;
                self.height = 1.0f;
                self.xpos   = 0.0f;
                self.ypos   = 0.0f;
                self.uri    = packer.MapFileToPackage(packer.CopyFile(img_file, "textures", img_name));
                relocation_map[tex.file] = std::move(self);
            }
        } // for

        unsigned atlas_number = 0;
        cur_step = 0;
        max_step = static_cast<int>(grayscale_textures.sources.size() +
                                    rgb_textures.sources.size() +
                                    rgba_textures.sources.size());

        for (auto* texture_category : all_textures)
        {
            auto& sources = texture_category->sources;
            while (!sources.empty())
            {
                progress("Packing textures...", cur_step++, max_step);

                app::PackRectangles({kTexturePackWidth, kTexturePackHeight}, sources, nullptr);
                // ok, some textures might have failed to pack on this pass.
                // separate the ones that were successfully packed from the ones that
                // weren't. then composite the image for the success cases.
                auto first_success = std::partition(sources.begin(), sources.end(),
                    [](const auto& pack_rect) {
                        // put the failed cases first.
                        return pack_rect.success == false;
                });
                const auto num_to_pack = std::distance(first_success, sources.end());
                // we should have already dealt with too big images already.
                ASSERT(num_to_pack > 0);
                if (num_to_pack == 1)
                {
                    // if we can only fit 1 single image in the container
                    // then what's the point ?
                    // we'd just end up wasting space, so just leave it as is.
                    const auto& rc = *first_success;
                    ASSERT(image_map.find(rc.cookie) != image_map.end());
                    QString file = image_map[rc.cookie];

                    GeneratedTextureEntry gen;
                    gen.uri    = packer.MapFileToPackage(packer.CopyFile(file, "textures"));
                    gen.width  = 1.0f;
                    gen.height = 1.0f;
                    gen.xpos   = 0.0f;
                    gen.ypos   = 0.0f;
                    relocation_map[rc.cookie] = gen;
                    sources.erase(first_success);
                    continue;
                }

                // composition buffer.
                QImage buffer(kTexturePackWidth, kTexturePackHeight, texture_category->format);
                buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));

                QPainter painter(&buffer);
                painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as-is

                // do the composite pass.
                for (auto it = first_success; it != sources.end(); ++it)
                {
                    const auto& rc = *it;
                    ASSERT(rc.success);
                    const auto padded_width = rc.width;
                    const auto padded_height = rc.height;
                    const auto width = padded_width - kTexturePadding * 2;
                    const auto height = padded_height - kTexturePadding * 2;

                    ASSERT(image_map.find(rc.cookie) != image_map.end());
                    const QString img_file = image_map[rc.cookie];
                    const QFileInfo info(img_file);
                    const QString file(info.absoluteFilePath());
                    // compensate for possible texture sampling issues by padding the
                    // image with some extra pixels by growing it a few pixels on both
                    // axis.
                    const QRectF dst(rc.xpos, rc.ypos, padded_width, padded_height);
                    const QRectF src(0, 0, width, height);
                    const QPixmap img(file);
                    if (img.isNull())
                    {
                        ERROR("Failed to open texture packing image. [file='%1']", file);
                        mNumErrors++;
                    }
                    else
                    {
                        painter.drawPixmap(dst, img, src);
                    }
                }

                const QString& name = QString("Generated_%1.png").arg(atlas_number);
                const QString& file = app::JoinPath(app::JoinPath(kOutDir, "textures"), name);

                QImageWriter writer;
                writer.setFormat("PNG");
                writer.setQuality(100);
                writer.setFileName(file);
                if (!writer.write(buffer))
                {
                    ERROR("Failed to write image. [file='%1']", file);
                    mNumErrors++;
                }
                const float pack_width = kTexturePackWidth; //packing_rect_result.width;
                const float pack_height = kTexturePackHeight; //packing_rect_result.height;

                // create mapping for each source texture to the generated
                // texture.
                for (auto it = first_success; it != sources.end(); ++it)
                {
                    const auto& rc = *it;
                    const auto padded_width = rc.width;
                    const auto padded_height = rc.height;
                    const auto width = padded_width - kTexturePadding * 2;
                    const auto height = padded_height - kTexturePadding * 2;
                    const auto xpos = rc.xpos + kTexturePadding;
                    const auto ypos = rc.ypos + kTexturePadding;
                    GeneratedTextureEntry gen;
                    gen.uri    = app::toString("pck://textures/%1", name);
                    gen.width  = (float)width / pack_width;
                    gen.height = (float)height / pack_height;
                    gen.xpos   = (float)xpos / pack_width;
                    gen.ypos   = (float)ypos / pack_height;
                    relocation_map[rc.cookie] = gen;
                    DEBUG("New image packing entry. [id='%1', dst='%2']", rc.cookie, gen.uri);
                }

                // done with these.
                sources.erase(first_success, sources.end());

                atlas_number++;
            } // while (!sources.empty())
        }

        cur_step = 0;
        max_step = static_cast<int>(mTextureMap.size());
        // update texture object mappings, file handles and texture boxes.
        // for each texture object, look up where the original file handle
        // maps to. Then the original texture box is now a box within a box.
        for (auto& pair : mTextureMap)
        {
            progress("Remapping textures...", cur_step++, max_step);

            TextureSource& tex = pair.second;
            const auto original_file = tex.file;
            const auto original_rect = tex.rect;

            auto it = relocation_map.find(original_file);
            if (it == relocation_map.end()) // font texture sources only have texture box.
                continue;
            //ASSERT(it != relocation_map.end());
            const GeneratedTextureEntry& relocation = it->second;

            const auto original_rect_x = original_rect.GetX();
            const auto original_rect_y = original_rect.GetY();
            const auto original_rect_width  = original_rect.GetWidth();
            const auto original_rect_height = original_rect.GetHeight();

            tex.file = app::ToUtf8(relocation.uri);
            tex.rect = gfx::FRect(relocation.xpos + original_rect_x * relocation.width,
                                  relocation.ypos + original_rect_y * relocation.height,
                                  relocation.width * original_rect_width,
                                  relocation.height * original_rect_height);
        }
    }
    unsigned GetNumErrors() const
    { return mNumErrors; }
private:
    const QString kOutDir;
    const unsigned kMaxTextureHeight = 0;
    const unsigned kMaxTextureWidth = 0;
    const unsigned kTexturePackWidth = 0;
    const unsigned kTexturePackHeight = 0;
    const unsigned kTexturePadding = 0;
    const bool kResizeLargeTextures = true;
    const bool kPackSmallTextures = true;
    unsigned mNumErrors = 0;

    struct TextureSource {
        std::string file;
        gfx::FRect  rect;
        bool can_be_combined = true;
        bool allowed_to_resize = true;
        bool allowed_to_combine = true;
    };
    std::unordered_map<ObjectHandle, TextureSource> mTextureMap;
    std::vector<QString> mTempFiles;
};


} // namespace

namespace app
{
ResourceArchive::ResourceArchive()
{
    mZip.setAutoClose(true);
    mZip.setFileNameCodec("UTF-8");
    mZip.setUtf8Enabled(true);
    mZip.setZip64Enabled(true);
}

bool ResourceArchive::Open(const QString& zip_file)
{
    mFile.setFileName(zip_file);
    if (!mFile.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open zip file for reading. [file='%1', error='%2']", zip_file, mFile.errorString());
        return false;
    }
    mZip.setIoDevice(&mFile);
    if (!mZip.open(QuaZip::mdUnzip))
    {
        ERROR("QuaZip open failed. [code=%1]", mZip.getZipError());
        return false;
    }
    DEBUG("QuaZip open successful. [file='%1']", zip_file);
    mZip.goToFirstFile();
    do
    {
        QuaZipFileInfo info;
        if (mZip.getCurrentFileInfo(&info))
        {
            DEBUG("Found file in zip. [file='%1']", info.name);
        }
    } while (mZip.goToNextFile());

    QByteArray content_bytes;
    QByteArray property_bytes;
    if (!ReadFile("content.json", &content_bytes))
    {
        ERROR("Could not find content.json file in zip archive. [file='%1']", zip_file);
        return false;
    }
    if (!ReadFile("properties.json", &property_bytes))
    {
        ERROR("Could not find properties.json file in zip archive. [file='%1']", zip_file);
        return false;
    }
    data::JsonObject content;
    const auto [ok, error] = content.ParseString(content_bytes.constData(), content_bytes.size());
    if (!ok)
    {
        ERROR("Failed to parse JSON content. [error='%1']", error);
        return false;
    }

    LoadMaterials<gfx::MaterialClass>("materials", content, mResources);
    LoadResources<gfx::ParticleEngineClass>("particles", content, mResources);
    LoadResources<gfx::PolygonClass>("shapes", content, mResources);
    LoadResources<game::EntityClass>("entities", content, mResources);
    LoadResources<game::SceneClass>("scenes", content, mResources);
    LoadResources<game::TilemapClass>("tilemaps", content, mResources);
    LoadResources<Script>("scripts", content, mResources);
    LoadResources<DataFile>("data_files", content, mResources);
    LoadResources<audio::GraphClass>("audio_graphs", content, mResources);
    LoadResources<uik::Window>("uis", content, mResources);

    // load property JSONs
    const auto& docu  = QJsonDocument::fromJson(property_bytes);
    const auto& props = docu.object();
    for (auto& resource : mResources)
    {
        resource->LoadProperties(props);
    }

    // Partition the resources such that the data objects come in first.
    // This is done because some resources such as tilemaps refer to the
    // data resources by URI and in order for the URI remapping to work
    // the packer must have packed the data object before packing the
    // tilemap object.
    std::partition(mResources.begin(), mResources.end(),
        [](const auto& resource) {
            return resource->IsDataFile();
        });

    mZipFile = zip_file;
    return true;
}

bool ResourceArchive::ReadFile(const QString& file, QByteArray* array) const
{
    if (!FindZipFile(file))
        return false;
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::ReadOnly);
    *array = zip_file.readAll();
    zip_file.close();
    return true;
}

bool ResourceArchive::FindZipFile(const QString& unix_style_name) const
{
    if (!mZip.goToFirstFile())
        return false;
    // on Windows the zip file paths are also windows style. (why, but of course)
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

Workspace::Workspace(const QString& dir)
  : mWorkspaceDir(FixWorkspacePath(dir))
{
    DEBUG("Create workspace");

    // initialize the primitive resources, i.e the materials
    // and drawables that are part of the workspace without any
    // user interaction.

    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    auto checkerboard = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Texture, std::string("_checkerboard"));
    checkerboard->SetTexture(gfx::LoadTextureFromFile("app://textures/Checkerboard.png"));
    checkerboard->SetName("Checkerboard");
    mResources.emplace_back(new MaterialResource(checkerboard, "Checkerboard"));

    // add some primitive colors.
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto color = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, "_" + color_name);
        color->SetBaseColor(val);
        color->SetName("_" + color_name);
        color->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mResources.emplace_back(new MaterialResource(color, color_name));
    }

    // setup primitive drawables with known/fixed class IDs
    // these IDs are also hardcoded in the engine/loader.cpp which uses
    // these same IDs to create primitive resources.
    mResources.emplace_back(new DrawableResource<gfx::CapsuleClass>(gfx::CapsuleClass("_capsule"), "Capsule"));
    mResources.emplace_back(new DrawableResource<gfx::RectangleClass>(gfx::RectangleClass("_rect"), "Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::IsoscelesTriangleClass>(gfx::IsoscelesTriangleClass("_isosceles_triangle"), "IsoscelesTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::RightTriangleClass>(gfx::RightTriangleClass("_right_triangle"), "RightTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::CircleClass>(gfx::CircleClass("_circle"), "Circle"));
    mResources.emplace_back(new DrawableResource<gfx::SemiCircleClass>(gfx::SemiCircleClass("_semi_circle"), "SemiCircle"));
    mResources.emplace_back(new DrawableResource<gfx::TrapezoidClass>(gfx::TrapezoidClass("_trapezoid"), "Trapezoid"));
    mResources.emplace_back(new DrawableResource<gfx::ParallelogramClass>(gfx::ParallelogramClass("_parallelogram"), "Parallelogram"));
    mResources.emplace_back(new DrawableResource<gfx::RoundRectangleClass>(gfx::RoundRectangleClass("_round_rect", "", 0.05f), "RoundRect"));
    mResources.emplace_back(new DrawableResource<gfx::ArrowCursorClass>(gfx::ArrowCursorClass("_arrow_cursor"), "Arrow Cursor"));
    mResources.emplace_back(new DrawableResource<gfx::BlockCursorClass>(gfx::BlockCursorClass("_block_cursor"), "Block Cursor"));

    for (auto& resource : mResources)
    {
        resource->SetIsPrimitive(true);
    }
    mSettings.application_identifier = app::RandomString();
}

Workspace::~Workspace()
{
    DEBUG("Destroy workspace");
}

QVariant Workspace::data(const QModelIndex& index, int role) const
{
    ASSERT(index.model() == this);
    ASSERT(index.row() < mResources.size());
    const auto& res = mResources[index.row()];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);
    else if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0: return toString(res->GetType());
            case 1: return res->GetName();
        }
    }
    else if (role == Qt::DecorationRole && index.column() == 0)
    {
        return res->GetIcon();
    }
    return QVariant();
}

QVariant Workspace::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case 0:  return "Type";
            case 1:  return "Name";
        }
    }
    return QVariant();
}

QAbstractFileEngine* Workspace::create(const QString& file) const
{
    // CAREFUL ABOUT RECURSION HERE.
    // DO NOT CALL QFile, QFileInfo or QDir !

    // only handle our special cases.
    QString ret = file;
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (file.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (file.startsWith("fs://"))
        ret.remove(0, 5);
    else return nullptr;

    DEBUG("Mapping Qt file '%1' => '%2'", file, ret);

    return new QFSFileEngine(ret);
}

std::unique_ptr<gfx::Material> Workspace::MakeMaterialByName(const AnyString& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClassByName(name));

}
std::unique_ptr<gfx::Drawable> Workspace::MakeDrawableByName(const AnyString& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClassByName(name));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetName() != name)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetId() != id)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}


std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassByName(const AnyString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}
std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}

std::shared_ptr<const game::TilemapClass> Workspace::GetTilemapClassById(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Tilemap)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::TilemapClass>(*resource).GetSharedResource();
    }
    BUG("No such tilemap class.");
    return nullptr;
}


engine::ClassHandle<const audio::GraphClass> Workspace::FindAudioGraphClassById(const std::string& id) const
{
    return FindClassHandleById<audio::GraphClass>(id, Resource::Type::AudioGraph);
}
engine::ClassHandle<const audio::GraphClass> Workspace::FindAudioGraphClassByName(const std::string& name) const
{
    return FindClassHandleByName<audio::GraphClass>(name, Resource::Type::AudioGraph);
}
engine::ClassHandle<const uik::Window> Workspace::FindUIByName(const std::string& name) const
{
    return FindClassHandleByName<uik::Window>(name, Resource::Type::UI);
}
engine::ClassHandle<const uik::Window> Workspace::FindUIById(const std::string& id) const
{
    return FindClassHandleById<uik::Window>(id, Resource::Type::UI);
}
engine::ClassHandle<const gfx::MaterialClass> Workspace::FindMaterialClassByName(const std::string& name) const
{
    return FindClassHandleByName<gfx::MaterialClass>(name, Resource::Type::Material);
}
engine::ClassHandle<const gfx::MaterialClass> Workspace::FindMaterialClassById(const std::string& klass) const
{
    return FindClassHandleById<gfx::MaterialClass>(klass, Resource::Type::Material);
}
engine::ClassHandle<const gfx::DrawableClass> Workspace::FindDrawableClassById(const std::string& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetIdUtf8() != klass)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::ParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    return nullptr;
}
engine::ClassHandle<const game::EntityClass> Workspace::FindEntityClassByName(const std::string& name) const
{
    return FindClassHandleByName<game::EntityClass>(name, Resource::Type::Entity);
}
engine::ClassHandle<const game::EntityClass> Workspace::FindEntityClassById(const std::string& id) const
{
    return FindClassHandleById<game::EntityClass>(id, Resource::Type::Entity);
}

engine::ClassHandle<const game::SceneClass> Workspace::FindSceneClassByName(const std::string& name) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetNameUtf8() != name)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
        break;
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& placement = ret->GetPlacement(i);
        auto klass = FindEntityClassById(placement.GetEntityId());
        if (!klass)
        {
            WARN("Scene entity placement entity class not found. [scene='%1', placement='%2']", ret->GetName(), placement.GetName());
            placement.ResetEntity();
            placement.ResetEntityParams();
        }
        else
        {
            placement.SetEntity(klass);
        }
    }
    return ret;
}
engine::ClassHandle<const game::SceneClass> Workspace::FindSceneClassById(const std::string& id) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetIdUtf8() != id)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& placement = ret->GetPlacement(i);
        auto klass = FindEntityClassById(placement.GetEntityId());
        if (!klass)
        {
            WARN("Scene entity placement entity class not found. [scene='%1', placement='%2']", ret->GetName(), placement.GetName());
            placement.ResetEntity();
            placement.ResetEntityParams();
        }
        else
        {
            placement.SetEntity(klass);
        }
    }
    return ret;
}

engine::ClassHandle<const game::TilemapClass> Workspace::FindTilemapClassById(const std::string& id) const
{
    return FindClassHandleById<game::TilemapClass>(id, Resource::Type::Tilemap);
}

engine::EngineDataHandle Workspace::LoadEngineDataUri(const std::string& URI) const
{
    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    return EngineBuffer::LoadFromFile(file);
}

engine::EngineDataHandle Workspace::LoadEngineDataFile(const std::string& filename) const
{
    return EngineBuffer::LoadFromFile(app::FromUtf8(filename));
}
engine::EngineDataHandle Workspace::LoadEngineDataId(const std::string& id) const
{
    for (size_t i=0; i < mUserResourceCount; ++i)
    {
        const auto& resource = mResources[i];
        if (resource->GetIdUtf8() != id)
            continue;

        std::string uri;
        if (resource->IsDataFile())
        {
            const DataFile* data = nullptr;
            resource->GetContent(&data);
            uri = data->GetFileURI();
        }
        else if (resource->IsScript())
        {
            const Script* script = nullptr;
            resource->GetContent(&script);
            uri = script->GetFileURI();
        }
        else BUG("Unknown ID in engine data loading.");
        const auto& file = MapFileToFilesystem(uri);
        DEBUG("URI '%1' => '%2'", uri, file);
        return EngineBuffer::LoadFromFile(file, resource->GetName());
    }
    return nullptr;
}

gfx::ResourceHandle Workspace::LoadResource(const std::string& URI)
{
    if (base::StartsWith(URI, "app://"))
        return LoadAppResource(URI);

    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    auto ret = GraphicsBuffer::LoadFromFile(file);
    return ret;
}

audio::SourceStreamHandle Workspace::OpenAudioStream(const std::string& URI,
    AudioIOStrategy strategy, bool enable_file_caching) const
{
    const auto& file = MapFileToFilesystem(URI);
    DEBUG("URI '%1' => '%2'", URI, file);
    return audio::OpenFileStream(app::ToUtf8(file), strategy, enable_file_caching);
}

game::TilemapDataHandle Workspace::LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const
{
    const auto& file = MapFileToFilesystem(desc.uri);
    DEBUG("URI '%1' => '%2'", desc.uri, file);
    if (desc.read_only)
        return TilemapMemoryMap::OpenFilemap(file);

    return TilemapBuffer::LoadFromFile(file);
}

// static
gfx::ResourceHandle Workspace::LoadAppResource(const std::string& URI)
{
    // static map of resources that are part of the application, i.e.
    // app://something. They're not expected to change.
    static std::unordered_map<std::string,
            std::shared_ptr<const GraphicsBuffer>> application_resources;

    auto it = application_resources.find(URI);
    if (it != application_resources.end())
        return it->second;

    QString file = app::FromUtf8(URI);
    file = CleanPath(file.replace("app://", GetAppDir()));

    auto ret = GraphicsBuffer::LoadFromFile(file);
    application_resources[URI] = ret;
    return ret;
}

bool Workspace::LoadWorkspace(MigrationLog* log)
{
    if (!LoadContent(JoinPath(mWorkspaceDir, "content.json"), log) ||
        !LoadProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // we don't really care if this fails or not. nothing permanently
    // important should be stored in this file. I.e deleting it
    // will just make the application forget some data that isn't
    // crucial for the operation of the application or for the
    // integrity of the workspace and its content.
    LoadUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    INFO("Loaded workspace '%1'", mWorkspaceDir);
    return true;
}

bool Workspace::SaveWorkspace()
{
    if (!SaveContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !SaveProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // should we notify the user if this fails or do we care?
    SaveUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    INFO("Saved workspace '%1'", mWorkspaceDir);
    return true;
}

QString Workspace::GetName() const
{
    return mWorkspaceDir;
}

QString Workspace::GetDir() const
{
    return mWorkspaceDir;
}

QString Workspace::GetSubDir(const QString& dir, bool create) const
{
    const auto& path = JoinPath(mWorkspaceDir, dir);

    if (create)
    {
        QDir d;
        d.setPath(path);
        if (d.exists())
            return path;
        if (!d.mkpath(path))
        {
            ERROR("Failed to create workspace sub directory. [path='%1']", path);
        }
    }
    return path;
}

AnyString Workspace::MapFileToWorkspace(const AnyString& name) const
{
    const QString filepath = name;
    if (filepath.isEmpty())
        return AnyString("");

    // don't remap already mapped files.
    if (filepath.startsWith("app://") ||
        filepath.startsWith("fs://") ||
        filepath.startsWith("ws://"))
        return filepath;

    // when the user is adding resource files to this project (workspace) there's the problem
    // of making the workspace "portable".
    // Portable here means two things:
    // * portable from one operating system to another (from Windows to Linux or vice versa)
    // * portable from one user's environment to another user's environment even when on
    //   the same operating system (i.e. from Windows to Windows or Linux to Linux)
    //
    // Using relative file paths (as opposed to absolute file paths) solves the problem
    // but then the problem is that the relative paths need to be resolved at runtime
    // and also some kind of "landmark" is needed in order to make the files
    // relative something to.
    //
    // The expectation is that during content creation most of the game resources
    // would be placed in a place relative to the workspace. In this case the
    // runtime path resolution would use paths relative to the current workspace.
    // However there's also some content (such as the pre-built shaders) that
    // is bundled with the Editor application and might be used from that location
    // as a game resource.

    // We could always copy the files into some location under the workspace to
    // solve this problem but currently this is not yet done.

    // if the file is in the current workspace path or in the path of the current executable
    // we can then express this path as a relative path.
    const auto& appdir = GetAppDir();
    const auto& file = CleanPath(filepath);

    if (file.startsWith(mWorkspaceDir))
    {
        QString ret = file;
        ret.remove(0, mWorkspaceDir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("ws://%1").arg(ret);
    }
    else if (file.startsWith(appdir))
    {
        QString ret = file;
        ret.remove(0, appdir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("app://%1").arg(ret);
    }
    // mapping other paths to identity. will not be portable to another
    // user's computer to another system, unless it's accessible on every
    // machine using the same path (for example a shared file system mount)
    return toString("fs://%1", file);
}

AnyString Workspace::MapFileToFilesystem(const AnyString& uri) const
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.

    QString ret = uri;
    if (ret.startsWith("ws://"))
        ret = CleanPath(ret.replace("ws://", mWorkspaceDir));
    else if (ret.startsWith("app://"))
        ret = CleanPath(ret.replace("app://", GetAppDir()));
    else if (ret.startsWith("fs://"))
        ret.remove(0, 5);

    // return as is
    return ret;
}

bool Workspace::LoadContent(const QString& filename, MigrationLog* log)
{
    data::JsonFile file;
    const auto [json_ok, error] = file.Load(app::ToUtf8(filename));
    if (!json_ok)
    {
        ERROR("Failed to load workspace JSON content file. [file='%1', error='%2']", filename, error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    bool ok = true;
    ok &= LoadMaterials<gfx::MaterialClass>("materials", root, mResources, log);
    ok &= LoadResources<gfx::ParticleEngineClass>("particles", root, mResources, log);
    ok &= LoadResources<gfx::PolygonClass>("shapes", root, mResources, log);
    ok &= LoadResources<game::EntityClass>("entities", root, mResources, log);
    ok &= LoadResources<game::SceneClass>("scenes", root, mResources, log);
    ok &= LoadResources<game::TilemapClass>("tilemaps", root, mResources, log);
    ok &= LoadResources<Script>("scripts", root, mResources, log);
    ok &= LoadResources<DataFile>("data_files", root, mResources, log);
    ok &= LoadResources<audio::GraphClass>("audio_graphs", root, mResources, log);
    ok &= LoadResources<uik::Window>("uis", root, mResources, log);

    // create an invariant that states that the primitive materials
    // are in the list of resources after the user defined ones.
    // this way the addressing scheme (when user clicks on an item
    // in the list of resources) doesn't need to change, and it's possible
    // to easily limit the items to be displayed only to those that are
    // user defined.
    auto primitives_start = std::stable_partition(mResources.begin(), mResources.end(),
        [](const auto& resource)  {
            return resource->IsPrimitive() == false;
        });
    mUserResourceCount = std::distance(mResources.begin(), primitives_start);

    // Invoke resource migration hook that allows us to perform one-off
    // activities when the underlying data has changed between different
    // data versions.
    for (auto& res : mResources)
    {
        if (!res->IsPrimitive())
            res->Migrate(log);
    }

    INFO("Loaded content file '%1'", filename);
    return true;
}

bool Workspace::SaveContent(const QString& filename) const
{
    data::JsonObject root;
    for (const auto& resource : mResources)
    {
        // skip persisting primitive resources since they're always
        // created as part of the workspace creation and their resource
        // IDs are fixed.
        if (resource->IsPrimitive())
            continue;
        // serialize the user defined resource.
        resource->Serialize(root);
    }
    data::JsonFile file;
    file.SetRootObject(root);
    const auto [ok, error] = file.Save(app::ToUtf8(filename));
    if (!ok)
    {
        ERROR("Failed to save JSON content file. [file='%1']", filename);
        return false;
    }
    INFO("Saved workspace content in '%1'", filename);
    return true;
}

bool Workspace::SaveProperties(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open properties file for writing. [file='%1']", filename);
        return false;
    }

    // our JSON root object
    QJsonObject json;

    QJsonObject project;
    JsonWrite(project, "multisample_sample_count", mSettings.multisample_sample_count);
    JsonWrite(project, "application_identifier"  , mSettings.application_identifier);
    JsonWrite(project, "application_name"        , mSettings.application_name);
    JsonWrite(project, "application_version"     , mSettings.application_version);
    JsonWrite(project, "application_library_win" , mSettings.application_library_win);
    JsonWrite(project, "application_library_lin" , mSettings.application_library_lin);
    JsonWrite(project, "debug_font"              , mSettings.debug_font);
    JsonWrite(project, "debug_show_fps"          , mSettings.debug_show_fps);
    JsonWrite(project, "debug_show_msg"          , mSettings.debug_show_msg);
    JsonWrite(project, "debug_draw"              , mSettings.debug_draw);
    JsonWrite(project, "debug_print_fps"         , mSettings.debug_print_fps);
    JsonWrite(project, "logging_debug"           , mSettings.log_debug);
    JsonWrite(project, "logging_warn"            , mSettings.log_debug);
    JsonWrite(project, "logging_info"            , mSettings.log_debug);
    JsonWrite(project, "logging_error"           , mSettings.log_debug);
    JsonWrite(project, "default_min_filter"      , mSettings.default_min_filter);
    JsonWrite(project, "default_mag_filter"      , mSettings.default_mag_filter);
    JsonWrite(project, "webgl_power_preference"  , mSettings.webgl_power_preference);
    JsonWrite(project, "webgl_antialias"         , mSettings.webgl_antialias);
    JsonWrite(project, "html5_developer_ui"      , mSettings.html5_developer_ui);
    JsonWrite(project, "canvas_mode"             , mSettings.canvas_mode);
    JsonWrite(project, "canvas_width"            , mSettings.canvas_width);
    JsonWrite(project, "canvas_height"           , mSettings.canvas_height);
    JsonWrite(project, "window_mode"             , mSettings.window_mode);
    JsonWrite(project, "window_width"            , mSettings.window_width);
    JsonWrite(project, "window_height"           , mSettings.window_height);
    JsonWrite(project, "window_can_resize"       , mSettings.window_can_resize);
    JsonWrite(project, "window_has_border"       , mSettings.window_has_border);
    JsonWrite(project, "window_vsync"            , mSettings.window_vsync);
    JsonWrite(project, "window_cursor"           , mSettings.window_cursor);
    JsonWrite(project, "config_srgb"             , mSettings.config_srgb);
    JsonWrite(project, "grab_mouse"              , mSettings.grab_mouse);
    JsonWrite(project, "save_window_geometry"    , mSettings.save_window_geometry);
    JsonWrite(project, "ticks_per_second"        , mSettings.ticks_per_second);
    JsonWrite(project, "updates_per_second"      , mSettings.updates_per_second);
    JsonWrite(project, "working_folder"          , mSettings.working_folder);
    JsonWrite(project, "command_line_arguments"  , mSettings.command_line_arguments);
    JsonWrite(project, "use_gamehost_process"    , mSettings.use_gamehost_process);
    JsonWrite(project, "enable_physics"          , mSettings.enable_physics);
    JsonWrite(project, "num_velocity_iterations" , mSettings.num_velocity_iterations);
    JsonWrite(project, "num_position_iterations" , mSettings.num_position_iterations);
    JsonWrite(project, "phys_gravity_x"          , mSettings.physics_gravity.x);
    JsonWrite(project, "phys_gravity_y"          , mSettings.physics_gravity.y);
    JsonWrite(project, "phys_scale_x"            , mSettings.physics_scale.x);
    JsonWrite(project, "phys_scale_y"            , mSettings.physics_scale.y);
    JsonWrite(project, "game_viewport_width"     , mSettings.viewport_width);
    JsonWrite(project, "game_viewport_height"    , mSettings.viewport_height);
    JsonWrite(project, "clear_color"             , mSettings.clear_color);
    JsonWrite(project, "mouse_pointer_material"  , mSettings.mouse_pointer_material);
    JsonWrite(project, "mouse_pointer_drawable"  , mSettings.mouse_pointer_drawable);
    JsonWrite(project, "mouse_pointer_visible"   , mSettings.mouse_pointer_visible);
    JsonWrite(project, "mouse_pointer_hotspot"   , mSettings.mouse_pointer_hotspot);
    JsonWrite(project, "mouse_pointer_size"      , mSettings.mouse_pointer_size);
    JsonWrite(project, "mouse_pointer_units"     , mSettings.mouse_pointer_units);
    JsonWrite(project, "game_script"             , mSettings.game_script);
    JsonWrite(project, "audio_channels"          , mSettings.audio_channels);
    JsonWrite(project, "audio_sample_rate"       , mSettings.audio_sample_rate);
    JsonWrite(project, "audio_sample_type"       , mSettings.audio_sample_type);
    JsonWrite(project, "audio_buffer_size"       , mSettings.audio_buffer_size);
    JsonWrite(project, "enable_audio_pcm_caching", mSettings.enable_audio_pcm_caching);
    JsonWrite(project, "desktop_audio_io_strategy", mSettings.desktop_audio_io_strategy);
    JsonWrite(project, "wasm_audio_io_strategy"  , mSettings.wasm_audio_io_strategy);
    JsonWrite(project, "preview_entity_script"   , mSettings.preview_entity_script);
    JsonWrite(project, "preview_scene_script"    , mSettings.preview_scene_script);

    // serialize the workspace properties into JSON
    json["workspace"] = QJsonObject::fromVariantMap(mProperties);
    json["project"]   = project;

    // serialize the properties stored in each and every
    // resource object.
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveProperties(json);
    }
    // set the root object to the json document then serialize
    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();

    INFO("Saved workspace data in '%1'", filename);
    return true;
}

void Workspace::SaveUserSettings(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1' for writing. (%2)", filename, file.error());
        return;
    }
    QJsonObject json;
    json["user"] = QJsonObject::fromVariantMap(mUserProperties);
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveUserProperties(json);
    }

    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();
    INFO("Saved private workspace data in '%1'", filename);
}

bool Workspace::LoadProperties(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    const auto& buff = file.readAll(); // QByteArray

    QJsonDocument docu(QJsonDocument::fromJson(buff));

    const QJsonObject& project = docu["project"].toObject();
    JsonReadSafe(project, "multisample_sample_count", &mSettings.multisample_sample_count);
    JsonReadSafe(project, "application_identifier",   &mSettings.application_identifier);
    JsonReadSafe(project, "application_name",         &mSettings.application_name);
    JsonReadSafe(project, "application_version",      &mSettings.application_version);
    JsonReadSafe(project, "application_library_win",  &mSettings.application_library_win);
    JsonReadSafe(project, "application_library_lin",  &mSettings.application_library_lin);
    JsonReadSafe(project, "debug_font"              , &mSettings.debug_font);
    JsonReadSafe(project, "debug_show_fps"          , &mSettings.debug_show_fps);
    JsonReadSafe(project, "debug_show_msg"          , &mSettings.debug_show_msg);
    JsonReadSafe(project, "debug_draw"              , &mSettings.debug_draw);
    JsonReadSafe(project, "debug_print_fps"         , &mSettings.debug_print_fps);
    JsonReadSafe(project, "logging_debug"           , &mSettings.log_debug);
    JsonReadSafe(project, "logging_warn"            , &mSettings.log_debug);
    JsonReadSafe(project, "logging_info"            , &mSettings.log_debug);
    JsonReadSafe(project, "logging_error"           , &mSettings.log_debug);
    JsonReadSafe(project, "default_min_filter",       &mSettings.default_min_filter);
    JsonReadSafe(project, "default_mag_filter",       &mSettings.default_mag_filter);
    JsonReadSafe(project, "webgl_power_preference"  , &mSettings.webgl_power_preference);
    JsonReadSafe(project, "webgl_antialias"         , &mSettings.webgl_antialias);
    JsonReadSafe(project, "html5_developer_ui"      , &mSettings.html5_developer_ui);
    JsonReadSafe(project, "canvas_mode"             , &mSettings.canvas_mode);
    JsonReadSafe(project, "canvas_width"            , &mSettings.canvas_width);
    JsonReadSafe(project, "canvas_height"           , &mSettings.canvas_height);
    JsonReadSafe(project, "window_mode",              &mSettings.window_mode);
    JsonReadSafe(project, "window_width",             &mSettings.window_width);
    JsonReadSafe(project, "window_height",            &mSettings.window_height);
    JsonReadSafe(project, "window_can_resize",        &mSettings.window_can_resize);
    JsonReadSafe(project, "window_has_border",        &mSettings.window_has_border);
    JsonReadSafe(project, "window_vsync",             &mSettings.window_vsync);
    JsonReadSafe(project, "window_cursor",            &mSettings.window_cursor);
    JsonReadSafe(project, "config_srgb",              &mSettings.config_srgb);
    JsonReadSafe(project, "grab_mouse",               &mSettings.grab_mouse);
    JsonReadSafe(project, "save_window_geometry",     &mSettings.save_window_geometry);
    JsonReadSafe(project, "ticks_per_second",         &mSettings.ticks_per_second);
    JsonReadSafe(project, "updates_per_second",       &mSettings.updates_per_second);
    JsonReadSafe(project, "working_folder",           &mSettings.working_folder);
    JsonReadSafe(project, "command_line_arguments",   &mSettings.command_line_arguments);
    JsonReadSafe(project, "use_gamehost_process",     &mSettings.use_gamehost_process);
    JsonReadSafe(project, "enable_physics",           &mSettings.enable_physics);
    JsonReadSafe(project, "num_position_iterations",  &mSettings.num_position_iterations);
    JsonReadSafe(project, "num_velocity_iterations",  &mSettings.num_velocity_iterations);
    JsonReadSafe(project, "phys_gravity_x",           &mSettings.physics_gravity.x);
    JsonReadSafe(project, "phys_gravity_y",           &mSettings.physics_gravity.y);
    JsonReadSafe(project, "phys_scale_x",             &mSettings.physics_scale.x);
    JsonReadSafe(project, "phys_scale_y",             &mSettings.physics_scale.y);
    JsonReadSafe(project, "game_viewport_width",      &mSettings.viewport_width);
    JsonReadSafe(project, "game_viewport_height",     &mSettings.viewport_height);
    JsonReadSafe(project, "clear_color",              &mSettings.clear_color);
    JsonReadSafe(project, "mouse_pointer_material",   &mSettings.mouse_pointer_material);
    JsonReadSafe(project, "mouse_pointer_drawable",   &mSettings.mouse_pointer_drawable);
    JsonReadSafe(project, "mouse_pointer_visible",    &mSettings.mouse_pointer_visible);
    JsonReadSafe(project, "mouse_pointer_hotspot",    &mSettings.mouse_pointer_hotspot);
    JsonReadSafe(project, "mouse_pointer_size",       &mSettings.mouse_pointer_size);
    JsonReadSafe(project, "mouse_pointer_units",      &mSettings.mouse_pointer_units);
    JsonReadSafe(project, "game_script",              &mSettings.game_script);
    JsonReadSafe(project, "audio_channels",           &mSettings.audio_channels);
    JsonReadSafe(project, "audio_sample_rate",        &mSettings.audio_sample_rate);
    JsonReadSafe(project, "audio_sample_type",        &mSettings.audio_sample_type);
    JsonReadSafe(project, "audio_buffer_size",        &mSettings.audio_buffer_size);
    JsonReadSafe(project, "enable_audio_pcm_caching", &mSettings.enable_audio_pcm_caching);
    JsonReadSafe(project, "desktop_audio_io_strategy", &mSettings.desktop_audio_io_strategy);
    JsonReadSafe(project, "wasm_audio_io_strategy"  , &mSettings.wasm_audio_io_strategy);
    JsonReadSafe(project, "preview_entity_script"   , &mSettings.preview_entity_script);
    JsonReadSafe(project, "preview_scene_script"    , &mSettings.preview_scene_script);

    // load the workspace properties.
    mProperties = docu["workspace"].toObject().toVariantMap();

    // so we expect that the content has been loaded first.
    // and then ask each resource object to load its additional
    // properties from the workspace file.
    for (auto& resource : mResources)
    {
        resource->LoadProperties(docu.object());
    }

    INFO("Loaded workspace file '%1'", filename);
    return true;
}

void Workspace::LoadUserSettings(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        WARN("Failed to open: '%1' (%2)", filename, file.error());
        return;
    }
    const auto& buff = file.readAll(); // QByteArray
    const QJsonDocument docu(QJsonDocument::fromJson(buff));
    mUserProperties = docu["user"].toObject().toVariantMap();

    for (auto& resource : mResources)
    {
        resource->LoadUserProperties(docu.object());
    }

    INFO("Loaded private workspace data: '%1'", filename);
}

Workspace::ResourceList Workspace::ListAllMaterials() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveMaterials());
    base::AppendVector(list, ListUserDefinedMaterials());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveMaterials() const
{
    return ListResources(Resource::Type::Material, true, true);
}

Workspace::ResourceList Workspace::ListUserDefinedMaps() const
{
    return ListResources(Resource::Type::Tilemap, false, true);
}

Workspace::ResourceList Workspace::ListUserDefinedScripts() const
{
    return ListResources(Resource::Type::Script, false, true);
}

Workspace::ResourceList Workspace::ListUserDefinedMaterials() const
{
    return ListResources(Resource::Type::Material, false, true);
}

Workspace::ResourceList Workspace::ListAllDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveDrawables());
    base::AppendVector(list, ListUserDefinedDrawables());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, true, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, true, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, true, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, false, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, false, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, false, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedEntities() const
{
    return ListResources(Resource::Type::Entity, false, true);
}

QStringList Workspace::ListUserDefinedEntityIds() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (!resource->IsEntity())
            continue;
        list.append(resource->GetId());
    }
    return list;
}

Workspace::ResourceList Workspace::ListResources(Resource::Type type, bool primitive, bool sort) const
{
    ResourceList list;
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive() == primitive &&
            resource->GetType() == type)
        {
            ResourceListItem item;
            item.name     = resource->GetName();
            item.id       = resource->GetId();
            item.resource = resource.get();
            list.push_back(item);
        }
    }
    if (sort)
    {
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    }
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedResources() const
{
    ResourceList ret;

    for (size_t i=0; i<mUserResourceCount; ++i)
    {
        const auto& resource = mResources[i];

        ResourceListItem item;
        item.name = resource->GetName();
        item.id   = resource->GetId();
        item.icon = resource->GetIcon();
        item.resource = resource.get();
        ret.push_back(item);
    }
    return ret;
}

Workspace::ResourceList Workspace::ListCursors() const
{
    ResourceList list;
    ResourceListItem arrow;
    arrow.name = "Arrow Cursor";
    arrow.id   = "_arrow_cursor";
    arrow.resource = FindResourceById("_arrow_cursor");
    list.push_back(arrow);
    ResourceListItem block;
    block.name = "Block Cursor";
    block.id   = "_block_cursor";
    block.resource = FindResourceById("_block_cursor");
    list.push_back(block);
    return list;
}

Workspace::ResourceList Workspace::ListDataFiles() const
{
    return ListResources(Resource::Type::DataFile, false, false);
}

ResourceList Workspace::ListDependencies(const ModelIndexList& indices) const
{
    std::unordered_map<QString, Resource*> resource_map;
    for (size_t i=0; i < mUserResourceCount; ++i)
    {
        auto& res = mResources[i];
        resource_map[res->GetId()] = res.get();
    }

    std::unordered_set<QString> unique_ids;

    for (size_t index : indices)
    {
        const auto& res = mResources[index];
        QStringList deps = res->ListDependencies();

        std::stack<QString> stack;
        for (auto id : deps)
            stack.push(id);

        while (!stack.empty())
        {
            auto top_id = stack.top();
            auto* resource = resource_map[top_id];
            // if it's a primitive resource then we're not going to find it here
            // and there's no need to explore it
            if (resource == nullptr)
            {
                stack.pop();
                continue;
            }
            // if we've already seen this resource we can skip
            // exploring from here.
            if (base::Contains(unique_ids, top_id))
            {
                stack.pop();
                continue;
            }

            unique_ids.insert(top_id);

            deps = resource->ListDependencies();
            for (auto id : deps)
                stack.push(id);
        }
    }

    ResourceList ret;
    for (const auto& id : unique_ids)
    {
        auto* res = resource_map[id];

        ResourceListItem item;
        item.name = res->GetName();
        item.id   = res->GetId();
        item.icon = res->GetIcon();
        item.resource = res;
        ret.push_back(item);
    }
    return ret;
}

ResourceList Workspace::ListResourceUsers(const ModelIndexList& list) const
{
    ResourceList users;

    // The dependency graph goes only one way from user -> dependant.
    // this means that right now to go the other way.
    // in order to make this operation run faster we'd need to track the
    // relationship the other way too.
    // This could be done either when the resource is saved or in the background
    // in the Workspace tick (or something)

    for (size_t i=0; i<mUserResourceCount; ++i)
    {
        // take a resource and find its deps
        const auto& current_res = mResources[i];
        const auto& current_deps = ListDependencies(i); // << warning this is the slow/heavy OP !

        // if the deps include any of the resources listed as args then this
        // current resource is a user.
        for (const auto& dep : current_deps)
        {
            bool found_match = false;

            for (auto index : list)
            {
                if (mResources[index]->GetId() == dep.id)
                {
                    found_match = true;
                    break;
                }
            }
            if (found_match)
            {
                ResourceListItem ret;
                ret.id = current_res->GetId();
                ret.name = current_res->GetName();
                ret.icon = current_res->GetIcon();
                ret.resource = current_res.get();
                users.push_back(ret);
            }
        }
    }
    return users;
}


QStringList Workspace::ListFileResources(const ModelIndexList& indices) const
{
    // setup a phoney resource packer that actually does no packing
    // but only captures all the resource URIs
    class Phoney : public ResourcePacker {
    public:
        explicit Phoney(const QString& workspace)
          : mWorkspaceDir(workspace)
        {}

        virtual void CopyFile(const app::AnyString& uri, const std::string& dir) override
        {
            RecordURI(uri);
        }

        virtual void WriteFile(const app::AnyString& uri, const std::string& dir, const void* data, size_t len) override
        {
            RecordURI(uri);
        }
        virtual bool ReadFile(const app::AnyString& uri, QByteArray* bytes) const override
        {
            return app::detail::LoadArrayBuffer(MapWorkspaceUri(uri, mWorkspaceDir), bytes);
        }
        virtual app::AnyString MapUri(const app::AnyString& uri) const override
        { return uri; }
        virtual bool HasMapping(const app::AnyString& uri) const override
        { return true; }

        QStringList ListUris() const
        {
            QStringList ret;
            for (const auto& uri : mURIs)
                ret.push_back(uri);
            return ret;
        }
        void RecordURI(const app::AnyString& uri)
        {
            // hack for now to copy the bitmap font image.
            // this will not work if:
            // - the file extension is not .png
            // - the file name is same as the .json file base name
            if (base::Contains(uri, "fonts/") && base::EndsWith(uri, ".json"))
            {
                const auto& src_png_uri = app::ReplaceAll(uri, ".json", ".png");
                mURIs.insert(src_png_uri);
            }
            mURIs.insert(uri); // keep track of the URIs we're seeing
        }

    private:
        const QString mWorkspaceDir;
    private:
        mutable std::unordered_set<app::AnyString> mURIs;

    } packer(mWorkspaceDir);

    for (size_t index : indices)
    {
        // this is mutable, but we know the contents do not change.
        // the API isn't exactly a perfect match since it was designed
        // for packing which mutates the resources at one go. It could
        // be refactored into 2 steps, first iterate and transact on
        // the resources and then update the resources.
        const auto& resource  = GetUserDefinedResource(index);
        const_cast<Resource&>(resource).Pack(packer);
    }
    return packer.ListUris();
}

void Workspace::SaveResource(const Resource& resource)
{
    RECURSION_GUARD(this, "ResourceList");

    const auto& id = resource.GetId();
    for (size_t i=0; i<mResources.size(); ++i)
    {
        auto& res = mResources[i];
        if (res->GetId() != id)
            continue;
        res->UpdateFrom(resource);
        emit ResourceUpdated(mResources[i].get());
        emit dataChanged(index(i, 0), index(i, 1));
        INFO("Saved resource '%1'", resource.GetName());
        NOTE("Saved resource '%1'", resource.GetName());
        return;
    }
    // if we're here no such resource exists yet.
    // Create a new resource and add it to the list of resources.
    beginInsertRows(QModelIndex(), mUserResourceCount, mUserResourceCount);
    // insert at the end of the visible range which is from [0, mUserResourceCount)
    mResources.insert(mResources.begin() + mUserResourceCount, resource.Copy());

    // careful! endInsertRows will trigger the view proxy to re-fetch the contents.
    // make sure to update this property before endInsertRows otherwise
    // we'll hit ASSERT incorrectly in GetUserDefinedMaterial
    mUserResourceCount++;

    endInsertRows();

    auto& back = mResources[mUserResourceCount - 1];
    ASSERT(back->GetId() == resource.GetId());
    ASSERT(back->GetName() == resource.GetName());
    emit ResourceAdded(back.get());

    INFO("Saved new resource '%1'", resource.GetName());
    NOTE("Saved new resource '%1'", resource.GetName());
}

QString Workspace::MapDrawableIdToName(const AnyString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapMaterialIdToName(const AnyString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapEntityIdToName(const AnyString &id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapResourceIdToName(const AnyString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    return "";
}

bool Workspace::IsValidMaterial(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&
            resource->GetType() == Resource::Type::Material)
            return true;
    }
    return false;
}

bool Workspace::IsValidDrawable(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&
            (resource->GetType() == Resource::Type::ParticleSystem ||
             resource->GetType() == Resource::Type::Shape ||
             resource->GetType() == Resource::Type::Drawable))
            return true;
    }
    return false;
}

bool Workspace::IsValidTilemap(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&  resource->IsTilemap())
            return true;
    }
    return false;
}

bool Workspace::IsValidScript(const AnyString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id && resource->IsScript())
            return true;
    }
    return false;
}

bool Workspace::IsUserDefinedResource(const AnyString& id) const
{
    for (const auto& res : mResources)
    {
        if (res->GetId() == id)
        {
            return !res->IsPrimitive();
        }
    }
    BUG("No such material was found.");
    return false;
}

Resource& Workspace::GetResource(const ModelIndex& index)
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

Resource& Workspace::GetPrimitiveResource(size_t index)
{
    const auto num_primitives = mResources.size() - mUserResourceCount;

    ASSERT(index < num_primitives);
    return *mResources[mUserResourceCount + index];
}

Resource& Workspace::GetUserDefinedResource(size_t index)
{
    ASSERT(index < mUserResourceCount);

    return *mResources[index];
}

Resource* Workspace::FindResourceById(const QString &id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}

Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}
Resource& Workspace::GetResourceById(const QString& id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return *res;
    }
    BUG("No such resource.");
}


const Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}

const Resource* Workspace::FindResourceById(const QString &id) const
{
    for (const auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}
const Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

const Resource& Workspace::GetResource(const ModelIndex& index) const
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

const Resource& Workspace::GetUserDefinedResource(size_t index) const
{
    ASSERT(index < mUserResourceCount);
    return *mResources[index];
}
const Resource& Workspace::GetPrimitiveResource(size_t index) const
{
    const auto num_primitives = mResources.size() - mUserResourceCount;

    ASSERT(index < num_primitives);
    return *mResources[mUserResourceCount + index];
}

void Workspace::DeleteResources(const ModelIndexList& list, std::vector<QString>* dead_files)
{
    RECURSION_GUARD(this, "ResourceList");

    std::vector<size_t> indices = list.GetData();

    std::vector<size_t> relatives;
    // scan the list of indices for associated data resources. 
    for (auto i : indices)
    { 
        // for each tilemap resource
        // look for the data resources associated with the map layers.
        // Add any data object IDs to the list of new indices of resources
        // to be deleted additionally.
        const auto& res = mResources[i];
        if (res->IsTilemap())
        {
            const game::TilemapClass* map = nullptr;
            res->GetContent(&map);
            for (unsigned i = 0; i < map->GetNumLayers(); ++i)
            {
                const auto& layer = map->GetLayer(i);
                for (size_t j = 0; j < mUserResourceCount; ++j)
                {
                    const auto& res = mResources[j];
                    if (!res->IsDataFile())
                        continue;

                    const app::DataFile* data = nullptr;
                    res->GetContent(&data);
                    if (data->GetTypeTag() == DataFile::TypeTag::TilemapData &&
                        data->GetOwnerId() == layer.GetId())
                    {
                        relatives.push_back(j);
                        break;
                    }
                }
            }
        }
    }
    // combine the original indices together with the associated
    // resource indices.
    base::AppendVector(indices, relatives);

    std::sort(indices.begin(), indices.end(), std::less<size_t>());
    // remove dupes. dupes could happen if the resource was already
    // in the original indices list and then was added for the second
    // time when scanning resources mentioned in the indices list for
    // associated resources that need to be deleted.
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    // because the high probability of unwanted recursion
    // messing this iteration up (for example by something
    // calling back to this workspace from Resource
    // deletion signal handler and adding a new resource) we
    // must take some special care here.
    // So, therefore first put the resources to be deleted into
    // a separate container while iterating and removing from the
    // removing from the primary list and only then invoke the signal
    // for each resource.
    std::vector<std::unique_ptr<Resource>> graveyard;

    for (int i=0; i<indices.size(); ++i)
    {
        const auto row = indices[i] - i;
        beginRemoveRows(QModelIndex(), row, row);

        auto it = std::begin(mResources);
        std::advance(it, row);
        graveyard.push_back(std::move(*it));
        mResources.erase(it);
        mUserResourceCount--;

        endRemoveRows();
    }
    // invoke a resource deletion signal for each resource now
    // by iterating over the separate container. (avoids broken iteration)
    for (const auto& carcass : graveyard)
    {
        emit ResourceRemoved(carcass.get());
    }

    // script and tilemap layer data resources are special in the sense that
    // they're the only resources where the underlying filesystem data file
    // is actually created by this editor. for everything else, shaders,
    // image files and font files the resources are created by other tools,
    // and we only keep references to those files.
    for (const auto& carcass : graveyard)
    {
        QString dead_file;
        if (carcass->IsScript())
        {
            // for scripts when the script resource is deleted we're actually
            // going to delete the underlying filesystem file as well.
            Script* script = nullptr;
            carcass->GetContent(&script);
            if (dead_files)
                dead_files->push_back(MapFileToFilesystem(script->GetFileURI()));
            else dead_file = MapFileToFilesystem(script->GetFileURI());
        }
        else if (carcass->IsDataFile())
        {
            // data files that link to a tilemap layer are also going to be
            // deleted when the map is deleted. These files would be completely
            // useless without any way to actually use them for anything.
            DataFile* data = nullptr;
            carcass->GetContent(&data);
            if (data->GetTypeTag() == DataFile::TypeTag::TilemapData)
                dead_file = MapFileToFilesystem(data->GetFileURI());
        }
        if (dead_file.isEmpty())
            continue;

        if (!QFile::remove(dead_file))
        {
            ERROR("Failed to delete file. [file='%1']", dead_file);
        }
        else
        {
            INFO("Deleted file '%1'.", dead_file);
        }
    }
}

void Workspace::DeleteResource(const AnyString& id, std::vector<QString>* dead_files)
{
    for (size_t i=0; i<GetNumUserDefinedResources(); ++i)
    {
        const auto& res = GetUserDefinedResource(i);
        if (res.GetId() == id)
        {
            DeleteResources(i, dead_files);
            return;
        }
    }
}

void Workspace::DuplicateResources(const ModelIndexList& list)
{
    RECURSION_GUARD(this, "ResourceList");

    auto indices = list.GetData();

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    std::map<const Resource*, size_t> insert_index;
    std::vector<std::unique_ptr<Resource>> dupes;

    for (int i=0; i<indices.size(); ++i)
    {
        const auto row_index = indices[i];
        const auto& src_resource = GetResource(row_index);

        auto cpy_resource = src_resource.Clone();
        cpy_resource->SetName(QString("Copy of %1").arg(src_resource.GetName()));

        if (src_resource.IsTilemap())
        {
            const game::TilemapClass* src_map = nullptr;
            game::TilemapClass* cpy_map = nullptr;
            src_resource.GetContent(&src_map);
            cpy_resource->GetContent(&cpy_map);
            ASSERT(src_map->GetNumLayers() == cpy_map->GetNumLayers());
            for (size_t i=0; i<src_map->GetNumLayers(); ++i)
            {
                const auto& src_layer = src_map->GetLayer(i);
                const auto& src_uri   = src_layer.GetDataUri();
                if (src_uri.empty())
                    continue;
                auto& cpy_layer = cpy_map->GetLayer(i);

                const auto& dst_uri = base::FormatString("ws://data/%1.bin", cpy_layer.GetId());
                const auto& src_file = MapFileToFilesystem(src_uri);
                const auto& dst_file = MapFileToFilesystem(dst_uri);
                const auto [success, error] = CopyFile(src_file, dst_file);
                if (!success)
                {
                    WARN("Failed to duplicate tilemap layer data file. [layer='%1', file='%2', error='%3']",
                         cpy_layer.GetName(), dst_file, error);
                    cpy_layer.ResetDataId();
                    cpy_layer.ResetDataUri();
                }
                else
                {
                    app::DataFile cpy_data;
                    cpy_data.SetFileURI(dst_uri);
                    cpy_data.SetOwnerId(cpy_layer.GetId());
                    cpy_data.SetTypeTag(app::DataFile::TypeTag::TilemapData);
                    const auto& cpy_data_resource_name = toString("%1 Layer Data", cpy_resource->GetName());
                    auto cpy_data_resource = std::make_unique<app::DataResource>(cpy_data, cpy_data_resource_name);
                    insert_index[cpy_data_resource.get()] = row_index;
                    dupes.push_back(std::move(cpy_data_resource));
                    cpy_layer.SetDataId(cpy_data.GetId());
                    cpy_layer.SetDataUri(dst_uri);
                    DEBUG("Duplicated tilemap layer data. [layer='%1', src='%2', dst='%3']",
                          cpy_layer.GetName(), src_file, dst_file);
                }
            }
        }
        insert_index[cpy_resource.get()] = row_index;
        dupes.push_back(std::move(cpy_resource));
    }

    for (size_t i=0; i<dupes.size(); ++i)
    {
        const auto index = insert_index[dupes[i].get()] + i;

        beginInsertRows(QModelIndex(), index, index);

        auto* dupe_ptr = dupes[i].get();
        mResources.insert(mResources.begin() + index, std::move(dupes[i]));
        mUserResourceCount++;
        endInsertRows();

        emit ResourceAdded(dupe_ptr);
    }
}

bool Workspace::ExportResourceJson(const ModelIndexList& indices, const QString& filename) const
{
    data::JsonObject json;
    for (size_t index : indices)
    {
        const auto& resource = GetResource(index);
        resource.Serialize(json);

        QJsonObject props;
        resource.SaveProperties(props);
        QJsonDocument docu(props);
        QByteArray bytes = docu.toJson().toBase64();

        const auto& prop_key = resource.GetIdUtf8();
        const auto& prop_val = std::string(bytes.constData(), bytes.size());
        json.Write(prop_key.c_str(), prop_val);
    }

    data::JsonFile file;
    file.SetRootObject(json);
    const auto [success, error] = file.Save(app::ToUtf8(filename));
    if (!success)
        ERROR("Export resource as JSON error '%1'.", error);
    else INFO("Exported %1 resource(s) into '%2'", indices.size(), filename);
    return success;
}

// static
bool Workspace::ImportResourcesFromJson(const QString& filename, std::vector<std::unique_ptr<Resource>>& resources)
{
    data::JsonFile file;
    auto [success, error] = file.Load(ToUtf8(filename));
    if (!success)
    {
        ERROR("Import resource as JSON error '%1'.", error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    success = true;
    success = success && LoadMaterials<gfx::MaterialClass>("materials", root, resources);
    success = success && LoadResources<gfx::ParticleEngineClass>("particles", root, resources);
    success = success && LoadResources<gfx::PolygonClass>("shapes", root, resources);
    success = success && LoadResources<game::EntityClass>("entities", root, resources);
    success = success && LoadResources<game::SceneClass>("scenes", root, resources);
    success = success && LoadResources<game::TilemapClass>("tilemaps", root, resources);
    success = success && LoadResources<Script>("scripts", root, resources);
    success = success && LoadResources<DataFile>("data_files", root, resources);
    success = success && LoadResources<audio::GraphClass>("audio_graphs", root, resources);
    success = success && LoadResources<uik::Window>("uis", root, resources);
    DEBUG("Loaded %1 resources from '%2'.", resources.size(), filename);

    // restore the properties.
    for (auto& resource : resources)
    {
        const auto& prop_key = resource->GetIdUtf8();
        std::string prop_val;
        if (!root.Read(prop_key.c_str(), &prop_val)) {
            WARN("No properties found for resource '%1'.", prop_key);
            continue;
        }
        if (prop_val.empty())
            continue;

        const auto& bytes = QByteArray::fromBase64(QByteArray(prop_val.c_str(), prop_val.size()));
        QJsonParseError error;
        const auto& docu = QJsonDocument::fromJson(bytes, &error);
        if (docu.isNull()) {
            WARN("Json parse error when parsing resource '%1' properties.", prop_key);
            continue;
        }
        resource->LoadProperties(docu.object());
    }
    return success;
}

void Workspace::ImportFilesAsResource(const QStringList& files)
{
    // todo: given a collection of file names some of the files
    // could belong together in a sprite/texture animation sequence.
    // for example if we have "bird_0.png", "bird_1.png", ... "bird_N.png" 
    // we could assume that these are all material animation frames
    // and should go together into one material.
    // On the other hand there are also cases like with tile sets that have
    // tiles named tile1.png, tile2.png ... and these should be separated.
    // not sure how to deal with this smartly. 

    for (const QString& file : files)
    {
        const QFileInfo info(file);
        if (!info.isFile())
        {
            WARN("File is not actually a file. [file='%1']", file);
            continue;
        }
        const auto& name   = info.baseName();
        const auto& suffix = info.completeSuffix().toUpper();
        const auto& uri = MapFileToWorkspace(file);
        if (suffix == "LUA")
        {
            Script script;
            script.SetFileURI(ToUtf8(uri));
            ScriptResource res(script, name);
            SaveResource(res);
            INFO("Imported new script file '%1' based on file '%2'", name, info.filePath());
        }
        else if (suffix == "JPEG" || suffix == "JPG" || suffix == "PNG" || suffix == "TGA" || suffix == "BMP")
        {
            gfx::detail::TextureFileSource texture;
            texture.SetFileName(ToUtf8(uri));
            texture.SetName(ToUtf8(name));

            gfx::TextureMap2DClass klass(gfx::MaterialClass::Type::Texture, base::RandomString(10));
            klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            klass.SetTexture(texture.Copy());
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Default);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter ::Default);
            MaterialResource res(klass, name);
            SaveResource(res);
            INFO("Imported new material '%1' based on image file '%2'", name, info.filePath());
        }
        else if (suffix == "MP3" || suffix == "WAV" || suffix == "FLAC" || suffix == "OGG")
        {
            audio::GraphClass klass(ToUtf8(name));
            audio::GraphClass::Element element;
            element.name = ToUtf8(name);
            element.id   = base::RandomString(10);
            element.type = "FileSource";
            element.args["file"] = ToUtf8(uri);
            klass.SetGraphOutputElementId(element.id);
            klass.SetGraphOutputElementPort("out");
            klass.AddElement(std::move(element));
            AudioResource res(klass, name);
            SaveResource(res);
            INFO("Imported new audio graph '%1' based on file '%2'", name, info.filePath());
        }
        else
        {
            DataFile data;
            data.SetFileURI(uri);
            data.SetTypeTag(DataFile::TypeTag::External);
            DataResource res(data, name);
            SaveResource(res);
            INFO("Imported new data file '%1' based on file '%2'", name, info.filePath());
        }
        DEBUG("Mapping imported file '%1' => '%2'", file, uri);
    }
}

void Workspace::Tick()
{

}

bool Workspace::ExportResourceArchive(const std::vector<const Resource*>& resources, const ExportOptions& options)
{
    ZipArchiveExporter zip(options.zip_file, mWorkspaceDir);
    if (!zip.Open())
        return false;

    // unfortunately we need to make copies of the resources
    // since packaging might modify the resources yet the
    // original resources should not be changed.
    // todo: perhaps rethink this.. what other ways would there be ?
    // constraints:
    //  - don't wan to duplicate the serialization/deserialization/JSON writing
    //  - should not know details of resources (materials, drawables etc)
    //  - material depends on resource packer, resource packer should not then
    //    know about material
    std::vector<std::unique_ptr<Resource>> mutable_copies;
    for (const auto* resource : resources)
    {
        ASSERT(!resource->IsPrimitive());
        mutable_copies.push_back(resource->Copy());
    }

    // Partition the resources such that the data objects come in first.
    // This is done because some resources such as tilemaps refer to the
    // data resources by URI and in order for the URI remapping to work
    // the packer must have packed the data object before packing the
    // tilemap object.
    std::partition(mutable_copies.begin(), mutable_copies.end(),
        [](const auto& resource) {
            return resource->IsDataFile();
        });

    QJsonObject properties;
    data::JsonObject content;
    for (auto& resource: mutable_copies)
    {
        resource->Pack(zip);
        resource->Serialize(content);
        resource->SaveProperties(properties);
    }
    QJsonDocument doc(properties);
    zip.WriteText(content.ToString(), "content.json");
    zip.WriteBytes(doc.toJson(), "properties.json");
    zip.Close();
    return true;
}

bool Workspace::ImportResourceArchive(ResourceArchive& zip)
{
    const auto& sub_folder = zip.GetImportSubFolderName();
    const auto& name_prefix = zip.GetResourceNamePrefix();
    ZipArchiveImporter importer(zip.mZipFile, sub_folder, mWorkspaceDir, zip.mZip);

    // it seems a bit funny here to be calling "pack" when actually we're
    // unpacking but the implementation of zip based resource packer is
    // such that data is copied (packed) from the zip and into the workspace
    for (size_t i=0; i<zip.mResources.size(); ++i)
    {
        if (zip.IsIndexIgnored(i))
            continue;
        auto& resource = zip.mResources[i];
        resource->Pack(importer);
        const auto& name = resource->GetName();
        resource->SetName(name_prefix + name);
    }

    for (size_t i=0; i<zip.mResources.size(); ++i)
    {
        if (zip.IsIndexIgnored(i))
            continue;
        auto& resource = zip.mResources[i];
        SaveResource(*resource);
    }
    return true;
}

bool Workspace::BuildReleasePackage(const std::vector<const Resource*>& resources, const ContentPackingOptions& options)
{
    const QString& outdir = JoinPath(options.directory, options.package_name);
    if (!MakePath(outdir))
    {
        ERROR("Failed to create output directory. [dir='%1']", outdir);
        return false;
    }

    // unfortunately we need to make copies of the resources
    // since packaging might modify the resources yet the
    // original resources should not be changed.
    // todo: perhaps rethink this.. what other ways would there be ?
    // constraints:
    //  - don't wan to duplicate the serialization/deserialization/JSON writing
    //  - should not know details of resources (materials, drawables etc)
    //  - material depends on resource packer, resource packer should not then
    //    know about material
    std::vector<std::unique_ptr<Resource>> mutable_copies;
    for (const auto* resource : resources)
    {
        ASSERT(!resource->IsPrimitive());
        mutable_copies.push_back(resource->Copy());
    }

    // Partition the resources such that the data objects come in first.
    // This is done because some resources such as tilemaps refer to the
    // data resources by URI and in order for the URI remapping to work
    // the packer must have packed the data object before packing the
    // tilemap object.
    std::partition(mutable_copies.begin(), mutable_copies.end(),
        [](const auto& resource) {
            return resource->IsDataFile();
        });

    DEBUG("Max texture size. [width=%1, height=%2]", options.max_texture_width, options.max_texture_height);
    DEBUG("Pack size. [width=%1, height=%2]", options.texture_pack_width, options.texture_pack_height);
    DEBUG("Pack flags. [resize=%1, combine=%2]", options.resize_textures, options.combine_textures);

    GfxTexturePacker texture_packer(outdir,
        options.max_texture_width,
        options.max_texture_height,
        options.texture_pack_width,
        options.texture_pack_height,
        options.texture_padding,
        options.resize_textures,
        options.combine_textures);

    // collect the resources in the packer.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Collecting resources...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to Resource interface ?
            const gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->BeginPacking(&texture_packer);
        }
    }

    MyResourcePacker file_packer(outdir, mWorkspaceDir);

    unsigned errors = 0;

    // copy some file based content around.
    // todo: this would also need some kind of file name collision
    // resolution and mapping functionality.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        mutable_copies[i]->Pack(file_packer);
    }

    texture_packer.PackTextures([this](const std::string& action, int step, int max) {
        emit ResourcePackingUpdate(FromLatin(action), step, max);
    }, file_packer);

    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Updating resources references...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&texture_packer);
        }
    }

    if (!mSettings.debug_font.isEmpty())
    {
        // todo: should change the font URI.
        // but right now this still also works since there's a hack for this
        // in the loader in engine/ (Also same app:// thing applies to the UI style files)
        file_packer.CopyFile(app::ToUtf8(mSettings.debug_font), "fonts/");
    }

    // write content file ?
    if (options.write_content_file)
    {
        emit ResourcePackingUpdate("Writing content JSON file...", 0, 0);
        // filename of the JSON based descriptor that contains all the
        // resource definitions.
        const auto &json_filename = JoinPath(outdir, "content.json");

        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open content JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
        }

        // finally serialize
        data::JsonObject json;
        json.Write("json_version", 1);
        json.Write("made_with_app", APP_TITLE);
        json.Write("made_with_ver", APP_VERSION);
        for (const auto &resource : mutable_copies)
        {
            resource->Serialize(json);
        }

        const auto &str = json.ToString();
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write content JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    // resolves the path.
    const QFileInfo engine_dll(mSettings.GetApplicationLibrary());
    QString engine_name = engine_dll.fileName();
    if (engine_name.startsWith("lib"))
        engine_name.remove(0, 3);
    if (engine_name.endsWith(".so"))
        engine_name.chop(3);
    else if (engine_name.endsWith(".dll"))
        engine_name.chop(4);

    // write config file?
    if (options.write_config_file)
    {
        emit ResourcePackingUpdate("Writing config JSON file...", 0, 0);

        const auto& json_filename = JoinPath(options.directory, "config.json");
        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
        }
        nlohmann:: json json;
        base::JsonWrite(json, "json_version",   1);
        base::JsonWrite(json, "made_with_app",  APP_TITLE);
        base::JsonWrite(json, "made_with_ver",  APP_VERSION);
        base::JsonWrite(json["config"], "red_size",     8);
        base::JsonWrite(json["config"], "green_size",   8);
        base::JsonWrite(json["config"], "blue_size",    8);
        base::JsonWrite(json["config"], "alpha_size",   8);
        base::JsonWrite(json["config"], "stencil_size", 8);
        base::JsonWrite(json["config"], "depth_size",   24);
        base::JsonWrite(json["config"], "srgb", mSettings.config_srgb);
        if (mSettings.multisample_sample_count == 0)
            base::JsonWrite(json["config"], "sampling", "None");
        else if (mSettings.multisample_sample_count == 4)
            base::JsonWrite(json["config"], "sampling", "MSAA4");
        else if (mSettings.multisample_sample_count == 8)
            base::JsonWrite(json["config"], "sampling", "MSAA8");
        else if (mSettings.multisample_sample_count == 16)
            base::JsonWrite(json["config"], "sampling", "MSAA16");
        base::JsonWrite(json["window"], "width",      mSettings.window_width);
        base::JsonWrite(json["window"], "height",     mSettings.window_height);
        base::JsonWrite(json["window"], "can_resize", mSettings.window_can_resize);
        base::JsonWrite(json["window"], "has_border", mSettings.window_has_border);
        base::JsonWrite(json["window"], "vsync",      mSettings.window_vsync);
        base::JsonWrite(json["window"], "cursor",     mSettings.window_cursor);
        base::JsonWrite(json["window"], "grab_mouse", mSettings.grab_mouse);
        if (mSettings.window_mode == ProjectSettings::WindowMode::Windowed)
            base::JsonWrite(json["window"], "set_fullscreen", false);
        else if (mSettings.window_mode == ProjectSettings::WindowMode::Fullscreen)
            base::JsonWrite(json["window"], "set_fullscreen", true);

        base::JsonWrite(json["application"], "library", ToUtf8(engine_name));
        base::JsonWrite(json["application"], "identifier", ToUtf8(mSettings.application_identifier));
        base::JsonWrite(json["application"], "title",    ToUtf8(mSettings.application_name));
        base::JsonWrite(json["application"], "version",  ToUtf8(mSettings.application_version));
        base::JsonWrite(json["application"], "content", ToUtf8(options.package_name));
        base::JsonWrite(json["application"], "game_script", ToUtf8(mSettings.game_script));
        base::JsonWrite(json["application"], "save_window_geometry", mSettings.save_window_geometry);
        base::JsonWrite(json["desktop"], "audio_io_strategy", mSettings.desktop_audio_io_strategy);
        base::JsonWrite(json["debug"], "font", ToUtf8(mSettings.debug_font));
        base::JsonWrite(json["debug"], "show_msg", mSettings.debug_show_msg);
        base::JsonWrite(json["debug"], "show_fps", mSettings.debug_show_fps);
        base::JsonWrite(json["debug"], "draw", mSettings.debug_draw);
        base::JsonWrite(json["debug"], "print_fps", mSettings.debug_print_fps);
        base::JsonWrite(json["logging"], "debug", mSettings.log_debug);
        base::JsonWrite(json["logging"], "warn", mSettings.log_warn);
        base::JsonWrite(json["logging"], "info", mSettings.log_info);
        base::JsonWrite(json["logging"], "error", mSettings.log_error);
        base::JsonWrite(json["html5"], "canvas_width", mSettings.canvas_width);
        base::JsonWrite(json["html5"], "canvas_height", mSettings.canvas_height);
        base::JsonWrite(json["html5"], "canvas_mode", mSettings.canvas_mode);
        base::JsonWrite(json["html5"], "webgl_power_pref", mSettings.webgl_power_preference);
        base::JsonWrite(json["html5"], "webgl_antialias", mSettings.webgl_antialias);
        base::JsonWrite(json["html5"], "developer_ui", mSettings.html5_developer_ui);
        base::JsonWrite(json["wasm"], "audio_io_strategy", mSettings.wasm_audio_io_strategy);
        base::JsonWrite(json["engine"], "default_min_filter", mSettings.default_min_filter);
        base::JsonWrite(json["engine"], "default_mag_filter", mSettings.default_mag_filter);
        base::JsonWrite(json["engine"], "ticks_per_second",   (float)mSettings.ticks_per_second);
        base::JsonWrite(json["engine"], "updates_per_second", (float)mSettings.updates_per_second);
        base::JsonWrite(json["engine"], "clear_color", ToGfx(mSettings.clear_color));
        base::JsonWrite(json["physics"], "enabled", mSettings.enable_physics);
        base::JsonWrite(json["physics"], "num_velocity_iterations", mSettings.num_velocity_iterations);
        base::JsonWrite(json["physics"], "num_position_iterations", mSettings.num_position_iterations);
        base::JsonWrite(json["physics"], "gravity", mSettings.physics_gravity);
        base::JsonWrite(json["physics"], "scale",   mSettings.physics_scale);
        base::JsonWrite(json["mouse_cursor"], "material", ToUtf8(mSettings.mouse_pointer_material));
        base::JsonWrite(json["mouse_cursor"], "drawable", ToUtf8(mSettings.mouse_pointer_drawable));
        base::JsonWrite(json["mouse_cursor"], "show", mSettings.mouse_pointer_visible);
        base::JsonWrite(json["mouse_cursor"], "hotspot", mSettings.mouse_pointer_hotspot);
        base::JsonWrite(json["mouse_cursor"], "size", mSettings.mouse_pointer_size);
        base::JsonWrite(json["mouse_cursor"], "units", mSettings.mouse_pointer_units);
        base::JsonWrite(json["audio"], "channels", mSettings.audio_channels);
        base::JsonWrite(json["audio"], "sample_rate", mSettings.audio_sample_rate);
        base::JsonWrite(json["audio"], "sample_type", mSettings.audio_sample_type);
        base::JsonWrite(json["audio"], "buffer_size", mSettings.audio_buffer_size);
        base::JsonWrite(json["audio"], "pcm_caching", mSettings.enable_audio_pcm_caching);

        // This is a lazy workaround for the fact that the unit tests don't set up the
        // game script properly as a script object in the workspace. This means there's
        // no proper script copying/URI mapping taking place for the game script.
        // So we check here for the real workspace to see if there's a mapping and if so
        // then replace the original game script value with the mapped script URI.
        if (file_packer.HasMapping(mSettings.game_script))
        {
            base::JsonWrite(json["application"], "game_script", file_packer.MapUri(mSettings.game_script));
        }

        const auto& str = json.dump(2);
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write config JSON file. [file='%1', error='%2']", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    if (options.write_html5_content_fs_image)
    {
        emit ResourcePackingUpdate("Generating HTML5 filesystem image...", 0, 0);

        QString package_script = app::JoinPath(options.emsdk_path, "/upstream/emscripten/tools/file_packager.py");

        if (!FileExists(options.python_executable))
        {
            ERROR("Python executable was not found. [python='%1']", options.python_executable);
            ++errors;
        }
        else if (!FileExists(package_script))
        {
            ERROR("Emscripten filesystem package script not found. [script='%1']", package_script);
            ++errors;
        }
        else
        {
            DEBUG("Generating HTML5 filesystem image. [emsdk='%1', python='%2']", options.emsdk_path,
                  options.python_executable);
            QString filesystem_image_name = "FILESYSTEM";
            QStringList args;
            args << package_script;
            args << filesystem_image_name;
            args << "--preload";
            args << options.package_name;
            args << "config.json";
            args << QString("--js-output=%1.js").arg(filesystem_image_name);
            Process::Error error_code = Process::Error::None;
            QStringList stdout_buffer;
            QStringList stderr_buffer;
            if (!Process::RunAndCapture(options.python_executable,
                                        options.directory, args,
                                        &stdout_buffer,
                                        &stderr_buffer,
                                        &error_code))
            {
                ERROR("Building HTML5/WASM filesystem image failed. [error='%1', python='%2', script='%2']",
                      error_code, options.python_executable, package_script);
                ++errors;
            }
        }
    }

    if (options.copy_html5_files)
    {
        // these should be in the dist/ folder and are the
        // built by the emscripten build in emscripten/
        struct HTML5_Engine_File {
            QString name;
            bool mandatory;
        };

        const HTML5_Engine_File files[] = {
            {"GameEngine.js", true},
            {"GameEngine.wasm", true},
            // the JS Web worker glue code. this file is only produced by Emscripten
            // if the threaded WASM build is being used.
            {"GameEngine.worker.js", false},
            // this is just a helper file for convenience
            {"http-server.py", false}
        };
        for (int i=0; i<4; ++i)
        {
            const auto& src = app::GetAppInstFilePath(files[i].name);
            const auto& dst = app::JoinPath(options.directory, files[i].name);
            auto[success, error] = app::CopyFile(src, dst);
            if (!success)
            {
                ERROR("Failed to copy game engine file. [src='%1', dst='%2', error='%3']", src, dst, error);
                ++errors;
            }
        }
    }
    if (options.write_html5_game_file)
    {
        const QString files[] = {
           "game.html"
        };
        for (int i=0; i<1; ++i)
        {
            const auto& src = app::GetAppInstFilePath(files[i]);
            const auto& dst = app::JoinPath(options.directory, files[i]);
            auto[success, error] = app::CopyFile(src, dst);
            if (!success)
            {
                ERROR("Failed to copy game html file. [src='1%', dst='%2', error='%3']", src, dst, error);
                ++errors;
            }
        }
    }

    // Copy game main executable/engine library
    if (options.copy_native_files)
    {
        // TODO: fix this name stuff here, only take it from the options.
        // the name stuff is duplicated in the package complete dialog
        // when trying to launch the native game.

        QString src_exec = "GameMain";
        QString dst_exec = mSettings.application_name;
        if (dst_exec.isEmpty())
            dst_exec = "GameMain";
#if defined(WINDOWS_OS)
        src_exec.append(".exe");
        dst_exec.append(".exe");
        engine_name.append(".dll");
#elif defined(LINUX_OS)
        engine_name.prepend("lib");
        engine_name.append(".so");
#endif
        dst_exec = app::JoinPath(options.directory, dst_exec);
        auto [success, error ] = app::CopyFile(src_exec, dst_exec);
        if (!success)
        {
            ERROR("Failed to copy game executable. [src='%1', dst='%2', error='%3']", src_exec, dst_exec, error);
            ++errors;
        }
        const auto& src_lib = MapFileToFilesystem(mSettings.GetApplicationLibrary());
        const auto& dst_lib = app::JoinPath(options.directory, engine_name);
        std::tie(success, error) = app::CopyFile(src_lib, dst_lib);
        if (!success)
        {
            ERROR("Failed to copy game engine library. [src='%1', dst='%2', error='%3']", src_lib, dst_lib, error);
            ++errors;
        }
    }

    if (const auto total_errors = errors + texture_packer.GetNumErrors() + file_packer.GetNumErrors())
    {
        WARN("Resource packing completed with errors (%1).", total_errors);
        WARN("Please see the log file for details.");
        return false;
    }

    INFO("Packed %1 resource(s) into '%2' successfully.", resources.size(), options.directory);
    return true;
}

void Workspace::UpdateResource(const Resource* resource)
{
    SaveResource(*resource);
}

void Workspace::UpdateUserProperty(const QString& name, const QVariant& data)
{
    mUserProperties[name] = data;
}

void WorkspaceProxy::DebugPrint() const
{
    DEBUG("Sorted resource order:");

    for (int i=0; i<rowCount(); ++i)
    {
        const auto foo_index = this->index(i, 0);
        const auto src_index = this->mapToSource(foo_index);
        const auto& res = mWorkspace->GetResource(src_index);
        DEBUG("%1 %2", res.GetType(), res.GetName());
    }

    DEBUG("");
}

bool WorkspaceProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    if (!mWorkspace)
        return false;
    const auto& resource = mWorkspace->GetUserDefinedResource(row);
    if (!mBits.test(resource.GetType()))
        return false;
    if (mFilterString.isEmpty())
        return true;
    const auto& name = resource.GetName();
    return name.contains(mFilterString, Qt::CaseInsensitive);
}

void WorkspaceProxy::sort(int column, Qt::SortOrder order)
{
    DEBUG("Sort workspace resources. [column=%1, order=%2]", column, order);
    QSortFilterProxyModel::sort(column, order);
}

bool WorkspaceProxy::lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const
{
    const auto& lhs_res = mWorkspace->GetResource(lhs);
    const auto& rhs_res = mWorkspace->GetResource(rhs);

    const auto& lhs_type_val = toString(lhs_res.GetType());
    const auto& rhs_type_val = toString(rhs_res.GetType());
    const auto& lhs_name = lhs_res.GetName();
    const auto& rhs_name = rhs_res.GetName();

    if (lhs.column() == 0 && rhs.column() == 0)
    {
        if (lhs_type_val < rhs_type_val)
            return true;
        else if (lhs_type_val == rhs_type_val)
            return lhs_name < rhs_name;
        return false;
    }
    else if (lhs.column() == 1 && rhs.column() == 1)
    {
        if (lhs_name < rhs_name)
            return true;
        else if (lhs_name == rhs_name)
            return lhs_type_val < rhs_type_val;
        return false;
    }
    else BUG("Unknown sorting column combination!");
    return false;
}


} // namespace
