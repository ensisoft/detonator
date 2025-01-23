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

namespace game {

    enum class BasicLightType {
        Ambient, Directional, Spot, Point
    };

    enum class RigidBodyJointSetting {
        EnableMotor,
        EnableLimit,
        MotorTorque,
        MotorSpeed,
        MotorForce,
        Stiffness,
        Damping,
    };

    // Selection for collision shapes when the collision shape detection
    // is set to manual.
    enum class CollisionShape {
        // The collision shape is a box based on the size of node's box.
        Box,
        // The collision shape is a circle based on the largest extent of
        // the node's box.
        Circle,
        // The collision shape is a right-angled triangle where the
        // height of the triangle is the height of the box and the
        // width is the width of the node's box
        RightTriangle,
        // Isosceles triangle
        IsoscelesTriangle,
        // Trapezoid
        Trapezoid,
        //
        Parallelogram,
        // The collision shape is the upper half of a circle.
        SemiCircle,
        // The collision shape is a convex polygon. The polygon shape id
        // must then be selected in order to be able to extract the
        // polygon's convex hull.
        Polygon
    };

    enum class RenderPass {
        DrawColor,
        MaskCover,
        MaskExpose
    };

    enum class RenderStyle {
        // Rasterize the outline of the shape as lines.
        // Only the fragments that are within the line are shaded.
        // Line width setting is applied to determine the width
        // of the lines.
        Outline,
        // Rasterize the interior of the drawable. This is the default
        Solid
    };

    enum class RenderView {
        AxisAligned, Dimetric
    };

    enum class RenderProjection {
        Orthographic, Perspective
    };

} // namespace