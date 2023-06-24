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
#include "graphics/renderpass.h"
#include "graphics/utility.h"
#include "engine/classlib.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/types.h"
#include "game/tilemap.h"
#include "engine/renderer.h"
#include "engine/graphics.h"
#include "engine/camera.h"

using namespace game;

namespace engine
{

Renderer::Renderer(const ClassLibrary* classlib)
  : mClassLib(classlib)
{
    mEffects.set(Effects::Bloom, false);
}

void Renderer::BeginFrame()
{
    if (mEditingMode)
    {
        for (auto& p: mPaintNodes)
            p.second.visited = false;
    }
}

void Renderer::CreateScene(const game::Scene& scene)
{
    mPaintNodes.clear();

    const auto& nodes = scene.CollectNodes();

    gfx::Transform transform;

    for (const auto& p : nodes)
    {
        const Entity* entity = p.entity_object;
        if (!entity->HasRenderableItems())
            continue;
        transform.Push(p.node_to_scene);
            MapEntity<Entity, EntityNode>(*p.entity_object, transform);
        transform.Pop();
    }

}

void Renderer::UpdateScene(const game::Scene& scene)
{
    const auto& nodes = scene.CollectNodes();

    gfx::Transform transform;

    for (const auto& p : nodes)
    {
        const Entity* entity = p.entity_object;

        // either delete dead entites here or create a "BeginLoop" type of function
        // and do the pruning there.
        if (entity->HasBeenKilled())
        {
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const EntityNode& node = entity->GetNode(i);
                mPaintNodes.erase(node.GetId());
            }
            continue;
        }

        transform.Push(p.node_to_scene);
            MapEntity<Entity, EntityNode>(*p.entity_object, transform);
        transform.Pop();
    }
}

void Renderer::Update(float time, float dt)
{
    for (auto& [key, node] : mPaintNodes)
    {
        UpdateNode<EntityNode>(node, time, dt);
    }
}

void Renderer::Draw(gfx::Painter& painter, EntityInstanceDrawHook* hook)
{
    std::vector<DrawPacket> packets;
    for (auto& [key, node] : mPaintNodes)
    {
        CreateDrawResources<Entity, EntityNode>(node);
        GenerateDrawPackets<Entity, EntityNode>(node, packets, hook);
        node.visited = true;
    }
    DrawPackets(painter, packets);
}

void Renderer::Draw(const Entity& entity,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    EntityInstanceDrawHook* hook)
{

    DrawEntity<Entity, EntityNode>(entity, painter, transform, hook);
}

void Renderer::Draw(const EntityClass& entity,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    EntityClassDrawHook* hook)
{

    DrawEntity<EntityClass, EntityNodeClass>(entity, painter, transform, hook);
}

void Renderer::Draw(const Scene& scene,
                    gfx::Painter& painter,
                    SceneInstanceDrawHook* scene_hook,
                    EntityInstanceDrawHook* entity_hook)
{
    DrawScene<Scene, Entity, EntityNode>(scene, painter, scene_hook, entity_hook);
}

void Renderer::Draw(const SceneClass& scene,
                    gfx::Painter& painter,
                    SceneClassDrawHook* scene_hook,
                    EntityClassDrawHook* entity_hook)
{
    DrawScene<SceneClass, SceneNodeClass, EntityNodeClass>(scene, painter, scene_hook, entity_hook);
}

