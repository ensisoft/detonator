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

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <type_traits>
#include <algorithm>
#include <unordered_set>

#include "base/logging.h"
#include "base/utility.h"
#include "base/trace.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "engine/classlib.h"
#include "game/entity.h"
#include "game/scene.h"
#include "engine/renderer.h"

using namespace game;

namespace engine
{

void Renderer::BeginFrame()
{
    for (auto& p : mPaintNodes)
        p.second.visited = false;
}

void Renderer::Draw(const Entity& entity,
                    gfx::Painter& painter, gfx::Transform& transform,
                    EntityInstanceDrawHook* hook)
{
    DrawEntity<Entity, EntityNode>(entity, painter, transform, hook);
}

void Renderer::Draw(const EntityClass& entity,
                    gfx::Painter& painter, gfx::Transform& transform,
                    EntityClassDrawHook* hook)
{
    DrawEntity<EntityClass, EntityNodeClass>(entity, painter, transform, hook);
}

void Renderer::Draw(const Scene& scene,
                    gfx::Painter& painter, gfx::Transform& transform,
                    SceneInstanceDrawHook* scene_hook,
                    EntityInstanceDrawHook* entity_hook)
{
    DrawScene<Scene, Entity, EntityNode>(scene,
        painter, transform, scene_hook, entity_hook);
}

void Renderer::Draw(const SceneClass& scene,
                    gfx::Painter& painter, gfx::Transform& transform,
                    SceneClassDrawHook* scene_hook,
                    EntityClassDrawHook* entity_hook)
{
    DrawScene<SceneClass, SceneNodeClass, EntityNodeClass>(scene,
        painter, transform, scene_hook, entity_hook);
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

void Renderer::Update(const SceneClass& scene, float time, float dt)
{
    // when updating scene class the scene class nodes
    // refer to an entity class. Multiple scene nodes that
    // define a placement for an entity of some type then
    // refer to the same entity class type and actually point
    // to the *same* entity class object.
    // this map is used to check against duplicate reference
    // in order to discard incorrect duplicate update iterations
    // of the entity class object.
    std::unordered_set<std::string> klass_set;

    for (size_t i=0; i<scene.GetNumNodes(); ++i)
    {
        const auto& node = scene.GetNode(i);
        const auto& klass = node.GetEntityClass();
        if (!klass)
            continue;
        else if (klass_set.find(klass->GetId()) != klass_set.end())
            continue;
        Update(*klass, time, dt);
        klass_set.insert(klass->GetId());
    }
}
void Renderer::Update(const Scene& scene, float time, float dt)
{
    for (size_t i=0; i<scene.GetNumEntities(); ++i)
    {
        const auto& entity = scene.GetEntity(i);
        Update(entity, time, dt);
    }
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

void Renderer::ClearPaintState()
{
    mPaintNodes.clear();
}

template<typename Node>
void Renderer::UpdateNode(const Node& node, float time, float dt)
{
    const auto* drawable = node.GetDrawable();
    if (!drawable)
        return;

    using DrawableItemType = typename Node::DrawableItemType;

    auto it = mPaintNodes.find("item/" + node.GetId());
    if (it == mPaintNodes.end())
        return;
    auto& paint = it->second;
    if (paint.material)
    {
        const auto time_scale = drawable->GetTimeScale();
        if (drawable->TestFlag(DrawableItemType::Flags::UpdateMaterial))
            paint.material->Update(dt * time_scale);
    }
    if (paint.drawable)
    {
        const auto time_scale = drawable->GetTimeScale();
        if (drawable->TestFlag(DrawableItemType::Flags::UpdateDrawable))
            paint.drawable->Update(dt * time_scale);
        if (drawable->TestFlag(DrawableItemType::Flags::RestartDrawable) && !paint.drawable->IsAlive())
            paint.drawable->Restart();
    }
}

template<typename SceneType, typename EntityType, typename NodeType>
void Renderer::DrawScene(const SceneType& scene,
                         gfx::Painter& painter, gfx::Transform& transform,
                         SceneDrawHook<EntityType>* scene_hook,
                         EntityDrawHook<NodeType>* entity_hook)
{
    auto nodes = scene.CollectNodes();

    // todo: use a faster sorting. (see the entity draw)
    std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
        return a.entity_object->GetLayer() < b.entity_object->GetLayer();
    });

    for (const auto& p : nodes)
    {
        if (scene_hook && !scene_hook->FilterEntity(*p.entity_object))
            continue;

        transform.Push(p.node_to_scene);

        if (scene_hook)
            scene_hook->BeginDrawEntity(*p.entity_object, painter, transform);

        if (p.visual_entity && p.entity_object->TestFlag(EntityType::Flags::VisibleInGame))
            Draw(*p.visual_entity, painter, transform, entity_hook);

        if (scene_hook)
            scene_hook->EndDrawEntity(*p.entity_object, painter, transform);

        transform.Pop();
    }
}

template<typename EntityType, typename Node>
void Renderer::DrawEntity(const EntityType& entity,
                          gfx::Painter& painter, gfx::Transform& transform,
                          EntityDrawHook<Node>* hook)

{
    using RenderTree = game::RenderTree<Node>;
    using DrawPacket = DrawPacket;
    using DrawHook   = EntityDrawHook<Node>;
    using DrawableItemType = typename Node::DrawableItemType;

    auto tree = entity.GetRenderTree();

    // here we could apply operations that would apply to the whole
    // animation but currently we don't need such things.
    // if we did we could begin new transformation scope for this
    // by pushing a new scope in the transformation stack.
    // transfrom.Push();

    std::vector<DrawPacket> packets;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const EntityType& entity, std::vector<DrawPacket>& packets,
                Renderer& renderer, gfx::Transform& transform, DrawHook* hook)
            : mEntity(entity)
            , mPackets(packets)
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
            const auto* text = node->GetTextItem();
            if (!item && !text)
            {
                if (mHook)
                {
                    // Allow the draw hook to append any extra draw packets for this node.
                    gfx::Transform t(mTransform);
                    mHook->AppendPackets(node, t, mPackets);
                }
                return;
            }

