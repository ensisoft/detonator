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
        // Rasterize the individual triangles as lines.
        Wireframe,
        // Rasterize the interior of the drawable. This is the default
        Solid,
        // Rasterize the shape's vertices as individual points.
        Points
    };

    // Graphical projections can be classified as follows:
    //
    // - Perspective projection (aka linear projection)
    //    - normally the projection ray through the center of the projection plane
    //      is perpendicular to the plane but it doesn't have to be.
    //
    //  - Parallel projection.
    //    In parallel projection the projection rays are all parallel. I.e. they all point at
    //    at the same direction from the projection plane. The angle of each vector wrt the
    //    projection plane can be perpendicular (orthographic projection) or at some angle (oblique projection)
    //
    //    - Orthographic projection.
    //        In orthogonal all projection rays are parallel *and* perpendicular to the projection plane.
    //        Depending on the view angles this projection can further be split into:
    //        - "Plan" or "Elevation" projection. The view is directly aligned (projection rays are
    //          parallel to one axis) which shortens one axis away when projected. The two remaining projected
    //          axis are at 90 degree angle.
    //       - Axonometric projections. The view is at some angle that lets all 3 (x,y,z) axis be projected onto
    //         the projection plane at various projection angles (i.e. the angles of the axis after projection).
    //         - Isometric projection.
    //            Angles between all projected axis vectors is 120 degrees. 1.732:1 pixel ratio.
    //         - Dimetric projection
    //            Two angles between projected vectors are 105 degrees and the third angle is 150.
    //             This creates a pixel ratio of 2:1 width:height. Tiles will be 2 as wide as they're
    //             high and lines parallel with the x, y rays in the coordinate grid move up/down 1 pixels
    //             for every 1 pixel horizontally (when projected)
    //         - Trimetric projection
    //            todo

    //    - Oblique projection.
    //        In oblique projection all projection rays are parallel but non perpendicular. In other words
    //        the projection rays are at slanted/skewed wrt the projection plane.
    //        - Military
    //        - Cavalier
    //        - Topdown
    //

    // Graphical perspective defines how graphical objects are displayed.
    // Each perspective requires a combination of a specific view/camera
    // vantage point and the right projection matrix in order to produce
    // the desired rendering. For example the dimetric (typically referred
    // to as 'isometric' in 2D games) is a camera angle of 45 deg around UP
    // axis and 30 deg down tilt combined with orthographic projection matrix.
    enum class Perspective {
        // Orthographic axis aligned projection infers a camera position that
        // is perpendicular to one of the coordinate space axis. This can be
        // used to produce "top down" or "side on" views.
        // When used in games this view can be used for example for side-scrollers,
        // top-down shooters, platform and puzzle games.
        AxisAligned,
        // Orthographic dimetric perspective infers a camera position that is
        // angled at a fixed yaw and tilt (pitch) to look in a certain direction.
        // This camera vantage point is then combined with an orthographic
        // projection to produce a 2D rendering where multiple sides of an
        // object are visible but without any perspective foreshortening.
        // This type of perspective is common in strategy and simulation games.
        // This is often (incorrectly) called "isometric" even though mathematically
        // isometric and dimetric are not the same projections.
        Dimetric,
        // todo: ObliqueTopDown
        //Oblique
    };

} // namespace