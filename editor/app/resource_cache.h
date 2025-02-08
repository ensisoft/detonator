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
#include <mutex>
#include <queue>
#include <deque>
#include <functional>

#include "base/threadpool.h"
#include "editor/app/project_settings.h"

namespace app
{
    class Resource;

    class ResourceCache
    {
    public:
        using TaskPtr = std::unique_ptr<base::ThreadTask>;
        using SubmitTask = std::function<base::TaskHandle (TaskPtr task, size_t threadId)>;

        explicit ResourceCache(SubmitTask submit_function);
       ~ResourceCache();

        void AddResource(std::string id, std::unique_ptr<Resource> copy);
        void DelResource(std::string id);
        void UpdateSettings(const ProjectSettings& settings);

        void SaveWorkspace(const QVariantMap& workspace_properties,
                           const QVariantMap& workspace_user_properties,
                           const QString& workspace_directory);

        void TickPendingWork();
        bool HasPendingWork() const;

        void ClearCache();

        base::TaskHandle GetFirstTask() const
        {
            return mPendingWork.front();
        }

    private:
        class AddResourceTask;
        class DelResourceTask;
        class SaveResourcesTask;
        class UpdateSettingsTask;

        using ResourceTable = std::unordered_map<std::string,
            std::unique_ptr<Resource>>;

        struct CacheState {
            ResourceTable resources;
            ProjectSettings settings;
        };

        SubmitTask mSubmitTask;
        std::shared_ptr<CacheState> mState;
        std::deque<base::TaskHandle> mPendingWork;
    };

} // namespace