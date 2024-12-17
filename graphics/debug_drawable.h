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

#include <memory>

#include "graphics/drawable.h"

namespace gfx
{

    class DebugDrawableBase : public Drawable
    {
    public:
        enum class Feature {
            Normals,
            Wireframe
        };

        void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState&  state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        Usage GetGeometryUsage() const override;
        size_t GetGeometryHash() const override;
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
        Type GetType() const override
        {
            return Type::DebugDrawable;
        }
        DrawPrimitive GetDrawPrimitive() const override
        {
            return DrawPrimitive::Lines;
        }
    private:
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
        Type GetType() const override
        {
            return Type::DebugDrawable;
        }
        DrawPrimitive GetDrawPrimitive() const override
        {
            return DrawPrimitive::Lines;
        }
    private:
        T mObject;
    };


} // namespace