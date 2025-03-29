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

#include <cstdint>
#include <vector>

#include "base/types.h"
#include "device/enum.h"
#include "device/uniform.h"

namespace dev
{
    struct ProgramState {
        std::vector<const Uniform*> uniforms;
    };

    struct ViewportState {
        // the device viewport into the render target. the viewport is in
        // device coordinates (pixels, texels) and the origin is at the bottom
        // left and Y axis grows upwards (towards the window top)
        base::IRect viewport;
        // the device scissor that can be used to limit the rendering to the
        // area inside the scissor. if the scissor rect is an empty rect
        // the scissor test is disabled.
        base::IRect scissor;
    };

    // Device state including the rasterizer state
    // that is to be applied for any draw operation.
    struct GraphicsPipelineState {
        using DepthTest   = dev::DepthTest;
        using Culling     = dev::Culling;
        using BlendOp     = dev::BlendOp;
        using StencilFunc = dev::StencilFunc;
        using StencilOp   = dev::StencilOp;
        using PolygonWindingOrder = dev::PolygonWindingOrder;

        DepthTest depth_test = DepthTest::Disabled;

        // polygon face culling setting.
        Culling culling = Culling::Back;

        PolygonWindingOrder winding_order = PolygonWindingOrder::CounterClockWise;

        BlendOp blending = BlendOp::None;
        // the stencil test function.
        StencilFunc  stencil_func  = StencilFunc::Disabled;
        // what to do when the stencil test fails.
        StencilOp    stencil_fail  = StencilOp::DontModify;
        // what to do when the stencil test passes and the depth passes
        StencilOp    stencil_dpass = StencilOp::DontModify;
        // what to do when the stencil test passes but depth test fails.
        StencilOp    stencil_dfail = StencilOp::DontModify;
        // todo:
        std::uint8_t stencil_mask  = 0xff;
        // todo:
        std::uint8_t stencil_ref   = 0x0;

        // rasterizer setting for line width when rasterizing
        // geometries with lines.
        float line_width = 1.0f;
        // should write color buffer or not.
        bool bWriteColor = true;

        bool premulalpha = false;
    };

    struct GraphicsDeviceResourceStats {
        // vertex buffer objects
        std::uint32_t dynamic_vbo_mem_use     = 0;
        std::uint32_t dynamic_vbo_mem_alloc   = 0;
        std::uint32_t static_vbo_mem_use      = 0;
        std::uint32_t static_vbo_mem_alloc    = 0;
        std::uint32_t streaming_vbo_mem_use   = 0;
        std::uint32_t streaming_vbo_mem_alloc = 0;
        // index buffer objects
        std::uint32_t dynamic_ibo_mem_use     = 0;
        std::uint32_t dynamic_ibo_mem_alloc   = 0;
        std::uint32_t static_ibo_mem_use      = 0;
        std::uint32_t static_ibo_mem_alloc    = 0;
        std::uint32_t streaming_ibo_mem_use   = 0;
        std::uint32_t streaming_ibo_mem_alloc = 0;
        // uniform buffer objects
        std::uint32_t dynamic_ubo_mem_use     = 0;
        std::uint32_t dynamic_ubo_mem_alloc   = 0;
        std::uint32_t static_ubo_mem_use      = 0;
        std::uint32_t static_ubo_mem_alloc    = 0;
        std::uint32_t streaming_ubo_mem_use   = 0;
        std::uint32_t streaming_ubo_mem_alloc = 0;
    };

    struct GraphicsDeviceCaps {
        unsigned num_texture_units = 0;
        unsigned max_fbo_width = 0;
        unsigned max_fbo_height = 0;
        bool instanced_rendering = false;
        bool multiple_color_attachments = false;
    };

} // dev
