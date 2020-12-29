// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "gamelib/animation.h"
#include "gamelib/entity.h"
#include "gamelib/scene.h"
#include "gamelib/tree.h"

namespace gfx {
    class Painter;
    class Transform;
    class Material;
    class Drawable;
}

namespace game
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
        RenderPass pass = RenderPass::Draw;
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

    using EntityClassDrawHook     = EntityDrawHook<EntityNodeClass>;
    using EntityInstanceDrawHook  = EntityDrawHook<EntityNode>;
    using SceneClassDrawHook      = SceneDrawHook<SceneNodeClass>;
    using SceneInstanceDrawHook   = SceneDrawHook<Entity>;

    class Renderer
    {
    public:
        Renderer(const ClassLibrary* loader = nullptr)
          : mLoader(loader) {}
        void SetLoader(const ClassLibrary* loader)
        { mLoader = loader; }

        void BeginFrame();

        // Draw the entity and its nodes.
        // Each node is transformed relative to the parent transformation "trans".
        // Optional draw hook can be used to modify the draw packets before submission to the
        // paint device.
        void Draw(const Entity& entity,
                  gfx::Painter& painter, gfx::Transform& transform,
                  EntityInstanceDrawHook* hook = nullptr);
        // Draw a representation of the entity class instance.
        // This functionality is mostly to support editor functionality
        // and to simplify working with an AnimationClass instance.
        void Draw(const EntityClass& entity,
                  gfx::Painter& painter, gfx::Transform& transform,
                  EntityClassDrawHook* hook = nullptr);

        void Draw(const Scene& scene,
                  gfx::Painter& painter, gfx::Transform& transform,
                  SceneInstanceDrawHook* scene_hook = nullptr,
                  EntityInstanceDrawHook* entity_hook = nullptr);
        void Draw(const SceneClass& scene,
                  gfx::Painter& painter, gfx::Transform& transform,
                  SceneClassDrawHook* scene_hook = nullptr,
                  EntityClassDrawHook* hook = nullptr);

        // Update the visual representation of the renderer's paint node
        // based on the given animation node.
        void Update(const EntityNodeClass& node, float time, float dt);
        void Update(const EntityClass& entity, float time, float dt);
        void Update(const EntityNode& node, float time, float dt);
        void Update(const Entity& entity, float time, float dt);
        void Update(const SceneClass& scene, float time, float dt);
        void Update(const Scene& scene, float time, float dt);

        void EndFrame();
    private:
        template<typename NodeType>
        void UpdateNode(const NodeType& node, float time, float dt);

        template<typename EntityType, typename NodeType>
        void DrawRenderTree(const TreeNode<EntityType>& tree,
                            gfx::Painter& painter, gfx::Transform& transform,
                            SceneDrawHook<EntityType>* scene_hook,
                            EntityDrawHook<NodeType>* entity_hook);

        template<typename NodeType>
        void DrawRenderTree(const TreeNode<NodeType>& tree,
                            gfx::Painter& painter, gfx::Transform& transform,
                            EntityDrawHook<NodeType>* hook);
    private:
        const ClassLibrary* mLoader = nullptr;
        struct PaintNode {
            bool visited = false;
            std::shared_ptr<gfx::Material> material;
            std::shared_ptr<gfx::Drawable> drawable;
            std::string material_class_id;
            std::string drawable_class_id;
        };
        std::unordered_map<std::string, PaintNode> mPaintNodes;
    };

} // namespace
