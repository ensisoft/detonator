// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"

namespace dev
{
    enum class ResourceType {
        GraphicsProgram,
        GraphicsShader,
        GraphicsBuffer,
        FrameBuffer,
        Texture,
    };

    enum class TextureType {
        Invalid,
        Texture2D
    };

    enum class ShaderType {
        Invalid, VertexShader, FragmentShader
    };

    enum class BufferType {
        Invalid,
        VertexBuffer,
        IndexBuffer,
        UniformBuffer
    };

    // Define how the geometry is to be rasterized.
    enum class DrawType : uint32_t {
        // Draw the given vertices as triangles, i.e.
        // each 3 vertices make a single triangle.
        Triangles,
        // Draw each given vertex as a separate point
        Points,
        // Draw a series of triangles all connected to the
        // first vertex
        TriangleFan,
        // Draw the vertices as a series of connected lines
        // where each pair of adjacent vertices are connected
        // by a line.
        // In this draw the line width setting applies.
        Lines,
        // Draw a line between the given vertices looping back
        // from the last vertex to the first.
        LineLoop
    };

    enum class IndexType {
        Index16, Index32
    };

    // Map the type of the index to index size in bytes.
    inline size_t GetIndexByteSize(IndexType type)
    {
        if (type == IndexType::Index16)
            return 2;
        else if (type == IndexType::Index32)
            return 4;
        else BUG("Missing index type.");
        return 0;
    }

    // Specify common usage hint for a GPU buffer such as
    // vertex buffer, index buffer etc.
    enum class BufferUsage {
        // The buffer is updated once and used multiple times.
        Static,
        // The buffer is updated multiple times and used once/few times.
        Stream,
        // The buffer is updated multiple times and used multiple times.
        Dynamic
    };

    enum class TextureFormat {
        // 8bit non-linear sRGB(A) encoded RGB data.
        sRGB,
        sRGBA,
        // 8bit linear RGB(A) data.
        RGB,
        RGBA,
        // Floating point 4 channel linear.
        RGBA32f,
        // 8bit linear alpha mask
        AlphaMask,
        // 32bit floating point depth texture.
        DepthComponent32f
    };

    // Texture minifying filter is used whenever the
    // pixel being textured maps to an area greater than
    // one texture element.
    enum class TextureMinFilter {
        // Use the texture element nearest to the
        // center of the pixel (Manhattan distance)
        Nearest,
        // Use the weighted average of the four texture
        // elements that are closest to the pixel.
        Linear,
        // Use mips (precomputed) minified textures.
        // Use the nearest texture element from the nearest
        // mipmap level
        Mipmap,
        // Use mips (precomputed) minified textures).
        // Use the weighted average of the four texture
        // elements that are sampled from the closest mipmap level.
        Bilinear,
        // Use mips (precomputed minified textures).
        // Use the weighted average of the four texture
        // elements that are sampled from the two nearest mipmap levels.
        Trilinear,
        // Use the default filtering set for the device.
        Default
    };

    // Texture magnifying filter is used whenever the
    // pixel being textured maps to an area less than
    // one texture element.
    enum class TextureMagFilter {
        // Use the texture element nearest to the center
        // of the pixel. (Manhattan distance).
        Nearest,
        // Use the weighted average of the four texture
        // elements that are closest to the pixel.
        Linear,
        // Use the default filtering set for the device.
        Default
    };

    // Texture wrapping options for how to deal with
    // texture coordinates outside of [0,1] range,
    enum class TextureWrapping {
        // Clamp the texture coordinate to the boundary.
        Clamp,
        // Wrap the coordinate by ignoring the integer part.
        Repeat,
        // todo:
        Mirror
    };

    enum class PolygonWindingOrder {
        CounterClockWise,
        ClockWise
    };

    // Which polygon faces to cull. Note that this only applies
    // to polygons, not to lines or points.
    enum class Culling {
        // Don't cull anything, both polygon front and back faces are rasterized
        None,
        // Cull front faces. Front face is determined by the polygon
        // winding order. Currently, counter-clockwise winding is used to
        // indicate front face.
        Front,
        // Cull back faces. This is the default.
        Back,
        // Cull both front and back faces.
        FrontAndBack
    };

    // the stencil action to take on various stencil tests.
    enum class StencilOp {
        DontModify,
        WriteZero,
        WriteRef,
        Increment,
        Decrement
    };

    // how to mix the fragment with the existing color buffer value.
    enum class BlendOp {
        None,
        Transparent,
        Additive
    };

    // stencil test
    enum class StencilFunc {
        Disabled,
        PassAlways,
        PassNever,
        RefIsLess,
        RefIsLessOrEqual,
        RefIsMore,
        RefIsMoreOrEqual,
        // The stencil test passes if (ref & mask) == (stencil & mask)
        RefIsEqual,
        RefIsNotEqual
    };

    enum class DepthTest {
        // Depth testing is disabled, depth buffer is also not updated.
        Disabled,
        // Depth test passes and color buffer is updated when the fragments
        // depth value is less or equal to previously written depth value.
        LessOrEQual,
        // Depth test passes always
        Always
    };

    // Each Format specifies the logical buffers and their bitwise representations.
    enum class FramebufferFormat {
        // Invalid handle value.
        Invalid,
        // RGBA color texture buffer(s) with 8bits (unsigned) per channel.
        // Multiple color targets are possible. MSAA is possible.
        ColorRGBA8,
        // RGBA color texture buffer(s) with 8bits (unsigned) per channel with 16bit depth buffer.
        // Multiple color targets are possible. MSAA is possible.
        ColorRGBA8_Depth16,
        // RGBA color texture buffer(s) with 8bits (unsigned) per channel with 24bit depth buffer and 8bit stencil buffer.
        // Multiple color buffers are possible. MSAA is possible.
        ColorRGBA8_Depth24_Stencil8,
        // 32bit floating point texture depth buffer. No color or stencil support. MSAA is not possible.
        DepthTexture32f
    };

    enum class ColorAttachment : uint8_t {
        Attachment0,
        Attachment1,
        Attachment2,
        Attachment3
    };

    // Framebuffer configuration.
    struct FramebufferConfig {
        FramebufferFormat format = FramebufferFormat::ColorRGBA8;
        // The width of the fbo in pixels.
        unsigned width = 0;
        // The height of the fbo in pixels.
        unsigned height = 0;

        bool msaa = false;
    };

} // namespace