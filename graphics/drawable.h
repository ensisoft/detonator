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

#include "data/fwd.h"
#include "graphics/geometry.h"
#include "graphics/instance.h"
#include "graphics/device.h"
#include "graphics/types.h"
#include "graphics/shader_source.h"

namespace gfx
{
    class Device;
    class Shader;
    class Geometry;
    class Program;
    class CommandBuffer;
    class ShaderSource;

    // DrawableClass defines a new type of drawable.
    class DrawableClass
    {
    public:
        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points.
        using Culling = Device::RasterState::Culling;

        using Usage = Geometry::Usage;

        // Style of the drawable's geometry determines how the geometry
        // is to be rasterized.
        using DrawPrimitive = gfx::DrawPrimitive;

        using RenderPass = gfx::RenderPass;

        // Type of the drawable (and its instances)
        enum class Type {
            ParticleEngine,
            Polygon,
            TileBatch,
            LineBatch2D,
            LineBatch3D,
            SimpleShape,
            GuideGrid,
            DebugDrawable,
            EffectsDrawable,
            Other
        };
        enum class MeshType {
            NormalRenderMesh,
            ShardedEffectMesh
        };
        struct ShardedEffectMeshArgs {
            unsigned mesh_subdivision_count = 0;
        };
        using MeshArgs = std::variant<std::monostate, ShardedEffectMeshArgs>;

        // The environment that possibly affects the geometry and drawable
        // generation and update in some way.
        struct Environment {
            RenderPass render_pass = RenderPass::ColorPass;
            // true if the draw is with "effects", i.e. per triangle transform
            MeshType mesh_type = MeshType::NormalRenderMesh;
            MeshArgs mesh_args;

            bool flip_uv_vertically = false;
            bool flip_uv_horizontally = false;
            // true to indicate that we're going to do instanced draw.
            bool use_instancing = false;
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

        struct DrawInstance {
            glm::mat4 model_to_world;
        };
        using DrawInstanceArray = std::vector<DrawInstance>;

        struct InstancedDraw {
            std::string gpu_id;
            std::string content_name;
            std::size_t content_hash;
            DrawInstanceArray instances;
            Usage usage = Usage::Dynamic;
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

        static DrawCategory MapDrawableCategory(Type type);

        DrawCategory GetDrawCategory() const
        { return MapDrawableCategory(GetType()); }
    private:
    };

    // Drawable interface represents some kind of drawable
    // object or shape such as quad/rectangle/mesh/particle engine.
    class Drawable
    {
    public:
        using Culling     = DrawableClass::Culling;
        using Environment = DrawableClass::Environment;
        using Type        = DrawableClass::Type;
        using Usage       = DrawableClass::Usage;
        using DrawCmd     = DrawableClass::DrawCmd;
        using DrawPrimitive     = DrawableClass::DrawPrimitive;
        using DrawInstance      = DrawableClass::DrawInstance;
        using DrawInstanceArray = DrawableClass::DrawInstanceArray;
        using InstancedDraw     = DrawableClass::InstancedDraw;
        using MeshType = DrawableClass::MeshType;
        using MeshArgs = DrawableClass::MeshArgs;
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
        // drawable objects that aren't based on any drawable clas!
        virtual const DrawableClass* GetClass() const { return nullptr; }

        DrawCategory GetDrawCategory() const
        { return DrawableClass::MapDrawableCategory(GetType()); }
    private:
    };

    bool Is3DShape(const Drawable& drawable) noexcept;
    bool Is3DShape(const DrawableClass& klass) noexcept;
    bool Is2DShape(const Drawable& drawable) noexcept;
    bool Is2DShape(const DrawableClass& klass) noexcept;

    std::unique_ptr<Drawable> CreateDrawableInstance(const std::shared_ptr<const DrawableClass>& klass);

} // namespace
