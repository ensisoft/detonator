// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <limits>
#include <vector>
#include <memory>
#include <cstddef>

#include "data/fwd.h"
#include "graphics/enum.h"

namespace gfx
{
    // DrawableClass defines a type of a drawable where each instance
    // of the drawable shares the same class/type defining characteristics
    // while having independent instance state. For example a particle
    // engine has parameters that define the motion of the particles
    // and while each instance of that particle engine type behaves the
    // same way they have different current instance state depending
    // on their lifetime etc.
    class DrawableClass
    {
    public:
        // Which polygon faces to cull. Note that this only applies
        // to polygons, not to lines or points (obviously).
        using Culling = gfx::Culling;

        // Define the expected buffer usage which determines how and
        // when the geometry buffers are refreshed (if ever) on the
        // device after the initial geometry upload.
        using Usage = gfx::BufferUsage;

        // Style of the drawable's geometry determines how the geometry
        // is to be rasterized. Points, lines or triangles.
        using DrawPrimitive = gfx::DrawPrimitive;

        // Define the current render pass that the current draw operation
        // applies on.
        using RenderPass = gfx::RenderPass;

        // Define the drawable spatial mode that identifies the
        // dimensionality of the drawable geometry.
        using SpatialMode = gfx::SpatialMode;

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
        // Get the drawable spatial mode.
        virtual SpatialMode GetSpatialMode() const = 0;
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

        static DrawCategory MapDrawableCategory(Type type) noexcept;

        auto GetDrawCategory() const noexcept
        { return MapDrawableCategory(GetType()); }
        bool IsTrue3D() const noexcept
        {  return GetSpatialMode() == SpatialMode::True3D; }
        bool IsFlat2D() const noexcept
        { return GetSpatialMode() == SpatialMode::Flat2D; }
        bool IsPerceptual3D() const noexcept
        { return GetSpatialMode() == SpatialMode::Perceptual3D; }
    private:
    };

} // namespace