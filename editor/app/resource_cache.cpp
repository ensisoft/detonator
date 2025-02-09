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
#include "editor/app/resource_tracker.h"

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
        const auto name = mResource->GetNameUtf8();
        mState->resources[mResourceId] = std::move(mResource);
        VERBOSE("Add resource to cache. [id=%1], name='%2']", mResourceId, name);
    }
private:
    std::shared_ptr<CacheState> mState;
    std::string mResourceId;
    std::unique_ptr<Resource> mResource;
};

class ResourceCache::DelResourceTask : public base::ThreadTask {
public:
    DelResourceTask(std::shared_ptr<CacheState> state, std::string id)
    : mState(std::move(state))
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
        DEBUG("Update project settings in cache.");
    }
private:
    std::shared_ptr<CacheState> mState;
    app::ProjectSettings mSettings;
};

class ResourceCache::BuildResourceGraphTask : public base::ThreadTask {
public:
    explicit BuildResourceGraphTask(std::shared_ptr<CacheState> state)
      : mState(std::move(state))
    {}
    void DoTask() override
    {
        DEBUG("Build resource cache");
        mState->graph.clear();

        for (const auto& pair : mState->resources)
        {
            // make sure to have a resource node in the graph for
            // *this* resource.
            if (!base::Contains(mState->graph, pair.first))
                mState->graph[pair.first] = ResourceNode {};

            ListDependencies(pair.first);
        }
        DEBUG("Resource cache graph is ready!");
    }
    void ListDependencies(const std::string& resource_id)
    {
        auto it = mState->resources.find(resource_id);
        if (it == mState->resources.end()) // broken dependency.
            return;
        const auto& resource = it->second;

        QStringList dependencies = resource->ListDependencies();

        for (const auto& dependency_id : dependencies)
        {
            TrackDependency(resource_id, app::ToUtf8(dependency_id));
            ListDependencies(app::ToUtf8(dependency_id));
        }
    }

    void TrackDependency(const std::string& this_id, const std::string& depends_on_id)
    {
        auto& this_node = mState->graph[this_id];
        auto& used_node = mState->graph[depends_on_id];

        //this_node.uses.insert(depends_on_id);
        used_node.used_by.insert(this_id);
    }

private:
    std::shared_ptr<CacheState> mState;
};

class ResourceCache::ValidateResourceTask : public base::ThreadTask {
public:
    explicit ValidateResourceTask(std::shared_ptr<CacheState> state, std::string resourceId, std::string resource_name)
      : mResourceId(std::move(resourceId))
      , mResourceName(std::move(resource_name))
      , mState(std::move(state))
    {}
    void DoTask() override
    {
        // this resource is valid when:
        // - all of it dependencies are met. both resources and files.
        // - all of it dependencies are valid.

        // after determining the validity of *this*
        // resource we must propagate this information to all the
        // users. if this resource is invalid then all users are
        // invalid as well. If this resource is valid then the
        // users must be inspected whether all their other dependent
        // resources are valid too.

        const auto is_valid = ValidateResource(mResourceId);

        VERBOSE("Analyzing resource. [id=%1, name='%2', valid=%3]", mResourceId, mResourceName, is_valid);

        auto* resource = base::SafeFind(mState->resources, mResourceId);
        ASSERT(resource);
        resource->SetProperty("is-valid", is_valid);

        // push a report pertaining to this resource.
        {
            std::lock_guard<std::mutex> lock(mState->mutex);

            AnalyzeResourceReport report;
            report.valid = is_valid;
            report.id    = mResourceId;
            mState->update_queue.emplace_back(std::move(report));
        }

        // find this node from the resource graph
        // and then schedule an analyze task on each of the resources
        // that depend on this resource.
        // We know that if this resource is broken then those are
        // broken too, but this task based approach avoids having
        // to recursion here.
        auto* graph_node = base::SafeFind(mState->graph, mResourceId);
        ASSERT(graph_node);

        for (const auto& id: graph_node->used_by)
        {
            std::lock_guard<std::mutex> lock(mState->mutex);

            auto* resource = base::SafeFind(mState->resources, id);

            if (is_valid)
            {
                // if this resource is valid then delete the cached validity
                // flag in order to precipitate re-analysis of the resource.
                // just because this resource is valid does not mean that the
                // user resource is valid since that resource's validity depends
                // on other dependents too!
                resource->DeleteProperty("is-valid");
            }
            else
            {
                // in the contrast if this resource is invalid then we already
                // know that the user resource will be broken too!
                // so we can set this flag and skip some of the more elaborate
                // analysis.
                resource->SetProperty("is-valid", false);
            }

            auto task = std::make_unique<ValidateResourceTask>(mState, id, resource->GetNameUtf8());
            task->SetTaskName("AnalyzeResource");
            task->SetTaskDescription(base::FormatString("Analyze '%1'", resource->GetNameUtf8()));
            mState->submit_queue.push_back(std::move(task));
        }

    }
    bool ValidateResource(const std::string& resourceId)
    {
        auto it = mState->resources.find(resourceId);
        if (it == mState->resources.end())
            return false;

        auto& resource = it->second;
        if (resource->IsPrimitive())
            return true;

        // we can cache the result of any previous validation in the resource
        // and use that as a shortcut. The property will implicitly get
        // cleared when the resource is updated since that will copy
        // the source resource over into the cache.
        if (resource->HasProperty("is-valid"))
        {
            return resource->GetProperty("is-valid", true);
        }

        QStringList dependencies = resource->ListDependencies();

        for (const auto& dep_id : dependencies)
        {
            if (!ValidateResource(app::ToUtf8(dep_id)))
            {
                resource->SetProperty("is-valid", false);
                return false;
            }
        }

        // check the files.
        std::unordered_set<AnyString> file_uris;

        ResourceTracker tracker(mState->workspace_dir, &file_uris);

        resource->Pack(tracker);

        for (const auto& uri : file_uris)
        {
            const auto& file = app::MapUriToFile(uri, mState->workspace_dir);
            if (!app::FileExists(file))
            {
                resource->SetProperty("is-valid", false);
                return false;
            }
        }
        resource->SetProperty("is-valid", true);
        return true;
    }
private:
    const std::string mResourceId;
    const std::string mResourceName;
    std::shared_ptr<CacheState> mState;
};