            if (text)
            {
                const auto& size = node->GetSize();
                auto& paint_node = mRenderer.mPaintNodes["text/" + node->GetId()];
                paint_node.visited = true;
                // use the instance hash as a material id to realize whether
                // we need to re-create the material. (i.e. the text in the text item has changed)
                // or the rasterization parameters have changed (node size -> raster buffer size)
                size_t hash = 0;
                hash = base::hash_combine(hash, text->GetHash());
                hash = base::hash_combine(hash, size.x);
                hash = base::hash_combine(hash, size.y);
                const auto& material = std::to_string(hash);
                if (paint_node.material_class_id != material)
                {
                    gfx::TextBuffer::Text text_and_style;
                    text_and_style.text = text->GetText();
                    text_and_style.font = text->GetFontName();
                    text_and_style.fontsize = text->GetFontSize();
                    text_and_style.lineheight = text->GetLineHeight();
                    text_and_style.underline  = text->TestFlag(TextItem::Flags::UnderlineText);
                    gfx::TextBuffer buffer(size.x, size.y);
                    if (text->GetVAlign() == TextItem::VerticalTextAlign::Top)
                        buffer.SetAlignment(gfx::TextBuffer::VerticalAlignment::AlignTop);
                    else if (text->GetVAlign() == TextItem::VerticalTextAlign::Center)
                        buffer.SetAlignment(gfx::TextBuffer::VerticalAlignment::AlignCenter);
                    else if (text->GetVAlign() == TextItem::VerticalTextAlign::Bottom)
                        buffer.SetAlignment(gfx::TextBuffer::VerticalAlignment::AlignBottom);
                    if (text->GetHAlign() == TextItem::HorizontalTextAlign::Left)
                        buffer.SetAlignment(gfx::TextBuffer::HorizontalAlignment::AlignLeft);
                    else if (text->GetHAlign() == TextItem::HorizontalTextAlign::Center)
                        buffer.SetAlignment(gfx::TextBuffer::HorizontalAlignment::AlignCenter);
                    else if (text->GetHAlign() == TextItem::HorizontalTextAlign::Right)
                        buffer.SetAlignment(gfx::TextBuffer::HorizontalAlignment::AlignRight);
                    buffer.AddText(std::move(text_and_style));

                    // setup material to shade text.
                    auto klass = std::make_shared<gfx::TextureMap2DClass>();
                    klass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
                    klass->SetBaseColor(text->GetTextColor());
                    klass->SetTexture(CreateTextureFromText(std::move(buffer)));
                    klass->EnableGC(true); // enable gc
                    paint_node.material = gfx::CreateMaterialInstance(klass);
                    paint_node.material_class_id = material;
                }
                bool visible_now = true;
                if (text->TestFlag(TextItemClass::Flags::BlinkText))
                {
                    const auto fps = 1.5;
                    const auto full_period = 2.0 / fps;
                    const auto half_period = full_period * 0.5;
                    const auto time = fmodf(base::GetTime(), full_period);
                    if (time >= half_period)
                        visible_now = false;
                }

                if (text->TestFlag(TextItemClass::Flags::VisibleInGame) && visible_now)
                {
                    mTransform.Push(node->GetModelTransform());

                    static auto rect = std::make_shared<gfx::Rectangle>();
                    DrawPacket packet;
                    packet.drawable  = rect;
                    packet.material  = paint_node.material;
                    packet.transform = mTransform.GetAsMatrix();
                    packet.pass  = RenderPass::Draw;
                    packet.layer = text->GetLayer();
                    if (!mHook || (mHook && mHook->InspectPacket(node, packet)))
                        mPackets.push_back(std::move(packet));

                    // pop the model transform.
                    mTransform.Pop();
                }
            }

