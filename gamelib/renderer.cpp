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

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include "base/logging.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "gamelib/animation.h"
#include "gamelib/classlib.h"
#include "gamelib/entity.h"
#include "gamelib/renderer.h"

namespace game
{

void Renderer::BeginFrame()
{
    for (auto& p : mPaintNodes)
        p.second.visited = false;
}

void Renderer::Draw(const AnimationClass& klass,
                    gfx::Painter& painter, gfx::Transform& transform,
                    AnimationClassDrawHook* hook)
{
    DrawRenderTree<AnimationNodeClass>(klass.GetRenderTree(), painter, transform, hook);
}
void Renderer::Draw(const Animation& animation,
                    gfx::Painter& painter, gfx::Transform& transform,
                    AnimationInstanceDrawHook* hook)
{
    DrawRenderTree<AnimationNode>(animation.GetRenderTree(), painter, transform, hook);
}

void Renderer::Draw(const Entity& entity,
                    gfx::Painter& painter, gfx::Transform& transform,
                    EntityInstanceDrawHook* hook)
{
    DrawRenderTree<EntityNode>(entity.GetRenderTree(), painter, transform, hook);
}

void Renderer::Draw(const EntityClass& entity,
                    gfx::Painter& painter, gfx::Transform& transform,
                    EntityClassDrawHook* hook)
{
    DrawRenderTree<EntityNodeClass>(entity.GetRenderTree(), painter, transform, hook);
}

void Renderer::Update(const AnimationNodeClass& node, float time, float dt)
{
    UpdateNode<AnimationNodeClass>(node, time, dt);
}
void Renderer::Update(const AnimationNode& node, float time, float dt)
{
    UpdateNode<AnimationNode>(node, time, dt);
}
void Renderer::Update(const AnimationClass& klass, float time, float dt)
{
    for (size_t i=0; i<klass.GetNumNodes(); ++i) 
    {
        const auto& node = klass.GetNode(i);
        UpdateNode<AnimationNodeClass>(node, time, dt);
    }
}
void Renderer::Update(const Animation& animation, float time, float dt)
{
    for (size_t i=0; i<animation.GetNumNodes(); ++i) 
    {
        const auto& node = animation.GetNode(i);
        UpdateNode<AnimationNode>(node, time, dt);
    }
}

void Renderer::Update(const EntityClass& entity, float time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        UpdateNode<EntityNodeClass>(node, time, dt);
    }
}

void Renderer::Update(const EntityNodeClass& node, float time, float dt)
{
    UpdateNode<EntityNodeClass>(node, time, dt);
}

void Renderer::Update(const Entity& entity, float time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        UpdateNode<EntityNode>(node, time, dt);
    }
}

void Renderer::Update(const EntityNode& node, float time, float dt)
{
    UpdateNode<EntityNode>(node, time, dt);
}

void Renderer::EndFrame()
{
    for (auto it = mPaintNodes.begin(); it != mPaintNodes.end();)
    {
        auto& p = it->second;
        if (p.visited)
        {
            ++it;
            continue;
        }
        it = mPaintNodes.erase(it);
    }
}

template<typename Node>
void Renderer::UpdateNode(const Node& node, float time, float dt)
{
    const auto* drawable = node.GetDrawable();
    if (!drawable)
        return;

    using DrawableItemType = typename Node::DrawableItemType;

    auto it = mPaintNodes.find(node.GetInstanceId());
    if (it == mPaintNodes.end())
        return;
    auto& paint = it->second;
    if (paint.material)
    {
        if (drawable->TestFlag(DrawableItemType::Flags::UpdateMaterial))
            paint.material->Update(dt);
    }
    if (paint.drawable)
    {
        if (drawable->TestFlag(DrawableItemType::Flags::UpdateDrawable))
            paint.drawable->Update(dt);
        if (drawable->TestFlag(DrawableItemType::Flags::RestartDrawable) && !paint.drawable->IsAlive())
            paint.drawable->Restart();
    }
}

template<typename Node>
void Renderer::DrawRenderTree(const TreeNode<Node>& tree,
                             gfx::Painter& painter, gfx::Transform& transform,
                             DrawHook<Node>* hook)

