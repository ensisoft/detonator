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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <glm/gtc/type_ptr.hpp>
#include "warnpop.h"

#include <algorithm>
#include <string>
#include <vector>
#include <limits>
#include <optional>
#include <variant>
#include <unordered_map>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/geometry.h"
#include "graphics/device.h"
#include "graphics/program.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;

    // DrawableClass defines a new type of drawable.
    class DrawableClass
    {
    public:
        using Culling = Device::State::Culling;

        using Usage = Geometry::Usage;

        // Type of the drawable (and its instances)
        enum class Type {
            ParticleEngine,
            Polygon,
            TileBatch,
            SimpleShape,
            Undefined
        };
         // Style of the drawable's geometry determines how the geometry
         // is to be rasterized.
         enum class Primitive {
             Points, Lines, Triangles
         };
         struct Environment {
             // true if running in an "editor mode", which means that even
             // content marked static might have changed and should be checked
             // in case it has been modified and should be re-uploaded.
             bool editing_mode = false;
             // how many render surface units (pixels, texels if rendering to a texture)
             // to a game unit.
             glm::vec2 pixel_ratio = {1.0f, 1.0f};
             // the current projection matrix that will be used to project the
             // vertices from the view space into Normalized Device Coordinates.
             const glm::mat4* proj_matrix = nullptr;
             // The current view matrix that will be used to transform the
             // vertices from the world space to the camera/view space.
             const glm::mat4* view_matrix = nullptr;
             // the current model matrix that will be used to transform the
             // vertices from the local space to the world space.
             const glm::mat4* model_matrix = nullptr;
             // the current world matrix that will be used to transform
             // vectors, such as gravity vector, to world space.
             const glm::mat4* world_matrix = nullptr;
         };

        struct DrawCmd {
            size_t draw_cmd_start = 0;
            size_t draw_cmd_count = std::numeric_limits<size_t>::max();
        };

        virtual ~DrawableClass() = default;
        // Get the type of the drawable.
        virtual Type GetType() const = 0;
        // Get the class ID.
        virtual std::string GetId() const = 0;
        // Get the human-readable class name.
        virtual std::string GetName() const = 0;
        // Set the human-readable class name.
        virtual void SetName(const std::string& name) = 0;
        // Create a copy of this drawable class object but with unique id.
        virtual std::unique_ptr<DrawableClass> Clone() const = 0;
        // Create an exact copy of this drawable object.
        virtual std::unique_ptr<DrawableClass> Copy() const = 0;
        // Get the hash of the drawable class object  based on its properties.
        virtual std::size_t GetHash() const = 0;
        // Serialize into JSON
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
    private:
    };


    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        using Primitive = DrawableClass::Primitive;
        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points.
        using Culling = DrawableClass::Culling;
        // The environment that possibly affects the geometry and drawable
        // generation and update in some way.
        using Environment = DrawableClass::Environment;

        using Type = DrawableClass::Type;

        using Usage = DrawableClass::Usage;

        using DrawCmd = DrawableClass::DrawCmd;

        // Rasterizer state that the geometry can manipulate.
        struct RasterState {
            // rasterizer setting for line width when the geometry
            // contains lines.
            float line_width = 1.0f;
            // Culling state for discarding back/front facing fragments.
            // Culling state only applies to polygon's not to points or lines.
            Culling culling = Culling::Back;
        };

        using CommandArg = std::variant<float, int, std::string>;
        struct Command {
            std::string name;
            std::unordered_map<std::string, CommandArg> args;
        };
        using CommandList = std::vector<Command>;

        virtual ~Drawable() = default;
        // Apply the drawable's state (if any) on the program and set the rasterizer state.
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const = 0;
        // Get the device specific shader source applicable for this drawable, its state
        // and the given environment in which it should execute.
        // Should return an empty string on any error.
        virtual std::string GetShader(const Environment& env, const Device& device) const = 0;
        // Get the shader ID applicable for this drawable, its state and the given
        // environment in which it should execute.
        virtual std::string GetShaderId(const Environment& env) const = 0;
        // Get the human readable debug name that should be associated with the
        // shader object generated from this drawable.
        virtual std::string GetShaderName(const Environment& env) const = 0;
        // Get the geometry name that will be used to identify the
        // drawable geometry on the device.
        virtual std::string GetGeometryId(const Environment& env) const = 0;
        // Construct geometry object create args.
        // Returns true if successful or false if geometry is unavailable.
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const = 0;
        // Update the state of the drawable object. dt is the
        // elapsed (delta) time in seconds.
        virtual void Update(const Environment& env, float dt) {}
        // Get the expected type of primitive used to rasterize the
        // geometry produced by the drawable. This is essentially the
        // "summary" of the draw commands in the geometry object.
        virtual Primitive GetPrimitive() const
        { return Primitive::Triangles; }
        // Get the drawable type.
        virtual Type GetType() const
        { return Type::Undefined; }
        // Get the intended usage of the geometry generated by the drawable.
        virtual Usage GetUsage() const
        { return Usage::Static; }
        // Returns true if the drawable is still considered to be alive.
        // For example a particle simulation still has live particles.
        virtual bool IsAlive() const
        { return true; }

        virtual size_t GetContentHash() const
        { return 0; }

        // Restart the drawable, if applicable.
        virtual void Restart(const Environment& env) {}

        // Execute drawable commands coming from the scripting environment.
        // The commands can be used to change the drawable, alter its parameters
        // or trigger its function such as particle emission.
        virtual void Execute(const Environment& env, const Command& command)
        {}

        virtual DrawCmd  GetDrawCmd() const
        {
            // return the defaults which will then draw every draw
            // low level command associated with the geometry itself.
            return {};
        }

    private:
    };

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


    namespace detail {
        using Environment = Drawable::Environment;
        using Style = SimpleShapeStyle;

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
            static void Generate(const Environment& env, Style style, GeometryBuffer& geometry);
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

    } // namespace

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
                CylinderShapeArgs, ConeShapeArgs, SphereShapeArgs>;
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
          , mArgs(std::move(args))
        {}
        SimpleShapeClass(const SimpleShapeClass& other, std::string id)
          : mId(std::move(id))
          , mName(other.mName)
          , mShape(other.mShape)
          , mArgs(other.mArgs)
        {}
        inline const detail::SimpleShapeArgs& GetShapeArgs() const noexcept
        { return mArgs; }
        inline void SetShapeArgs(detail::SimpleShapeArgs args) noexcept
        { mArgs = std::move(args); }
        inline Shape GetShapeType() const noexcept
        { return mShape; }

        virtual Type GetType() const override
        { return Type::SimpleShape; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual std::unique_ptr<DrawableClass> Clone() const override
        { return std::make_unique<SimpleShapeClass>(*this, base::RandomString(10)); }
        virtual std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<SimpleShapeClass>(*this); }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        std::string mId;
        std::string mName;
        SimpleShapeType mShape;
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

        explicit SimpleShapeInstance(const std::shared_ptr<const Class>& klass, Style style = Style::Solid) noexcept
          : mClass(klass)
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
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;
        virtual Type GetType() const override;
        virtual Primitive GetPrimitive() const override;
        virtual Usage GetUsage() const override;

        inline Shape GetShape() const noexcept
        { return mClass->GetShapeType(); }
        inline Style GetStyle() const noexcept
        { return mStyle; }
        inline void SetStyle(Style style) noexcept
        { mStyle = style; }
    private:
        std::shared_ptr<const Class> mClass;
        Style mStyle;
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
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;
        virtual Type GetType() const override;
        virtual Primitive GetPrimitive() const override;
        virtual Usage GetUsage() const override;

        inline Shape GetShape() const noexcept
        { return mShape; }
        inline Style GetStyle() const noexcept
        { return mStyle; }
        inline void SetStyle(Style style) noexcept
        { mStyle = style; }
    private:
        SimpleShapeType mShape;
        detail::SimpleShapeArgs mArgs;
        Style mStyle;
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

    // render a series of intersecting horizontal and vertical lines
    // at some particular interval (gap distance)
    class Grid : public Drawable
    {
    public:
        // the num vertical and horizontal lines is the number of lines
        // *inside* the grid. I.e. not including the enclosing border lines
        Grid(unsigned num_vertical_lines, unsigned num_horizontal_lines, bool border_lines = true) noexcept
          : mNumVerticalLines(num_vertical_lines)
          , mNumHorizontalLines(num_horizontal_lines)
          , mBorderLines(border_lines)
        {}
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& geometry) const override;

        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
        virtual Usage GetUsage() const override
        { return Usage::Static; }
    private:
        unsigned mNumVerticalLines = 1;
        unsigned mNumHorizontalLines = 1;
        bool mBorderLines = false;
    };

    // Combines multiple primitive draw commands into a single
    // drawable shape.
    class PolygonMeshClass : public DrawableClass
    {
    public:
        enum class MeshType {
            Simple2D,
            Simple3D,
            Model3D
        };

        explicit PolygonMeshClass(std::string id = base::RandomString(10),
                                  std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
        {}
        // Return whether the polygon's data is considered to be
        // static or not. Static content is not assumed to change
        // often and will map the polygon to a geometry object
        // based on the polygon's data. Thus, each polygon with
        // different data will have different geometry object.
        // However, If the polygon is updated frequently this would
        // then lead to the proliferation of excessive geometry objects.
        // In this case static can be set to false and the polygon
        // will map to a (single) dynamic geometry object more optimized
        // for draw/discard type of use.
        inline bool IsStatic() const noexcept
        { return mStatic; }
        // Set the polygon static or not. See comments in IsStatic.
        inline void SetStatic(bool on_off) noexcept
        { mStatic = on_off; }
        inline void SetDynamic(bool on_off) noexcept
        { mStatic = !on_off; }
        inline void SetContentHash(size_t hash) noexcept
        { mContentHash = hash; }
        inline size_t GetContentHash() const noexcept
        { return mContentHash; }
        inline bool HasInlineData() const noexcept
        { return mData.has_value(); }
        inline bool HasContentUri() const noexcept
        { return !mContentUri.empty(); }
        inline void ResetContentUri() noexcept
        { mContentUri.clear(); }
        inline void SetContentUri(std::string uri) noexcept
        { mContentUri = std::move(uri); }
        inline void SetMeshType(MeshType type) noexcept
        { mMesh = type; }
        inline std::string GetContentUri() const
        { return mContentUri; }
        inline MeshType GetMeshType() const noexcept
        { return mMesh; }

        void SetIndexBuffer(IndexBuffer&& buffer) noexcept;
        void SetIndexBuffer(const IndexBuffer& buffer);

        void SetVertexLayout(VertexLayout layout) noexcept;

        void SetVertexBuffer(VertexBuffer&& buffer) noexcept;
        void SetVertexBuffer(std::vector<uint8_t>&& buffer) noexcept;
        void SetVertexBuffer(const VertexBuffer& buffer);
        void SetVertexBuffer(const std::vector<uint8_t>& buffer);

        void SetCommandBuffer(CommandBuffer&& buffer) noexcept;
        void SetCommandBuffer(std::vector<Geometry::DrawCommand>&& buffer) noexcept;
        void SetCommandBuffer(const CommandBuffer& buffer);
        void SetCommandBuffer(const std::vector<Geometry::DrawCommand>& buffer);

        const VertexLayout* GetVertexLayout() const noexcept;
        const void* GetVertexBufferPtr() const noexcept;
        size_t GetVertexBufferSize() const noexcept;
        size_t GetNumDrawCmds() const noexcept;
        const Geometry::DrawCommand* GetDrawCmd(size_t index) const noexcept;

        std::string GetGeometryId(const Environment& env) const;

        bool Construct(const Environment& env, Geometry::CreateArgs& create) const;

        void SetSubMeshDrawCmd(const std::string& key, const DrawCmd& cmd);

        const DrawCmd* GetSubMeshDrawCmd(const std::string& key) const noexcept;

        virtual Type GetType() const override
        { return Type::Polygon; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual std::size_t GetHash() const override;
        virtual std::unique_ptr<DrawableClass> Clone() const override;
        virtual std::unique_ptr<DrawableClass> Copy() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;

    private:
        std::string mId;
        std::string mName;
        std::size_t mContentHash = 0;
        // Content URI for a larger mesh (see InlineData)
        std::string mContentUri;
        // this is to support the simple 2D geometry with
        // only a few vertices. Could be migrated to use
        // a separate file but this is simply just so much
        // simpler for the time being even though it wastes
        // a bit of space since the data is kep around all
        // the time.
        struct InlineData {
            std::vector<uint8_t> vertices;
            std::vector<uint8_t> indices;
            std::vector<Geometry::DrawCommand> cmds;
            VertexLayout layout;
            Geometry::IndexType index_type = Geometry::IndexType::Index16;
        };
        std::optional<InlineData> mData;
        MeshType mMesh = MeshType::Simple2D;
        std::unordered_map<std::string, DrawCmd> mSubMeshes;
        bool mStatic = true;
    };


    class PolygonMeshInstance : public Drawable
    {
    public:
        using MeshType = PolygonMeshClass::MeshType;

        explicit PolygonMeshInstance(const std::shared_ptr<const PolygonMeshClass>& klass,
                                     std::string sub_mesh_key = "")
          : mClass(klass)
          , mSubMeshKey(std::move(sub_mesh_key))
        {}
        explicit PolygonMeshInstance(const PolygonMeshClass& klass, std::string sub_mesh_key = "")
          : mClass(std::make_shared<PolygonMeshClass>(klass))
          , mSubMeshKey(std::move(sub_mesh_key))
        {}

        inline MeshType GetMeshType() const noexcept
        { return mClass->GetMeshType(); }
        inline std::string GetSubMeshKey() const
        { return mSubMeshKey; }
        inline void SetSubMeshKey(std::string key) noexcept
        { mSubMeshKey = std::move(key); }

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;

        virtual DrawCmd GetDrawCmd() const override;

        virtual Primitive GetPrimitive() const override
        { return Primitive::Triangles; }
        virtual Type GetType() const override
        { return Type::Polygon; }
        virtual Usage GetUsage() const override;

        virtual size_t GetContentHash() const override;

    private:
        std::shared_ptr<const PolygonMeshClass> mClass;
        std::string mSubMeshKey;
        mutable bool mError = false;
    };


    // Particle engine interface. Particle engines implement some kind of
    // "particle" / n-body simulation where a variable number of small objects
    // are simulated or animated in some particular way.
    class ParticleEngine
    {
    public:
        // Destructor
        virtual ~ParticleEngine() = default;
        // Todo: add particle engine specific methods if needed.
    private:
    };

    // ParticleEngineClass holds data for some type of
    // particle engine. The data and the class implementation together
    // are used to define a "type" / class for particle engines.
    // For example the user might have defined a particle engine called
    // "smoke" with some particular set of parameters. Once instance
    // (a c++ object) of ParticleEngineClass is then used to
    // represent this particle engine type. ParticleEngineInstance
    // objects are then instances of some engine type and point to the
    // class for class specific behaviour while containing their
    // instance specific data. (I.e. one instance of "smoke" can have
    // particles in different stages as some other instance of "smoke".
    class ParticleEngineClass : public DrawableClass
    {
    public:
        struct Particle {
            // The current particle position in simulation space.
            glm::vec2 position;
            // The current direction of travel in simulation space
            // times velocity.
            glm::vec2 direction;
            // The current particle point size.
            float pointsize = 1.0f;
            // The particle time accumulator
            float time  = 0.0f;
            // Scaler for expressing the lifetime of the particle
            // as a fraction of the maximum lifetime.
            float time_scale = 1.0f;
            // The current distance travelled in simulation units.
            float distance  = 0.0f;
            // random float value between 0.0f and 1.0f
            float randomizer = 0.0f;
            // alpha value between 0.0f and 1.0f
            // 0.0f = particle is fully transparent.
            // 1.0f = particle is fully opaque.
            float alpha = 1.0f;
        };

        // Define the motion of the particle.
        enum class Motion {
            // Follow a linear path.
            Linear,
            // Particles follow a curved path where gravity applies
            // to the vertical component of the particles' velocity
            // vector.
            Projectile
        };
            // Control what happens to the particle when it reaches
        // the boundary of the simulation.
        enum class BoundaryPolicy {
            // Clamp boundary to the boundary value.
            // Use case example: Bursts of particles that blow up
            // and then remain stationary once they land.
            Clamp,
            // Wraps around the boundary.
            // Use case example: never ending star field animation
            Wrap,
            // Kill the particle at the boundary.
            Kill,
            // Reflect the particle at the boundary
            Reflect
        };


        // Control when to spawn particles.
        enum class SpawnPolicy {
            // Spawn only once the initial number of particles
            // and then no more particles.
            Once,
            // Maintain a fixed number of particles in the
            // simulation spawning new particles as old ones die.
            Maintain,
            // Continuously spawn new particles on every update
            // of the simulation. num particles is the number of
            // particles to spawn per second.
            Continuous,
            // Spawn only on emission command
            Command
        };

        // Control the simulation space for the particles.
        enum class CoordinateSpace {
            // Particles are simulated inside a local coordinate space
            // relative to a local origin which is then transformed
            // with the model transform to the world (and view) space.
            Local,
            // Particles are simulated in the global/world coordinate space
            // directly and only transformed to view space.
            Global
        };

        // The shape of the area in which the particles are spawned.
        enum class EmitterShape {
            // uses the rectangular area derived either from the local
            // emitter size and position relative to the local simulation
            // origin or the size and position of the rectangle derived from
            // the position and size of the drawable in world space.
            Rectangle,
            // Uses a circular emitter with the diameter equalling the shortest
            // side of either the local emitter rectangle or the rectangle
            // containing the drawable in world space.
            Circle
        };

        // Control where the particles are spawned.
        enum class Placement {
            // Inside the emitter shape
            Inside,
            // On the edge of the emitter shape only
            Edge,
            // Outside the emitter shape
            Outside,
            // Center of the emitter shape
            Center
        };

        // Control what is the initial direction of the particles
        enum class Direction {
            // Traveling outwards from the center of the emitter
            // through the initial particle position
            Outwards,
            // Traveling inwards from the initial position towards
            // the center of the emitter
            Inwards,
            // Traveling direction according to the direction sector.
            Sector
        };

        // initial engine configuration params
        struct Params {
            Direction direction = Direction::Sector;
            // Placement of particles wrt. the emitter shape
            Placement placement = Placement::Inside;
            // the shape of the area inside which the particles are spawned
            EmitterShape shape = EmitterShape::Rectangle;
            // The coordinate space in which the emitters are spawned
            CoordinateSpace coordinate_space = CoordinateSpace::Local;
            // type of motion for the particles
            Motion motion = Motion::Linear;
            // when to spawn particles.
            SpawnPolicy mode = SpawnPolicy::Maintain;
            // What happens to a particle at the simulation boundary
            BoundaryPolicy boundary = BoundaryPolicy::Clamp;
            // delay until the particles are spawned after start.
            float delay = 0.0f;
            // the maximum time the simulation will ever be alive
            // regardless of whether there are particles or not.
            float max_time = std::numeric_limits<float>::max();
            // the minimum time the simulation will be alive even if
            // there are no particles.
            float min_time = 0;
            // the number of particles this engine shall create
            // the behaviour of this parameter depends on the spawn mode.
            float num_particles = 100;
            // minimum and maximum particle lifetime.
            // any particle will have randomly selected lifetime between
            // the min and max value.
            float min_lifetime = 0.0f;
            float max_lifetime = std::numeric_limits<float>::max();
            // the maximum bounds of travel for each particle.
            // at the bounds the particles either die, wrap or clamp
            float max_xpos = 1.0f;
            float max_ypos = 1.0f;
            // the starting rectangle for each particle. each particle
            // begins life at a random location within this rectangle
            float init_rect_xpos   = 0.0f;
            float init_rect_ypos   = 0.0f;
            float init_rect_width  = 1.0f;
            float init_rect_height = 1.0f;
            // each particle has an initial velocity between min and max
            float min_velocity = 1.0f;
            float max_velocity = 1.0f;
            // each particle has a direction vector within a sector
            // defined by start angle and the size of the sector
            // expressed by angle
            float direction_sector_start_angle = 0.0f;
            float direction_sector_size = math::Pi * 2.0f;
            // min max points sizes.
            float min_point_size = 1.0f;
            float max_point_size = 1.0f;
            // min max alpha values.
            float min_alpha = 1.0f;
            float max_alpha = 1.0f;
            // rate of change with respect to unit of time.
            float rate_of_change_in_size_wrt_time = 0.0f;
            // rate of change with respect to unit of distance.
            float rate_of_change_in_size_wrt_dist = 0.0f;
            // rate of change in alpha value with respect to unit of time.
            float rate_of_change_in_alpha_wrt_time = 0.0f;
            // rate of change in alpha value with respect to unit of distance.
            float rate_of_change_in_alpha_wrt_dist = 0.0f;
            // the gravity that applies when using projectile particles.
            glm::vec2 gravity = {0.0f, 0.3f};
        };

        // State of any instance of ParticleEngineInstance.
        struct InstanceState {
            // cached world gravity vector for this particle system instance.
            // exists only to avoid recomputing the gravity on every update.
            std::optional<glm::vec2> cached_world_gravity;
            // the simulation particles.
            std::vector<Particle> particles;
            // delay until the particles are first initially emitted.
            float delay = 0.0f;
            // simulation time.
            float time = 0.0f;
            // fractional count of new particles being hatched.
            float hatching = 0.0f;
        };

        explicit ParticleEngineClass(const Params& init, std::string id = base::RandomString(10), std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
          , mParams(init)
        {}
        explicit ParticleEngineClass(std::string id = base::RandomString(10), std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
        {}

        bool Construct(const Environment& env, const InstanceState& state, Geometry::CreateArgs& create) const;
        std::string GetShader(const Environment& env, const Device& device) const;
        std::string GetProgramId(const Environment& env) const;
        std::string GetShaderName(const Environment& env) const;
        std::string GetGeometryId(const Environment& env) const;

        void ApplyDynamicState(const Environment& env, ProgramState& program) const;
        void Update(const Environment& env, InstanceState& state, float dt) const;
        void Restart(const Environment& env, InstanceState& state) const;
        bool IsAlive(const InstanceState& state) const;

        void Emit(const Environment& env, InstanceState& state, int count) const;

        // Get the params.
        inline const Params& GetParams() const noexcept
        { return mParams; }
        // Set the params.
        inline void SetParams(const Params& params) noexcept
        { mParams = params;}

        virtual Type GetType() const override
        { return DrawableClass::Type::ParticleEngine; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; };
        virtual void SetName(const std::string& name) override
        { mName = name; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::unique_ptr<DrawableClass> Clone() const override;
        virtual std::unique_ptr<DrawableClass> Copy() const override;
    private:
        void InitParticles(const Environment& env, InstanceState& state, size_t num) const;
        void KillParticle(InstanceState& state, size_t i) const;
        bool UpdateParticle(const Environment& env, InstanceState& state, size_t i, float dt) const;
    private:
        std::string mId;
        std::string mName;
        Params mParams;
    };

    // ParticleEngineInstance implements particle simulation
    // based on pure motion without reference to the forces
    // or the masses acting upon the particles.
    // It represents an instance of a some type of ParticleEngineClass,
    // which is the "type definition" i.e. class for some particle engine.
    class ParticleEngineInstance : public Drawable,
                                   public ParticleEngine
    {
    public:
        using Params = ParticleEngineClass::Params;

        // Create a new particle engine based on an existing particle engine
        // class definition.
        explicit ParticleEngineInstance(const std::shared_ptr<const ParticleEngineClass>& klass) noexcept
          : mClass(klass)
        {}
        explicit ParticleEngineInstance(const ParticleEngineClass& klass)
          : mClass(std::make_shared<ParticleEngineClass>(klass))
        {}
        explicit ParticleEngineInstance(const ParticleEngineClass::Params& params)
          : mClass(std::make_shared<ParticleEngineClass>(params))
        {}
        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment&  env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual void Update(const Environment& env, float dt) override;
        virtual bool IsAlive() const override;
        virtual void Restart(const Environment& env) override;
        virtual void Execute(const Environment& env, const Command& cmd) override;
        virtual Type GetType() const override
        { return Type::ParticleEngine; }
        virtual Primitive  GetPrimitive() const override
        { return Primitive::Points; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }

        // Get the current number of alive particles.
        inline size_t GetNumParticlesAlive() const noexcept
        { return mState.particles.size(); }

        inline const Params& GetParams() const noexcept
        { return mClass->GetParams(); }

    private:
        // this is the "class" object for this particle engine type.
        std::shared_ptr<const ParticleEngineClass> mClass;
        // this is this particle engine's state.
        ParticleEngineClass::InstanceState mState;
    };


    class TileBatch : public Drawable
    {
    public:
        enum class TileShape {
            Automatic, Square, Rectangle
        };
        enum class Projection {
            Dimetric, AxisAligned
        };

        struct Tile {
            Vec3 pos;
        };
        TileBatch() = default;
        explicit TileBatch(const std::vector<Tile>& tiles)
          : mTiles(tiles)
        {}
        explicit TileBatch(std::vector<Tile>&& tiles) noexcept
          : mTiles(std::move(tiles))
        {}

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& raster) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override;
        virtual Type GetType() const override
        { return Type::TileBatch; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }

        inline void AddTile(const Tile& tile)
        { mTiles.push_back(tile); }
        inline void ClearTiles() noexcept
        { mTiles.clear(); }
        inline size_t GetNumTiles() const noexcept
        { return mTiles.size(); }
        inline const Tile& GetTile(size_t index) const noexcept
        { return mTiles[index]; }
        inline Tile& GetTile(size_t index) noexcept
        { return mTiles[index]; }
        inline glm::vec3 GetTileWorldSize() const noexcept
        { return mTileWorldSize; }
        inline glm::vec2 GetTileRenderSize() const noexcept
        { return mTileRenderSize; }

        // Set the tile width in tile world units.
        inline void SetTileWorldWidth(float width) noexcept
        { mTileWorldSize.x = width; }
        inline void SetTileWorldHeight(float height) noexcept
        { mTileWorldSize.y = height; }
        inline void SetTileWorldDepth(float depth) noexcept
        { mTileWorldSize.z = depth; }
        inline void SetTileWorldSize(const glm::vec3& size) noexcept
        { mTileWorldSize = size; }
        inline void SetTileRenderWidth(float width) noexcept
        { mTileRenderSize.x = width; }
        inline void SetTileRenderHeight(float height) noexcept
        { mTileRenderSize.y = height; }
        inline void SetTileRenderSize(const glm::vec2& size) noexcept
        { mTileRenderSize = size; }
        inline void SetProjection(Projection projection) noexcept
        { mProjection = projection; }

        TileShape ResolveTileShape() const noexcept
        {
            if (mShape == TileShape::Automatic) {
                if (math::equals(mTileRenderSize.x, mTileRenderSize.y))
                    return TileShape::Square;
                else return TileShape::Rectangle;
            }
            return mShape;
        }
        inline TileShape GetTileShape() const noexcept
        { return mShape; }
        inline void SetTileShape(TileShape shape) noexcept
        { mShape = shape; }
    private:
        Projection mProjection = Projection::AxisAligned;
        TileShape mShape = TileShape::Automatic;
        std::vector<Tile> mTiles;
        glm::vec3 mTileWorldSize = {0.0f, 0.0f, 0.0f};
        glm::vec2 mTileRenderSize = {0.0f, 0.0f};
    };

    class DynamicLine3D : public Drawable
    {
    public:
        DynamicLine3D(const glm::vec3& a, const glm::vec3& b, float line_width=1.0f) noexcept
          : mPointA(a)
          , mPointB(b)
        {}
        virtual void ApplyDynamicState(const Environment& environment, ProgramState& program, RasterState& state) const override;
        virtual std::string GetShader(const Environment& environment, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& environment) const override;
        virtual std::string GetShaderName(const Environment& environment) const override;
        virtual std::string GetGeometryId(const Environment& environment) const override;
        virtual bool Construct(const Environment& environment, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }
    private:
        glm::vec3 mPointA;
        glm::vec3 mPointB;
    };


    class DebugDrawableBase : public Drawable
    {
    public:
        enum class Feature {
            Normals,
            Wireframe
        };

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState&  state) const override;
        virtual std::string GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual Usage GetUsage() const override;
        virtual size_t GetContentHash() const override;
        virtual Primitive GetPrimitive() const override
        { return Primitive::Lines; }
    protected:
        DebugDrawableBase(const Drawable* drawable, Feature feature)
          : mDrawable(drawable)
          , mFeature(feature)
        {}
        DebugDrawableBase() = default;

        const Drawable* mDrawable = nullptr;
        Feature mFeature;
    };

    class DebugDrawableInstance : public DebugDrawableBase
    {
    public:
        DebugDrawableInstance(const std::shared_ptr<const Drawable> drawable, Feature feature)
          : DebugDrawableBase(drawable.get(), feature)
        {
            mSharedDrawable = drawable;
        }
    private:
        std::shared_ptr<const Drawable> mSharedDrawable;
    };

    class WireframeInstance : public DebugDrawableInstance
    {
    public:
        WireframeInstance(const std::shared_ptr<const Drawable>& drawable)
          : DebugDrawableInstance(drawable, Feature::Wireframe)
        {}
    };

    template<typename T>
    class DebugDrawable : public DebugDrawableBase
    {
    public:
        using Feature = DebugDrawableBase::Feature;

        template<typename... Args>
        DebugDrawable(Feature feature, Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = feature;
        }
    private:
        T mObject;
    };
    template<typename T>
    class Wireframe : public DebugDrawableBase
    {
    public:
        template<typename... Args>
        Wireframe(Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = Feature::Wireframe;
        }
    private:
        T mObject;
    };


    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

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

    inline bool Is3DShape(const Drawable& drawable) noexcept
    {
        const auto type = drawable.GetType();
        if (type == Drawable::Type::Polygon)
        {
            if (const auto* instance = dynamic_cast<const PolygonMeshInstance*>(&drawable))
            {
                const auto mesh = instance->GetMeshType();
                if (mesh == PolygonMeshInstance::MeshType::Simple3D ||
                    mesh == PolygonMeshInstance::MeshType::Model3D)
                    return true;
            }
        }

        if (type != Drawable::Type::SimpleShape)
            return false;
        if (const auto* instance = dynamic_cast<const SimpleShapeInstance*>(&drawable))
            return Is3DShape(instance->GetShape());
        else if (const auto* instance = dynamic_cast<const SimpleShape*>(&drawable))
            return Is3DShape(instance->GetShape());
        else BUG("Unknown drawable shape type.");
    }

} // namespace
