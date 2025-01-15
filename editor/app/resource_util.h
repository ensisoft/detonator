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

#include <vector>

#include "data/reader.h"
#include "data/json.h"

#include "graphics/material_class.h"
#include "editor/app/resource.h"
#include "editor/app/resource_migration_log.h"
#include "editor/app/workspace_observer.h"

namespace app
{

template<typename ClassType>
bool LoadResources(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<app::Resource>>& vector,
                   app::ResourceMigrationLog* log = nullptr,
                   app::WorkspaceAsyncWorkObserver* observer = nullptr)
{
    VERBOSE("Loading resources. [type='%1']", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        auto chunk = data.GetChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->GetReader()->Read("resource_name", &name) ||
            !chunk->GetReader()->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        unsigned version = 0;
        chunk->GetReader()->Read("resource_ver", &version);

        chunk = app::detail::MigrateResourceDataChunk<ClassType>(std::move(chunk), log);

        ClassType ret;
        if (!ret.FromJson(*chunk->GetReader()))
        {
            WARN("Incomplete resource load from JSON. [type='%1', name='%2']", type, name);
            success = false;
        }
        auto resource = std::make_unique<app::GameResource<ClassType>>(std::move(ret), name);
        resource->SetProperty("__version", version);
        vector.push_back(std::move(resource));
        VERBOSE("Loaded workspace resource. [name='%1']", name);

        if (observer)
            observer->EnqueueStepIncrement();
    }

    return success;
}

template<typename ClassType>
bool LoadMaterials(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<app::Resource>>& vector,
                   app::ResourceMigrationLog* log = nullptr,
                   app::WorkspaceAsyncWorkObserver* observer = nullptr)
{

    VERBOSE("Loading resources. [type='%1']", type);
    bool success = true;
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        auto chunk = data.GetChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->GetReader()->Read("resource_name", &name) ||
            !chunk->GetReader()->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            success = false;
            continue;
        }
        unsigned version = 0;
        chunk->GetReader()->Read("resource_ver", &version);

        chunk = app::detail::MigrateResourceDataChunk<ClassType>(std::move(chunk), log);

        auto ret = ClassType::ClassFromJson(*chunk->GetReader());
        if (!ret)
        {
            WARN("Incomplete resource load from JSON. [type='%1', name='%2']", type, name);
            success = false;
            continue;
        }
        auto resource = std::make_unique<app::MaterialResource>(std::move(ret), name);
        resource->SetProperty("__version", version); // a small hack here.
        vector.push_back(std::move(resource));
        VERBOSE("Loaded workspace resource. [name='%1']", name);

        if (observer)
            observer->EnqueueStepIncrement();
    }
    return success;
}



} // namespace