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

#include <string>

namespace game
{
    class Animation;
    class AnimationClass;

    // AssetTable is the high level interface for a game/app to
    // access the high level game content/assets.
    class AssetTable
    {
    public:
        virtual ~AssetTable() = default;
        // Find an animation class by the given name. If not found will return a nullptr.
        // The returned instance is the single global instance of this animation class object
        // and every call will return the same object. Note that the names are user defined
        // names given in the editor when creating the content. If a resource has it's name
        // changed you will need to remember to update your code that calls FindAnimationClassByName
        // as well. For robustness against name changes a better option is to use the class object
        // IDs which are immutable.
        virtual const AnimationClass* FindAnimationClassByName(const std::string& name) const = 0;
        // Find a class object by it's unique class object identifier.
        virtual const AnimationClass* FindAnimationClassById(const std::string& id) const = 0;
        // Create an instance of animation of the specific animation class type identified by name.
        virtual std::unique_ptr<Animation> CreateAnimationByName(const std::string& name) const = 0;
        virtual std::unique_ptr<Animation> CreateAnimationById(const std::string& id) const = 0;
        // Load content from a JSON file. Expects the file to be well formed, on
        // an ill-formed JSON file an exception is thrown.
        // No validation is done regarding the completeness of the loaded content,
        // I.e. it's possible that assets refer to resources (or other assets)
        // that aren't available.
        virtual void LoadFromFile(const std::string& dir, const std::string& file) = 0;
    private:
    };

} // namespace
