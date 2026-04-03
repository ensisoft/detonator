// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <variant>
#include <vector>

#include "graphics/vertex.h"

namespace gfx
{
    class GeometryBuffer;

    // Style of the drawable's geometry determines how the geometry
    // is to be rasterized.
    enum class SimpleShapeStyle {
        // Rasterize the outline of the shape as lines.
        // Only the fragments that are within the line are shaded.
        // Line width setting is applied to determine the width
        // of the lines.
        Outline,
        // Rasterize the interior of the drawable. This is the default
        Solid
    };

    enum class SimpleShapeType {
        Arrow,
        ArrowCursor,
        BlockCursor,
        Capsule,
        Circle,
        Cone,
        Cube,
        Cylinder,
        IsoscelesTriangle,
        Parallelogram,
        Pyramid,
        Rectangle,
        RightTriangle,
        RoundRect,
        Sector,
        Sphere,
        SemiCircle,
        StaticLine,
        Trapezoid,
        Triangle
    };

    enum class SimpleShapeAttribute {
        // applicable to round rect and capsule
        CornerRadius,
        Orientation
    };

    enum class SimpleShapeOrientation {
        Horizontal, Vertical
    };

    namespace detail {
        struct SimpleShapeEnvironment {
            const glm::mat4* model_matrix = nullptr;
        };
        using Style = SimpleShapeStyle;

        struct RightTriangleArgs {
            enum class Corner {
                BottomLeft,
                BottomRight,
                TopLeft,
                TopRight
            };
            Corner corner = Corner::BottomLeft;
        };

        struct CapsuleArgs {
            using Orientation = SimpleShapeOrientation;
            unsigned slices = 50;
            float radius = 0.25f;
            Orientation orientation = Orientation::Horizontal;
        };

        struct SectorShapeArgs {
            float fill_percentage = 0.25f;
        };
        struct RoundRectShapeArgs {
            float corner_radius = 0.05f;
        };
        struct CylinderShapeArgs {
            unsigned slices = 100;
        };
        struct ConeShapeArgs {
            unsigned slices = 100;
        };
        struct SphereShapeArgs {
            unsigned slices = 100;
        };

        struct ArrowGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct StaticLineGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct CapsuleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, const CapsuleArgs& args);
            static void GenerateVertical(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, const CapsuleArgs& args);
            static void GenerateHorizontal(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, const CapsuleArgs& args);
        };
        struct SemiCircleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct CircleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct RectangleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct IsoscelesTriangleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& device);
        };
        struct RightTriangleGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, const RightTriangleArgs& args);
        };
        struct TrapezoidGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& device);
        };
        struct ParallelogramGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct SectorGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, float fill_percentage);
        };
        struct RoundRectGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, float corner_radius);
        };
        struct ArrowCursorGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct BlockCursorGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
        };
        struct CubeGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
            static void MakeFace(size_t vertex_offset, Index16* indices, Vertex3D* vertices,
                                 const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                 const Vec3& normal);
            static void AddLine(const Vec3& v0, const Vec3& v1, std::vector<Vertex3D>& vertex);
        };
        struct CylinderGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, unsigned slices);
        };
        struct PyramidGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry);
            static void MakeFace(std::vector<Vertex3D>& verts, const Vertex3D& apex, const Vertex3D& base0, const Vertex3D& base1);
        };
        struct ConeGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, unsigned slices);
        };
        struct SphereGeometry {
            static void Generate(const SimpleShapeEnvironment& env, Style style, GeometryBuffer& geometry, unsigned slices);
        };

        using SimpleShapeArgs = std::variant<std::monostate, SectorShapeArgs, RoundRectShapeArgs,
                CylinderShapeArgs, ConeShapeArgs, SphereShapeArgs, RightTriangleArgs,
                CapsuleArgs>;

        void ConstructSimpleShape(const SimpleShapeArgs& args,
                                  const SimpleShapeEnvironment& environment,
                                  SimpleShapeStyle style,
                                  SimpleShapeType type,
                                  GeometryBuffer& geometry);
        std::string GetSimpleShapeGeometryId(const SimpleShapeArgs& args,
                                             const SimpleShapeEnvironment& env,
                                             SimpleShapeStyle style,
                                             SimpleShapeType type);

    } // detail

    SpatialMode GetSimpleShapeSpatialMode(SimpleShapeType shape);

    inline bool Is3DShape(const SimpleShapeType shape) noexcept
    {
        return GetSimpleShapeSpatialMode(shape) == SpatialMode::True3D;
    }
    inline bool Is2DShape(const SimpleShapeType shape) noexcept
    {
        return GetSimpleShapeSpatialMode(shape) == SpatialMode::Flat2D;
    }

} // namespace