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
#include "graphics/transform.h"
#include "graphics/device.h"

namespace gfx
{
    // fwd declarations
    class Drawable;
    class Material;
    class ShaderPass;
    class Framebuffer;

    // Painter class implements some algorithms for
    // drawing different types of objects on the screen.
    class Painter
    {
    public:
        virtual ~Painter() = default;
        // Get the current graphics set on the painter.
        virtual Device* GetDevice() = 0;
        // Set flag that enables "editing" mode which indicates
        // that even content marked "static" should be checked against
        // modifications and possibly regenerated/reuploaded to the device.
        // The default is false.
        virtual void SetEditingMode(bool on_off) = 0;
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
        // Get the current view matrix if any (could be I)
        virtual const glm::mat4& GetViewMatrix() const = 0;
        // Get the current projection matrix.
        virtual const glm::mat4& GetProjMatrix() const = 0;
        // Get the current configured render surface size. See the comments in the
        // SetSurfaceSize regarding the meaning of this setting.
        virtual USize GetSurfaceSize() const = 0;
        // Clear the current render target color buffer with the given clear color.
        virtual void ClearColor(const Color4f& color) = 0;
        // Clear the current render target stencil buffer with the given stencil value.
        virtual void ClearStencil(const StencilClearValue& stencil) = 0;

        using StencilFunc = Device::State::StencilFunc;
        using StencilOp   = Device::State::StencilOp;
        using DepthTest   = Device::State::DepthTest;

        struct RenderPassState {
            bool write_color = true;
            StencilOp stencil_op = StencilOp::DontModify;
            // the stencil test function.
            StencilFunc  stencil_func  = StencilFunc::Disabled;
            // what to do when the stencil test fails.
            StencilOp    stencil_fail  = StencilOp::DontModify;
            // what to do when the stencil test passes.
            StencilOp    stencil_dpass = StencilOp::DontModify;
            // what to do when the stencil test passes but depth test fails.
            StencilOp    stencil_dfail = StencilOp::DontModify;
            // todo:
            std::uint8_t stencil_mask  = 0xff;
            // todo:
            std::uint8_t stencil_ref   = 0x0;

            DepthTest  depth_test = DepthTest::LessOrEQual;
        };

        struct DrawShape {
            const glm::mat4* transform = nullptr;
            const Drawable* drawable   = nullptr;
            const Material* material   = nullptr;
            // This is a user defined object pointer that is passed to
            // ShaderPass::FilterDraw for doing low level shape/draw filtering.
            // Using dodgy/Unsafe void* here for performance reasons. vs. std::any
            void* user = nullptr;
        };
        using DrawList = std::vector<DrawShape>;

        // Draw multiple objects inside a render pass. Each object has a drawable shape
        // which provides the geometrical information of the object to be drawn. A material
        // which provides the "look&feel" i.e. the surface properties for the shape
        // and finally a transform which defines the model-to-world transform.
        virtual void Draw(const DrawList& shapes, const RenderPassState& renderp, const ShaderPass& shaderp) = 0;

        // Create new painter implementation using the given graphics device.
        static std::unique_ptr<Painter> Create(std::shared_ptr<Device> device);
        static std::unique_ptr<Painter> Create(Device* device);

        // Utility helpers
        void SetViewport(int x, int y, unsigned width, unsigned height);
        void SetScissor(int x, int y, unsigned width, unsigned height);
        void SetSurfaceSize(unsigned width, unsigned height);

        inline void ResetViewMatrix()
        { SetViewMatrix(glm::mat4(1.0f)); }

        // Similar to the legacy draw except that allows the device state to be
        // changed through state and shader pass objects.
        void Draw(const Drawable& shape,
                  const glm::mat4& model,
                  const Material& material,
                  const RenderPassState& renderp,
                  const ShaderPass& shaderp);

        // legacy functions.

        // Legacy immediate mode draw function.
        // Draw the shape with the material and transformation immediately in the
        // current render target. The following default render target state is used:
        // - writes color buffer, all channels
        // - doesn't do depth testing
        // - doesn't do stencil testing
        // Using this function is inefficient and requires the correct draw order
        // to be managed by the caller since depth testing isn't used. Using this
        // function is not efficient since the device state is changed on every draw
        // to the required state. If possible prefer the vector format which allows
        // to draw multiple objects at once.
        void Draw(const Drawable& drawable, const glm::mat4& model, const Material& material);
    private:
    };

} // namespace
