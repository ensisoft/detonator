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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QString>
#  include <QVariantMap>
#include "warnpop.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <deque>
#include <functional>
#include <mutex>

#include "base/threadpool.h"
#include "editor/app/project_settings.h"

namespace app
{
    class Resource;

    class ResourceCache
    {
    public:
        using TaskPtr = std::unique_ptr<base::ThreadTask>;
        using SubmitTask = std::function<base::TaskHandle (TaskPtr task)>;

        struct AnalyzeResourceReport {
            std::string id;
            std::string msg;
            bool valid = true;
        };

        explicit ResourceCache(const QString& ws_dir, SubmitTask submit_function);
       ~ResourceCache();

        void AddResource(std::string id, std::unique_ptr<Resource> copy);
        void DelResource(std::string id);
        void UpdateSettings(const ProjectSettings& settings);

        void SaveWorkspace(const QVariantMap& workspace_properties,
                           const QVariantMap& workspace_user_properties,
                           const QString& workspace_directory);



        void TickPendingWork();
        bool HasPendingWork() const;
        void BuildCache();
        void ClearCache();

        base::TaskHandle GetFirstTask() const
        {
            if (mPendingWork.empty())
                return {};
            return mPendingWork.front();
        }

        using ResourceUpdate = std::variant<AnalyzeResourceReport>;
        using ResourceUpdateList = std::vector<ResourceUpdate>;

        void DequeuePendingUpdates(ResourceUpdateList* out);

        struct ResourceNode {
            // the collection of resources that this resource
            // depends on, identified by their IDs
            //std::unordered_set<std::string> uses; // not used right now

            // The collection of resources that depend on this resource.
            std::unordered_set<std::string> used_by;
        };
        using ResourceTable = std::unordered_map<std::string, std::unique_ptr<Resource>>;
        using ResourceGraph = std::unordered_map<std::string, ResourceNode>;

        // These functions exist only for testing and are NOT THREAD SAFE
        // so don't use them outside of testing (unless you know that the
        // access is safe and there are no current tasks).
        const ResourceGraph& GetResourceGraph_UNSAFE() const
        {
            // THIS IS NOT THREAD SAFE. FOR TESTING ONLY
            return mState->graph;
        }
        const ResourceTable& GetResourceTable_UNSAFE() const
        {
            // THIS IS NOT THREAD SAFE. FOR TESTING ONLY
            return  mState->resources;
        }

    private:
        class AddResourceTask;
        class DelResourceTask;
        class SaveResourcesTask;
        class UpdateSettingsTask;
        class BuildResourceGraphTask;
        class ValidateResourceTask;
        class InvalidateResourceTask;

        struct CacheState {
            QString workspace_dir;
            ResourceGraph  graph;
            ResourceTable resources;
            ProjectSettings settings;

            std::mutex mutex; // protect the submit and update queues
            std::vector<TaskPtr> submit_queue;
            std::vector<ResourceUpdate> update_queue;
        };
        const QString mWorkspaceDir;
        SubmitTask mSubmitTask;
        std::shared_ptr<CacheState> mState;
        std::deque<base::TaskHandle> mPendingWork;
        bool mHaveGraph = false;
    };

} // namespace