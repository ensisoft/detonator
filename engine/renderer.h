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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

#include "base/bitflag.h"
#include "graphics/fwd.h"
#include "graphics/drawable.h"
#include "game/enum.h"
#include "game/tilemap.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/tree.h"
#include "engine/graphics.h"
#include "engine/types.h"

namespace engine
{
    class ClassLibrary;

    template<typename Node>
    class EntityDrawHook
    {
    public:
        virtual ~EntityDrawHook() = default;
        // This is a hook function to inspect and  modify the draw packet produced by the
        // given animation node. The return value can be used to indicate filtering.
        // If the function returns false the packet is dropped. Otherwise, it's added to the
        // current draw list with any possible modifications.
        virtual bool InspectPacket(const Node* node, DrawPacket& packet) { return true; }
        // This is a hook function to append extra draw packets to the current draw list
        // based on the node. Transform is the combined transformation hierarchy containing
        // the transformations from this current node to "view".
        virtual void AppendPackets(const Node* node, gfx::Transform& trans, std::vector<DrawPacket>& packets) {}
    protected:
    };

    template<typename EntityType>
    class SceneDrawHook
    {
    public:
        virtual ~SceneDrawHook() = default;
        virtual bool FilterEntity(const EntityType& entity, gfx::Painter& painter, gfx::Transform& trans) { return true; }
        virtual void BeginDrawEntity(const EntityType& entity, gfx::Painter& painter, gfx::Transform& trans) {}
        virtual void EndDrawEntity(const EntityType& entity, gfx::Painter& painter, gfx::Transform& trans) {}
    private:
    };

    using EntityClassDrawHook     = EntityDrawHook<game::EntityNodeClass>;
    using EntityInstanceDrawHook  = EntityDrawHook<game::EntityNode>;
    using SceneClassDrawHook      = SceneDrawHook<game::SceneNodeClass>;
    using SceneInstanceDrawHook   = SceneDrawHook<game::Entity>;

    class Renderer
    {
    public:
        enum class Effects {
            Bloom
        };

        struct BloomParams {
            float threshold = 0.0f;
            float red   = 0.0f;
            float green = 0.0f;
            float blue  = 0.0f;
        };

        struct Camera {
            glm::vec2 position = {0.0f, 0.0f};
            glm::vec2 scale    = {1.0f, 1.0f};
            game::FRect viewport;
            float rotation = 0.0f;
        };

        // The rendering window/surface details.
        struct Surface {
            // Device viewport in which part of the surface to render.
            IRect viewport;
            // Rendering surface size in pixels.
            USize size;
        };

        Renderer(const ClassLibrary* classlib = nullptr);

        inline void SetBloom(const BloomParams& bloom) noexcept
        { mBloom = bloom; }
        inline void SetClassLibrary(const ClassLibrary* classlib) noexcept
        { mClassLib = classlib; }
        inline void SetEditingMode(bool on_off) noexcept
        { mEditingMode = on_off; }
        inline void SetName(std::string name) noexcept
        { mRendererName = std::move(name); }
        inline void EnableEffect(Effects effect, bool enabled) noexcept
        { mEffects.set(effect, enabled); }
        inline bool IsEnabled(Effects effect) const noexcept
        { return mEffects.test(effect); }
        inline void SetCamera(const Camera& camera) noexcept
        { mCamera = camera; }
        inline void SetSurface(const Surface& surface) noexcept
        { mSurface = surface; }

        void BeginFrame();

        void CreateScene(const game::Scene& scene);
        void UpdateScene(const game::Scene& scene);
        void Draw(gfx::Painter& painter, EntityInstanceDrawHook* hook);
        void Update(float time, float dt);

        void Draw(const game::Entity& entity,
                  gfx::Painter& painter,
                  EntityInstanceDrawHook* hook = nullptr)
        {
            gfx::Transform model;
            Draw(entity, painter, model, hook);
        }
        void Draw(const game::EntityClass& entity,
                  gfx::Painter& painter,
                  EntityClassDrawHook* hook = nullptr)
        {
            gfx::Transform model;
            Draw(entity, painter, model, hook);
        }

        void Draw(const game::Entity& entity,
                  gfx::Painter& painter, gfx::Transform& model,
                  EntityInstanceDrawHook* hook = nullptr);
        void Draw(const game::EntityClass& entity,
                  gfx::Painter& painter, gfx::Transform& model,
                  EntityClassDrawHook* hook = nullptr);

