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

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "graphics/types.h"
#include "graphics/color4f.h"

namespace gfx
{
    // fwd declarations
    class Device;
    class Drawable;
    class Material;
    class Transform;

    // Painter class implements some algorithms for
    // drawing different types of objects on the screen.
    class Painter
    {
    public:
        virtual ~Painter() = default;
        // Set the ratio of rendering surface pixels to game units.
        virtual void SetPixelRatio(const glm::vec2& ratio) = 0;
        // Set the size of the target rendering surface. (The surface that
        // is the backing surface of the device context).
        // The surface size is needed for viewport and scissor settings.
        // You should call this whenever the surface (for example the window)
        // has been resized. The setting will be kept until the next call
        // to surface size.
        virtual void SetSurfaceSize(const USize& size) = 0;
        // Set the current render target/device view port.
        // x and y are the top left coordinate (in pixels) of the
        // viewport's location wrt to the actual render target.
        // width and height are the dimensions of the viewport.
        virtual void SetViewport(const IRect& viewport) = 0;
        // Set the current clip (scissor) rect that will limit the rasterized
        // fragments inside the scissor rectangle only and discard any fragments
        // that would be rendered outside the scissor rect.
        // The scissor rectangle is defined in the render target coordinate space
        // (i.e window coordinates). X and Y are the top left corner of the
        // scissor rectangle with width extending to the right and height
        // extending towards the bottom.
        virtual void SetScissor(const IRect& scissor) = 0;
        // Clear any scissor setting.
        virtual void ClearScissor() = 0;
        // Set the current projection matrix for projecting the scene onto a 2D plane.
        virtual void SetProjectionMatrix(const glm::mat4& proj) = 0;
        // Set an additional view transformation matrix that gets multiplied into
        // every view matrix/transform when doing any actual draw operations. This
        // is useful when using some drawing system that doesn't support arbitrary
        // transformations.
        virtual void SetViewMatrix(const glm::mat4& view) = 0;
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
        // todo:
        virtual void Draw(const std::vector<DrawShape>& shapes) = 0;

        // Create new painter implementation using the given graphics device.
        static std::unique_ptr<Painter> Create(std::shared_ptr<Device> device);
        static std::unique_ptr<Painter> Create(Device* device);

        // convenience and helper functions.

        static glm::mat4 MakeOrthographicProjection(const FRect& rect);
        // Set the projection matrix of the painter to an orthographic
        // projection matrix. Essentially this creates a logical viewport
        // (and coordinate transformation) into the scene to be rendered
        // such that objects that are placed within the rectangle defined
        // by the top left and bottom right coordinates are visible in the
        // rendered scene.
        // For example if left = 0.0f and width = 10.0f an object A that is
        // 5.0f in width and is at coordinate -5.0f would not be shown.
        // While an object B that is at 1.0f and is 2.0f units wide would be
        // visible in the scene.
        // Do not confuse this with SetViewport which defines the viewport
        // in terms of device/render target coordinate space. I.e. the area
        // of the rendering surface where the pixels of the rendered scene are
        // placed in the surface.
        void SetOrthographicView(const FRect& view);
        void SetOrthographicView(float left, float top, float width, float height);
        void SetOrthographicView(float width, float height);

        void SetViewport(int x, int y, unsigned width, unsigned height);
        void SetScissor(int x, int y, unsigned width, unsigned height);
        void SetSurfaceSize(unsigned width, unsigned height);

        inline void ResetViewMatrix()
        { SetViewMatrix(glm::mat4(1.0f)); }
    private:
    };

} // namespace
