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

#include "base/bitflag.h"
#include "graphics/drawable.h"

namespace gfx
{

    class DebugDrawableBase : public Drawable
    {
    public:
        enum class Feature {
            NormalMesh,
            Wireframe
        };
        enum class Flags {
            Normals, Tangents, Bitangents
        };
        using FlagBits = base::bitflag<Flags>;

        void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState&  state) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;
        std::string GetGeometryId(const Environment& env) const override;
        bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        Usage GetGeometryUsage() const override;
        size_t GetGeometryHash() const override;
    protected:
        DebugDrawableBase(const Drawable* drawable, Feature feature) noexcept
          : mDrawable(drawable)
          , mFeature(feature)
        {}
        DebugDrawableBase(const Drawable* drawable, Feature feature, FlagBits flags) noexcept
          : mDrawable(drawable)
          , mFeature(feature)
          , mFlags(flags)
        {}
        DebugDrawableBase() = default;

        const Drawable* mDrawable = nullptr;
        Feature mFeature = Feature::Wireframe;
        FlagBits mFlags = GetDefaultFlags();
    private:
        static FlagBits GetDefaultFlags() noexcept
        {
            static FlagBits bits(Flags::Normals);
            return bits;
        }
    };

    class DebugDrawableInstance : public DebugDrawableBase
    {
    public:
        DebugDrawableInstance(std::shared_ptr<const Drawable> drawable, Feature feature) noexcept
          : DebugDrawableBase(drawable.get(), feature)
        {
            mSharedDrawable = std::move(drawable);
        }
        DebugDrawableInstance(std::shared_ptr<const Drawable> drawable, Feature feature, FlagBits flags) noexcept
          : DebugDrawableBase(drawable.get(), feature, flags)
        {
            mSharedDrawable = std::move(drawable);
        }
    private:
        std::shared_ptr<const Drawable> mSharedDrawable;
    };

    class WireframeInstance : public DebugDrawableInstance
    {
    public:
        explicit WireframeInstance(std::shared_ptr<const Drawable> drawable) noexcept
          : DebugDrawableInstance(std::move(drawable), Feature::Wireframe)
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

    class NormalMeshInstance : public DebugDrawableInstance
    {
    public:
        explicit NormalMeshInstance(std::shared_ptr<const Drawable> drawable) noexcept
          : DebugDrawableInstance(std::move(drawable), Feature::NormalMesh)
        {}
        NormalMeshInstance(std::shared_ptr<const Drawable> drawable, FlagBits flags) noexcept
           : DebugDrawableInstance(std::move(drawable), Feature::NormalMesh, flags)
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
        template<typename... Args>
        DebugDrawable(FlagBits flags, Feature feature, Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = feature;
            mFlags    = flags;
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

    template<typename T>
    class NormalMesh : public DebugDrawableBase
    {
    public:
        template<typename... Args>
        NormalMesh(Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = Feature::NormalMesh;
        }
        template<typename... Args>
        NormalMesh(FlagBits flags, Args&&... args) : mObject(std::forward<Args>(args)...)
        {
            mDrawable = &mObject;
            mFeature  = Feature::NormalMesh;
            mFlags    = flags;
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
