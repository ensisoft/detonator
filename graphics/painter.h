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
#include "graphics/drawable.h"
#include "graphics/material.h"

#include "base/snafu.h"

namespace gfx
{
    // fwd declarations
    class Drawable;
    class Material;
    class ShaderProgram;
    class Framebuffer;

    // Painter class implements some algorithms for
    // drawing different types of objects on the screen.
    class Painter
    {
    public:
        Painter() = default;
        explicit Painter(std::shared_ptr<Device> device)
          : mDeviceInst(device)
          , mDevice(device.get())
        {}
        explicit Painter(Device* device)
          : mDevice(device)
        {}

        // Set the current projection matrix for projecting the scene onto a 2D plane.
        inline void SetProjectionMatrix(const glm::mat4& matrix) noexcept
        { mProjMatrix = matrix; }
        // Set the current view matrix for transforming the scene into camera space.
        inline void SetViewMatrix(const glm::mat4& matrix) noexcept
        { mViewMatrix = matrix;}
        // Set the size of the target rendering surface. (The surface that
        // is the backing surface of the device context).
        // The surface size is needed for viewport and scissor settings.
        // You should call this whenever the surface (for example the window)
        // has been resized. The setting will be kept until the next call
        // to surface size.
        inline void SetSurfaceSize(const USize& size) noexcept
        { mSize = size; }
        inline void SetSurfaceSize(unsigned width, unsigned height) noexcept
        { mSize = USize(width, height); }
        // Set the current render target/device view port.
        // x and y are the top left coordinate (in pixels) of the
        // viewport's location wrt to the actual render target.
        // width and height are the dimensions of the viewport.
        inline void SetViewport(const IRect& viewport) noexcept
        { mViewport = viewport; }
        inline void SetViewport(int x, int y, unsigned width, unsigned height) noexcept
        { mViewport = IRect(x, y, width, height); }
        // Set the current clip (scissor) rect that will limit the rasterized
        // fragments inside the scissor rectangle only and discard any fragments
        // that would be rendered outside the scissor rect.
        // The scissor rectangle is defined in the render target coordinate space
        // (i.e window coordinates). X and Y are the top left corner of the
        // scissor rectangle with width extending to the right and height
        // extending towards the bottom.
        inline void SetScissor(const IRect& scissor) noexcept
        { mScissor = scissor; }
        inline void SetScissor(int x, int y, unsigned width, unsigned height) noexcept
        { mScissor = IRect(x, y, width, height); }
        // Clear the scissor rect to nothing, i.e. no scissor is set.
        inline void ClearScissor() noexcept
        { mScissor = IRect(0, 0, 0, 0); }
        // Set the ratio of rendering surface pixels to game units.
        inline void SetPixelRatio(const glm::vec2& ratio) noexcept
        { mPixelRatio = ratio; }
        // Reset the view matrix to identity matrix.
        inline void ResetViewMatrix() noexcept
        { mViewMatrix = glm::mat4{1.0f}; }
        // Set flag that enables "editing" mode which indicates, that even
        // content marked "static" should be checked against modifications
        // and possibly regenerated/re-uploaded to the device. Set this to
        // true in order to have live changes made to content take place
        // dynamically on every render.
        inline void SetEditingMode(bool on_off) noexcept
        { mEditingMode = on_off; }
        inline void SetDevice(std::shared_ptr<Device> device) noexcept
        { mDeviceInst = device; mDevice = mDeviceInst.get(); }
        inline void SetDevice(Device* device) noexcept
        { mDevice = device; }
        inline void SetFramebuffer(Framebuffer* fbo) noexcept
        { mFrameBuffer = fbo; }

        inline const glm::mat4& GetViewMatrix() const noexcept
        { return mViewMatrix; }
        inline const glm::mat4& GetProjMatrix() const noexcept
        { return mProjMatrix; }
        inline USize GetSurfaceSize() const noexcept
        { return mSize; }
        inline IRect GetViewport() const noexcept
        { return mViewport; }
        inline IRect GetScissor() const noexcept
        { return mScissor; }
        inline glm::vec2 GetPixelRatio() const noexcept
        { return mPixelRatio; }
        // Get the current graphics set on the painter.
        inline Device* GetDevice() noexcept
        { return mDevice; }
        inline const Device* GetDevice() const noexcept
        { return mDevice; }
        inline Framebuffer* GetFramebuffer() noexcept
        { return mFrameBuffer; }
        inline const Framebuffer* GetFramebuffer() const noexcept
        { return mFrameBuffer; }

