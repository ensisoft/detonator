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
#include "warnpop.h"

#include <memory>
#include <string>
#include <cstdint>

#include "base/utility.h"
#include "graphics/drawable.h"
#include "graphics/simple_shape_class.h"

namespace gfx
{
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
        using ShapeAttribute = SimpleShapeClass::ShapeAttribute;

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
        void SetFlag(Flags flag, bool on_off) noexcept override
        {
            mFlags = base::SetFlag(mFlags, flag, on_off);
        }
        bool TestFlag(Flags flag) const noexcept override
        {
            return base::TestFlag(mFlags, flag);
        }

        bool ApplyDynamicState(const Environment& env, const DrawCall& draw, Device& device, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Device&, Geometry::CreateArgs& geometry) const override;
        Type GetType() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Usage GetGeometryUsage() const override;
        SpatialMode GetSpatialMode() const override;

        const DrawableClass* GetClass() const override
        { return mClass.get(); }

        Shape GetShape() const noexcept
        { return mClass->GetShapeType(); }
        Style GetStyle() const noexcept
        { return mStyle; }
        void SetStyle(Style style) noexcept
        { mStyle = style; }
        float GetShapeAttribute(ShapeAttribute attribute) const noexcept
        { return mClass->GetShapeAttribute(attribute); }
    private:
        bool ConstructShardMesh(const Environment& env, Device& device, Geometry::CreateArgs& create,
            unsigned mesh_subdivision_count, bool discard_skinny_slivers) const;
    private:
        std::shared_ptr<const Class> mClass;
        Style mStyle = Style::Solid;
        std::uint32_t mFlags = 0;
    };

    // Instance of a simple shape without class object.
    // Optimized version of SimpleShapeInstance for immediate mode
    // drawing, i.e. when drawing with a temporary shape object.
    class SimpleShape : public Drawable
    {
    public:
        using Shape = SimpleShapeType;
        using Style = SimpleShapeStyle;
        using ShapeAttribute = SimpleShapeAttribute;

        explicit SimpleShape(SimpleShapeType shape, Style style = Style::Solid) noexcept
          : mShape(shape)
          , mStyle(style)
        {}
        explicit SimpleShape(SimpleShapeType shape, detail::SimpleShapeArgs args, Style style = Style::Solid) noexcept
          : mShape(shape)
          , mArgs(args)
          , mStyle(style)
        {}
        void SetFlag(Flags flag, bool on_off) noexcept override
        {
            mFlags = base::SetFlag(mFlags, flag, on_off);
        }
        bool TestFlag(Flags flag) const noexcept override
        {
            return base::TestFlag(mFlags, flag);
        }

        bool ApplyDynamicState(const Environment& env, const DrawCall& draw, Device& device, ProgramState& program, RasterState& state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const override;
        Type GetType() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Usage GetGeometryUsage() const override;
        SpatialMode GetSpatialMode() const override;

        Shape GetShape() const noexcept
        { return mShape; }
        Style GetStyle() const noexcept
        { return mStyle; }
        void SetStyle(Style style) noexcept
        { mStyle = style; }

        float GetShapeAttribute(ShapeAttribute attribute) const noexcept;
    private:
        SimpleShapeType mShape;
        detail::SimpleShapeArgs mArgs;
        Style mStyle = Style::Solid;
        std::uint32_t mFlags = 0;
    };

} // namespace