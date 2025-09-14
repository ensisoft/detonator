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

#include "device/enum.h"

namespace gfx
{
    enum class MaterialFlags : uint32_t {
        EnableBloom = 0x1,
        EnableLight = 0x2
    };

    enum class DrawableFlags : uint32_t {
        Flip_UV_Vertically = 0x1,
        Flip_UV_Horizontally = 0x2
    };

    enum class BasicLightType : int32_t {
        Ambient,
        Directional,
        Spot,
        Point
    };

    enum class BasicFogMode : uint32_t {
        Linear, Exponential1, Exponential2
    };

    enum class RenderPass {
        ColorPass,
        StencilPass,
    };

    // Text alignment inside a rect
    enum TextAlign {
        // Vertical text alignment
        AlignTop     = 0x1,
        AlignVCenter = 0x2,
        AlignBottom  = 0x4,
        // Horizontal text alignment
        AlignLeft    = 0x10,
        AlignHCenter = 0x20,
        AlignRight   = 0x40
    };

    enum TextProp {
        None      = 0x0,
        Underline = 0x1,
        Blinking  = 0x2
    };

    using BufferUsage = dev::BufferUsage;

    // Style of the drawable's geometry determines how the geometry
    // is to be rasterized.
    enum class DrawPrimitive {
        Points, Lines, Triangles
    };

    // broad category of drawables for describing the semantic
    // meaning of the drawable. Each drawable in this category
    // has specific constraint and specific requirements in order
    // for the drawable and material to work together.
    //
    // Category    Particle Data  Tile Data  Tris Points  Lines
    //----------------------------------------------------------
    // Basic                                  *     *       *
    // Particles        *                           *       *
    // TileBatch                     *        *     *
    //
    // In other words if we're for example rendering particles
    // then the material can expect there to be per particle data
    // and the rasterizer draw primitive is either POINTS or LINES.
    //
    // If any material's shader source is written to support
    // multiple different drawable types then care must be taken
    // to ensure that the right set of varyings are exposed  in
    // the vertex-fragment shader interface. In other words if we
    // have a fragment shader that can module it's output with per
    // particle alpha but the shader is used with a non-particle
    // drawable then there's not going to be incoming per particle
    // alpha value which means that if the fragment shader has
    // 'in float vParticleAlpha' it will cause a program link error.
    enum class DrawCategory {
        // Lines, line batches, polygons, simple shapes such as rect
        // round rect, circle, 2D and 3D
        Basic,
        // Particles with per particle data.
        Particles,
        // Tiles with per tile data.
        TileBatch
    };

    enum class UniformType {
        Int, Float, Vec2, Vec3, Vec4, Color, String
    };

} // namespace