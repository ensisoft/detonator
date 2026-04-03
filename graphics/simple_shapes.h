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

#include "graphics/simple_shape_base.h"
#include "graphics/simple_shape_class.h"
#include "graphics/simple_shape_instance.h"

namespace gfx
{
    namespace detail {
        template<SimpleShapeType type>
        struct SimpleShapeClassTypeShim : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "") noexcept
              : SimpleShapeClass(type, std::monostate(), std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::Capsule> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              CapsuleArgs::Orientation orientation = CapsuleArgs::Orientation::Horizontal,
                                              unsigned slices = 50,
                                              float radius = 0.25f) noexcept
            : SimpleShapeClass(SimpleShapeType::Capsule, CapsuleArgs { slices, radius, orientation }, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::RightTriangle> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              RightTriangleArgs::Corner corner =  RightTriangleArgs::Corner::BottomLeft) noexcept
              : SimpleShapeClass(SimpleShapeType::RightTriangle, RightTriangleArgs{corner}, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::Sector> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              float fill_percentage = 0.25f) noexcept
              : SimpleShapeClass(SimpleShapeType::Sector, SectorShapeArgs{fill_percentage}, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::RoundRect> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              float corner_radius = 0.05f) noexcept
              : SimpleShapeClass(SimpleShapeType::RoundRect, RoundRectShapeArgs{corner_radius}, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::Cylinder> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              unsigned  slices = 100) noexcept
              : SimpleShapeClass(SimpleShapeType::Cylinder, CylinderShapeArgs{slices}, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::Cone> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              unsigned  slices = 100) noexcept
              : SimpleShapeClass(SimpleShapeType::Cone, ConeShapeArgs{slices}, std::move(id), std::move(name))
            {}
        };

        template<>
        struct SimpleShapeClassTypeShim<SimpleShapeType::Sphere> : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                              std::string name = "",
                                              unsigned  slices = 100) noexcept
              : SimpleShapeClass(SimpleShapeType::Sphere, SphereShapeArgs{slices}, std::move(id), std::move(name))
            {}
        };

        // use a template to generate a new specific shape type
        // and provide some more constructor arguments specific to a type.
        // This is needed for backwards compatibility and for convenience.
        template<SimpleShapeType type>
        struct SimpleShapeInstanceTypeShim : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid) noexcept
              : SimpleShape(type, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::Capsule> : public SimpleShape
        {
            using Orientation = CapsuleArgs::Orientation;
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid,
                Orientation orientation = Orientation::Horizontal, unsigned slices = 50, float radius = 0.25f) noexcept
            : SimpleShape(SimpleShapeType::Capsule, CapsuleArgs { slices, radius, orientation }, style)
            {}
            explicit SimpleShapeInstanceTypeShim(Orientation orientation, float radius = 0.25f, unsigned slices = 50, Style style = Style::Solid) noexcept
            : SimpleShape(SimpleShapeType::Capsule, CapsuleArgs { slices, radius, orientation}, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::RightTriangle> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid,
                                                 RightTriangleArgs::Corner corner = RightTriangleArgs::Corner::BottomLeft) noexcept
              : SimpleShape(SimpleShapeType::RightTriangle, RightTriangleArgs{corner}, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::Sector> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid, float fill_percentage = 0.25f) noexcept
              : SimpleShape(SimpleShapeType::Sector, SectorShapeArgs{fill_percentage}, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::RoundRect> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid, float corner_radius = 0.05f) noexcept
              : SimpleShape(SimpleShapeType::RoundRect, RoundRectShapeArgs{corner_radius}, style)
            {}
            explicit SimpleShapeInstanceTypeShim(float corner_radius, Style style = Style::Solid) noexcept
            : SimpleShape(SimpleShapeType::RoundRect, RoundRectShapeArgs{corner_radius}, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::Cylinder> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid, unsigned slices = 100) noexcept
              : SimpleShape(SimpleShapeType::Cylinder, CylinderShapeArgs { slices }, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::Cone> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid, unsigned slices = 100) noexcept
              : SimpleShape(SimpleShapeType::Cone, ConeShapeArgs { slices }, style)
            {}
        };

        template<>
        struct SimpleShapeInstanceTypeShim<SimpleShapeType::Sphere> : public SimpleShape
        {
            explicit SimpleShapeInstanceTypeShim(Style style = Style::Solid, unsigned slices = 100) noexcept
              : SimpleShape(SimpleShapeType::Sphere, SphereShapeArgs { slices }, style)
            {}
        };
    } // detail

    using ArrowClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Arrow>;
    using ArrowInstance = SimpleShapeInstance;
    using Arrow         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Arrow>;

    using ArrowCursorClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::ArrowCursor>;
    using ArrowCursorInstance = SimpleShapeInstance;
    using ArrowCursor         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::ArrowCursor>;

    using BlockCursorClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::BlockCursor>;
    using BlockCursorInstance = SimpleShapeInstance;
    using BlockCursor         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::BlockCursor>;

    using CapsuleClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Capsule>;
    using CapsuleInstance = SimpleShapeInstance;
    using Capsule         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Capsule>;

    using CircleClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Circle>;
    using CircleInstance = SimpleShapeInstance;
    using Circle         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Circle>;

    using ConeClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Cone>;
    using ConeInstance = SimpleShapeInstance;
    using Cone         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Cone>;

    using CubeClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Cube>;
    using CubeInstance = SimpleShapeInstance;
    using Cube         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Cube>;

    using CylinderClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Cylinder>;
    using CylinderInstance = SimpleShapeInstance;
    using Cylinder         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Cylinder>;

    using IsoscelesTriangleClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::IsoscelesTriangle>;
    using IsoscelesTriangleInstance = SimpleShapeInstance;
    using IsoscelesTriangle         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::IsoscelesTriangle>;

    using ParallelogramClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Parallelogram>;
    using ParallelogramInstance = SimpleShapeInstance;
    using Parallelogram         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Parallelogram>;

    using PyramidClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Pyramid>;
    using PyramidInstance = SimpleShapeInstance;
    using Pyramid         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Pyramid>;

    using RectangleClass         = detail::SimpleShapeClassTypeShim<SimpleShapeType::Rectangle>;
    using RectangleClassInstance = SimpleShapeInstance;
    using Rectangle              = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Rectangle>;

    using RightTriangleClass         = detail::SimpleShapeClassTypeShim<SimpleShapeType::RightTriangle>;
    using RightTriangleClassInstance = SimpleShapeInstance;
    using RightTriangle              = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::RightTriangle>;

    using SemiCircleClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::SemiCircle>;
    using SemiCircleInstance = SimpleShapeInstance;
    using SemiCircle         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::SemiCircle>;

    using RoundRectangleClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::RoundRect>;
    using RoundRectangleInstance = SimpleShapeInstance;
    using RoundRectangle         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::RoundRect>;

    using SphereClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Sphere>;
    using SphereInstance = SimpleShapeInstance;
    using Sphere         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Sphere>;

    using StaticLineClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::StaticLine>;
    using StaticLineInstance = SimpleShapeInstance;
    using StaticLine         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::StaticLine>;

    using TrapezoidClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Trapezoid>;
    using TrapezoidInstance = SimpleShapeInstance;
    using Trapezoid         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Trapezoid>;

    using SectorClass    = detail::SimpleShapeClassTypeShim<SimpleShapeType::Sector>;
    using SectorInstance = SimpleShapeInstance;
    using Sector         = detail::SimpleShapeInstanceTypeShim<SimpleShapeType::Sector>;

} // namespace