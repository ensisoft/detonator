// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>
#include <string>
#include <vector>
#include <limits>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "graphics/geometry.h"
#include "graphics/device.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;
    class ResourcePacker;

    // DrawableClass defines a new type of drawable.
    // Currently not all drawable shapes have a drawable specific
    // class since theres's no actual class specific state, so
    // the class state can be folded directly into he instance classes.
    class DrawableClass
    {
    public:
        using Culling = Device::State::Culling;
        // Type of the drawable (and its instances)
        enum class Type {
            Arrow,
            Capsule,
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
            Trapezoid
        };
        virtual ~DrawableClass() = default;
        // Get the type of the drawable.
        virtual Type GetType() const = 0;
        // Get the class object resource id.
        virtual std::string GetId() const = 0;
        // Create a copy of this drawable class object but with unique id.
        virtual std::unique_ptr<DrawableClass> Clone() const = 0;
        // Create an exact copy of this drawable object.
        virtual std::unique_ptr<DrawableClass> Copy() const = 0;
        // Get the hash of the drawable class object  based on its properties.
        virtual std::size_t GetHash() const = 0;
        // Pack the drawable resources.
        virtual void Pack(ResourcePacker* packer) const = 0;
        // Serialize into JSON
        virtual nlohmann::json ToJson() const = 0;
        // Load state from JSON object. Returns true if successful
        // otherwise false.
        virtual bool LoadFromJson(const nlohmann::json& json) = 0;
    private:
    };


    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
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

        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points.
        using Culling = DrawableClass::Culling;

        struct Environment {
            // how many render surface units (pixels, texels if rendering to a texture)
            // to a game unit.
            glm::vec2 pixel_ratio = {1.0f, 1.0f};
            // the current projection matrix that will be used to project the
            // vertices from the view space into Normalized Device Coordinates.
            const glm::mat4* proj_matrix = nullptr;
            // The current view matrix that will be used to transform the
            // vertices to the game camera/view space.
            const glm::mat4* view_matrix = nullptr;
        };

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
        virtual void ApplyState(Program& program, RasterState& state) const {}
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
        virtual void Update(float dt) {}
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
        virtual void Restart() {}
        // Get the ID of the drawable shape. Used to map the
        // drawable to a device specific program object.
        inline std::string GetId() const
        { return typeid(*this).name(); }
    private:
    };



    class Arrow : public Drawable
    {
    public:
        Arrow() = default;
        Arrow(Style style) : mStyle(style)
        {}
        Arrow(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Style mStyle = Style::Solid;
        Culling mCulling = Culling::Back;
        float mLineWidth = 1.0f;
    };

    class Line : public Drawable
    {
    public:
        Line() = default;
        Line(float line_width) : mLineWidth(line_width)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return Style::Solid; }
    private:
        float mLineWidth = 1.0f;
    };

    class Capsule : public Drawable
    {
    public:
        Capsule() = default;
        Capsule(Style style) : mStyle(style)
        {}
        Capsule(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class Circle : public Drawable
    {
    public:
        Circle() = default;
        Circle(Style style) : mStyle(style)
        {}
        Circle(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };


    class Rectangle : public Drawable
    {
    public:
        Rectangle() = default;
        Rectangle(Style style) : mStyle(style)
        {}
        Rectangle(Style style, float linewidth)  : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetStyle(Style style) override
        { mStyle = style; }
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class RoundRectangleClass : public DrawableClass
    {
    public:
        RoundRectangleClass()
        { mId = base::RandomString(10); }
        RoundRectangleClass(float radius) : mRadius(radius)
        { mId = base::RandomString(10); }
        Shader* GetShader(Device& device) const;
        Geometry* Upload(Drawable::Style style, Device& device) const;

        float GetRadius() const
        { return mRadius; }
        void SetRadius(float radius)
        { mRadius = radius;}

        virtual Type GetType() const override
        { return DrawableClass::Type::RoundRectangle; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::unique_ptr<DrawableClass> Clone() const override
        {
            auto ret = std::make_unique<RoundRectangleClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<RoundRectangleClass>(*this); }
        virtual std::size_t GetHash() const override
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mRadius);
            return hash;
        }
        virtual void Pack(ResourcePacker* packer) const override;
        virtual nlohmann::json ToJson() const override;
        virtual bool LoadFromJson(const nlohmann::json& json) override;
    private:
        std::string mId;
        float mRadius = 0.05;
    };

    // Rectangle with rounded corners
    class RoundRectangle : public Drawable
    {
    public:
        RoundRectangle()
        {
            mClass = std::make_shared<const RoundRectangleClass>();
        }
        RoundRectangle(Style style) : RoundRectangle()
        {
            mStyle = style;
        }
        RoundRectangle(Style style, float linewidth) : RoundRectangle()
        {
            mStyle = style;
            mLineWidth = linewidth;
        }
        RoundRectangle(const std::shared_ptr<const RoundRectangleClass>& klass)
            : mClass(klass)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override
        {
            state.line_width = mLineWidth;
            state.culling    = mCulling;
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(mStyle , device); }
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style; }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        std::shared_ptr<const RoundRectangleClass> mClass;
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class IsoscelesTriangle : public Drawable
    {
    public:
        IsoscelesTriangle() = default;
        IsoscelesTriangle(Style style) : mStyle(style)
        {}
        IsoscelesTriangle(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class RightTriangle : public Drawable
    {
    public:
        RightTriangle() = default;
        RightTriangle(Style style) : mStyle(style)
        {}
        RightTriangle(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };


    class Trapezoid : public Drawable
    {
    public:
        Trapezoid() = default;
        Trapezoid(Style style) : mStyle(style)
        {}
        Trapezoid(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class Parallelogram : public Drawable
    {
    public:
        Parallelogram() = default;
        Parallelogram(Style style) : mStyle(style)
        {}
        Parallelogram(Style style, float linewidth) : mStyle(style), mLineWidth(linewidth)
        {}
        virtual void ApplyState(Program& program, RasterState& state) const override;
        virtual Shader* GetShader(Device& device) const override;
        virtual Geometry* Upload(const Environment& env, Device& device) const override;
        virtual void SetCulling(Culling cull) override
        { mCulling = cull; }
        virtual void SetStyle(Style style) override
        { mStyle = style;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return mStyle; }
    private:
        Culling mCulling = Culling::Back;
        Style mStyle = Style::Solid;
        float mLineWidth = 1.0f;
    };

    class GridClass : public DrawableClass
    {
    public:
        GridClass()
        { mId = base::RandomString(10); }
        Shader* GetShader(Device& device) const;
        Geometry* Upload(Device& device) const;
        void SetNumVerticalLines(unsigned lines)
        { mNumVerticalLines = lines; }
        void SetNumHorizontalLines(unsigned lines)
        { mNumHorizontalLines = lines; }
        void SetBorders(bool on_off)
        { mBorderLines = on_off; }
        int GetNumVerticalLines() const
        { return mNumVerticalLines; }
        int GetNumHorizontalLines() const
        { return mNumHorizontalLines; }
        bool HasBorderLines() const
        { return mBorderLines; }

        virtual Type GetType() const override
        { return DrawableClass::Type::Grid; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::unique_ptr<DrawableClass> Clone() const override
        {
            auto ret = std::make_unique<GridClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<GridClass>(*this); }
        virtual std::size_t GetHash() const override
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mNumHorizontalLines);
            hash = base::hash_combine(hash, mNumVerticalLines);
            hash = base::hash_combine(hash, mBorderLines);
            return hash;
        }
        virtual void Pack(ResourcePacker* packer) const override;
        virtual nlohmann::json ToJson() const override;
        virtual bool LoadFromJson(const nlohmann::json& json) override;
    private:
        std::string mId;
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
        virtual void ApplyState(Program& program, RasterState& state) const override
        {
            state.line_width = mLineWidth;
            state.culling    = Culling::None;
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(device); }
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
        virtual Style GetStyle() const override
        { return Style::Outline; }

    private:
        std::shared_ptr<const GridClass> mClass;
        float mLineWidth = 1.0f;
    };

    // Combines multiple primitive draw commands into a single
    // drawable shape.
    class PolygonClass : public DrawableClass
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

        PolygonClass()
        { mId = base::RandomString(10); }

        Shader* GetShader(Device& device) const;
        Geometry* Upload(Device& device) const;

        void Clear()
        {
            mVertices.clear();
            mDrawCommands.clear();
            mName.clear();
        }
        void ClearDrawCommands()
        {
            mDrawCommands.clear();
            mName.clear();
        }
        void ClearVertices()
        {
            mVertices.clear();
            mName.clear();
        }

        void AddVertices(const std::vector<Vertex>& verts)
        {
            std::copy(std::begin(verts), std::end(verts), std::back_inserter(mVertices));
            mName.clear();
        }
        void AddVertices(std::vector<Vertex>&& verts)
        {
            std::move(std::begin(verts), std::end(verts), std::back_inserter(mVertices));
            mName.clear();
        }
        void AddVertices(const Vertex* vertices, size_t num_verts)
        {
            for (size_t i=0; i<num_verts; ++i)
                mVertices.push_back(vertices[i]);
            mName.clear();
        }
        void AddDrawCommand(const DrawCommand& cmd)
        {
            mDrawCommands.push_back(cmd);
            mName.clear();
        }

        void AddDrawCommand(const std::vector<Vertex>& verts, const DrawCommand& cmd)
        {
            std::copy(std::begin(verts), std::end(verts),
                std::back_inserter(mVertices));
            mDrawCommands.push_back(cmd);
            mName.clear();
        }
        void AddDrawCommand(std::vector<Vertex>&& verts, const DrawCommand& cmd)
        {
            std::move(std::begin(verts), std::end(verts),
                std::back_inserter(mVertices));
            mDrawCommands.push_back(cmd);
            mName.clear();
        }
        void AddDrawCommand(const Vertex* vertices, size_t num_vertices,
            const DrawCommand& cmd)
        {
            for (size_t i=0; i<num_vertices; ++i)
                mVertices.push_back(vertices[i]);
            mDrawCommands.push_back(cmd);
            mName.clear();
        }
        size_t GetNumVertices() const
        { return mVertices.size(); }
        size_t GetNumDrawCommands() const
        { return mDrawCommands.size(); }
        const DrawCommand& GetDrawCommand(size_t index) const
        { return mDrawCommands[index]; }
        const Vertex& GetVertex(size_t index) const
        { return mVertices[index]; }

        void UpdateVertex(const Vertex& vert, size_t index)
        {
            ASSERT(index < mVertices.size());
            mVertices[index] = vert;
            mName.clear();
        }
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
        void InsertVertex(const Vertex& vert, size_t cmd_index, size_t index);

        void UpdateDrawCommand(const DrawCommand& cmd, size_t index)
        {
            ASSERT(index < mDrawCommands.size());
            mDrawCommands[index] = cmd;
            mName.clear();
        }

        // Find the draw command that contains the vertex at the given index.
        // Returns index to the draw command.
        const size_t FindDrawCommand(size_t vertex_index) const;

        // Return whether the polygon's data is considered to be
        // static or not. Static content is not assumed to change
        // often and will map the polygon to a geometry object
        // based on the polygon's data. Thus each polygon with
        // different data will have different geometry object.
        // However If the polygon is updated frequently this would
        // then lead to the proliferation of excessive geometry objects.
        // In this case static can be set to false and the polygon
        // will map to a (single) dynamic geometry object more optimized
        // for draw/discard type of use.
        bool IsStatic() const
        { return mStatic; }

        // Set the polygon static or not. See comments in IsStatic.
        void SetStatic(bool on_off)
        { mStatic = on_off; }
        void SetDynamic(bool on_off)
        { mStatic = !on_off; }

        // Get a (non human) readable name of the polygon based
        // on the content.
        std::string GetName() const;

        virtual void Pack(ResourcePacker* packer) const override;
        virtual Type GetType() const override
        { return Type::Polygon; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::unique_ptr<DrawableClass> Clone() const override
        {
            auto ret = std::make_unique<PolygonClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<PolygonClass>(*this); }

        virtual std::size_t GetHash() const override;
        virtual nlohmann::json ToJson() const override;
        virtual bool LoadFromJson(const nlohmann::json& json) override;

        // Load from JSON
        static std::optional<PolygonClass> FromJson(const nlohmann::json& object);
    private:
        std::string mId;
        std::vector<Vertex> mVertices;
        std::vector<DrawCommand> mDrawCommands;
        mutable std::string mName; // cached name
        bool mStatic = true;
    };

    class Polygon : public Drawable
    {
    public:
        Polygon(const std::shared_ptr<const PolygonClass>& klass)
            : mClass(klass)
        {}
        Polygon(const PolygonClass& klass)
        {
            mClass = std::make_shared<PolygonClass>(klass);
        }
        virtual void ApplyState(Program& program, RasterState& state) const
        {
            state.culling = mCulling;
            state.line_width = mLineWidth;
        }
        virtual Shader* GetShader(Device& device) const override
        { return mClass->GetShader(device); }
        virtual Geometry* Upload(const Environment& env, Device& device) const override
        { return mClass->Upload(device); }
        virtual Style GetStyle() const override
        { return Style::Solid; }
        virtual void SetCulling(Culling culling) override
        { mCulling = culling;}
        virtual void SetLineWidth(float width) override
        { mLineWidth = width; }
    private:
        std::shared_ptr<const PolygonClass> mClass;
        Culling mCulling = Culling::Back;
        float mLineWidth = 1.0f;
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
    class KinematicsParticleEngineClass : public DrawableClass
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
        enum SpawnPolicy {
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

        // initial engine configuration params
        struct Params {
            // type of motion for the particles
            Motion motion = Motion::Linear;
            // when to spawn particles.
            SpawnPolicy mode = SpawnPolicy::Maintain;
            // What happens to a particle at the simulation boundary
            BoundaryPolicy boundary = BoundaryPolicy::Clamp;
            // the number of particles this engine shall create
            // the behaviour of this parameter depends on the spawn mode.
            float num_particles = 100;
            // maximum particle lifetime
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
            // simulation time.
            float time     = 0.0f;
            // fractional count of new particles being hatched.
            float hatching = 0.0f;
        };

        KinematicsParticleEngineClass()
        { mId = base::RandomString(10); }

        KinematicsParticleEngineClass(const Params& init) : mParams(init)
        { mId = base::RandomString(10); }

        Shader* GetShader(Device& device) const;
        Geometry* Upload(const Drawable::Environment& env, const InstanceState& state, Device& device) const;

        void Update(InstanceState& state, float dt) const;
        void Restart(InstanceState& state) const;
        bool IsAlive(const InstanceState& state) const;

        // Get the params.
        const Params& GetParams() const
        { return mParams; }
        // Set the params.
        void SetParams(const Params& p)
        { mParams = p;}

        virtual Type GetType() const override
        { return DrawableClass::Type::KinematicsParticleEngine; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::unique_ptr<DrawableClass> Clone() const override
        {
            auto ret = std::make_unique<KinematicsParticleEngineClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<KinematicsParticleEngineClass>(*this); }
        virtual void Pack(ResourcePacker* packer) const override;
        virtual nlohmann::json ToJson() const override;
        virtual bool LoadFromJson(const nlohmann::json& json) override;
        // Get a hash value based on the engine parameters
        // and excluding any runtime data.
        virtual std::size_t GetHash() const override;

        // Load from JSON
        static std::optional<KinematicsParticleEngineClass> FromJson(const nlohmann::json& object);
    private:
        void InitParticles(InstanceState& state, size_t num) const;
        void KillParticle(InstanceState& state, size_t i) const;
        bool UpdateParticle(InstanceState& state, size_t i, float dt) const;
    private:
        std::string mId;
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
        KinematicsParticleEngine(std::shared_ptr<const KinematicsParticleEngineClass> klass)
            : mClass(klass)
        {
            Restart();
        }
        KinematicsParticleEngine(const KinematicsParticleEngineClass& klass)
        {
            mClass = std::make_shared<KinematicsParticleEngineClass>(klass);
            Restart();
        }
        KinematicsParticleEngine(const KinematicsParticleEngineClass::Params& params)
        {
            mClass = std::make_shared<KinematicsParticleEngineClass>(params);
            Restart();
        }
        virtual void ApplyState(Program& program, RasterState& state) const override
        {
            state.line_width = 1.0;
            state.culling    = Culling::None;
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
        virtual void Update(float dt) override
        {
            mClass->Update(mState, dt);
        }

        virtual bool IsAlive() const override
        {
            return mClass->IsAlive(mState);
        }
        virtual void Restart() override
        {
            mClass->Restart(mState);
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

    namespace detail {
        // generic shim to help refactoring efforts.
        // todo: replace this with the actual class types for
        // RoundRectangle and Circle (anything that has parameters
        // that affect the geometry generation)
        template<DrawableClass::Type ActualType>
        class GenericDrawableClass : public DrawableClass
        {
        public:
            virtual std::string GetId() const override
            {
                // since the generic drawable class doesn't actually
                // have any state that would define new drawable types
                // the IDs can be fixed.
                using types = DrawableClass::Type;
                if constexpr (ActualType == types::Arrow)
                    return "_arrow";
                else if (ActualType == types::Capsule)
                    return "_capsule";
                else if (ActualType == types::Circle)
                    return "_circle";
                else if (ActualType == types::IsoscelesTriangle)
                    return "_isosceles_triangle";
                else if (ActualType == types::Line)
                    return "_line";
                else if (ActualType == types::Parallelogram)
                    return "_parallelogram";
                else if (ActualType == types::Rectangle)
                    return "_rect";
                else if (ActualType == types::RoundRectangle)
                    return "_round_rect";
                else if (ActualType == types::RightTriangle)
                    return "_right_triangle";
                else if (ActualType == types::Trapezoid)
                    return "_trapezoid";
                else BUG("???");
            }
            virtual std::unique_ptr<DrawableClass> Clone() const override
            { return std::make_unique<GenericDrawableClass>(*this); }
            virtual std::unique_ptr<DrawableClass> Copy() const override
            { return std::make_unique<GenericDrawableClass>(*this); }
            virtual std::size_t GetHash() const override
            { return base::hash_combine(0, GetId()); }
            virtual Type GetType() const override
            { return ActualType; }
            virtual void Pack(ResourcePacker*) const override {}
            virtual nlohmann::json ToJson() const override
            { return nlohmann::json {}; }
            virtual bool LoadFromJson(const nlohmann::json&) override
            { return true; }
        private:

        };
    } // namespace

    // shim type definitions for drawables that don't have their
    // own actual type class yet.
    using ArrowClass = detail::GenericDrawableClass<DrawableClass::Type::Arrow>;
    using CapsuleClass = detail::GenericDrawableClass<DrawableClass::Type::Capsule>;
    using CircleClass = detail::GenericDrawableClass<DrawableClass::Type::Circle>;
    using IsoscelesTriangleClass = detail::GenericDrawableClass<DrawableClass::Type::IsoscelesTriangle>;
    using LineClass = detail::GenericDrawableClass<DrawableClass::Type::Line>;
    using ParallelogramClass = detail::GenericDrawableClass<DrawableClass::Type::Parallelogram>;
    using RectangleClass = detail::GenericDrawableClass<DrawableClass::Type::Rectangle>;
    using RightTriangleClass = detail::GenericDrawableClass<DrawableClass::Type::RightTriangle>;
    using TrapezoidClass = detail::GenericDrawableClass<DrawableClass::Type::Trapezoid>;

    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

} // namespace
