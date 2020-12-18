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

#pragma once

#include "config.h"

#include <memory>

namespace gfx {
    class MaterialClass;
    class DrawableClass;
} // gfx

namespace game
{
    class AnimationClass;
    class SceneClass;

    // Interface for looking up game resource class objects such as materials, drawables etc.
    // Every call to find any particular class object will always return the same single
    // instance of the class object. The class objects should be treated as immutable
    // resources created by the asset pipeline and loaded from the descriptor file(s).
    // Note about user defined resource names:
    // If a resource has it's name changed you will need to remember to update your code that
    // calls some method to look up the resource by its name such as FindAnimationClassByName.
    // For robustness against name changes a better option is to use the class object IDs which
    // are immutable.
    class ClassLibrary
    {
    public:
        virtual ~ClassLibrary() = default;

        virtual std::shared_ptr<const gfx::MaterialClass> FindMaterialClass(const std::string& id) const = 0;
        virtual std::shared_ptr<const gfx::DrawableClass> FindDrawableClass(const std::string& id) const = 0;
        // Find an animation class by the given name.
        // If not found will return a nullptr.
        virtual std::shared_ptr<const AnimationClass> FindAnimationClassByName(const std::string& name) const = 0;
        // Find an animation class object by it's unique class object identifier.
        // If not found will return a nullptr.
        virtual std::shared_ptr<const AnimationClass> FindAnimationClassById(const std::string& id) const = 0;
        // Find a scene class object by the given name.
        // If not found will return a nullptr.
        virtual std::shared_ptr<const SceneClass> FindSceneClassByName(const std::string& name) const = 0;
        // Find a scene class object by the given id.
        // if not found will return a nullptr.
        virtual std::shared_ptr<const SceneClass> FindSceneClassById(const std::string& id) const = 0;
        // Load content from a JSON file. Expects the file to be well formed, on
        // an ill-formed JSON file an exception is thrown.
        // No validation is done regarding the completeness of the loaded content,
        // I.e. it's possible that classes refer to resources (i.e. other classes)
        // that aren't available.
        virtual void LoadFromFile(const std::string& dir, const std::string& file) = 0;
    private:
    };
} // namespace