            if (item)
            {
                const auto& material = item->GetMaterialId();
                const auto& drawable = item->GetDrawableId();
                auto& paint_node = mRenderer.mPaintNodes["item/" + node->GetId()];
                paint_node.visited = true;
                if (item->GetRenderPass() == RenderPass::Draw && paint_node.material_class_id != material)
                {
                    paint_node.material.reset();
                    paint_node.material_class_id = material;
                    auto klass = mRenderer.mClassLib->FindMaterialClassById(item->GetMaterialId());
                    if (klass)
                        paint_node.material = gfx::CreateMaterialInstance(klass);
                    if (!paint_node.material)
                        WARN("No such material class '%1' found for '%2/%3')", material, mEntity.GetName(), node->GetName());
                }
                if (paint_node.drawable_class_id != drawable)
                {
                    paint_node.drawable.reset();
                    paint_node.drawable_class_id = drawable;

                    auto klass = mRenderer.mClassLib->FindDrawableClassById(item->GetDrawableId());
                    if (klass)
                        paint_node.drawable = gfx::CreateDrawableInstance(klass);
                    if (!paint_node.drawable)
                        WARN("No such drawable class '%1' found for '%2/%3'", drawable, mEntity.GetName(), node->GetName());
                }
                if (paint_node.material)
                {
                    paint_node.material->ResetUniforms();
                    paint_node.material->SetUniforms(item->GetMaterialParams());
                }
                if (paint_node.drawable)
                {
                    gfx::Drawable::Style style;
                    if (item->GetRenderStyle() == RenderStyle::Solid)
                        style = gfx::Drawable::Style::Solid;
                    else if (item->GetRenderStyle() == RenderStyle::Wireframe)
                        style = gfx::Drawable::Style::Wireframe;
                    else if (item->GetRenderStyle() == RenderStyle::Outline)
                        style = gfx::Drawable::Style::Outline;
                    else if (item->GetRenderStyle() == RenderStyle::Points)
                        style = gfx::Drawable::Style::Points;
                    else BUG("Unsupported rendering style.");

                    paint_node.drawable->SetStyle(style);
                    paint_node.drawable->SetLineWidth(item->GetLineWidth());
                    if (item->TestFlag(DrawableItemType::Flags::FlipVertically))
                        paint_node.drawable->SetCulling(gfx::Drawable::Culling::Front);
                    else paint_node.drawable->SetCulling(gfx::Drawable::Culling::Back);
                }

                // if it doesn't render then no draw packets are generated
                if (item->TestFlag(DrawableItemType::Flags::VisibleInGame))
                {
                    mTransform.Push(node->GetModelTransform());
                    if (item->TestFlag(DrawableItemType::Flags::FlipVertically))
                    {
                        mTransform.Push();
                        mTransform.Scale(-1.0f , 1.0f);
                        mTransform.Translate(1.0f , 0.0f);
                    }

                    DrawPacket packet;
                    packet.material  = paint_node.material;
                    packet.drawable  = paint_node.drawable;
                    packet.layer     = item->GetLayer();
                    packet.pass      = item->GetRenderPass();
                    packet.transform = mTransform.GetAsMatrix();
                    if (!mHook || (mHook && mHook->InspectPacket(node , packet)))
                        mPackets.push_back(std::move(packet));

                    if (item->TestFlag(DrawableItemType::Flags::FlipVertically))
                        mTransform.Pop();

                    // pop the model transform
                    mTransform.Pop();
                }
            }

            // append any extra packets if needed.
            if (mHook)
            {
                // Allow the draw hook to append any extra draw packets for this node.
                gfx::Transform t(mTransform);
                mHook->AppendPackets(node, t, mPackets);
            }
        }

        virtual void LeaveNode(const Node *node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }

    private:
        const EntityType& mEntity;
        std::vector<DrawPacket>& mPackets;
        Renderer& mRenderer;
        gfx::Transform& mTransform;
        DrawHook* mHook = nullptr;
    };

    Visitor visitor(entity, packets, *this, transform, hook);
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
        if (packet.pass == RenderPass::Draw && !packet.material)
            continue;
        else if (!packet.drawable)
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
        }
        else if (packet.pass == RenderPass::Mask)
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
