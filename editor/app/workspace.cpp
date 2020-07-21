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
#include "warnpop.h"

#include "eventlog.h"
#include "workspace.h"
#include "utility.h"
#include "format.h"

namespace {

    QString GetAppDir()
    {
        static const auto& dir = QCoreApplication::applicationDirPath() + "/";
        return dir;
    }
} // namespace

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

std::shared_ptr<gfx::Material> Workspace::MakeMaterial(const std::string& name) const
{
    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    if (name == "Checkerboard")
        return std::make_shared<gfx::Material>(gfx::TextureMap("app://textures/Checkerboard.png"));

    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string enum_name(magic_enum::enum_name(val));
        if (enum_name == name)
            return std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color4f(val)));
    }


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
    //            == About resource loading ==
    // User defined resources have a combination of type and name
    // where type is the underlying class type and name identifies
    // the set of resources that the user edits that instances of that
    // said type then use.
    // Primitive / (non user defined resources) don't need a name
    // since the name is irrelevant since the objects are stateless
    // in this sense and don't have properties that would change between instances.
    // For example with drawable rectangles (gfx::Rectangle) all rectangles
    // that we might want to draw are basically the same. We don't need to
    // configure each rectangle object with properties that would distinguish
    // it from other rectangles.
    // In fact there's basically even no need to create more than 1 instance
    // of such resource and then share it between all users.
    //
    // User defined resources on the other hand *can be* unique.
    // For example particle engines, the underlying object type is same for
    // particle engine A and B, but their defining properties are completely
    // different. To distinguish the set of properties the user gives each
    // particle engine a "name". Then when loading such objects we must
    // load them by name. Additionally the resources may or may not be
    // shared. For example when a fleet of alien spaceships are rendered
    // each spaceship might have their own particle engine (own simulation state)
    // thus producing a unique rendering for each spaceship. However the problem
    // is that this might be computationally heavy. For N ships we'd need to do
    // N particle engine simulations.
    // However it's also possible that the particle engines are shared and each
    // ship (of the same type) just refers to the same particle engine. Then
    // each ship will render the same particle stream.
    if (name == "Rectangle")
        return std::make_shared<gfx::Rectangle>();
    else if (name == "Triangle")
        return std::make_shared<gfx::Triangle>();
    else if (name == "Circle")
        return std::make_shared<gfx::Circle>();
    else if (name == "Arrow")
        return std::make_shared<gfx::Arrow>();
    else if (name == "RoundRect")
        return std::make_shared<gfx::RoundRectangle>();

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

std::string Workspace::ResolveFile(gfx::ResourceLoader::ResourceType type, const std::string& file) const
{
    // see comments in AddFile about resource path mapping.
    // this method is only called by the graphics/ subsystem

    QString ret = FromUtf8(file);
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (ret.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (ret.startsWith("fs://"))
        ret.remove(0, 5);

    // special case for resources that are hard coded in the engine without
    // any resource location identifier. (such as shaders)
    // if it's just relative we expect it's relative to the application's
    // installation folder.
    const QFileInfo info(ret);
    if (info.isRelative())
    {
        // assuming it's a resource deployed as part of this application.
        // so we resolve the path based on the applications's
        // installation location. (for example a shader file)
        ret = JoinPath(GetAppDir(), ret);
    }

    DEBUG("Mapping gfx resource '%1' => '%2'", file, ret);

    return ToUtf8(ret);
}

bool Workspace::Load(const QString& dir)
{
    ASSERT(mWorkspaceDir.isEmpty());

    if (!LoadContent(JoinPath(dir, "content.json")) ||
        !LoadWorkspace(JoinPath(dir, "workspace.json")))
        return false;

    INFO("Loaded workspace '%1'", dir);
    mWorkspaceDir = dir;
    if (!mWorkspaceDir.endsWith("/") && !mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("/");
    return true;
}

bool Workspace::OpenNew(const QString& dir)
{
    // this is where we could initialize the workspace with some resources
    // or whatnot.
    mWorkspaceDir = dir;
    if (!mWorkspaceDir.endsWith("/") && !mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("/");

    return true;
}

bool Workspace::Save()
{
    ASSERT(!mWorkspaceDir.isEmpty());

    if (!SaveContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !SaveWorkspace(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    INFO("Saved workspace '%1'", mWorkspaceDir);
    return true;
}

QString Workspace::GetName() const
{
    return mWorkspaceDir;
}

QString Workspace::AddFileToWorkspace(const QString& file)
{
    // don't remap already mapped files.
    if (file.startsWith("app://") ||
        file.startsWith("fs://") ||
        file.startsWith("ws://"))
        return file;

    // when the user is adding resource files to this project (workspace) there's the problem
    // of making the workspace "portable".
    // Portable here means two things:
    // * portable from one operating system to another (from Windows to Linux or vice versa)
    // * portable from one user's enviroment to another user's environment even when on
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
    static const auto& appdir = QCoreApplication::applicationDirPath();

    if (file.startsWith(mWorkspaceDir))
    {
        QString ret = file;
        ret.remove(0, mWorkspaceDir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);

        return QString("ws://%1").arg(ret);
    }
    else if (file.startsWith(appdir))
    {
        QString ret = file;
        ret.remove(0, appdir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);

        return QString("app://%1").arg(ret);
    }
    // mapping other paths to identity. will not be portable to another
    // user's computer to another system, unless it's accessible on every
    // machine using the same path (for example a shared file system mount)
    return QString("fs://%1").arg(file);
}

QString Workspace::MapFileToFilesystem(const QString& file) const
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.

    QString ret = file;
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (file.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (file.startsWith("fs://"))
        ret.remove(0, 5);

    // return as is
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
        INFO("No workspace content found in file: '%1'", filename);
        return true;
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
    if (json.contains("animations"))
    {
        for (const auto& json_p : json["animations"].items())
        {
            const auto& name = app::FromUtf8(json_p.value()["resource_name"]);
            std::optional<game::Animation> ret = game::Animation::FromJson(json_p.value());
            if (!ret.has_value())
            {
                ERROR("Failed to load animation '%1' properties.", name);
                continue;
            }
            auto& animation = ret.value();
            DEBUG("Loaded animation: '%1'", name);
            resources.push_back(std::make_unique<AnimationResource>(std::move(animation), name));
        }
    }

    mResources = std::move(resources);
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

    INFO("Saved workspace content in '%1'", filename);
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
    return true;
}

QStringList Workspace::ListMaterials() const
{
    QStringList list;
    list << "Checkerboard";

    QStringList colors;
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string name(magic_enum::enum_name(val));
        colors << FromUtf8(name);
    }
    colors.sort();
    list << colors;

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
    list << "Arrow";
    list << "Circle";
    list << "Rectangle";
    list << "RoundRect";
    list << "Triangle";

    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::ParticleSystem)
            list.append(res->GetName());
    }
    return list;
}

bool Workspace::HasMaterial(const QString& name) const
{
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