{
    using RenderTree = TreeNode<Node>;
    using DrawPacket = DrawPacket;
    using DrawHook   = DrawHook<Node>;
    using DrawableItemType = typename Node::DrawableItemType;

    // here we could apply operations that would apply to the whole
    // animation but currently we don't need such things.
    // if we did we could begin new transformation scope for this
    // by pushing a new scope in the transformation stack.
    // transfrom.Push();

    std::vector<DrawPacket> packets;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(std::vector<DrawPacket>& packets, Renderer& renderer,
                gfx::Transform& transform, DrawHook* hook)
                : mPackets(packets)
                , mRenderer(renderer)
                , mTransform(transform)
                , mHook(hook)
        {}

        virtual void EnterNode(const Node* node) override
        {
            if (!node)
                return;

            // always push the node's transform, it might have children that
            // do render.
            mTransform.Push(node->GetNodeTransform());

            const auto* item = node->GetDrawable();
            if (!item)
                return;

            const auto& material = item->GetMaterialId();
            const auto& drawable = item->GetDrawableId();
            auto& paint_node = mRenderer.mPaintNodes[node->GetInstanceId()];
            paint_node.visited = true;
            if (!paint_node.material || paint_node.material_class_id != material)
            {
                paint_node.material.reset();
                paint_node.material_class_id.clear();
                if (!material.empty())
                {
                    auto klass = mRenderer.mLoader->FindMaterialClass(item->GetMaterialId());
                    paint_node.material = gfx::CreateMaterialInstance(klass);
                    paint_node.material_class_id = material;
                }
            }
            if (!paint_node.drawable || paint_node.drawable_class_id != drawable)
            {
                paint_node.drawable.reset();
                paint_node.drawable_class_id.clear();
                if (!drawable.empty())
                {
                    auto klass = mRenderer.mLoader->FindDrawableClass(item->GetDrawableId());
                    paint_node.drawable = gfx::CreateDrawableInstance(klass);
                    paint_node.drawable_class_id = drawable;
                }
            }
            if (paint_node.material)
            {
                if (item->TestFlag(DrawableItemType::Flags::OverrideAlpha))
                    paint_node.material->SetAlpha(item->GetAlpha());
                else paint_node.material->ResetAlpha();
            }
            if (paint_node.drawable)
            {
                paint_node.drawable->SetStyle(item->GetRenderStyle());
                paint_node.drawable->SetLineWidth(item->GetLineWidth());
            }

            // if it doesn't render then no draw packets are generated
            if (node->TestFlag(Node::Flags::VisibleInGame))
            {
                mTransform.Push(node->GetModelTransform());

                DrawPacket packet;
                packet.material  = paint_node.material;
                packet.drawable  = paint_node.drawable;
                packet.layer     = item->GetLayer();
                packet.pass      = item->GetRenderPass();
                packet.transform = mTransform.GetAsMatrix();
                if (!mHook || (mHook && mHook->InspectPacket(node, packet)))
                    mPackets.push_back(std::move(packet));

                // pop the model transform
                mTransform.Pop();
            }

            // append any extra packets if needed.
            if (mHook)
            {
                // Allow the draw hook to append any extra draw packets for this node.
                gfx::Transform t(mTransform);
                std::vector<DrawPacket> extra;
                mHook->AppendPackets(node, t, extra);
                for (auto& packet : extra)
                    mPackets.push_back(std::move(packet));
            }
        }

        virtual void LeaveNode(const Node *node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }

    private:
        std::vector<DrawPacket>& mPackets;
        Renderer& mRenderer;
        gfx::Transform& mTransform;
        DrawHook* mHook = nullptr;
    };

    Visitor visitor(packets, *this, transform, hook);
    tree.PreOrderTraverse(visitor);

    // the layer value is negative but for the indexing below
    // we must have positive values only.
    int first_layer_index = 0;
    for (auto &packet : packets)
    {
        first_layer_index = std::min(first_layer_index, packet.layer);
    }
    // offset the layers.
    for (auto &packet : packets)
    {
        packet.layer += std::abs(first_layer_index);
    }

    struct Layer {
        std::vector<gfx::Painter::DrawShape> draw_list;
        std::vector<gfx::Painter::MaskShape> mask_list;
    };
    std::vector<Layer> layers;

    for (auto &packet : packets)
    {
        if (!packet.material || !packet.drawable)
            continue;

        const auto layer_index = packet.layer;
        if (layer_index >= layers.size())
            layers.resize(layer_index + 1);

        Layer &layer = layers[layer_index];
        if (packet.pass == RenderPass::Draw)
        {
            gfx::Painter::DrawShape shape;
            shape.transform = &packet.transform;
            shape.drawable = packet.drawable.get();
            shape.material = packet.material.get();
            layer.draw_list.push_back(shape);
        } else if (packet.pass == RenderPass::Mask)
        {
            gfx::Painter::MaskShape shape;
            shape.transform = &packet.transform;
            shape.drawable = packet.drawable.get();
            layer.mask_list.push_back(shape);
        }
    }
    for (const auto &layer : layers)
    {
        if (layer.mask_list.empty())
            painter.Draw(layer.draw_list);
        else painter.Draw(layer.draw_list, layer.mask_list);
    }
    // if we used a new transformation scope pop it here.
    //transform.Pop();

}

} // namespace
