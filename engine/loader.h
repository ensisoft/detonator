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

#pragma once

#include "config.h"

#include <unordered_map>
#include <memory>

#include "engine/classlib.h"
#include "graphics/resource.h"
#include "graphics/material.h"
#include "graphics/drawable.h"

namespace game
{
    class EntityClass;

    // Game content (assets + gfx resources) loader.
    // Provides access to high level game content, i.e. game assets such as
    // animations based on their descriptions in the JSON file(s).
    // Additionally implements the gfx::ResourceLoader in order to implement
    // access to the low level file based graphics resources such as
    // texture, font and shader files.
    class ContentLoader : public ClassLibrary,
                          public gfx::ResourceLoader
    {
    public:
        // ClassLibrary implementation.
        // resource creation for the game level subsystem.
        virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& name) const override;
        virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& name) const override;
        virtual ClassHandle<const EntityClass> FindEntityClassByName(const std::string& name) const override;
        virtual ClassHandle<const EntityClass> FindEntityClassById(const std::string& id) const override;
        virtual ClassHandle<const SceneClass> FindSceneClassByName(const std::string& name) const override;
        virtual ClassHandle<const SceneClass> FindSceneClassById(const std::string& id) const override;
        virtual void LoadFromFile(const std::string& dir, const std::string& file) override;
        // gfx::ResourceLoader implementation. Provides access to the
        // low level byte buffer / file system file resources such as
        // textures/font files, shaders used by the graphics subsystem.
        virtual std::string ResolveURI(gfx::ResourceLoader::ResourceType type, const std::string& URI) const override;

    private:
        std::string mResourceDir;
        std::string mResourceFile;
        // These are the material types that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::MaterialClass>> mMaterials;
        // These are the particle engine types that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::KinematicsParticleEngineClass>> mParticleEngines;
        // These are the custom shapes (polygons) that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::PolygonClass>> mCustomShapes;
        // These are the entities that have been loaded from
        // the resource file.
        std::unordered_map<std::string, std::shared_ptr<EntityClass>> mEntities;
        // These are the scenes that have been loaded from
        // the resource file.
        std::unordered_map<std::string, std::shared_ptr<SceneClass>> mScenes;
        // name table maps entity names to ids.
        std::unordered_map<std::string, std::string> mEntityNameTable;
        // name table maps scene names to ids.
        std::unordered_map<std::string, std::string> mSceneNameTable;

        // cache of URIs that have been resolved to file
        // names already.
        mutable std::unordered_map<std::string, std::string> mUriCache;

    };
} // namespace
