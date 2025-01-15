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
#  include <QJsonDocument>
#include "warnpop.h"

#include <algorithm>

#include "data/json.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource_util.h"
#include "editor/app/zip_archive.h"

namespace app
{

ZipArchive::ZipArchive()
{
    mZip.setAutoClose(true);
    mZip.setFileNameCodec("UTF-8");
    mZip.setUtf8Enabled(true);
    mZip.setZip64Enabled(true);
}

bool ZipArchive::Open(const QString& zip_file)
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
    LoadResources<gfx::PolygonMeshClass>("shapes", content, mResources);
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

bool ZipArchive::ReadFile(const QString& file, QByteArray* array) const
{
    if (!FindZipFile(file))
        return false;
    QuaZipFile zip_file(&mZip);
    zip_file.open(QIODevice::ReadOnly);
    *array = zip_file.readAll();
    zip_file.close();
    return true;
}

bool ZipArchive::FindZipFile(const QString& unix_style_name) const
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

} // namespace