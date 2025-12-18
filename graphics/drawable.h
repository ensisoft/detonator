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
#include "graphics/instance.h"
#include "graphics/types.h"
#include "graphics/drawable_class.h"
#include "graphics/shader_source.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;
    class ProgramState;
    class CommandBuffer;
    class ShaderSource;

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
        using SpatialMode           = DrawableClass::SpatialMode;
        using Culling               = DrawableClass::Culling;
        using Environment           = DrawableClass::Environment;
        using Type                  = DrawableClass::Type;
        using Usage                 = DrawableClass::Usage;
        using DrawCmd               = DrawableClass::DrawCmd;
        using DrawPrimitive         = DrawableClass::DrawPrimitive;
        using DrawInstance          = DrawableClass::DrawInstance;
        using DrawInstanceArray     = DrawableClass::DrawInstanceArray;
        using InstancedDraw         = DrawableClass::InstancedDraw;
        using MeshType              = DrawableClass::MeshType;
        using MeshArgs              = DrawableClass::MeshArgs;
        using ShardedEffectMeshArgs = DrawableClass::ShardedEffectMeshArgs;

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
        virtual bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& state) const = 0;
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
        // Get the geometry name that will be used to identify the
        // drawable geometry on the device.
        virtual std::string GetGeometryId(const Environment& env) const = 0;
        // Construct geometry object create args.
        // Returns true if successful or false if geometry is unavailable.
        virtual bool Construct(const Environment& env, Device& device, Geometry::CreateArgs& geometry) const = 0;
        // Construct geometry instance buffer.
        // Returns true if successful or false if geometry is unavailable.
        virtual bool Construct(const Environment& env, Device& device, const InstancedDraw& draw, gfx::InstancedDraw::CreateArgs& args) const { return false; }
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
        // Get the intended usage of the geometry generated by the drawable.
        virtual Usage GetGeometryUsage() const
        { return Usage::Static; }
        // Get the hash value based on the drawable geometry data.
        virtual size_t GetGeometryHash() const
        { return 0; }

        // Returns true if the drawable is still considered to be alive.
        // For example a particle simulation still has live particles.
        virtual bool IsAlive() const
        { return true; }
        // Restart the drawable, if applicable. See IsAlive
        virtual void Restart(const Environment& env) {}

        // Instanced rendering support.
        // If the geometry has instance specific data then it should implement
        // GetInstanceUsage, GetInstanceHash and GetInstanceId and account for
        // both the client side per instance data and the geometry side per
        // instance data. So for example the instance hash should be a combination
        // of this data.

        // Get the instance buffer usage based on the combination of the usage
        // coming from the client for the client side instance data and the
        // geometry specific instance data.
        virtual Usage GetInstanceUsage(const InstancedDraw& draw) const
        { return draw.usage; }

        // Get the hash value based on the client instance data combined
        // with the per instance geometry data.
        virtual size_t GetInstanceHash(const InstancedDraw& draw) const
        { return draw.content_hash; }

        virtual std::string GetInstanceId(const Environment& env, const InstancedDraw& draw) const
        { return draw.gpu_id; }

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

        // Get the drawable class instance if any. Warning, this may be null for
        // drawable objects that aren't based on any drawable class!
        virtual const DrawableClass* GetClass() const { return nullptr; }

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

        static ShaderSource CreateShader(const Environment& env, const Device& device, Shader shader);
        static std::string GetShaderId(const Environment& env, Shader shader);
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
