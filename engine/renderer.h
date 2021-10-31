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
#include <vector>
#include <unordered_map>

#include "graphics/fwd.h"
#include "game/animation.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/tree.h"

namespace engine
{
    class ClassLibrary;

    struct DrawPacket {
        // shortcut to the node's material.
        std::shared_ptr<const gfx::Material> material;
        // shortcut to the node's drawable.
        std::shared_ptr<const gfx::Drawable> drawable;
        // transform that pertains to the draw.
        glm::mat4 transform;
        // the animation layer this draw belongs to.
        int layer = 0;
        // the render pass this draw belongs to.
        game::RenderPass pass = game::RenderPass::Draw;
    };

    template<typename Node>
    class EntityDrawHook
    {
    public:
        virtual ~EntityDrawHook() = default;
        // This is a hook function to inspect and  modify the the draw packet produced by the
        // given animation node. The return value can be used to indicate filtering.
        // If the function returns false the packet is dropped. Otherwise it's added to the
        // current drawlist with any possible modifications.
        virtual bool InspectPacket(const Node* node, DrawPacket& packet) { return true; }
        // This is a hook function to append extra draw packets to the current drawlist
        // based on the node.
        // Transform is the combined transformation hierarchy containing the transformations
        // from this current node to "view".
        virtual void AppendPackets(const Node* node, gfx::Transform& trans, std::vector<DrawPacket>& packets) {}
    protected:
    };

    template<typename EntityType>
    class SceneDrawHook
    {
    public:
        virtual ~SceneDrawHook() = default;
        virtual bool FilterEntity(const EntityType& entity) { return true; }
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
        Renderer(const ClassLibrary* classlib = nullptr)
          : mClassLib(classlib) {}
        void SetClassLibrary(const ClassLibrary* classlib)
        { mClassLib = classlib; }
        void SetEditingMode(bool on_off)
        { mEditingMode = on_off; }

        void BeginFrame();

        // Draw the entity and its nodes.
        // Each node is transformed relative to the parent transformation "trans".
        // Optional draw hook can be used to modify the draw packets before submission to the
        // paint device.
        void Draw(const game::Entity& entity,
                  gfx::Painter& painter, gfx::Transform& transform,
                  EntityInstanceDrawHook* hook = nullptr);
        // Draw a representation of the entity class instance.
        // This functionality is mostly to support editor functionality
        // and to simplify working with an AnimationClass instance.
        void Draw(const game::EntityClass& entity,
                  gfx::Painter& painter, gfx::Transform& transform,
                  EntityClassDrawHook* hook = nullptr);

        void Draw(const game::Scene& scene,
                  gfx::Painter& painter, gfx::Transform& transform,
                  SceneInstanceDrawHook* scene_hook = nullptr,
                  EntityInstanceDrawHook* entity_hook = nullptr);
        void Draw(const game::SceneClass& scene,
                  gfx::Painter& painter, gfx::Transform& transform,
                  SceneClassDrawHook* scene_hook = nullptr,
                  EntityClassDrawHook* hook = nullptr);

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
    private:
        template<typename NodeType>
        void UpdateNode(const NodeType& node, float time, float dt);

        template<typename SceneType, typename EntityType, typename NodeType>
        void DrawScene(const SceneType& scene,
                       gfx::Painter& painter, gfx::Transform& transform,
                       SceneDrawHook<EntityType>* scene_hook,
                       EntityDrawHook<NodeType>* entity_hook);

        template<typename EntityType, typename NodeType>
        void DrawEntity(const EntityType& entity,
                            gfx::Painter& painter, gfx::Transform& transform,
                            EntityDrawHook<NodeType>* hook);
    private:
        const ClassLibrary* mClassLib = nullptr;
        struct PaintNode {
            bool visited = false;
            std::shared_ptr<gfx::Material> material;
            std::shared_ptr<gfx::Drawable> drawable;
            std::string material_class_id;
            std::string drawable_class_id;
        };
        std::unordered_map<std::string, PaintNode> mPaintNodes;
        bool mEditingMode = false;
    };

} // namespace
