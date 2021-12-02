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

#include <memory>

#include "engine/classlib.h"
#include "engine/data.h"
#include "audio/loader.h"
#include "graphics/loader.h"

namespace engine
{
    // in order to de-couple the loader application (GameMain) from
    // transitive dependencies such as the graphics subsystem etc.
    // this header is *pure* interface only. The loader implementations
    // can then be built into the shared engine library which will need
    // those deps in any case. This means that GameMain doesn't need to
    // link against all the various libs such as GfxLib, AudioLib etc.

    class GameData;
    using GameDataHandle = std::shared_ptr<const GameData>;

    // Interface for accessing game objects' data packaged with the game.
    class Loader
    {
    public:
        using GameDataHandle = engine::GameDataHandle;

        virtual ~Loader() = default;
        // Load game data based on a URI. The URI undergoes a resolution
        // and the content may be loaded from a resource pack etc.
        virtual GameDataHandle LoadGameData(const std::string& uri) const = 0;
        // Load game data from a file on the file system.
        virtual GameDataHandle LoadGameDataFromFile(const std::string& filename) const = 0;
    };

    // Loader implementation for loading all kinds of subsystem
    // resources and game data.
    class FileResourceLoader : public gfx::Loader,
                               public engine::Loader,
                               public audio::Loader
    {
    public:
        // Set the filesystem path of the current running binary on the file system.
        // Used for resolving data references relative to the application binary.
        virtual void SetApplicationPath(const std::string& path) = 0;
        // Set the filesystem path in which to look for resource files
        virtual void SetContentPath(const std::string& path) = 0;
        // Create new file resource loader.
        static std::unique_ptr<FileResourceLoader> Create();
    private:
    };

    // Load the Entity, Scene, Material etc. classes from a JSON file.
    class JsonFileClassLoader : public ClassLibrary
    {
    public:
        // Load game content from a JSON file. Expects the file to be well formed, on
        // an ill-formed JSON file an exception is thrown.
        // No validation is done regarding the completeness of the loaded content,
        // I.e. it's possible that classes refer to resources (i.e. other classes)
        // that aren't available.
        virtual void LoadFromFile(const std::string& file) = 0;
        // create new content loader.
        static std::unique_ptr<JsonFileClassLoader> Create();
    private:
    };
} // namespace