void Renderer::Draw(const game::Tilemap& map,
                    gfx::Painter& painter,
                    bool draw_render_layer,
                    bool draw_data_layer)
{

    std::vector<TileBatch> batches;

    for (unsigned layer_index=0; layer_index<map.GetNumLayers(); ++layer_index)
    {
        const auto& layer = map.GetLayer(layer_index);
        if (!layer.IsLoaded())
            continue;

        const auto& klass = layer.GetClass();
        if (!klass.TestFlag(game::TilemapLayerClass::Flags::VisibleInEditor) ||
            !klass.TestFlag(game::TilemapLayerClass::Flags::Visible))
            continue;

        // these are the tile sizes in units
        const auto map_tile_width_units    = map.GetTileWidth();
        const auto map_tile_height_units   = map.GetTileHeight();
        const auto layer_tile_width_units  = map_tile_width_units * layer.GetTileSizeScaler();
        const auto layer_tile_height_units = map_tile_height_units * layer.GetTileSizeScaler();

        const unsigned layer_width_tiles  = layer.GetWidth();
        const unsigned layer_height_tiles = layer.GetHeight();
        const float layer_width_units  = layer_width_tiles  * layer_tile_width_units;
        const float layer_height_units = layer_height_tiles * layer_tile_height_units;

        const auto visible_region = game::URect(0, 0, layer_width_tiles, layer_height_tiles);

        const auto type = layer.GetType();
        if (draw_render_layer && layer->HasRenderComponent())
        {
            if (type == game::TilemapLayer::Type::Render)
                PrepareTileBatches<game::TilemapLayer_Render>(map, layer, visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataUInt4)
                PrepareTileBatches<game::TilemapLayer_Render_DataUInt4>(map, layer, visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataSInt4)
                PrepareTileBatches<game::TilemapLayer_Render_DataSInt4>(map, layer, visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataSInt8)
                PrepareTileBatches<game::TilemapLayer_Render_DataSInt8>(map, layer, visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataUInt8)
                PrepareTileBatches<game::TilemapLayer_Render_DataUInt8>(map, layer, visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataUInt24)
                PrepareTileBatches<game::TilemapLayer_Render_DataUInt24>(map, layer,visible_region, batches, layer_index);
            else if (type == game::TilemapLayer::Type::Render_DataSInt24)
                PrepareTileBatches<game::TilemapLayer_Render_DataSInt24>(map, layer,visible_region, batches, layer_index);
            else BUG("Unknown render layer type.");
        }

        if (draw_data_layer && layer->HasDataComponent())
        {
            const auto tile_size = gfx::FSize(layer_tile_width_units, layer_tile_height_units);

            if (type == game::TilemapLayer::Type::Render_DataUInt4)
                DrawDataLayer<game::TilemapLayer_Render_DataUInt4>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::Render_DataSInt4)
                DrawDataLayer<game::TilemapLayer_Render_DataSInt4>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::Render_DataSInt8)
                DrawDataLayer<game::TilemapLayer_Render_DataSInt8>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::Render_DataUInt8)
                DrawDataLayer<game::TilemapLayer_Render_DataUInt8>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::Render_DataUInt24)
                DrawDataLayer<game::TilemapLayer_Render_DataUInt24>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::Render_DataSInt24)
                DrawDataLayer<game::TilemapLayer_Render_DataSInt24>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::DataSInt8)
                DrawDataLayer<game::TilemapLayer_Data_SInt8>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::DataUInt8)
                DrawDataLayer<game::TilemapLayer_Data_UInt8>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::DataSInt16)
                DrawDataLayer<game::TilemapLayer_Data_SInt16>(map, layer, visible_region, tile_size, painter);
            else if (type == game::TilemapLayer::Type::DataUInt16)
                DrawDataLayer<game::TilemapLayer_Data_UInt16>(map, layer, visible_region, tile_size, painter);
            else BUG("Unknown data layer type.");
        }
    }

    DrawTileBatches(map, batches, painter);
}

void Renderer::Update(const EntityClass& entity, float time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
            UpdateNode<EntityNodeClass>(*paint, time, dt);
    }
}

void Renderer::Update(const EntityNodeClass& node, float time, float dt)
{
    if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
        UpdateNode<EntityNodeClass>(*paint, time, dt);
}

void Renderer::Update(const Entity& entity, float time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
            UpdateNode<EntityNode>(*paint, time, dt);
    }
}

