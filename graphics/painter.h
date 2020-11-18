// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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
#  include <glm/glm.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>

namespace gfx
{
    // fwd declarations
    class Device;
    class Drawable;
    class Material;
    class Color4f;
    class Transform;

    // Painter class implements some algorithms for
    // drawing different types of objects on the screen.
    class Painter
    {
    public:
        virtual ~Painter() = default;

        // Set the current render target view port.
        // x and y are the top left coordinate (in pixels) of the
        // viewport's location wrt to the actual render target.
        // width and height are the dimensions of the viewport.
        virtual void SetViewport(int x, int y, unsigned with, unsigned height) = 0;

        // Clear the render target with the given clear color.
        // You probably want to do this as the first step before
        // doing any other drawing.
        virtual void Clear(const Color4f& color) = 0;

        // Draw the given shape using the given material with the given transformation.
        virtual void Draw(const Drawable& shape, const Transform& transform, const Material& mat) = 0;

        // Draw using a mask to cover some areas where painting is not desired.
        // This is a two step draw operation:
        // 1. Draw the mask shape using the given mask transform into the stencil buffer.
        // 2. Draw the actual shape using the given material and transform into the color
        //    buffer in the area not covered by the mask.
        virtual void Draw(const Drawable& drawShape, const Transform& drawTransform,
                          const Drawable& maskShape, const Transform& maskTransform,
                          const Material& material) = 0;

        struct MaskShape {
            const glm::mat4* transform = nullptr;
            const Drawable*  drawable  = nullptr;
        };
        struct DrawShape {
            const glm::mat4* transform = nullptr;
            const Drawable* drawable   = nullptr;
            const Material* material   = nullptr;
        };
        // Draw the shapes in the draw list after combining all the shapes in the mast list
        // into a single mask that covers some areas of the render buffer.
        virtual void Draw(const std::vector<DrawShape>& draw_list, const std::vector<MaskShape>& mask_list) = 0;

        virtual void Draw(const std::vector<DrawShape>& shapes) = 0;

        // Create new painter implementation using the given graphics device.
        static std::unique_ptr<Painter> Create(std::shared_ptr<Device> device);
        static std::unique_ptr<Painter> Create(Device* device);
    private:
    };

} // namespace