class ResourceCache::InvalidateResourceTask : public base::ThreadTask {
public:
    InvalidateResourceTask(std::shared_ptr<CacheState> state, std::string resourceId)
      : mResourceId(std::move(resourceId))
      , mState(std::move(state))
    {}
    void DoTask() override
    {
        // when a resource is deleted, everything that depends on this resource
        // will be broken.

        const auto* graph_node = base::SafeFind(mState->graph, mResourceId);
        ASSERT(graph_node);

        for (const auto& id : graph_node->used_by)
        {
            std::lock_guard<std::mutex> lock(mState->mutex);

            auto* resource = base::SafeFind(mState->resources, id);
            resource->SetProperty("is-valid", false);

            // use task scheduling instead of recursion.
            auto task = std::make_unique<ValidateResourceTask>(mState, id, resource->GetNameUtf8());
            task->SetTaskName("AnalyzeResource");
            task->SetTaskDescription(base::FormatString("Analyze '%1'", resource->GetNameUtf8()));
            mState->submit_queue.push_back(std::move(task));
        }
    }
private:
    const std::string mResourceId;
    std::shared_ptr<CacheState> mState;
};

ResourceCache::ResourceCache(const QString& ws_dir, SubmitTask submit_function)
  : mWorkspaceDir(FixWorkspacePath(ws_dir))
  , mSubmitTask(submit_function)
  , mState(std::make_shared<CacheState>())
{
    mState->workspace_dir = mWorkspaceDir;
}

ResourceCache::~ResourceCache()
{
    ASSERT(!HasPendingWork());
}

void ResourceCache::AddResource(std::string id, std::unique_ptr<Resource> copy)
{
    copy->DeleteProperty("is-valid");

    const auto name = copy->GetNameUtf8();

    auto add_task = std::make_unique<AddResourceTask>(mState, id, std::move(copy));
    add_task->SetTaskName("AddCacheResource");
    add_task->SetTaskDescription(base::FormatString("Add resource '%1' to cache.", name));
    mPendingWork.push_back(mSubmitTask(std::move(add_task)));

    if (!mHaveGraph)
        return;

    auto build_graph_task = std::make_unique<BuildResourceGraphTask>(mState);
    build_graph_task->SetTaskName("BuildGraph");
    build_graph_task->SetTaskDescription("Update resource graph");
    mPendingWork.push_back(mSubmitTask(std::move(build_graph_task)));

    auto validate_resource_task = std::make_unique<ValidateResourceTask>(mState, id, name);
    validate_resource_task->SetTaskName("AnalyzeResource");
    validate_resource_task->SetTaskDescription(base::FormatString("Analyze '%1'", name));
    mPendingWork.push_back(mSubmitTask(std::move(validate_resource_task)));
}