void Renderer::Update(const EntityNode& node, float time, float dt)
{
    if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
        UpdateNode<EntityNode>(*paint, time, dt);
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
    if (mEditingMode)
    {
        for (auto it = mPaintNodes.begin(); it != mPaintNodes.end();)
        {
            const auto& paint_node = it->second;
            if (paint_node.visited)
                ++it;
            else it = mPaintNodes.erase(it);
        }
    }
}

void Renderer::ClearPaintState()
{
    mPaintNodes.clear();
    mTilemapPalette.clear();
}

template<typename EntityNodeType>
void Renderer::UpdateNode(PaintNode& paint_node, float time, float dt)
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    const auto& node = *std::get<const EntityNodeType*>(paint_node.entity_node);
    const auto* item = node.GetDrawable();
    const auto* text = node.GetTextItem();

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
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

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            if (item->HasMaterialTimeAdjustment())
            {
                const float adjusted_time = item->GetMaterialTimeAdjustment();
                paint_node.item_material->SetRuntime(adjusted_time);
                item->ClearMaterialTimeAdjustment();
            }
        }

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            item->SetCurrentMaterialTime(paint_node.item_material->GetRuntime());
        }
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
        // this is the model, aka model_to_world transform
        gfx::Transform transform(p.node_to_scene);

        // draw when there's no scene hook or when the scene hook returns
        // true for the filtering operation.
        const bool should_draw = !scene_hook || (scene_hook && scene_hook->FilterEntity(*p.entity_object, painter, transform));

        if (should_draw)
        {
            if (scene_hook)
                scene_hook->BeginDrawEntity(*p.entity_object, painter, transform);

            if (p.visual_entity && p.entity_object->TestFlag(EntityType::Flags::VisibleInGame))
                DrawEntity(*p.visual_entity, painter, transform, entity_hook);

            if (scene_hook)
                scene_hook->EndDrawEntity(*p.entity_object, painter, transform);
        }
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
                paint_node.world_pos      = box.GetTopLeft();
                paint_node.world_scale    = box.GetSize();
                paint_node.world_rotation = box.GetRotation();
                paint_node.entity_node    = node;
                paint_node.entity         = &mEntity;
#if !defined(NDEBUG)
                paint_node.debug_name     = mEntity.GetName() + "/" + node->GetName();
#endif
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
void Renderer::DrawEntity(const EntityType& entity,
                          gfx::Painter& painter,
                          gfx::Transform& transform,
                          EntityDrawHook<NodeType>* hook)
{
    MapEntity<EntityType, NodeType>(entity, transform);

    std::vector<DrawPacket> packets;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
        {
            CreateDrawResources<EntityType, NodeType>(*paint);
            GenerateDrawPackets<EntityType, NodeType>(*paint, packets, hook);
        }
        else if (hook)
        {
            transform.Push(entity.FindNodeTransform(&node));
                hook->AppendPackets(&node, transform, packets);
            transform.Pop();
        }
    }
    DrawPackets(painter, packets);
}


