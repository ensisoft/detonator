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

#include <string>
#include <cstdint>

#include "graphics/device.h"
#include "graphics/types.h"

namespace gfx
{
    class Device;

    class RenderPass
    {
    public:
        enum class Type {
            Color,
            Stencil
        };

        using StencilFunc = Device::State::StencilFunc;
        using StencilOp   = Device::State::StencilOp;
        using DepthTest   = Device::State::DepthTest;

        struct State {
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

        virtual void Begin(Device& device, State* state) const {}

        virtual void Finish(Device& device) const {}

        virtual std::string GetName() const = 0;
        virtual Type GetType() const = 0;
    private:
    };

    namespace detail {
        class GenericRenderPass : public RenderPass
        {
        public:
            virtual void Begin(Device& device, State* state) const override;

            virtual std::string GetName() const override
            { return "GenericColor"; }
            virtual Type GetType() const override
            { return Type::Color; }
        private:
        };

        class StencilMaskPass : public RenderPass
        {
        public:
            StencilMaskPass(uint8_t clear_value, uint8_t write_value)
                : mStencilClearValue(clear_value)
                , mStencilWriteValue(write_value)
            {}
            virtual void Begin(Device& device, State* state) const override;

            virtual std::string GetName() const override
            { return "Stencil"; }
            virtual Type GetType() const override
            { return Type::Stencil; }
        private:
            const uint8_t mStencilClearValue = 1;
            const uint8_t mStencilWriteValue = 1;
        };

        class StencilTestColorWritePass : public RenderPass
        {
        public:
            explicit StencilTestColorWritePass(uint8_t stencil_pass_value)
              : mStencilRefValue(stencil_pass_value)
            {}
            virtual void Begin(Device& device, State* state) const override;

            virtual std::string GetName() const override
            { return "StencilMaskColorWrite"; }
            virtual Type GetType() const override
            { return Type::Color; }
        private:
            const uint8_t mStencilRefValue;
        };

    } // namespace

} // namespace