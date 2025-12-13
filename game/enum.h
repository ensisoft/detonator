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

namespace game {

    enum class TileOcclusion : uint8_t {
        // warning, do not reorganize this enum
        // the order of the fields is used directly in sorting.
        Top, Left, None, Bottom, Right
    };

    enum class BasicLightType {
        Ambient, Directional, Spot, Point
    };

    enum class BasicFogMode {
        None, Linear, Exp1, Exp2
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

    enum class SceneShadingMode {
        Flat, BasicLight
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

    // Render view describes the logical view into a scene (or tilemap)
    // i.e. the way the camera looks into the scene. Note that this is
    // only the logical camera view. Conceptually this is different from
    // the *projection* which defines how the 3D objects get projected
    // to the 2D render target/surface. That being said in practice
    // we only have few expected combinations.
    // Axis aligned + perspective projection
    // Axis aligned + orthographic projection
    // Dimetric + orthographic projection
    enum class RenderView {
        // Axis aligned projection infers a camera position that
        // is perpendicular to one of the coordinate space axis. This can be
        // used to produce "top down" or "side on" views.
        // When used in games this view can be used for example for side-scrollers,
        // top-down shooters, platform and puzzle games.
        AxisAligned,
        // Dimetric perspective infers a camera position that is
        // angled at a fixed yaw and tilt (pitch) to look in a certain direction.
        // This camera vantage point is then combined with an orthographic
        // projection to produce a 2D rendering where multiple sides of an
        // object are visible but without any perspective foreshortening.
        // This type of perspective is common in strategy and simulation games.
        // This is often (incorrectly) called "isometric" even though mathematically
        // isometric and dimetric are not the same 2D projections.
        Dimetric,
        // todo
        Isometric
    };

    // Projection defines how objects are transformed geometrically
    // when being projected onto the 2D render target/surface.
    // To conclude the final rendering we must use a combination
    // of "logical camera view" + "projection".
    // In order to produce an axonometric rendering result such as
    // "dimetric" rendering we need to combine the dimetric logical
    // view with orthographic projection. I.e. no illusion of depth
    // or perspective.
    enum class RenderProjection {
        Orthographic,
        Perspective
    };

    // Shorthand combination of RenderView + RenderProjection.
    enum class SceneProjection {
        AxisAlignedOrthographic,
        AxisAlignedPerspective,
        Dimetric,
        Isometric,
        //Oblique
    };

    inline bool IsAxonometricProjection(SceneProjection p) noexcept
    {
        if (p == SceneProjection::Dimetric || p == SceneProjection::Isometric)
            return true;
        return false;
    }

    enum class CoordinateSpace {
        // The entity exists in scene space, meaning it is positioned and transformed
        // relative to the game world. It moves as the camera moves.
        Scene,

        // The entity exists in camera space, meaning it stays fixed relative to
        // the camera. Useful for UI elements or background effects that should
        // always remain in the viewport.
        Camera
    };

    enum class EntityFlags {
        // Only pertains to editor (todo: maybe this flag should be removed)
        VisibleInEditor,
        LockedInEditor,
        // node is visible in the game or not.
        // Even if this is true the node will still need to have some
        // renderable items attached to it such as a shape or
        // animation item.
        VisibleInGame,
        // Limit the lifetime to some maximum amount
        // after which the entity is killed.
        LimitLifetime,
        // Whether to automatically kill entity when it reaches
        // its end of lifetime.
        KillAtLifetime,
        // Whether to automatically kill entity when it reaches (goes past)
        // the border of the scene
        KillAtBoundary,
        // Invoke the tick function on the entity
        TickEntity,
        // Invoke the update function on the entity
        UpdateEntity,
        // Invoke the node update function on the entity
        UpdateNodes,
        // Invoke the post update function on the entity.
        PostUpdate,
        // Whether to pass keyboard events to the entity or not
        WantsKeyEvents,
        // Whether to pass mouse events to the entity or not.
        WantsMouseEvents,
    };

} // namespace