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

    // AssetTable is the high level interface for a game/app to
    // access the high level game content/assets.
    class AssetTable
    {
    public:
        virtual ~AssetTable() = default;

        // Find an animation by the given name. If not found will return a nullptr.
        // The returned instance is the single global instance of this animation
        // and every call will return the same object.
        virtual const Animation* FindAnimation(const std::string& name) const = 0;

        // Find an animation by the given name. If not found will return a nullptr.
        // The returned instance is the single object instance of this animation
        // and every call will return the same object.
        virtual Animation* FindAnimation(const std::string& name) = 0;

        // Load content from a JSON file. Expects the file to be well formed, on
        // an ill-formed JSON file an exception is thrown.
        // No validation is done regarding the completeness of the loaded content,
        // I.e. it's possible that assets refer to resources (or other assets)
        // that aren't available.
        virtual void LoadFromFile(const std::string& dir, const std::string& file) = 0;
    private:
    };

} // namespace
