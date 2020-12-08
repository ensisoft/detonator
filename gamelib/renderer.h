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
#include "gamelib/tree.h"

namespace gfx {
    class Painter;
    class Transform;
    class Material;
    class Drawable;
}

namespace game
{
    class Animation;
    class AnimationClass;
    class ClassLibrary;

    struct AnimationDrawPacket {
        // shortcut to the node's material.
        std::shared_ptr<const gfx::Material> material;
        // shortcut to the node's drawable.
        std::shared_ptr<const gfx::Drawable> drawable;
        // transform that pertains to the draw.
        glm::mat4 transform;
        // the animation layer this draw belongs to.
        int layer = 0;
        // the render pass this draw belongs to.
        AnimationNode::RenderPass pass = AnimationNode::RenderPass::Draw;
    };

    template<typename Node>
    class AnimationDrawHook
    {
    public:
        virtual ~AnimationDrawHook() = default;
        // This is a hook function to inspect and  modify the the draw packet produced by the
        // given animation node. The return value can be used to indicate filtering.
        // If the function returns false the packet is dropped. Otherwise it's added to the
        // current drawlist with any possible modifications.
        virtual bool InspectPacket(const Node* node, AnimationDrawPacket& packet) { return true; }
        // This is a hook function to append extra draw packets to the current drawlist
        // based on the node.
        // Transform is the combined transformation hierarchy containing the transformations
        // from this current node to "view".
        virtual void AppendPackets(const Node* node, gfx::Transform& trans,
                                   std::vector<AnimationDrawPacket>& packets) {}
    protected:
    };

    using AnimationClassDrawHook = AnimationDrawHook<AnimationNodeClass>;
    using AnimationInstanceDrawHook = AnimationDrawHook<AnimationNode>;

    class Renderer
    {
    public:
        Renderer(const ClassLibrary* loader = nullptr)
          : mLoader(loader) {}
        void SetLoader(const ClassLibrary* loader)
        { mLoader = loader; }

        void BeginFrame();

        // Draw a representation of the animation class instance.
        // This functionality is mostly to support editor functionality
        // and to simplify working with an AnimationClass instance.
        void Draw(const AnimationClass& klass,
                  gfx::Painter& painter, gfx::Transform& transform,
                  AnimationDrawHook<AnimationNodeClass>* hook = nullptr);
        // Draw the animation and its nodes.
        // Each node is transformed relative to the parent transformation "trans".
        // Optional draw hook can be used to modify the draw packets before submission to the
        // paint device.
        void Draw(const Animation& animation,
                  gfx::Painter& painter, gfx::Transform& transform,
                  AnimationDrawHook<AnimationNode>* hook = nullptr);

        // Update the visual representation of the renderer's paint node
        // based on the given animation node.
        void Update(const AnimationNodeClass& node, float time, float dt);
        void Update(const AnimationNode& node, float time, float dt);
        void Update(const AnimationClass& klass, float time, float dt);
        void Update(const Animation& animation, float time, float dt);

        void EndFrame();
    private:
        template<typename NodeType>
        void Update(const NodeType& node, float time, float dt);

        template<typename NodeType>
        void Draw(const TreeNode<NodeType>& tree,
                  gfx::Painter& painter, gfx::Transform& transform,
                  AnimationDrawHook<NodeType>* hook);
    private:
        const ClassLibrary* mLoader;
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