template<typename EntityType, typename EntityNodeType>
void Renderer::CreateDrawResources(PaintNode& paint_node)
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    const auto& entity = *std::get<const EntityType*>(paint_node.entity);
    const auto& node   = *std::get<const EntityNodeType*>(paint_node.entity_node);

    if (const auto* text = node.GetTextItem())
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
            buffer.SetText(std::move(text_and_style));

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
    }
    if (const auto* item = node.GetDrawable())
    {
        const auto& material = item->GetMaterialId();
        const auto& drawable = item->GetDrawableId();
        if (paint_node.item_material_id != material)
        {
            paint_node.item_material.reset();
            paint_node.item_material_id = material;
            auto klass = mClassLib->FindMaterialClassById(material);
            if (klass)
                paint_node.item_material = gfx::CreateMaterialInstance(klass);
            if (!paint_node.item_material)
                WARN("No such material class found. [material='%1', entity='%2', node='%3']", material, entity.GetName(), node.GetName());
        }
        if (paint_node.item_drawable_id != drawable)
        {
            paint_node.item_drawable.reset();
            paint_node.item_drawable_id = drawable;

            auto klass = mClassLib->FindDrawableClassById(drawable);
            if (klass)
                paint_node.item_drawable = gfx::CreateDrawableInstance(klass);
            if (!paint_node.item_drawable)
                WARN("No such drawable class found. [drawable='%1', entity='%2, node='%3']", drawable, entity.GetName(), node.GetName());
            if (paint_node.item_drawable)
            {
                gfx::Transform transform;
                transform.Scale(paint_node.world_scale);
                transform.RotateAroundZ(paint_node.world_rotation);
                transform.Translate(paint_node.world_pos);

                transform.Push(node.GetModelTransform());

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
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::GenerateDrawPackets(PaintNode& paint_node,
                                   std::vector<DrawPacket>& packets,
                                   EntityDrawHook<EntityNodeType>* hook)
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    const auto& entity = *std::get<const EntityType*>(paint_node.entity);
    const auto& node   = *std::get<const EntityNodeType*>(paint_node.entity_node);
    const bool entity_visible = entity.TestFlag(EntityType::Flags::VisibleInGame);

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    transform.Push(node.GetModelTransform());

    if (const auto* text = node.GetTextItem())
    {
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
        if (text->TestFlag(TextItemClass::Flags::VisibleInGame) && entity_visible && visible_now)
        {
            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, text->TestFlag(TextItemClass::Flags::PP_EnableBloom));
            packet.drawable  = paint_node.text_drawable;
            packet.material  = paint_node.text_material;
            packet.transform = transform.GetAsMatrix();
            packet.pass      = RenderPass::DrawColor;
            packet.entity_node_layer = text->GetLayer();
            packet.scene_node_layer  = entity.GetLayer();
            if (!hook || hook->InspectPacket(&node, packet))
                packets.push_back(std::move(packet));
        }
    }

    if (const auto* item = node.GetDrawable())
    {
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
        if (item->TestFlag(DrawableItemType::Flags::VisibleInGame) && entity_visible)
        {
            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, item->TestFlag(DrawableItemType::Flags::PP_EnableBloom));
            packet.material  = paint_node.item_material;
            packet.drawable  = paint_node.item_drawable;
            packet.transform = transform.GetAsMatrix();
            packet.pass      = item->GetRenderPass();
            packet.entity_node_layer = item->GetLayer();
            packet.scene_node_layer  = entity.GetLayer();
            if (!hook || hook->InspectPacket(&node , packet))
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
    int first_entity_node_layer_index = 0;
    int first_scene_node_layer_index  = 0;
    for (auto &packet : packets)
    {
        first_entity_node_layer_index = std::min(first_entity_node_layer_index, packet.entity_node_layer);
        first_scene_node_layer_index  = std::min(first_scene_node_layer_index, packet.scene_node_layer);
    }
    // offset the layers.
    for (auto &packet : packets)
    {
        packet.entity_node_layer += std::abs(first_entity_node_layer_index);
        packet.scene_node_layer  += std::abs(first_scene_node_layer_index);
    }


    // Each entity in the scene is assigned to a scene/entity layer and each
    // entity node within an entity is assigned to an entity layer.
    // Thus, to have the right ordering both indices of each
    // render packet must be considered!
    std::vector<std::vector<RenderLayer>> layers;

    for (auto& packet : packets)
    {
        if (!packet.material || !packet.drawable)
            continue;

        const auto scene_layer_index = packet.scene_node_layer;
        if (scene_layer_index >= layers.size())
            layers.resize(scene_layer_index + 1);

        auto& entity_scene_layer = layers[scene_layer_index];

        const auto entity_node_layer_index = packet.entity_node_layer;
        if (entity_node_layer_index >= entity_scene_layer.size())
            entity_scene_layer.resize(entity_node_layer_index + 1);

        RenderLayer& entity_layer = entity_scene_layer[entity_node_layer_index];
        if (packet.pass == RenderPass::DrawColor)
        {
            gfx::Painter::DrawShape shape;
            shape.user      = &packet;
            shape.transform = &packet.transform;
            shape.drawable  = packet.drawable.get();
            shape.material  = packet.material.get();
            entity_layer.draw_color_list.push_back(shape);
        }
        else if (packet.pass == RenderPass::MaskCover)
        {
            gfx::Painter::DrawShape shape;
            shape.user      = &packet;
            shape.transform = &packet.transform;
            shape.drawable  = packet.drawable.get();
            shape.material  = packet.material.get();
            entity_layer.mask_cover_list.push_back(shape);
        }
        else if (packet.pass == RenderPass::MaskExpose)
        {
            gfx::Painter::DrawShape shape;
            shape.user      = &packet;
            shape.transform = &packet.transform;
            shape.drawable  = packet.drawable.get();
            shape.material  = packet.material.get();
            entity_layer.mask_expose_list.push_back(shape);
        }
    }

    const gfx::Color4f bloom_color(mBloom.red, mBloom.green, mBloom.blue, 1.0f);
    const BloomPass bloom(mRendererName,  bloom_color, mBloom.threshold, painter);

    if (IsEnabled(Effects::Bloom))
        bloom.Draw(layers);

    MainRenderPass main(painter);
    main.Draw(layers);
    main.Composite(IsEnabled(Effects::Bloom) ? &bloom : nullptr);

}

template<typename LayerType>
void Renderer::PrepareTileBatches(const game::Tilemap& map,
                                  const game::TilemapLayer& layer,
                                  const game::URect& visible_region,
                                  std::vector<TileBatch>& batches,
                                  std::uint16_t layer_index)

{
    using TileType = typename LayerType::TileType;
    using LayerTraits = game::detail::TilemapLayerTraits<TileType>;

    const auto* ptr = game::TilemapLayerCast<LayerType>(&layer);

    const auto tile_row = visible_region.GetY();
    const auto tile_col = visible_region.GetX();
    const auto max_row  = visible_region.GetHeight();
    const auto max_col  = visible_region.GetWidth();

    // these are the tile sizes in units
    const auto map_tile_width_units    = map.GetTileWidth();
    const auto map_tile_height_units   = map.GetTileHeight();
    const auto layer_tile_width_units  = map_tile_width_units * layer.GetTileSizeScaler();
    const auto layer_tile_height_units = map_tile_height_units * layer.GetTileSizeScaler();

    for (unsigned row=tile_row; row<max_row; ++row)
    {
        auto last_material_index = LayerTraits::MaxPaletteIndex;

        for (unsigned col=tile_col; col<max_col; ++col)
        {
            // the tiles index value is an index into the tile map
            // sprite palette.
            const auto material_index = ptr->GetTile(row, col).index;
            // special index max means "no value". this is needed in order
            // to be able to have "holes" in some layer and let the layer
            // below show through. could have used zero but that would
            // make the palette indexing more inconvenient.
            if (material_index == LayerTraits::MaxPaletteIndex)
                continue;

            // if the material changes start a new tile batch.
            if (last_material_index != material_index)
            {
                batches.emplace_back();
                auto& batch = batches.back();
                batch.material_index = material_index;
                batch.layer_index    = layer_index;
                batch.row_index      = row;
                batch.tile_size      = glm::vec2{layer_tile_width_units, layer_tile_height_units};
            }

            // append to tile to the current batch
            gfx::TileBatch::Tile tile;
            tile.pos.x = col;
            tile.pos.y = row;
            auto& batch = batches.back();
            batch.tiles.AddTile(tile);

            last_material_index  = material_index;
        }
    }
}


void Renderer::DrawTileBatches(const game::Tilemap& map,
                               std::vector<TileBatch>& batches,
                               gfx::Painter& painter)
{
    // The isometric tile rendering is quite complicated assuming pre-rendered tiles.
    // Conceptually the tiles are 2D squares in the "tile world space",
    // which is on the XY plane in a 3D coordinate space. When such a tile is
    // projected onto the 2D projection plane with dimetric projection the outline
    // shape that is produced is a rhombus. Finally the shape of the object that
    // is actually rendered to represent the tile visually in screen space is
    // either a square or a rectangle depending on the tileset.
    // The correct rendering position is computed by selecting some point on the
    // tile or inside the tile (such as one of the corners or the center point)
    // and projecting it from the tile world to render world and then aligning
    // a rectangle around this point.
    //
    // Another complication is the correct rendering order to solve for the
    // visibility problem. If the rendering is dimetric the rendering order
    // must be from left to right, top to bottom in tile grid coordinates
    // because the rendered tiles are actually overlapping. (This doesn't work
    // with tile batching based on material index when using multiple materials.
    // If there's a single material and all tiles inside the batch are sorted
    // to their correct order then it works as expected)
    // Random thoughts...
    // - depth buffering doesn't work out of the box since the vertex depth
    //   (distance from camera) is useless with orthographic projection.
    //   Rather a depth value for a tile would need to be fabricated from the
    //   tile x,y coordinates.
    //  - OpenGL ES2 doesn't have gl_FragDepth
    //  - Do a depth pre-pass, render the tile depth to a texture, when doing
    //   the color pass compute the depth again and compare against the depth
    //   value in the texture. then discard or write. Would work for alpha cut-outs (?)
    //   but not for semi-transparent objects(!)

    // Setup painter for rendering in the screen coordinates
    const auto viewport_width  = mSurface.viewport.GetWidth();
    const auto viewport_height = mSurface.viewport.GetHeight();

    painter.ResetViewMatrix();
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(viewport_width, viewport_height));
    painter.SetViewport(gfx::IRect(0, 0, viewport_width, viewport_height));
    painter.SetPixelRatio(glm::vec2{1.0f, 1.0f});

    const auto& src_view_to_clip = CreateProjectionMatrix(mCamera.perspective, mCamera.viewport);
    const auto& src_world_to_view = CreateViewMatrix(mCamera.position, mCamera.scale, mCamera.perspective, mCamera.rotation);

    const auto& dst_view_to_clip = painter.GetProjMatrix();
    const auto& dst_world_to_view = painter.GetViewMatrix();
    // This matrix will project a coordinate in isometric tile world space into
    // 2D screen space/surface coordinate.
    const auto& tile_projection_transform_matrix = GetProjectionTransformMatrix(src_view_to_clip,
                                                                                src_world_to_view,
                                                                                dst_view_to_clip,
                                                                                dst_world_to_view);

    std::sort(batches.begin(), batches.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.row_index < rhs.row_index)
            return true;
        else if (lhs.row_index == rhs.row_index)
            return lhs.layer_index < rhs.layer_index;
        return false;
    });

    for (auto& batch : batches)
    {
        const auto tile_render_size = ComputeTileRenderSize(tile_projection_transform_matrix, batch.tile_size, mCamera.perspective);
        const auto tile_render_width_scale = map->GetTileRenderWidthScale();
        const auto tile_render_height_scale = map->GetTileRenderHeightScale();
        const auto tile_width_render_units = tile_render_size.x * tile_render_width_scale;
        const auto tile_height_render_units = tile_render_size.y * tile_render_height_scale;

        const auto material_index = batch.material_index;
        const auto layer_index    = batch.layer_index;

        // create new layer palette for the material instances if needed.
        if (layer_index >= mTilemapPalette.size())
            mTilemapPalette.resize(layer_index+1);

        auto& layer_palette = mTilemapPalette[layer_index];

        // allocate new tile map material node.
        if (material_index >= layer_palette.size())
            layer_palette.resize(material_index + 1);

        auto& layer_material_node = layer_palette[material_index];

        // find the material ID for this index from the layer.
        const auto& layer = map.GetLayer(layer_index);
        const auto& material_id = layer.GetPaletteMaterialId(material_index);

        // create the material instances if the material Id has changed.
        if (layer_material_node.material_id != material_id)
        {
            layer_material_node.material_id = material_id;
            layer_material_node.material.reset();
            if (layer_material_node.material_id.empty())
            {
                WARN("Tilemap layer has no material ID set for material palette index. [layer='%1', index=%2]", layer.GetClassName(), material_index);
                continue;
            }
            auto klass = mClassLib->FindMaterialClassById(material_id);
            if (!klass)
            {
                WARN("No such tilemap layer material class found. [layer='%1', material='%2']", layer.GetClassName(), material_id);
                continue;
            }
            layer_material_node.material = gfx::CreateMaterialInstance(klass);
        }
        if (!layer_material_node.material)
            continue;

        auto& tiles = batch.tiles;
        tiles.SetTileWorldSize(batch.tile_size);
        tiles.SetTileRenderWidth(tile_width_render_units);
        tiles.SetTileRenderHeight(tile_height_render_units);
        tiles.SetTileShape(gfx::TileBatch::TileShape::Automatic);
        if (mCamera.perspective == game::Perspective::AxisAligned)
            tiles.SetProjection(gfx::TileBatch::Projection::AxisAligned);
        else if (mCamera.perspective == game::Perspective::Dimetric)
            tiles.SetProjection(gfx::TileBatch::Projection::Dimetric);
        else BUG("unknown projection");

        painter.Draw(tiles, tile_projection_transform_matrix, *layer_material_node.material);
    }
}

