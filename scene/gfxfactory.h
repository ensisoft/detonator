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

#include "warnpush.h"

#include "warnpop.h"

namespace gfx {
    class Material;
    class Drawable;
}// namespace

namespace scene
{
    // Producer/loader of GFX resources, i.e. materials and drawables.
    // Used to load/create GFX resource instances of types created
    // by the user in the editor such as particle engines or materials.
    // Simple shapes such as "rectangle" or "circle" should be created
    // through some other method.
    class GfxFactory
    {
    public:
        virtual ~GfxFactory() = default;
        //
        // == About sharing/not-sharing gfx resources ==
        //
        // The objects can either be private (and unique) or shared.
        // A unique object means that for example each animation would have it's own
        // instance of some material 'foo' and is responsible for updating the material
        // to update a sprite animation for example. This means that multiple such instances
        // of 'foo' material will consume space multiple times (i.e. the material parameters will
        // be duplicated, but the immutable GPU resources are always shared)
        // and each such instance will have it's own animation state. In other words they can
        // be at different phases of animation.
        // Sharing an instance however reduces the memory consumption but such objects need to be
        // updated only once and will render the same outcome when used by multiple shapes.

        // Create an instance of a material identified by the material name.
        // todo: share/nonshare flag
        virtual std::shared_ptr<gfx::Material> MakeMaterial(const std::string& name) const = 0;
        virtual std::shared_ptr<gfx::Drawable> MakeDrawable(const std::string& name) const = 0;
    private:
    };
} // namespace
