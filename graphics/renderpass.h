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

#include "graphics/types.h"
#include "graphics/painter.h"
#include "graphics/shader_programs.h"
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
            state.render_pass  = RenderPass::ColorPass;
            state.write_color  = true;
            state.stencil_func = Painter::StencilFunc::Disabled;
            state.depth_test   = Painter::DepthTest::Disabled;
            FlatShadedColorProgram program;
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
        bool Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::DrawState state;
            state.render_pass   = RenderPass::StencilPass;
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

            StencilShaderProgram program;
            return mPainter.Draw(drawable, transform, material, state, program);
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
        bool Draw(const Drawable& drawable, const Transform& transform, const Material& material) const
        {
            Painter::DrawState state;
            state.render_pass   = RenderPass::ColorPass;
            state.write_color   = true;
            state.depth_test    = Painter::DepthTest::Disabled;
            state.stencil_func  = Painter::StencilFunc::RefIsEqual;
            state.stencil_dpass = Painter::StencilOp::DontModify;
            state.stencil_dfail = Painter::StencilOp::DontModify;
            state.stencil_ref   = mStencilRefValue;
            state.stencil_mask  = 0xff;

            FlatShadedColorProgram program;
            return mPainter.Draw(drawable, transform, material, state, program);
        }
    private:
        const PassValue mStencilRefValue;
        Painter& mPainter;
    };

    class ShadowMapRenderPass
    {
    public:
        explicit ShadowMapRenderPass(std::string renderer_name, BasicLightProgram& program, Device* device) noexcept
          : mRendererName(std::move(renderer_name))
          , mProgram(program)
          , mDevice(device)
        {}

        using DrawCommand     = Painter::DrawCommand;
        using DrawCommandList = Painter::DrawCommandList;
        using LightProjectionType = BasicLightProgram::LightProjectionType;

        void InitState() const;

        bool Draw(const DrawCommandList& draw_cmd_list) const;

        bool Draw(const Drawable& drawable,
                  const Material& material,
                  const Transform& transform) const
        {
            const glm::mat4& model_to_world = transform.GetAsMatrix();
            DrawCommand cmd;
            cmd.drawable = &drawable;
            cmd.material = &material;
            cmd.model    = &model_to_world;
            DrawCommandList cmd_list;
            cmd_list.push_back(cmd);
            return Draw(cmd_list);
        }
        Texture* GetDepthTexture() const;

        glm::mat4 GetLightViewMatrix(unsigned light_index) const;
        glm::mat4 GetLightProjectionMatrix(unsigned light_index) const;
        float GetLightProjectionNearPlane(unsigned light_index) const;
        float GetLightProjectionFarPlane(unsigned light_index) const;

        LightProjectionType GetLightProjectionType(unsigned light_index) const;
    private:
        Framebuffer* CreateFramebuffer() const;

    private:
        std::string mRendererName;
        BasicLightProgram& mProgram;
        Device* mDevice = nullptr;
    };

} // namespace
