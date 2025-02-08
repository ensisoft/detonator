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

#include "base/assert.h"
#include "base/utility.h"
#include "data/json.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/resource_cache.h"

// WARNING WARNING WARNING
// ONLY DEBUG / VERBOSE LEVEL LOGS ARE THREAD SAFE
// APP EVENT LOG IS NOT YET THREAD SAFE !!

namespace app
{

class ResourceCache::AddResourceTask : public base::ThreadTask {
public:
public:
    AddResourceTask(std::shared_ptr<CacheState> state, std::string id, std::unique_ptr<Resource> copy)
       : mState(std::move(state))
       , mResourceId(std::move(id))
       , mResource(std::move(copy))
    {}
    void DoTask() override
    {
        mState->resources[mResourceId] = std::move(mResource);
        VERBOSE("Add resource to cache. [id=%1]", mResourceId);
    }
private:
    std::shared_ptr<CacheState> mState;
    std::string mResourceId;
    std::unique_ptr<Resource> mResource;
};

class ResourceCache::DelResourceTask : public base::ThreadTask {
public:
    DelResourceTask(std::shared_ptr<CacheState> state, std::string id)
    : mState(state)
    , mResourceId(std::move(id))
    {}

    void DoTask() override
    {
        mState->resources.erase(mResourceId);
        VERBOSE("Delete resource from cache. [id=%1]", mResourceId);
    }
private:
    std::shared_ptr<CacheState> mState;
    std::string mResourceId;
};

class ResourceCache::SaveResourcesTask : public base::ThreadTask {
public:
    SaveResourcesTask(std::shared_ptr<CacheState> state,
                      const QVariantMap& workspace_properties,
                      const QVariantMap& workspace_user_properties,
                      const QString& workspace_directory)
      : mState(std::move(state))
      , mWorkspaceProperties(workspace_properties)
      , mWorkspaceUserProperties(workspace_user_properties)
      , mWorkspaceDirectory(workspace_directory)
    {}

    void DoTask() override
    {
        mTimer.Start();

        SaveContent();
        SaveProperties();
        SaveUserProperties();

        const auto seconds = mTimer.SinceStart();
        DEBUG("Workspace save took %1s", seconds);

    }
    bool SaveContent()
    {
        const auto filename = app::JoinPath(mWorkspaceDirectory, "content.json");

        data::JsonObject root;
        for (const auto& pair : mState->resources)
        {
            const auto& resource = pair.second;
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
            SetError(base::FormatString("Failed to write file. [file='%1']", app::ToUtf8(filename)));
            return false;
        }
        DEBUG("Wrote workspace content file. [file='%1']", filename);
        return true;
    }
    bool SaveProperties()
    {
        const auto filename = app::JoinPath(mWorkspaceDirectory, "workspace.json");

        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly))
        {
            SetError(base::FormatString("Failed to open file for writing. [file='%1']", app::ToUtf8(filename)));
            return false;
        }

        // our JSON root object
        QJsonObject json;

        QJsonObject project;
        IntoJson(project, mState->settings);

        // serialize the workspace properties into JSON
        json["workspace"] = QJsonObject::fromVariantMap(mWorkspaceProperties);
        json["project"]   = project;

        // serialize the properties stored in each and every
        // resource object.
        for (const auto& pair : mState->resources)
        {
            const auto& resource = pair.second;

            if (resource->IsPrimitive())
                continue;
            resource->SaveProperties(json);
        }
        // set the root object to the json document then serialize
        QJsonDocument docu(json);
        file.write(docu.toJson());
        file.close();
        DEBUG("Wrote workspace properties file. [file='%1']", filename);
        return true;
    }

    bool SaveUserProperties()
    {
        const auto filename = app::JoinPath(mWorkspaceDirectory, ".workspace_private.json");

        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly))
        {
            SetError(base::FormatString("Failed to open file for writing. [file='%1']", app::ToUtf8(filename)));
            return false;
        }
        QJsonObject json;
        json["user"] = QJsonObject::fromVariantMap(mWorkspaceUserProperties);
        for (const auto& pair : mState->resources)
        {
            const auto& resource = pair.second;

            if (resource->IsPrimitive())
                continue;
            resource->SaveUserProperties(json);
        }

        QJsonDocument docu(json);
        file.write(docu.toJson());
        file.close();
        DEBUG("Wrote user properties file. [file='%1']", filename);
        return true;
    }

