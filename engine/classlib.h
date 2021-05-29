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

namespace gfx {
    class MaterialClass;
    class DrawableClass;
} // gfx

namespace game
{
    class EntityClass;
    class SceneClass;

    // tiny abstraction around the fact that the class objects
    // are passed around as shared_ptrs which should be an
    // implementation detail. Todo: maybe write a proper handle
    // class to fully hide this.
    template<typename T>
    using ClassHandle = std::shared_ptr<const T>;

    // Interface for looking up game resource class objects such as materials, drawables etc.
    // Every call to find any particular class object will always return the same single
    // instance of the class object. The class objects should be treated as immutable
    // resources created by the asset pipeline and loaded from the descriptor file(s).
    // Note about user defined resource names:
    // If a resource has it's name changed you will need to remember to update your code that
    // calls some method to look up the resource by its name such as FindEntityClassByName.
    // For robustness against name changes a better option is to use the class object IDs which
    // are immutable.
    class ClassLibrary
    {
    public:
        virtual ~ClassLibrary() = default;

        virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const = 0;
        virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const = 0;
        // Find a entity class object by the given name.
        // If not found will return a nullptr.
        virtual ClassHandle<const EntityClass> FindEntityClassByName(const std::string& name) const = 0;
        // Find a entity class object by the given id.
        // if not found will return a nullptr.
        virtual ClassHandle<const EntityClass> FindEntityClassById(const std::string& id) const = 0;
        // Find a scene class object by the given name.
        // If not found will return a nullptr.
        virtual ClassHandle<const SceneClass> FindSceneClassByName(const std::string& name) const = 0;
        // Find a scene class object by the given id.
        // if not found will return a nullptr.
        virtual ClassHandle<const SceneClass> FindSceneClassById(const std::string& id) const = 0;
        // Load content from a JSON file. Expects the file to be well formed, on
        // an ill-formed JSON file an exception is thrown.
        // No validation is done regarding the completeness of the loaded content,
        // I.e. it's possible that classes refer to resources (i.e. other classes)
        // that aren't available.
        virtual void LoadFromFile(const std::string& dir, const std::string& file) = 0;
    private:
    };
} // namespace
