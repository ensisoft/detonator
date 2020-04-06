// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <QtAlgorithms>
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QByteArray>
#  include <QFile>
#  include <QIcon>
#include "warnpop.h"

#include "eventlog.h"
#include "workspace.h"
#include "utility.h"
#include "format.h"

namespace app
{

Workspace::Workspace()
{
    DEBUG("Create workspace");
}

Workspace::~Workspace()
{
    DEBUG("Destroy workspace");
}

QVariant Workspace::data(const QModelIndex& index, int role) const
{
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
        switch (res->GetType())
        {
            case Resource::Type::Material:
                return QIcon("icons:material.png");
            case Resource::Type::ParticleSystem:
                return QIcon("icons:particle.png");
            default: break;
        }
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

std::shared_ptr<gfx::Material> Workspace::MakeMaterial(const std::string& name) const
{
    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    if (name == "Checkerboard")
        return std::make_shared<gfx::Material>(gfx::TextureMap("textures/Checkerboard.png"));


    const Resource& resource = GetResource(FromUtf8(name), Resource::Type::Material);
    const gfx::Material* content = nullptr;
    resource.GetContent(&content);
    auto ret = std::make_shared<gfx::Material>(*content);

    // add a copy in the collection of private instances so that
    // if/when the resource is modified the object using the resource
    // will also reflect those changes.
    auto handle = std::make_unique<WeakGraphicsResourceHandle<gfx::Material>>(FromUtf8(name), ret);
    mPrivateInstances.push_back(std::move(handle));
    return ret;
}
std::shared_ptr<gfx::Drawable> Workspace::MakeDrawable(const std::string& name) const
{
    // we have only particle engines right now as drawables
    if (HasResource(FromUtf8(name), Resource::Type::ParticleSystem))
    {
        const Resource& resource = GetResource(FromUtf8(name), Resource::Type::ParticleSystem);
        const gfx::KinematicsParticleEngine* engine = nullptr;
        resource.GetContent(&engine);
        auto ret = std::make_shared<gfx::KinematicsParticleEngine>(*engine);

        // add a copy in the collection of private instances so that
        // if/when the resource is modified the object using the resource
        // will also reflect those changes.
        auto handle = std::make_unique<WeakGraphicsResourceHandle<gfx::KinematicsParticleEngine>>(FromUtf8(name), ret);
        mPrivateInstances.push_back(std::move(handle));

        return ret;
    }

    ERROR("Request for a drawable that doesn't exist: '%1'", name);
    std::shared_ptr<gfx::Drawable> ret;
    return ret;
}

bool Workspace::LoadContent(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }
    const auto& buff = file.readAll(); // QByteArray
    if (buff.isEmpty())
    {
        WARN("No workspace content found in file: '%1'", filename);
        return false;
    }

    const auto* beg = buff.data();
    const auto* end = buff.data() + buff.size();
    // todo: this can throw, use the nothrow and log error
    const auto& json = nlohmann::json::parse(beg, end);

    std::vector<std::unique_ptr<Resource>> resources;

    if (json.contains("materials"))
    {
        for (const auto& json_mat : json["materials"].items())
        {
            const auto& name = app::FromUtf8(json_mat.value()["resource_name"]);

            std::optional<gfx::Material> ret = gfx::Material::FromJson(json_mat.value());
            if (!ret.has_value())
            {
                ERROR("Failed to load material '%1' properties.", name);
                continue;
            }
            auto& material = ret.value();
            DEBUG("Loaded material: '%1'", name);
            resources.push_back(std::make_unique<MaterialResource>(std::move(material), name));
        }
    }
    if (json.contains("particles"))
    {
        for (const auto& json_p : json["particles"].items())
        {
            const auto& name = app::FromUtf8(json_p.value()["resource_name"]);
            std::optional<gfx::KinematicsParticleEngine> ret = gfx::KinematicsParticleEngine::FromJson(json_p.value());
            if (!ret.has_value())
            {
                ERROR("Failed to load particle system '%1' properties.", name);
                continue;
            }
            auto& particle = ret.value();
            DEBUG("Loaded particle system: '%1'", name);
            resources.push_back(std::make_unique<ParticleSystemResource>(std::move(particle), name));
        }
    }

    mResources = std::move(resources);
    mContentFile = filename;
    INFO("Loaded content file '%1'", filename);
    return true;
}

bool Workspace::SaveContent(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    nlohmann::json json;
    for (const auto& resource : mResources)
    {
        resource->Serialize(json);
    }

    if (json.is_null())
    {
        WARN("Workspace contains no actual data. Skipped saving.");
        return true;
    }

    const auto& str = json.dump(2);
    if (file.write(&str[0], str.size()) == -1)
    {
        ERROR("File write failed. '%1'", filename);
        return false;
    }
    file.flush();
    file.close();
    mContentFile = filename;
    INFO("Saved workspace content in '%'", filename);
    NOTE("Workspace content saved.");
    return true;
}

bool Workspace::SaveWorkspace(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    // our JSON root object
    QJsonObject json;

    // serialize the workspace properties into JSON
    json["workspace"] = QJsonObject::fromVariantMap(mProperties);

    // serialize the properties stored in each and every
    // resource object.
    for (const auto& resource : mResources)
    {
        resource->Serialize(json);
    }
    // set the root object to the json document then serialize
    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();

    INFO("Saved workspace data in '%1'", filename);
    NOTE("Workspace data saved.");
    mWorkspaceFile = filename;
    return true;
}

bool Workspace::LoadWorkspace(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    const auto& buff = file.readAll(); // QByteArray

    QJsonDocument docu(QJsonDocument::fromJson(buff));

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
    mWorkspaceFile = filename;
    return true;
}

QStringList Workspace::ListMaterials() const
{
    QStringList list;
    list << "Checkerboard";

    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::Material)
            list.append(res->GetName());
    }
    return list;
}