private:
    std::shared_ptr<CacheState> mState;
    QVariantMap mWorkspaceProperties;
    QVariantMap mWorkspaceUserProperties;
    QString mWorkspaceDirectory;
    base::ElapsedTimer mTimer;
};


class ResourceCache::UpdateSettingsTask : public base::ThreadTask {
public:
    UpdateSettingsTask(std::shared_ptr<CacheState> state, const ProjectSettings& settings)
        : mState(std::move(state))
        , mSettings(settings)
    {}

    void DoTask() override
    {
        mState->settings = mSettings;
        VERBOSE("Update project settings in cache.");
    }
private:
    std::shared_ptr<CacheState> mState;
    app::ProjectSettings mSettings;
};


ResourceCache::ResourceCache(SubmitTask submit_function)
  : mSubmitTask(submit_function)
  , mState(std::make_shared<CacheState>())
{}

ResourceCache::~ResourceCache()
{
    ASSERT(mPendingWork.empty());
}

void ResourceCache::AddResource(std::string id, std::unique_ptr<Resource> copy)
{
    if (HasPendingWork())
    {
        auto task = std::make_unique<AddResourceTask>(mState, std::move(id), std::move(copy));
        task->SetTaskName("AddCacheResource");
        task->SetTaskDescription("Add resource to cache.");
        auto work = mSubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
        mPendingWork.push_back(work);
    }
    else
    {
        mState->resources[id] = std::move(copy);
        VERBOSE("Add resource to cache. [id=%1]", id);
    }
}
void ResourceCache::DelResource(std::string id)
{
    if (HasPendingWork())
    {
        auto task = std::make_unique<DelResourceTask>(mState, std::move(id));
        task->SetTaskName("DeleteCacheResource");
        task->SetTaskDescription("Delete resource from cache.");
        auto work = mSubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
        mPendingWork.push_back(work);
    }
    else
    {
        mState->resources.erase(id);
        VERBOSE("Delete resource frm cache. [id=%1]", id);
    }
}

void ResourceCache::UpdateSettings(const ProjectSettings& settings)
{
    if (HasPendingWork())
    {
        auto task = std::make_unique<UpdateSettingsTask>(mState, settings);
        task->SetTaskName("UpdateCacheSettings");
        task->SetTaskDescription("Update project settings in cache.");
        auto work = mSubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
        mPendingWork.push_back(work);
        VERBOSE("Update project settings in cache.");
    }
    else
    {
        mState->settings = settings;
        VERBOSE("Update project settings in cache.");
    }
}

void ResourceCache::SaveWorkspace(const QVariantMap& workspace_properties,
                                  const QVariantMap& workspace_user_properties,
                                  const QString& workspace_directory)
{
    auto task = std::make_unique<SaveResourcesTask>(mState, workspace_properties,
                                                    workspace_user_properties, workspace_directory);
    task->SetTaskName("SaveWorkspace");
    task->SetTaskDescription("Save project workspace.");
    auto work = mSubmitTask(std::move(task), base::ThreadPool::Worker0ThreadID);
    mPendingWork.push_back(work);
}

void ResourceCache::TickPendingWork()
{
    while (!mPendingWork.empty())
    {
        auto work = mPendingWork.front();
        if (!work.IsComplete())
            return;

        auto* task = work.GetTask();

        INFO("%1", task->GetTaskDescription());

        mPendingWork.pop_front();
    }
}

bool ResourceCache::HasPendingWork() const
{
    return !mPendingWork.empty();
}

void ResourceCache::ClearCache()
{
    ASSERT(!HasPendingWork());
}

} // namespace