        inline bool HasScissor() const noexcept
        { return !mScissor.IsEmpty(); }

        // Clear the current render target color buffer with the given clear color.
        void ClearColor(const Color4f& color) const;
        // Clear the current render target stencil buffer with the given stencil value.
        void ClearStencil(const StencilClearValue& stencil) const;
        void ClearDepth(float depth) const;

        using StencilFunc = Device::State::StencilFunc;
        using StencilOp   = Device::State::StencilOp;
        using DepthTest   = Device::State::DepthTest;
        using Culling     = Device::State::Culling;

        struct DrawState {
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

            Culling culling = Culling::Back;

            // line width setting used to rasterize lines when the drawable style is Lines
            float line_width = 1.0f;
        };

        struct DrawCommand {
            // Optional projection matrix that will override the painter's projection matrix.
            const glm::mat4* projection = nullptr;
            // Optional view matrix that will override the painter's view matrix.
            const glm::mat4* view = nullptr;
            // The model to world transformation. Used to transform the
            // associated shape's vertices into the world space.
            const glm::mat4* model = nullptr;
            // The drawable object that will be drawn.
            const Drawable* drawable = nullptr;
            // The material that is applied on the drawable shape.
            const Material* material = nullptr;
            // This is a user defined object pointer that is passed to
            // ShaderProgram::FilterDraw for doing low level shape/draw filtering.
            // Using dodgy/Unsafe void* here for performance reasons. vs. std::any
            void* user = nullptr;
            // State aggregate for the device state for depth testing
            // stencil testing etc.
            DrawState state;
        };
        using DrawList = std::vector<DrawCommand>;

        // Draw multiple objects inside a render pass. Each object has a drawable shape,
        // which provides the geometrical information of the object to be drawn, a material,
        // which provides the "look&feel" i.e. the surface properties for the shape
        // and finally a transform which defines the model-to-world transform.
        void Draw(const DrawList& list, const ShaderProgram& program) const;

        // legacy draw functions.

        struct LegacyDrawState
        {
            float line_width = 1.0f;
            Culling culling = Culling::Back;

            LegacyDrawState() {}

            LegacyDrawState(float line_width)
              : line_width(line_width)
            {}
            LegacyDrawState(Culling culling)
              : culling(culling)
            {}
            LegacyDrawState(float line_width, Culling culling)
              : line_width(line_width)
              , culling(culling)
            {}
            LegacyDrawState(Culling culling, float line_width)
              : line_width(line_width)
              , culling(culling)
            {}
        };

        // Similar to the legacy draw except that allows the device state to be
        // changed through state and shader pass objects.
        void Draw(const Drawable& shape,
                  const glm::mat4& model,
                  const Material& material,
                  const DrawState& state,
                  const ShaderProgram& program,
                  const LegacyDrawState& legacy_draw_state = LegacyDrawState()) const;
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
        void Draw(const Drawable& drawable,
                  const glm::mat4& model,
                  const Material& material,
                  const LegacyDrawState& draw_state = LegacyDrawState()) const;

        // Create new painter implementation using the given graphics device.
        static std::unique_ptr<Painter> Create(std::shared_ptr<Device> device);
        static std::unique_ptr<Painter> Create(Device* device);

        IRect MapToDevice(const IRect& rect) const noexcept;

    private:
        Program* GetProgram(const ShaderProgram& program,
                            const Drawable& drawable,
                            const Material& material,
                            const Drawable::Environment& drawable_environment,
                            const Material::Environment& material_environment) const;

    private:
        std::shared_ptr<Device> mDeviceInst;
        Device* mDevice = nullptr;
    private:
        gfx::Framebuffer* mFrameBuffer = nullptr;
        gfx::USize mSize;
        gfx::IRect mViewport;
        gfx::IRect mScissor;
        glm::vec2 mPixelRatio {1.0f, 1.0f};
        glm::mat4 mProjMatrix {1.0f};
        glm::mat4 mViewMatrix {1.0f};
    private:
        bool mEditingMode = false;
    };

} // namespace