template<typename LayerType>
void Renderer::DrawDataLayer(const game::Tilemap& map,
                             const game::TilemapLayer& layer,
                             const game::URect& visible_region,
                             const game::FSize& tile_size,
                             gfx::Painter& painter)
{
    // Data render drawing is not optimized in any way and it doesn't need to be
    // since this is only for the visualization of data layer data during design.

    using TileType = typename LayerType::TileType;
    using LayerTraits = game::detail::TilemapLayerTraits<TileType>;

    const auto* ptr = game::TilemapLayerCast<LayerType>(&layer);

    const auto tile_row = visible_region.GetY();
    const auto tile_col = visible_region.GetX();
    const auto max_row  = visible_region.GetHeight();
    const auto max_col  = visible_region.GetWidth();

    // Setup painter for rendering in the screen coordinates
    const auto viewport_width  = mSurface.viewport.GetWidth();
    const auto viewport_height = mSurface.viewport.GetHeight();

    painter.SetPixelRatio({glm::vec2{1.0f, 1.0f}});
    painter.SetViewport(gfx::IRect(0, 0, viewport_width, viewport_height));
    painter.SetViewMatrix(CreateViewMatrix(mCamera.position, mCamera.scale, mCamera.perspective, mCamera.rotation));
    painter.SetProjectionMatrix(CreateProjectionMatrix(mCamera.perspective, mCamera.viewport));

    const auto tile_width = tile_size.GetWidth();
    const auto tile_height = tile_size.GetHeight();

    for (unsigned row=tile_row; row<max_row; ++row)
    {
        for (unsigned col=tile_col; col<max_col; ++col)
        {
            // the tiles index value is an index into the tile map
            // sprite palette.
            const auto value = NormalizeTileDataValue(ptr->GetTile(row, col));
            const auto r = value;
            const auto g = value;
            const auto b = value;
            const auto a = 1.0f;
            const gfx::Color4f color(r, g, b, a);
            gfx::Transform model_to_world;
            model_to_world.Scale(tile_size);
            model_to_world.MoveTo(tile_width * col, tile_height * row);
            painter.Draw(gfx::Rectangle(), model_to_world, gfx::CreateMaterialFromColor(color));
        }
    }
}

} // namespace