QStringList Workspace::ListDrawables() const
{
    QStringList list;
    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::ParticleSystem)
            list.append(res->GetName());
    }
    return list;
}

bool Workspace::HasMaterial(const QString& name) const
{
    if (name == "Checkerboard")
        return true;
    return HasResource(name, Resource::Type::Material);
}

bool Workspace::HasParticleSystem(const QString& name) const
{
    return HasResource(name, Resource::Type::ParticleSystem);
}

bool Workspace::HasResource(const QString& name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return true;
    }
    return false;
}

Resource& Workspace::GetResource(const QString& name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    ASSERT(!"no such resource");
}

const Resource& Workspace::GetResource(const QString& name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    ASSERT(!"no such resource");
}

void Workspace::DeleteResources(QModelIndexList& list)
{
    qSort(list);

    int removed = 0;

    // because the high probability of unwanted recursion
    // fucking this iteration up (for example by something
    // calling back to this workspace from Resource
    // deletion signal handler and adding a new resource) we 
    // must take some special care here.
    // So therefore first put the resources to be deleted into
    // a separate container while iterating and removing from the 
    // removing from the primary list and only then invoke the signal
    // for each resource.
    std::vector<std::unique_ptr<Resource>> graveyard;

    for (int i=0; i<list.size(); ++i)
    {
        const auto row = list[i].row() - removed;
        beginRemoveRows(QModelIndex(), row, row);

        auto it = std::begin(mResources);
        std::advance(it, row);
        graveyard.push_back(std::move(*it));
        mResources.erase(it);

        endRemoveRows();
        ++removed;
    }

    // invoke a resource deletion signal for each resource now
    // by iterating over the separate container. (avoids broken iteration)
    for (const auto& carcass : graveyard)
    {
        emit ResourceToBeDeleted(carcass.get());
    }
}

void Workspace::Tick()
{
    // cleanup expired handles that point to stale objects that
    // are no longer actually referenced.
    for (size_t i=0; i<mPrivateInstances.size(); )
    {
        if (mPrivateInstances[i]->IsExpired())
        {
            const auto name = mPrivateInstances[i]->GetName();
            const auto last = mPrivateInstances.size() - 1;
            std::swap(mPrivateInstances[i], mPrivateInstances[last]);
            mPrivateInstances.pop_back();
            DEBUG("Deleted resource tracking handle %1", name);
        }
        else
        {
            ++i;
        }

    }
}

} // namespace
