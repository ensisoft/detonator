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
#include "graphics/shader_program.h"
#include "graphics/transform.h"

namespace gfx
{
    class Material;
    class Drawable;

    class GenericRenderPass
    {
    public:
        GenericRenderPass(Painter& painter)
          : mPainter(painter)
        {}
        void Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::DrawState state;
            state.write_color  = true;
            state.stencil_func = Painter::StencilFunc::Disabled;
            state.depth_test   = Painter::DepthTest::Disabled;
            detail::GenericShaderProgram program;
            mPainter.Draw(drawable, transform, material, state, program);
        }
    private:
        Painter& mPainter;
    };

    class StencilMaskPass
    {
    public:
        using ClearValue = StencilClearValue;
        using WriteValue = StencilWriteValue;
        enum class StencilFunc {
            Overwrite,
            BitwiseAnd,
            OverlapIncrement
        };
        StencilMaskPass(StencilClearValue clear_value, StencilWriteValue write_value, Painter& painter, StencilFunc func = StencilFunc::Overwrite)
          : mStencilWriteValue(write_value)
          , mStencilFunc(func)
          , mPainter(painter)
        {
            painter.ClearStencil(clear_value);
        }
        StencilMaskPass(StencilWriteValue write_value, Painter& painter, StencilFunc func = StencilFunc::Overwrite)
          : mStencilWriteValue(write_value)
          , mStencilFunc(func)
          , mPainter(painter)
        {}
        void Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::DrawState state;
            state.write_color   = false;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_dpass = Painter::StencilOp::WriteRef;
            state.stencil_dfail = Painter::StencilOp::WriteRef;
            state.stencil_ref   = mStencilWriteValue;
            state.stencil_mask  = 0xff;

            if (mStencilFunc == StencilFunc::Overwrite)
            {
                state.stencil_func = Painter::StencilFunc::PassAlways;
            }
            else if (mStencilFunc == StencilFunc::BitwiseAnd)
            {
                state.stencil_ref  = 1;
                state.stencil_func = Painter::StencilFunc::RefIsEqual;
                state.stencil_fail = Painter::StencilOp::WriteZero;
            }
            else if (mStencilFunc == StencilFunc::OverlapIncrement)
            {
                state.stencil_func  = Painter::StencilFunc::RefIsEqual;
                state.stencil_dpass = Painter::StencilOp::Increment;
                state.stencil_fail  = Painter::StencilOp::WriteZero;
            }

            detail::StencilShaderProgram program;
            mPainter.Draw(drawable, transform, material, state, program);
        }
    private:
        const WriteValue mStencilWriteValue = 1;
        const StencilFunc mStencilFunc;
        Painter& mPainter;
    };

    class StencilTestColorWritePass
    {
    public:
        using PassValue = StencilPassValue;

        explicit StencilTestColorWritePass(PassValue stencil_pass_value, Painter& painter)
            : mStencilRefValue(stencil_pass_value)
            , mPainter(painter)
        {}
        void Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::DrawState state;
            state.write_color   = true;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::RefIsEqual;
            state.stencil_dpass = Painter::StencilOp::DontModify;
            state.stencil_dfail = Painter::StencilOp::DontModify;
            state.stencil_ref   = mStencilRefValue;
            state.stencil_mask  = 0xff;

            detail::GenericShaderProgram program;
            mPainter.Draw(drawable, transform, material, state, program);
        }
    private:
        const PassValue mStencilRefValue;
        Painter& mPainter;
    };

} // namespace
