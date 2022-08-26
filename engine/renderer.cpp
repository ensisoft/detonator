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
#include "game/types.h"
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
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    EntityInstanceDrawHook* hook)
{
    MapEntity<Entity, EntityNode>(entity, transform);

    std::vector<DrawPacket> packets;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        PrimeNode(entity, entity.GetNode(i), transform, packets, hook);
    }
    DrawPackets(painter, packets);
}

void Renderer::Draw(const EntityClass& entity,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    EntityClassDrawHook* hook)
{

    MapEntity<EntityClass, EntityNodeClass>(entity, transform);

    std::vector<DrawPacket> packets;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        PrimeNode(entity, entity.GetNode(i), transform, packets, hook);
    }
    DrawPackets(painter, packets);
}

void Renderer::Draw(const Scene& scene,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    SceneInstanceDrawHook* scene_hook,
                    EntityInstanceDrawHook* entity_hook)
{
    DrawScene<Scene, Entity, EntityNode>(scene,
        painter, transform, scene_hook, entity_hook);
}

void Renderer::Draw(const SceneClass& scene,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
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
        UpdateNode<EntityNodeClass>(entity.GetNode(i), time, dt);
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
        UpdateNode<EntityNode>(entity.GetNode(i), time, dt);
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
    const auto* item = node.GetDrawable();
    const auto* text = node.GetTextItem();
    if (!item && !text)
        return;

    using DrawableItemType = typename Node::DrawableItemType;

    auto it = mPaintNodes.find(node.GetId());
    if (it == mPaintNodes.end())
        return;
    auto& paint_node = it->second;

    gfx::Transform transform;
    transform.Scale(paint_node.world_size);
    transform.Rotate(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    transform.Push(node.GetModelTransform());
    const auto& model = transform.GetAsMatrix();

    gfx::Drawable::Environment env;
    env.model_matrix = &model;

    if (item && paint_node.item_material)
    {
        const auto time_scale = item->GetTimeScale();
        if (item->TestFlag(DrawableItemType::Flags::UpdateMaterial))
            paint_node.item_material->Update(dt * time_scale);
    }
    if (item && paint_node.item_drawable)
    {
        const auto time_scale = item->GetTimeScale();
        if (item->TestFlag(DrawableItemType::Flags::UpdateDrawable))
            paint_node.item_drawable->Update(env, dt * time_scale);
        if (item->TestFlag(DrawableItemType::Flags::RestartDrawable) && !paint_node.item_drawable->IsAlive())
            paint_node.item_drawable->Restart(env);
    }

    if (text && paint_node.text_material)
    {
        paint_node.text_material->Update(dt);
    }
    if (text && paint_node.text_drawable)
    {
        paint_node.text_drawable->Update(env, dt);
    }
}

template<typename SceneType, typename EntityType, typename NodeType>
void Renderer::DrawScene(const SceneType& scene,
                         gfx::Painter& painter,
                         gfx::Transform& transform,
                         SceneDrawHook<EntityType>* scene_hook,
                         EntityDrawHook<NodeType>* entity_hook)
{
    auto nodes = scene.CollectNodes();

    // todo: use a faster sorting. (see the entity draw)
    std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
        return a.entity_object->GetLayer() < b.entity_object->GetLayer();
    });

    TRACE_SCOPE("Renderer::DrawEntities", "entities=%u", nodes.size());
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

template<typename EntityType, typename NodeType>
void Renderer::MapEntity(const EntityType& entity, gfx::Transform& transform)
{
    using RenderTree = game::RenderTree<NodeType>;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const EntityType& entity, Renderer& renderer, gfx::Transform& transform)
          : mEntity(entity)
          , mRenderer(renderer)
          , mTransform(transform)
        {}
        virtual void EnterNode(const NodeType* node) override
        {
            if (!node)
                return;

            // always push the node's transform, it might have children that
            // do render even if this node itself doesn't
            mTransform.Push(node->GetNodeTransform());

            const auto* item = node->GetDrawable();
            const auto* text = node->GetTextItem();
            if (item || text)
            {
                const game::FBox box(mTransform.GetAsMatrix());

                auto& paint_node = mRenderer.mPaintNodes[node->GetId()];
                paint_node.visited        = true;
                paint_node.world_pos      = box.GetCenter();
                paint_node.world_size     = box.GetSize();
                paint_node.world_rotation = box.GetRotation();
            }
        }
        virtual void LeaveNode(const NodeType* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const EntityType& mEntity;
        Renderer& mRenderer;
        gfx::Transform& mTransform;
    } visitor(entity, *this, transform);

    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

