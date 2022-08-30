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
    class Packer;

    // DrawableClass defines a new type of drawable.
     class DrawableClass
    {
    public:
        using Culling = Device::State::Culling;
        // Type of the drawable (and its instances)
        enum class Type {
            Arrow,
            Capsule,
            SemiCircle,
            Sector,
            Cursor,
            Circle,
            Grid,
            IsoscelesTriangle,
            KinematicsParticleEngine,
            Line,
            Parallelogram,
            Polygon,
            Rectangle,
            RightTriangle,
            RoundRectangle,
            Trapezoid,
        };
         // Style of the drawable's geometry determines how the geometry
         // is to be rasterized.
         enum class Style {
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
         };

        virtual ~DrawableClass() = default;
        // Get the type of the drawable.
        virtual Type GetType() const = 0;
        // Get the class ID.
        virtual std::string GetId() const = 0;
        // Create a copy of this drawable class object but with unique id.
        virtual std::unique_ptr<DrawableClass> Clone() const = 0;
        // Create an exact copy of this drawable object.
        virtual std::unique_ptr<DrawableClass> Copy() const = 0;
        // Get the hash of the drawable class object  based on its properties.
        virtual std::size_t GetHash() const = 0;
        // Pack the drawable resources.
        virtual void Pack(Packer* packer) const = 0;
        // Serialize into JSON
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool LoadFromJson(const data::Reader& data) = 0;
    private:
    };


    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        using Style = DrawableClass::Style;
        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points.
        using Culling = DrawableClass::Culling;
        // The environment that possibly affects the geometry and drawable
        // generation and update in some way.
        using Environment = DrawableClass::Environment;

        // Rasterizer state that the geometry can manipulate.
        struct RasterState {
            // rasterizer setting for line width when the geometry
            // contains lines.
            float line_width = 1.0f;
            // Culling state for discarding back/front facing fragments.
            // Culling state only applies to polygon's not to points or lines.
            Culling culling = Culling::Back;
        };

        virtual ~Drawable() = default;
        // Apply the drawable's state (if any) on the program
        // and set the rasterizer state.
        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const = 0;
        // Get the device specific shader object.
        // If the shader does not yet exist on the device it's created
        // and compiled.  On any errors nullptr should be returned.
        virtual Shader* GetShader(Device& device) const = 0;
        // Get the device specific geometry object. If the geometry
        // does not yet exist on the device it's created and the
        // contents from this drawable object are uploaded in some
        // device specific data format.
        virtual Geometry* Upload(const Environment& env, Device& device) const = 0;
        // Update the state of the drawable object. dt is the
        // elapsed (delta) time in seconds.
        virtual void Update(const Environment& env, float dt) {}
        // Request a particular line width to be used when style
        // is either Outline or Wireframe.
        virtual void SetLineWidth(float width) {}
        // Set the style to be used for rasterizing the drawable
        // shapes's fragments.
        // Not all drawables support all Styles.
        virtual void SetStyle(Style style) {}
        // Set the culling flag for the drawable.
        virtual void SetCulling(Culling culling) {}
        // Get the current style.
        virtual Style GetStyle() const = 0;
        // Returns true if the drawable is still considered to be alive.
        // For example a particle simulation still has live particles.
        virtual bool IsAlive() const
        { return true; }
        // Restart the drawable, if applicable.
        virtual void Restart(const Environment& env) {}
        // Get the vertex program ID for shape. Used to map the
        // drawable to a device specific program object.
        virtual std::string GetProgramId() const = 0;
    private:
    };

    namespace detail {
        // helper class template to stomp out the class implementation classes
        // in cases where some class state is needed including a class ID
        template<typename Class>
        class DrawableClassBase : public DrawableClass
        {
        public:
            using BaseClass = DrawableClassBase;

            virtual std::string GetId() const override
            { return mId; }
            virtual std::unique_ptr<DrawableClass> Clone() const override
            {
                auto ret = std::make_unique<Class>(*static_cast<const Class*>(this));
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<DrawableClass> Copy() const override
            { return std::make_unique<Class>(*static_cast<const Class*>(this)); }
        protected:
            DrawableClassBase()
              : mId(base::RandomString(10))
            {}
            DrawableClassBase(const std::string& id)
              : mId(id)
            {}
        protected:
            std::string mId;
        };

        // helper class template to stomp out generic class type that
        // doesn't have any associated class state other than ID.
        template<DrawableClass::Type DrawableType>
        class GenericDrawableClass : public DrawableClass
        {
        public:
            GenericDrawableClass()
              : mId(base::RandomString(10))
            {}
            GenericDrawableClass(const std::string& id)
              : mId(id)
            {}
            virtual Type GetType() const override
            { return DrawableType; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::unique_ptr<DrawableClass> Clone() const override
            {
                auto ret = std::make_unique<GenericDrawableClass>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual std::unique_ptr<DrawableClass> Copy() const override
            { return std::make_unique<GenericDrawableClass>(*this); }
            virtual std::size_t GetHash() const override
            { return base::hash_combine(0, mId); }
            virtual void Pack(Packer*) const override {}
            virtual void IntoJson(data::Writer& writer) const override
            {
                writer.Write("id", mId);
            }
            virtual bool LoadFromJson(const data::Reader& reader) override
            {
                reader.Read("id", &mId);
                return true;
            }
        protected:
            std::string mId;
        };


        // helper class template to create a mostly generic drawable instance
        // that is customized through the DrawableGeometry template parameter
        template<typename DrawableGeometry>
        class GenericDrawable : public Drawable
        {
        public:
            GenericDrawable() = default;
            GenericDrawable(Style style, float linewidth)
              : mStyle(style)
              , mLineWidth(linewidth)
            {}
            GenericDrawable(Style style)
              : mStyle(style)
            {}
            GenericDrawable(float linewidth)
              : mLineWidth(linewidth)
            {}
            virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
            {
                state.line_width = mLineWidth;
                state.culling    = mCulling;
                const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
                const auto& kProjectionMatrix = *env.proj_matrix;
                program.SetUniform("kProjectionMatrix",
                    *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
                program.SetUniform("kModelViewMatrix",
                   *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
            }
            virtual Shader* GetShader(Device& device) const override
            { return DrawableGeometry::GetShader(device); }
            virtual std::string GetProgramId() const override
            { return DrawableGeometry::GetProgramId(); }
            virtual Geometry* Upload(const Environment& env, Device& device) const override
            { return DrawableGeometry::Generate(env, mStyle, device); }
            virtual void SetCulling(Culling culling) override
            { mCulling = culling; }
            virtual void SetLineWidth(float width) override
            { mLineWidth = width; }
            virtual void SetStyle(Style style) override
            { mStyle = style; }
            virtual Style GetStyle() const override
            { return mStyle; }
        private:
            Style mStyle     = DrawableGeometry::InitialStyle;
            Culling mCulling = DrawableGeometry::InitialCulling;
            float mLineWidth = 1.0f;
        };

        struct GeometryBase {
            using Environment = Drawable::Environment;
            using Style       = Drawable::Style;
            using Culling     = Drawable::Culling ;
            static constexpr Culling InitialCulling = Culling::Back;
            static constexpr Style   InitialStyle   = Style::Solid;
            static Shader* GetShader(Device& device);
            static std::string GetProgramId();
        };
        struct ArrowGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct LineGeometry : public GeometryBase {
            static constexpr Culling InitialCulling = Culling::None;
            static constexpr Style   InitialStyle   = Style::Outline;
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct CapsuleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct SemiCircleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct CircleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct RectangleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct IsoscelesTriangleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct RightTriangleGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct TrapezoidGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
        struct ParallelogramGeometry : public GeometryBase {
            static Geometry* Generate(const Environment& env, Style style, Device& device);
        };
    } // namespace

    // shim type definitions for drawables that don't have or need their
    // own actual type class yet.
    using ArrowClass             = detail::GenericDrawableClass<DrawableClass::Type::Arrow>;
    using CapsuleClass           = detail::GenericDrawableClass<DrawableClass::Type::Capsule>;
    using SemiCircleClass        = detail::GenericDrawableClass<DrawableClass::Type::SemiCircle>;
    using CircleClass            = detail::GenericDrawableClass<DrawableClass::Type::Circle>;
    using IsoscelesTriangleClass = detail::GenericDrawableClass<DrawableClass::Type::IsoscelesTriangle>;
    using LineClass              = detail::GenericDrawableClass<DrawableClass::Type::Line>;
    using ParallelogramClass     = detail::GenericDrawableClass<DrawableClass::Type::Parallelogram>;
    using RectangleClass         = detail::GenericDrawableClass<DrawableClass::Type::Rectangle>;
    using RightTriangleClass     = detail::GenericDrawableClass<DrawableClass::Type::RightTriangle>;
    using TrapezoidClass         = detail::GenericDrawableClass<DrawableClass::Type::Trapezoid>;

    // generic drawable definitions for drawables that don't need special
    // behaviour at runtime.
    using Arrow             = detail::GenericDrawable<detail::ArrowGeometry>;
    using Capsule           = detail::GenericDrawable<detail::CapsuleGeometry>;
    using SemiCircle        = detail::GenericDrawable<detail::SemiCircleGeometry>;
    using Circle            = detail::GenericDrawable<detail::CircleGeometry>;
    using IsoscelesTriangle = detail::GenericDrawable<detail::IsoscelesTriangleGeometry>;
    using Line              = detail::GenericDrawable<detail::LineGeometry>;
    using Parallelogram     = detail::GenericDrawable<detail::ParallelogramGeometry>;
    using Rectangle         = detail::GenericDrawable<detail::RectangleGeometry>;
    using RightTriangle     = detail::GenericDrawable<detail::RightTriangleGeometry>;
    using Trapezoid         = detail::GenericDrawable<detail::TrapezoidGeometry>;

    class SectorClass : public detail::DrawableClassBase<SectorClass>
    {
    public:
        SectorClass(float percentage = 0.25f)
          : BaseClass()
          , mPercentage(percentage)
        {}
        SectorClass(const std::string& id, float percentage = 0.25f)
          : BaseClass(id)
          , mPercentage(percentage)
        {}
        Shader* GetShader(Device& device) const;
        Geometry* Upload(const Environment& environment, Style style, Device& device) const;
        std::string GetProgramId() const;

        virtual Type GetType() const override
        { return Type::Sector; }
        virtual std::size_t GetHash() const override;
        virtual void Pack(Packer* packer) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;
    private:
        float mPercentage = 0.25f;
    };
    class Sector : public Drawable
    {
    public:
        Sector(const std::shared_ptr<const SectorClass>& klass)
          : mClass(klass)
        {}
        Sector()
          : mClass(std::make_shared<SectorClass>())
        {}
        Sector(Style style) : Sector()
        {
            mStyle = style;
        }
        Sector(Style style, float linewidth) : Sector()
        {
            mStyle = style;
            mLineWidth = linewidth;
        }
        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            state.line_width = mLineWidth;
            state.culling    = mCulling;
            const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
            const auto& kProjectionMatrix = *env.proj_matrix;
            program.SetUniform("kProjectionMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
            program.SetUniform("kModelViewMatrix",
               *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(env, mStyle, device); }
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
        virtual std::string GetProgramId() const override
        { return mClass->GetProgramId(); }
    private:
        std::shared_ptr<const SectorClass> mClass;
        Style mStyle     = Style::Solid;
        float mLineWidth = 1.0f;
        Culling mCulling = Culling::Back;
    };

    class RoundRectangleClass : public detail::DrawableClassBase<RoundRectangleClass>
    {
    public:
        RoundRectangleClass(float corner_radius = 0.05)
          : BaseClass()
          , mRadius(corner_radius)
        {}
        RoundRectangleClass(const std::string& id, float corner_radius = 0.05)
          : BaseClass(id)
          , mRadius(corner_radius)
        {}

        float GetRadius() const
        { return mRadius; }
        void SetRadius(float radius)
        { mRadius = radius;}

        Shader* GetShader(Device& device) const;
        Geometry* Upload(const Drawable::Environment& env, Style style, Device& device) const;
        std::string GetProgramId() const;

        virtual Type GetType() const override
        { return Type::RoundRectangle; }
        virtual std::size_t GetHash() const override;
        virtual void Pack(Packer* packer) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;
    private:
        float mRadius = 0.05;
    };

    // Rectangle with rounded corners
    class RoundRectangle : public Drawable
    {
    public:
        RoundRectangle(const std::shared_ptr<const RoundRectangleClass>& klass)
          : mClass(klass)
        {}
        RoundRectangle(const RoundRectangleClass& klass)
          : mClass(std::make_shared<RoundRectangleClass>(klass))
        {}
        RoundRectangle()
          : mClass(std::make_shared<RoundRectangleClass>())
        {}
        RoundRectangle(Style style) : RoundRectangle()
        {
            mStyle = style;
        }
        RoundRectangle(Style style, float linewidth) : RoundRectangle()
        {
            mStyle = style;
            mLineWidth = linewidth;
        }

        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            state.line_width = mLineWidth;
            state.culling    = mCulling;
            const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
            const auto& kProjectionMatrix = *env.proj_matrix;
            program.SetUniform("kProjectionMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
            program.SetUniform("kModelViewMatrix",
               *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(env, mStyle, device); }
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
        virtual std::string GetProgramId() const override
        { return mClass->GetProgramId(); }
    private:
        std::shared_ptr<const RoundRectangleClass> mClass;
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class GridClass : public detail::DrawableClassBase<GridClass>
    {
    public:
        GridClass() : BaseClass()
        {}
        GridClass(const std::string& id) : BaseClass(id)
        {}
        Shader* GetShader(Device& device) const;
        Geometry* Upload(Device& device) const;
        std::string GetProgramId() const;
        void SetNumVerticalLines(unsigned lines)
        { mNumVerticalLines = lines; }
        void SetNumHorizontalLines(unsigned lines)
        { mNumHorizontalLines = lines; }
        void SetBorders(bool on_off)
        { mBorderLines = on_off; }
        unsigned GetNumVerticalLines() const
        { return mNumVerticalLines; }
        unsigned GetNumHorizontalLines() const
        { return mNumHorizontalLines; }
        bool HasBorderLines() const
        { return mBorderLines; }

        virtual Type GetType() const override
        { return Type::Grid; }
        virtual std::size_t GetHash() const override;
        virtual void Pack(Packer* packer) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;
    private:
        unsigned mNumVerticalLines = 1;
        unsigned mNumHorizontalLines = 1;
        bool mBorderLines = false;
    };

    // render a series of intersecting horizontal and vertical lines
    // at some particular interval (gap distance)
    class Grid : public Drawable
    {
    public:
        Grid(const std::shared_ptr<const GridClass>& klass)
            : mClass(klass)
        {}
        Grid(const GridClass& klass)
           : mClass(std::make_shared<GridClass>(klass))
        {}

        // the num vertical and horizontal lines is the number of lines
        // *inside* the grid. I.e. not including the enclosing border lines
        Grid(unsigned num_vertical_lines, unsigned num_horizontal_lines, bool border_lines = true)
        {
            auto klass = std::make_shared<GridClass>();
            klass->SetNumVerticalLines(num_vertical_lines);
            klass->SetNumHorizontalLines(num_horizontal_lines);
            klass->SetBorders(border_lines);
            mClass = klass;
        }
        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            state.line_width = mLineWidth;
            state.culling    = Culling::None;
            const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
            const auto& kProjectionMatrix = *env.proj_matrix;
            program.SetUniform("kProjectionMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
            program.SetUniform("kModelViewMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(device); }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return Style::Outline; }
        virtual std::string GetProgramId() const override
        { return mClass->GetProgramId(); }
    private:
        std::shared_ptr<const GridClass> mClass;
        float mLineWidth = 1.0f;
    };

    // Combines multiple primitive draw commands into a single
    // drawable shape.
    class PolygonClass : public detail::DrawableClassBase<PolygonClass>
    {
    public:
        // Define how the geometry is to be rasterized.
        using DrawType = Geometry::DrawType;

        struct DrawCommand {
            DrawType type = DrawType::Triangles;
            size_t offset = 0;
            size_t count  = 0;
        };

        using Vertex = gfx::Vertex;

        PolygonClass() : BaseClass()
        {}
        PolygonClass(const std::string& id) : BaseClass(id)
        {}

        void Clear();
        void ClearDrawCommands();
        void ClearVertices();

        // Add the array of vertices to the existing vertex buffer.
        void AddVertices(const std::vector<Vertex>& vertices);
        void AddVertices(std::vector<Vertex>&& vertices);
        void AddVertices(const Vertex* vertices, size_t num_vertices);
        // Update the vertex at the given index.
        void UpdateVertex(const Vertex& vert, size_t index);
        // Erase the vertex at the given index.
        void EraseVertex(size_t index);
        // Insert a vertex into the vertex array where the index is
        // an index within the given draw command. Index can be in
        // the range [0, cmd.count].
        // After the new vertex has been inserted the list of draw
        // commands is then modified to include the new vertex.
        // I.e. the draw command that includes the
        // new vertex will grow its count by 1 and all draw commands
        // that come after the will have their starting offsets
        // incremented by 1.
        void InsertVertex(const Vertex& vertex, size_t cmd_index, size_t index);

        void AddDrawCommand(const DrawCommand& cmd);

        void UpdateDrawCommand(const DrawCommand& cmd, size_t index);
        // Find the draw command that contains the vertex at the given index.
        // Returns index to the draw command.
        const size_t FindDrawCommand(size_t vertex_index) const;

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
        bool IsStatic() const
        { return mStatic; }

        size_t GetContentHash() const;
        size_t GetNumVertices() const
        { return mVertices.size(); }
        size_t GetNumDrawCommands() const
        { return mDrawCommands.size(); }
        const DrawCommand& GetDrawCommand(size_t index) const
        { return mDrawCommands[index]; }
        const Vertex& GetVertex(size_t index) const
        { return mVertices[index]; }
        // Set the polygon static or not. See comments in IsStatic.
        void SetStatic(bool on_off)
        { mStatic = on_off; }
        void SetDynamic(bool on_off)
        { mStatic = !on_off; }
        Shader* GetShader(Device& device) const;
        Geometry* Upload(bool editing_mode, Device& device) const;
        std::string GetProgramId() const;

        virtual Type GetType() const override
        { return Type::Polygon; }
        virtual void Pack(Packer* packer) const override;
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;

        // Load from JSON
        static std::optional<PolygonClass> FromJson(const data::Reader& data);
    private:
        std::vector<Vertex> mVertices;
        std::vector<DrawCommand> mDrawCommands;
        bool mStatic = true;
    };

    class Polygon : public Drawable
    {
    public:
        Polygon(const std::shared_ptr<const PolygonClass>& klass)
          : mClass(klass)
        {}
        Polygon(const PolygonClass& klass)
          : mClass(std::make_shared<PolygonClass>(klass))
        {}

        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            state.culling = mCulling;
            state.line_width = mLineWidth;
            const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
            const auto& kProjectionMatrix = *env.proj_matrix;
            program.SetUniform("kProjectionMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
            program.SetUniform("kModelViewMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(env.editing_mode, device); }
        virtual Style GetStyle() const override
        { return Style::Solid; }
        virtual void SetCulling(Culling culling) override
        { mCulling = culling;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual std::string GetProgramId() const override
        { return mClass->GetProgramId(); }
    private:
        std::shared_ptr<const PolygonClass> mClass;
        Culling mCulling = Culling::Back;
        float mLineWidth = 1.0f;
    };

    class CursorClass : public detail::DrawableClassBase<CursorClass>
    {
    public:
        enum class Shape {
            Arrow,
            Block
        };
        CursorClass(Shape shape)
          : BaseClass()
          , mShape(shape)
        {}

        CursorClass(const std::string& id, Shape shape)
          : BaseClass(id)
          , mShape(shape)
        {}

        Shape GetShape() const
        { return mShape; }

        virtual Type GetType() const override
        { return Type::Cursor; }
        virtual std::size_t GetHash() const override;
        virtual void Pack(Packer* packer) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;
    private:
        Shape mShape = Shape::Arrow;
    };

    class Cursor : public Drawable
    {
    public:
        using Shape = CursorClass::Shape;

        Cursor(const std::shared_ptr<const CursorClass>& klass)
          : mClass(klass)
        {}
        Cursor(const CursorClass& klass)
          : mClass(std::make_shared<CursorClass>(klass))
        {}
        Cursor(Shape shape)
          : mClass(std::make_shared<CursorClass>(shape))
        {}
        Shape GetShape() const
        { return mClass->GetShape(); }

        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            const auto& kModelViewMatrix  = (*env.view_matrix) * (*env.model_matrix);
            const auto& kProjectionMatrix = *env.proj_matrix;
            program.SetUniform("kProjectionMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kProjectionMatrix));
            program.SetUniform("kModelViewMatrix",
                *(const Program::Matrix4x4 *) glm::value_ptr(kModelViewMatrix));
        }
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual Style GetStyle() const override
        { return Style::Solid; }
        virtual std::string GetProgramId() const override;
    private:
        std::shared_ptr<const CursorClass> mClass;
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

    // KinematicsParticleEngineClass holds data for some type of
    // particle engine. The data and the class implementation together
    // are used to define a "type" / class for particle engines.
    // For example the user might have defined a particle engine called
    // "smoke" with some particular set of parameters. Once instance
    // (a c++ object) of KinematicsParticleEngineClass is then used to
    // represent this particle engine type. KinematicsParticleEngine
    // objects are then instances of some engine type and point to the
    // class for class specific behaviour while containing their
    // instance specific data. (I.e. one instance of "smoke" can have
    // particles in different stages as some other instance of "smoke".
    class KinematicsParticleEngineClass : public detail::DrawableClassBase<KinematicsParticleEngineClass>
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
            // The expected lifetime of the particle.
            float lifetime  = 0.0f;
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
            Continuous
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
            Outside
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

        // State of any instance of KinematicsParticleEngine.
        struct InstanceState {
            // the simulation particles.
            std::vector<Particle> particles;
            // delay until the particles are first initially emitted.
            float delay = 0.0f;
            // simulation time.
            float time = 0.0f;
            // fractional count of new particles being hatched.
            float hatching = 0.0f;

        };

        KinematicsParticleEngineClass()
          : BaseClass()
        {}
        KinematicsParticleEngineClass(const std::string& id)
          : BaseClass(id)
        {}
        KinematicsParticleEngineClass(const Params& init)
          : BaseClass()
          , mParams(init)
        {}
        KinematicsParticleEngineClass(const std::string& id, const Params& init)
          : BaseClass(id)
          , mParams(init)
        {}

        Shader* GetShader(Device& device) const;
        Geometry* Upload(const Environment& env, const InstanceState& state, Device& device) const;

        std::string GetProgramId() const;

        void ApplyDynamicState(const Environment& env, Program& program) const;
        void Update(const Environment& env, InstanceState& state, float dt) const;
        void Restart(const Environment& env, InstanceState& state) const;
        bool IsAlive(const InstanceState& state) const;

        // Get the params.
        const Params& GetParams() const
        { return mParams; }
        // Set the params.
        void SetParams(const Params& p)
        { mParams = p;}

        virtual Type GetType() const override
        { return DrawableClass::Type::KinematicsParticleEngine; }
        virtual std::size_t GetHash() const override;
        virtual void Pack(Packer* packer) const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool LoadFromJson(const data::Reader& data) override;
        // Load from JSON
        static std::optional<KinematicsParticleEngineClass> FromJson(const data::Reader& data);
    private:
        void InitParticles(const Environment& env, InstanceState& state, size_t num) const;
        void KillParticle(InstanceState& state, size_t i) const;
        bool UpdateParticle(const Environment& env, InstanceState& state, size_t i, float dt) const;
    private:
        Params mParams;
    };

    // KinematicsParticleEngine implements particle simulation
    // based on pure motion without reference to the forces
    // or the masses acting upon the particles.
    // It represents an instance of a some type of KinematicsParticleEngineClass,
    // which is the "type definition" i.e. class for some particle engine.
    class KinematicsParticleEngine : public Drawable,
                                     public ParticleEngine
    {
    public:
        // Create a new particle engine based on an existing particle engine
        // class definition.
        KinematicsParticleEngine(const std::shared_ptr<const KinematicsParticleEngineClass>& klass)
          : mClass(klass)
        {}
        KinematicsParticleEngine(const KinematicsParticleEngineClass& klass)
          : mClass(std::make_shared<KinematicsParticleEngineClass>(klass))
        {}
        KinematicsParticleEngine(const KinematicsParticleEngineClass::Params& params)
          : mClass(std::make_shared<KinematicsParticleEngineClass>(params))
        {}
        virtual void ApplyDynamicState(const Environment& env, Program& program, RasterState& state) const override
        {
            state.line_width = 1.0;
            state.culling    = Culling::None;
            mClass->ApplyDynamicState(env, program);
        }
        // Drawable implementation. Compile the shader.
        virtual Shader* GetShader(Device& device) const override
        {
            return mClass->GetShader(device);
        }
        // Drawable implementation. Upload particles to the device.
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        {
            return mClass->Upload(env, mState, device);
        }
        virtual Style GetStyle() const override
        {
            return Style::Points;
        }

        // Update the particle simulation.
        virtual void Update(const Environment& env, float dt) override
        {
            mClass->Update(env, mState, dt);
        }

        virtual bool IsAlive() const override
        {
            return mClass->IsAlive(mState);
        }
        virtual void Restart(const Environment& env) override
        {
            mClass->Restart(env, mState);
        }
        virtual std::string GetProgramId() const override
        {
            return mClass->GetProgramId();
        }

        // Get the current number of alive particles.
        size_t GetNumParticlesAlive() const
        { return mState.particles.size(); }

    private:
        // this is the "class" object for this particle engine type.
        std::shared_ptr<const KinematicsParticleEngineClass> mClass;
        // this is this particle engine's state.
        KinematicsParticleEngineClass::InstanceState mState;
    };

    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

} // namespace
