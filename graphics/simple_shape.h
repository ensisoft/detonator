// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#include "warnpop.h"

#include <string>

#include "base/assert.h"
#include "base/utility.h"
#include "graphics/drawable.h"

namespace gfx
{
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

    namespace detail {
        using Environment = Drawable::Environment;
        using Style = SimpleShapeStyle;

        struct RightTriangleArgs {
            enum class Corner {
                BottomLeft, BottomRight,
                TopLeft, TopRight
            };
            Corner corner = Corner::BottomLeft;
        };

        struct ArrowGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct StaticLineGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct CapsuleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct SemiCircleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct CircleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct RectangleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct IsoscelesTriangleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& device);
        };
        struct RightTriangleGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, const RightTriangleArgs& args);
        };
        struct TrapezoidGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& device);
        };
        struct ParallelogramGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct SectorGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, float fill_percentage);
        };
        struct RoundRectGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, float corner_radius);
        };
        struct ArrowCursorGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct BlockCursorGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
        };
        struct CubeGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
            static void MakeFace(size_t vertex_offset, Index16* indices, Vertex3D* vertices,
                                 const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                 const Vec3& normal);
            static void AddLine(const Vec3& v0, const Vec3& v1, std::vector<Vertex3D>& vertex);
        };
        struct CylinderGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned slices);
        };
        struct PyramidGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
            static void MakeFace(std::vector<Vertex3D>& verts, const Vertex3D& apex, const Vertex3D& base0, const Vertex3D& base1);
        };
        struct ConeGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned slices);
        };
        struct SphereGeometry {
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry, unsigned slices);
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

        using SimpleShapeArgs = std::variant<std::monostate, SectorShapeArgs, RoundRectShapeArgs,
                CylinderShapeArgs, ConeShapeArgs, SphereShapeArgs, RightTriangleArgs>;
        using SimpleShapeEnvironment = DrawableClass::Environment;

        void ConstructSimpleShape(const SimpleShapeArgs& args,
                                  const SimpleShapeEnvironment& environment,
                                  SimpleShapeStyle style,
                                  SimpleShapeType type,
                                  Geometry::CreateArgs& geometry);
        std::string GetSimpleShapeGeometryId(const SimpleShapeArgs& args,
                                             const SimpleShapeEnvironment& env,
                                             SimpleShapeStyle style,
                                             SimpleShapeType type);

    } // detail


    class SimpleShapeClass : public DrawableClass
    {
    public:
        using Style = SimpleShapeStyle;
        using Shape = SimpleShapeType;

        SimpleShapeClass() = default;
        explicit SimpleShapeClass(SimpleShapeType shape, detail::SimpleShapeArgs args, std::string id, std::string name) noexcept
          : mId(std::move(id))
          , mName(std::move(name))
          , mShape(shape)
          , mArgs(args)
        {}
        SimpleShapeClass(const SimpleShapeClass& other, std::string id)
          : mId(std::move(id))
          , mName(other.mName)
          , mShape(other.mShape)
          , mArgs(other.mArgs)
        {}
        const detail::SimpleShapeArgs& GetShapeArgs() const noexcept
        { return mArgs; }
        void SetShapeArgs(detail::SimpleShapeArgs args) noexcept
        { mArgs = args; }
        Shape GetShapeType() const noexcept
        { return mShape; }

        Type GetType() const override
        { return Type::SimpleShape; }
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        std::unique_ptr<DrawableClass> Clone() const override
        { return std::make_unique<SimpleShapeClass>(*this, base::RandomString(10)); }
        std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<SimpleShapeClass>(*this); }
        std::size_t GetHash() const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
    private:
        std::string mId;
        std::string mName;
        SimpleShapeType mShape = SimpleShapeType::Arrow;
        detail::SimpleShapeArgs mArgs;
    };

    namespace detail {
        template<SimpleShapeType type>
        struct SimpleShapeClassTypeShim : public SimpleShapeClass {
            explicit SimpleShapeClassTypeShim(std::string id = base::RandomString(10),
                                     std::string name = "") noexcept
              : SimpleShapeClass(type, std::monostate(), std::move(id), std::move(name))
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

    } // detail

    // Instance of a simple shape when a class object is needed.
    // if you're drawing in "immediate" mode, i.e. creating the
    // drawable shape on the fly (as in a temporary just for the
    // draw call) the optimized version is to use SimpleShape,
    // which will eliminate the need to use a class object.
    class SimpleShapeInstance : public Drawable
    {
    public:
        using Class = SimpleShapeClass;
        using Shape = SimpleShapeClass::Shape;
        using Style = SimpleShapeClass::Style;

        explicit SimpleShapeInstance(std::shared_ptr<const Class> klass, Style style = Style::Solid) noexcept
          : mClass(std::move(klass))
          , mStyle(style)
        {}
        explicit SimpleShapeInstance(const Class& klass, Style style = Style::Solid)
          : mClass(std::make_shared<Class>(klass))
          , mStyle(style)
        {}
        explicit SimpleShapeInstance(Class&& klass, Style style = Style::Solid)
          : mClass(std::make_shared<Class>(std::move(klass)))
          , mStyle(style)
        {}
        bool ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;
        bool Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;
        Type GetType() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Usage GetGeometryUsage() const override;

        const DrawableClass* GetClass() const override
        { return mClass.get(); }

        Shape GetShape() const noexcept
        { return mClass->GetShapeType(); }
        Style GetStyle() const noexcept
        { return mStyle; }
        void SetStyle(Style style) noexcept
        { mStyle = style; }
    private:
        std::shared_ptr<const Class> mClass;
        Style mStyle = Style::Solid;
    };

    // Instance of a simple shape without class object.
    // Optimized version of SimpleShapeInstance for immediate mode
    // drawing, i.e. when drawing with a temporary shape object.
    class SimpleShape : public Drawable
    {
    public:
        using Shape = SimpleShapeType;
        using Style = SimpleShapeStyle;
        explicit SimpleShape(SimpleShapeType shape, Style style = Style::Solid) noexcept
          : mShape(shape)
          , mStyle(style)
        {}
        explicit SimpleShape(SimpleShapeType shape, detail::SimpleShapeArgs args, Style style = Style::Solid) noexcept
          : mShape(shape)
          , mArgs(args)
          , mStyle(style)
        {}
        bool ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;
        bool Construct(const Environment& env, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const override;
        Type GetType() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Usage GetGeometryUsage() const override;

        Shape GetShape() const noexcept
        { return mShape; }
        Style GetStyle() const noexcept
        { return mStyle; }
        void SetStyle(Style style) noexcept
        { mStyle = style; }
    private:
        SimpleShapeType mShape;
        detail::SimpleShapeArgs mArgs;
        Style mStyle = Style::Solid;
    };

    namespace detail {
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

    } // namespace

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


    inline bool Is3DShape(SimpleShapeType shape) noexcept
    {
        if (shape == SimpleShapeType::Cone ||
            shape == SimpleShapeType::Cube ||
            shape == SimpleShapeType::Cylinder ||
            shape == SimpleShapeType::Pyramid ||
            shape == SimpleShapeType::Sphere)
            return true;
        return false;
    }

    inline SimpleShapeType GetSimpleShapeType(const Drawable& drawable)
    {
        if (const auto* ptr = dynamic_cast<const SimpleShapeInstance*>(&drawable))
            return ptr->GetShape();
        if (const auto* ptr = dynamic_cast<const SimpleShape*>(&drawable))
            return ptr->GetShape();
        BUG("Not a simple shape!");
        return SimpleShapeType::Rectangle;
    }

} // namespace