template<typename EntityType, typename NodeType>
void Renderer::PrimeNode(const EntityType& entity,
                         const NodeType& node,
                         gfx::Transform& scene,
                         std::vector<DrawPacket>& packets,
                         EntityDrawHook<NodeType>* hook)
{
    using DrawableItemType = typename NodeType::DrawableItemType;

    const auto* item = node.GetDrawable();
    const auto* text = node.GetTextItem();
    if (!item && !text)
    {
        if (hook)
        {
            scene.Push(entity.FindNodeTransform(&node));
            hook->AppendPackets(&node, scene, packets);
            scene.Pop();
        }
        return;
    }
    auto& paint_node = mPaintNodes[node.GetId()];

    //gfx::Transform transform(paint_node.transform);
    gfx::Transform transform;
    transform.Scale(paint_node.world_size);
    transform.Rotate(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    transform.Push(node.GetModelTransform());

    if (text)
    {
        const auto& node_size = node.GetSize();
        const auto text_raster_width  = text->GetRasterWidth();
        const auto text_raster_height = text->GetRasterHeight();
        const auto raster_width  = text_raster_width ? text_raster_width : node_size.x;
        const auto raster_height = text_raster_height ? text_raster_height : node_size.y;
        // use the instance hash as a material id to realize whether
        // we need to re-create the material. (i.e. the text in the text item has changed)
        // or the rasterization parameters have changed (node size -> raster buffer size)
        size_t hash = 0;
        hash = base::hash_combine(hash, raster_width);
        hash = base::hash_combine(hash, raster_height);
        // should the changes in the content be reflected or not?
        if (mEditingMode || !text->IsStatic())
            hash = base::hash_combine(hash, text->GetHash());

        const auto& material = std::to_string(hash);
        const auto& drawable = "_rect";
        if (paint_node.text_material_id != material)
        {
            gfx::TextBuffer::Text text_and_style;
            text_and_style.text       = text->GetText();
            text_and_style.font       = text->GetFontName();
            text_and_style.fontsize   = text->GetFontSize();
            text_and_style.lineheight = text->GetLineHeight();
            text_and_style.underline  = text->TestFlag(TextItem::Flags::UnderlineText);
            gfx::TextBuffer buffer(raster_width, raster_height);
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
            auto mat = gfx::CreateMaterialInstance(std::move(buffer));
            mat->SetColor(text->GetTextColor());
            paint_node.text_material = std::move(mat);
            paint_node.text_material_id = material;
        }
        if (!paint_node.text_drawable)
        {
            auto klass = mClassLib->FindDrawableClassById(drawable);
            paint_node.text_drawable = gfx::CreateDrawableInstance(klass);
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
            DrawPacket packet;
            packet.drawable  = paint_node.text_drawable;
            packet.material  = paint_node.text_material;
            packet.transform = transform.GetAsMatrix();
            packet.pass      = RenderPass::Draw;
            packet.layer     = text->GetLayer();
            if (!hook || (hook && hook->InspectPacket(&node, packet)))
                packets.push_back(std::move(packet));
        }
    }

    if (item)
    {
        const auto& material = item->GetMaterialId();
        const auto& drawable = item->GetDrawableId();
        if (item->GetRenderPass() == RenderPass::Draw && paint_node.item_material_id != material)
        {
            paint_node.item_material.reset();
            paint_node.item_material_id = material;
            auto klass = mClassLib->FindMaterialClassById(material);
            if (klass)
                paint_node.item_material = gfx::CreateMaterialInstance(klass);
            if (!paint_node.item_material)
                WARN("No such material class '%1' found for '%2/%3')", material, entity.GetName(), node.GetName());
        }
        if (paint_node.item_drawable_id != drawable)
        {
            paint_node.item_drawable.reset();
            paint_node.item_drawable_id = drawable;

            auto klass = mClassLib->FindDrawableClassById(drawable);
            if (klass)
                paint_node.item_drawable = gfx::CreateDrawableInstance(klass);
            if (!paint_node.item_drawable)
                WARN("No such drawable class '%1' found for '%2/%3'", drawable, entity.GetName(), node.GetName());
            if (paint_node.item_drawable)
            {
                const auto& model = transform.GetAsMatrix();
                gfx::Drawable::Environment env; // todo:
                env.model_matrix = &model;
                paint_node.item_drawable->Restart(env);
            }
        }
        if (paint_node.item_material)
        {
            paint_node.item_material->ResetUniforms();
            paint_node.item_material->SetUniforms(item->GetMaterialParams());
        }
        if (paint_node.item_drawable)
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

            paint_node.item_drawable->SetStyle(style);
            paint_node.item_drawable->SetLineWidth(item->GetLineWidth());
            const auto flip_h = item->TestFlag(DrawableItemType::Flags::FlipHorizontally);
            const auto flip_v = item->TestFlag(DrawableItemType::Flags::FlipVertically);
            if (flip_h ^ flip_v)
                paint_node.item_drawable->SetCulling(gfx::Drawable::Culling::Front);
            else paint_node.item_drawable->SetCulling(gfx::Drawable::Culling::Back);
        }
        if (item->TestFlag(DrawableItemType::Flags::FlipHorizontally))
        {
            transform.Push();
            transform.Scale(-1.0f , 1.0f);
            transform.Translate(1.0f , 0.0f);
        }
        if (item->TestFlag(DrawableItemType::Flags::FlipVertically))
        {
            transform.Push();
            transform.Scale(1.0f, -1.0f);
            transform.Translate(0.0f, 1.0f);
        }
        // if it doesn't render then no draw packets are generated
        if (item->TestFlag(DrawableItemType::Flags::VisibleInGame))
        {
            DrawPacket packet;
            packet.material  = paint_node.item_material;
            packet.drawable  = paint_node.item_drawable;
            packet.layer     = item->GetLayer();
            packet.pass      = item->GetRenderPass();
            packet.transform = transform.GetAsMatrix();
            if (!hook || (hook && hook->InspectPacket(&node , packet)))
                packets.push_back(std::move(packet));
        }
        if (item->TestFlag(DrawableItemType::Flags::FlipHorizontally))
            transform.Pop();
        if (item->TestFlag(DrawableItemType::Flags::FlipVertically))
            transform.Pop();
    }

    transform.Pop();

    // append any extra packets if needed.
    if (hook)
    {
        // Allow the draw hook to append any extra draw packets for this node.
        hook->AppendPackets(&node, transform, packets);
    }
}

void Renderer::DrawPackets(gfx::Painter& painter, std::vector<DrawPacket>& packets)
{
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
}

} // namespace
