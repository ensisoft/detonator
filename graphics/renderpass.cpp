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

#include "config.h"

#include "graphics/renderpass.h"
#include "graphics/device.h"

namespace gfx {
namespace detail {
void GenericRenderPass::Begin(Device& device, State* state) const
{
    state->write_color  = true;
    state->stencil_func = StencilFunc::Disabled;
    state->depth_test   = DepthTest::Disabled;
}

void StencilMaskPass::Begin(Device& device, State* state) const
{
    if (mStencilClearValue.has_value())
        device.ClearStencil(mStencilClearValue.value());
    state->write_color   = false;
    state->depth_test    = DepthTest::Disabled;
    state->stencil_func  = StencilFunc::PassAlways;
    state->stencil_dpass = StencilOp::WriteRef;
    state->stencil_dfail = StencilOp::WriteRef;
    state->stencil_ref   = mStencilWriteValue;
    state->stencil_mask  = 0xff;
}

void StencilTestColorWritePass::Begin(Device& device, State* state) const
{
    state->write_color   = true;
    state->depth_test    = DepthTest::Disabled;
    state->stencil_func  = StencilFunc::RefIsEqual;
    state->stencil_dpass = StencilOp::DontModify;
    state->stencil_dfail = StencilOp::DontModify;
    state->stencil_ref   = mStencilRefValue;
    state->stencil_mask  = 0xff;
}

} // namespace
} // namespace
