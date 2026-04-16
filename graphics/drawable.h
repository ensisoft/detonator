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

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_map>

#include "graphics/geometry.h"
#include "graphics/types.h"
#include "graphics/drawable_class.h"
#include "graphics/shader_source.h"
#include "graphics/instance_data.h"
#include "graphics/drawable_geometry.h"
#include "graphics/drawable_effect.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;
    class ProgramState;
    class CommandBuffer;
    class ShaderSource;
    class DrawCall;

    // Drawable interface represents some kind of drawable object
    // or shape such as quad/rectangle/mesh/particle engine.
    //
    // The drawables combine state and functionality into a single
    // component that can produce geometry data, i.e. vertex and index
    // buffers either programmatically or by loading data from some file.
    //
    // Drawable may or may not be an instance of a drawable class.
    class Drawable
    {
    public:
        using Flags                 = DrawableFlags;
        using SpatialMode           = DrawableClass::SpatialMode;
        using Culling               = DrawableClass::Culling;
        using Environment           = DrawableClass::Environment;
        using Type                  = DrawableClass::Type;
        using Usage                 = DrawableClass::Usage;
        using DrawPrimitive         = DrawableClass::DrawPrimitive;
        using MeshType              = DrawableClass::MeshType;
        using MeshFlags             = DrawableClass::MeshFlags;

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

        virtual void SetFlag(Flags flag, bool on_off) noexcept { }
        virtual bool TestFlag(Flags flag) const noexcept { return false; }

        // Apply the drawable's state (if any) on the program and set the rasterizer state.
        virtual bool ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
            Device& device, ProgramState& program, RasterState& state) const = 0;
        // Get the device specific shader source applicable for this drawable, its state
        // and the given environment in which it should execute.
        // Should return an empty string on any error.
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const = 0;
        // Get the shader ID applicable for this drawable, its state and the given
        // environment in which it should execute.
        virtual std::string GetShaderId(const Environment& env) const = 0;
        // Get the human readable debug name that should be associated with the
        // shader object generated from this drawable.
        virtual std::string GetShaderName(const Environment& env) const = 0;
        // todo:
        virtual DrawGeometryHandle GetGeometry(const Environment& env, Device& device) const = 0;
        // Construct geometry object create args.
        // Returns true if successful or false if geometry is unavailable.
        virtual DrawGeometryBuffer Construct(const Environment& env) const = 0;
        // Update the state of the drawable object. dt is the
        // elapsed (delta) time in seconds.
        virtual void Update(const Environment& env, float dt) {}
        // Get the expected type of primitive used to rasterize the
        // geometry produced by the drawable. This is essentially the
        // "summary" of the draw commands in the geometry object.
        virtual DrawPrimitive GetDrawPrimitive() const = 0;
        // Get the drawable type.
        virtual Type GetType() const = 0;
        // Get the drawable spatial mode.
        virtual SpatialMode GetSpatialMode() const = 0;

        // Returns true if the drawable is still considered to be alive.
        // For example a particle simulation still has live particles.
        virtual bool IsAlive() const
        { return true; }
        // Restart the drawable, if applicable. See IsAlive
        virtual void Restart(const Environment& env) {}

        // Execute drawable commands coming from the scripting environment.
        // The commands can be used to change the drawable, alter its parameters
        // or trigger its function such as particle emission.
        virtual void Execute(const Environment& env, const Command& command)
        {}

        // Get the drawable class instance if any. Warning, this may be null for
        // drawable objects that aren't based on any drawable class!
        virtual const DrawableClass* GetClass() const { return nullptr; }

        virtual std::string GetName() const
        {
            if (const auto* klass = GetClass())
                return klass->GetName();
            return "";
        }

        virtual bool SetEffect(DrawableEffect effect) { return false; }
        virtual void DeleteEffect() {};

        auto GetDrawCategory() const noexcept
        { return DrawableClass::MapDrawableCategory(GetType()); }
        bool IsTrue3D() const noexcept
        { return GetSpatialMode() == SpatialMode::True3D; }
        bool IsFlat2D() const noexcept
        { return GetSpatialMode() == SpatialMode::Flat2D; }
        bool IsPerceptual3D() const noexcept
        { return GetSpatialMode() == SpatialMode::Perceptual3D; }

        enum class Shader {
            Simple2D, Simple3D, Model3D, Perceptual3D
        };

        static ShaderSource CreateShader(bool instancing, bool effects, const Device& device, Shader shader);
        static std::string GetShaderId(bool instancing, bool effects, Shader shader);
        static std::string GetShaderName(const Environment& env, Shader shader);

    private:
    };

    inline bool Is3DShape(const Drawable& drawable) noexcept
    {
        return drawable.IsTrue3D();
    }
    inline bool Is3DShape(const DrawableClass& klass) noexcept
    {
        return klass.IsTrue3D();
    }
    inline bool Is2DShape(const Drawable& drawable) noexcept
    {
        return drawable.IsFlat2D();
    }
    inline bool Is2DShape(const DrawableClass& klass) noexcept
    {
        return klass.IsFlat2D();
    }

    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

} // namespace