void ResourceCache::DelResource(std::string id)
{
    auto del_task = std::make_unique<DelResourceTask>(mState, id);
    del_task->SetTaskName("DeleteCacheResource");
    del_task->SetTaskDescription("Delete resource from cache.");
    auto work = mSubmitTask(std::move(del_task));
    mPendingWork.push_back(work);

    if (!mHaveGraph)
        return;

    auto invalidate_resource_task = std::make_unique<InvalidateResourceTask>(mState, id);
    invalidate_resource_task->SetTaskName("InvalidateResource");
    invalidate_resource_task->SetTaskDescription("Invalidate resource users.");
    mPendingWork.push_back(mSubmitTask(std::move(invalidate_resource_task)));

    auto build_graph_task = std::make_unique<BuildResourceGraphTask>(mState);
    build_graph_task->SetTaskName("BuildGraph");
    build_graph_task->SetTaskDescription("Update resource graph");
    mPendingWork.push_back(mSubmitTask(std::move(build_graph_task)));
}

void ResourceCache::UpdateSettings(const ProjectSettings& settings)
{
    auto task = std::make_unique<UpdateSettingsTask>(mState, settings);
    task->SetTaskName("UpdateCacheSettings");
    task->SetTaskDescription("Update project settings in cache.");
    auto work = mSubmitTask(std::move(task));
    mPendingWork.push_back(work);
}

void ResourceCache::SaveWorkspace(const QVariantMap& workspace_properties,
                                  const QVariantMap& workspace_user_properties,
                                  const QString& workspace_directory)
{
    auto task = std::make_unique<SaveResourcesTask>(mState, workspace_properties,
                                                    workspace_user_properties, workspace_directory);
    task->SetTaskName("SaveWorkspace");
    task->SetTaskDescription("Save project workspace.");
    auto work = mSubmitTask(std::move(task));
    mPendingWork.push_back(work);
}

void ResourceCache::TickPendingWork()
{
    std::vector<std::unique_ptr<base::ThreadTask>> next_tasks;

    if (mState->mutex.try_lock())
    {
        next_tasks = std::move(mState->submit_queue);
        mState->submit_queue.clear();
        mState->mutex.unlock();
    }

    for (auto& task : next_tasks)
    {
        mPendingWork.push_back(mSubmitTask(std::move(task)));
    }

    while (!mPendingWork.empty())
    {
        auto work = mPendingWork.front();
        if (!work.IsComplete())
            return;

        auto* task = work.GetTask();

        VERBOSE("Task '%1' is complete.", task->GetTaskDescription());

        mPendingWork.pop_front();
    }
}

bool ResourceCache::HasPendingWork() const
{
    if (!mPendingWork.empty())
        return true;

    std::lock_guard<std::mutex> lock(mState->mutex);
    if (!mState->submit_queue.empty())
        return true;

    return false;
}

void ResourceCache::BuildCache()
{
    auto build_graph_task = std::make_unique<BuildResourceGraphTask>(mState);
    build_graph_task->SetTaskName("BuildGraph");
    build_graph_task->SetTaskDescription("Build resource graph");
    auto work = mSubmitTask(std::move(build_graph_task));
    mPendingWork.push_back(work);

    for (const auto& it : mState->resources)
    {
        const auto& id = it.first;
        const auto& res = it.second;
        if (res->IsPrimitive())
            continue;

        auto task = std::make_unique<ValidateResourceTask>(mState, id, res->GetNameUtf8());
        task->SetTaskName("AnalyzeResource");
        task->SetTaskDescription(base::FormatString("Analyze '%1'", res->GetNameUtf8()));
        mPendingWork.push_back(mSubmitTask(std::move(task)));
    }

    mHaveGraph = true;
}

void ResourceCache::ClearCache()
{
    ASSERT(!HasPendingWork());
}

void ResourceCache::DequeuePendingUpdates(ResourceUpdateList* out)
{
    if (mState->mutex.try_lock())
    {
        *out = std::move(mState->update_queue);
        mState->mutex.unlock();
    }
}


} // namespace