        void Draw(const game::Scene& scene,
                  gfx::Painter& painter,
                  SceneInstanceDrawHook* scene_hook = nullptr,
                  EntityInstanceDrawHook* entity_hook = nullptr);
        void Draw(const game::SceneClass& scene,
                  gfx::Painter& painter,
                  SceneClassDrawHook* scene_hook = nullptr,
                  EntityClassDrawHook* hook = nullptr);

        void Draw(const game::Tilemap& map,
                  gfx::Painter& painter,
                  bool draw_render_layer,
                  bool draw_data_layer);

        // Update the visual representation of the renderer's paint node
        // based on the given animation node.
        void Update(const game::EntityNodeClass& node, float time, float dt);
        void Update(const game::EntityClass& entity, float time, float dt);
        void Update(const game::EntityNode& node, float time, float dt);
        void Update(const game::Entity& entity, float time, float dt);
        void Update(const game::SceneClass& scene, float time, float dt);
        void Update(const game::Scene& scene, float time, float dt);

        void EndFrame();

        void ClearPaintState();

        size_t GetNumPaintNodes() const
        { return mPaintNodes.size(); }
    private:
        template<typename SceneType, typename EntityType, typename NodeType>
        void DrawScene(const SceneType& scene,
                       gfx::Painter& painter,
                       SceneDrawHook<EntityType>* scene_hook,
                       EntityDrawHook<NodeType>* entity_hook);

        template<typename EntityType, typename NodeType>
        void MapEntity(const EntityType& entity, gfx::Transform& transform);

        template<typename EntityType, typename NodeType>
        void DrawEntity(const EntityType& entity,
                        gfx::Painter& painter,
                        gfx::Transform& transform,
                        EntityDrawHook<NodeType>* hook);

        struct PaintNode;
        template<typename EntityNodeType>
        void UpdateNode(PaintNode& paint_node, float time, float dt);

        template<typename EntityType, typename EntityNodeType>
        void CreateDrawResources(PaintNode& paint_node);

        template<typename EntityType, typename EntityNodeType>
        void GenerateDrawPackets(PaintNode& paint_node,
                                 std::vector<DrawPacket>& packets,
                                 EntityDrawHook<EntityNodeType>* hook);

        void DrawPackets(gfx::Painter& painter, std::vector<DrawPacket>& packets);

        struct TileBatch;

        template<typename LayerType>
        void PrepareTileBatches(const game::Tilemap& map,
                                const game::TilemapLayer& layer,
                                const game::URect& visible_region,
                                std::vector<TileBatch>& batches,
                                std::uint16_t layer_index);

        void DrawTileBatches(const game::Tilemap& map,
                             std::vector<TileBatch>& batches,
                             gfx::Painter& painter);


        template<typename LayerType>
        void DrawDataLayer(const game::Tilemap& map,
                           const game::TilemapLayer& layer,
                           const game::URect& tile_rect,
                           const game::FSize& tile_size,
                           gfx::Painter& painter);

    private:
        const ClassLibrary* mClassLib = nullptr;
        using EntityRef = std::variant<
                const game::Entity*,
                const game::EntityClass*>;
        using EntityNodeRef = std::variant<
                const game::EntityNode*,
                const game::EntityNodeClass*>;

        struct PaintNode {
            bool visited = false;
            std::string text_material_id;
            std::string item_material_id;
            std::string item_drawable_id;
            std::shared_ptr<gfx::Material> text_material;
            std::shared_ptr<gfx::Drawable> text_drawable;
            std::shared_ptr<gfx::Material> item_material;
            std::shared_ptr<gfx::Drawable> item_drawable;
            glm::vec2 world_scale;
            glm::vec2 world_pos;
            float world_rotation = 0.0f;
            EntityRef     entity;
            EntityNodeRef entity_node;
#if !defined(NDEBUG)
            std::string debug_name;
#endif
        };
        std::unordered_map<std::string, PaintNode> mPaintNodes;

        struct TilemapLayerPaletteEntry {
            std::string material_id;
            std::shared_ptr<gfx::Material> material;
        };
        struct TileBatch {
            gfx::TileBatch tiles;
            std::uint16_t material_index;
            std::uint16_t layer_index;
            std::uint32_t row_index;
            glm::vec2 tile_size;
        };
        using TilemapLayerPalette = std::vector<TilemapLayerPaletteEntry>;
        std::vector<TilemapLayerPalette> mTilemapPalette;

        std::string mRendererName;
        base::bitflag<Effects> mEffects;
        bool mEditingMode = false;
        BloomParams mBloom;
        Camera mCamera;
        Surface mSurface;
    };

} // namespace
