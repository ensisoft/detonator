// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <optional>

#include "graphics/types.h"
#include "graphics/painter.h"
#include "graphics/shaderpass.h"
#include "graphics/transform.h"

namespace gfx
{
    class Material;
    class Drawable;

    class GenericRenderPass
    {
    public:
        using DrawList = Painter::DrawList;

        GenericRenderPass(Painter& painter)
          : mPainter(painter)
        {}

        void Draw(const DrawList& list) const
        {
            Painter::RenderPassState state;
            state.write_color  = true;
            state.stencil_func = Painter::StencilFunc::Disabled;
            state.depth_test   = Painter::DepthTest::Disabled;
            detail::GenericShaderPass pass;
            mPainter.Draw(list, state, pass);
        }
    private:
        Painter& mPainter;
    };

    class StencilMaskPass
    {
    public:
        using ClearValue = StencilClearValue;
        using WriteValue = StencilWriteValue;
        using DrawShape  = Painter::DrawShape;
        using DrawList   = Painter::DrawList;

        StencilMaskPass(StencilClearValue clear_value, StencilWriteValue write_value, Painter& painter)
          : mStencilWriteValue(write_value)
          , mPainter(painter)
        {
            painter.ClearStencil(clear_value);
        }
        StencilMaskPass(StencilWriteValue write_value, Painter& painter)
          : mStencilWriteValue(write_value)
          , mPainter(painter)
        {}
        void Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::RenderPassState state;
            state.write_color   = false;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::PassAlways;
            state.stencil_dpass = Painter::StencilOp::WriteRef;
            state.stencil_dfail = Painter::StencilOp::WriteRef;
            state.stencil_ref   = mStencilWriteValue;
            state.stencil_mask  = 0xff;

            detail::StencilShaderPass pass;
            mPainter.Draw(drawable, transform, material, state, pass);
        }
        void Draw(const DrawList& list) const
        {
            Painter::RenderPassState state;
            state.write_color   = false;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::PassAlways;
            state.stencil_dpass = Painter::StencilOp::WriteRef;
            state.stencil_dfail = Painter::StencilOp::WriteRef;
            state.stencil_ref   = mStencilWriteValue;
            state.stencil_mask  = 0xff;

            detail::StencilShaderPass pass;
            mPainter.Draw(list, state, pass);
        }
    private:
        const WriteValue mStencilWriteValue = 1;
        Painter& mPainter;
    };

    class StencilTestColorWritePass
    {
    public:
        using DrawList = Painter::DrawList;
        using PassValue = StencilPassValue;

        explicit StencilTestColorWritePass(PassValue stencil_pass_value, Painter& painter)
            : mStencilRefValue(stencil_pass_value)
            , mPainter(painter)
        {}
        void Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::RenderPassState state;
            state.write_color   = true;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::RefIsEqual;
            state.stencil_dpass = Painter::StencilOp::DontModify;
            state.stencil_dfail = Painter::StencilOp::DontModify;
            state.stencil_ref   = mStencilRefValue;
            state.stencil_mask  = 0xff;

            detail::GenericShaderPass pass;
            mPainter.Draw(drawable, transform, material, state, pass);
        }
        void Draw(const DrawList& list) const
        {
            Painter::RenderPassState state;
            state.write_color   = true;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::RefIsEqual;
            state.stencil_dpass = Painter::StencilOp::DontModify;
            state.stencil_dfail = Painter::StencilOp::DontModify;
            state.stencil_ref   = mStencilRefValue;
            state.stencil_mask  = 0xff;

            detail::GenericShaderPass pass;
            mPainter.Draw(list, state, pass);
        }
    private:
        const PassValue mStencilRefValue;
        Painter& mPainter;
    };

} // namespace
