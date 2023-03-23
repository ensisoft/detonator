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
#include "engine/classlib.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/types.h"
#include "game/tilemap.h"
#include "engine/renderer.h"
#include "engine/graphics.h"

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
    MapEntity<Entity, EntityNode>(entity, transform);

    std::vector<DrawPacket> packets;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
        {
            CreateDrawResources<Entity, EntityNode>(*paint);
            GenerateDrawPackets<Entity, EntityNode>(*paint, packets, hook);
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

void Renderer::Draw(const EntityClass& entity,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    EntityClassDrawHook* hook)
{

    MapEntity<EntityClass, EntityNodeClass>(entity, transform);

    std::vector<DrawPacket> packets;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
        {
            CreateDrawResources<EntityClass, EntityNodeClass>(*paint);
            GenerateDrawPackets<EntityClass, EntityNodeClass>(*paint, packets, hook);
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

void Renderer::Draw(const game::Tilemap& map,
                    const game::FRect& viewport,
                    gfx::Painter& painter,
                    gfx::Transform& transform)
{
    for (size_t i=0; i<map.GetNumLayers(); ++i)
    {
        const auto& layer = map.GetLayer(i);
        if (layer.IsVisible() && layer.HasRenderComponent() && layer.IsLoaded())
        {
            constexpr auto draw_render_layer = true;
            constexpr auto draw_data_layer = false;
            Draw(map, layer, viewport, painter, transform, i, draw_render_layer,  draw_data_layer);
        }
    }
}
void Renderer::Draw(const game::Tilemap& map,
                    const game::TilemapLayer& layer,
                    const game::FRect& viewport,
                    gfx::Painter& painter,
                    gfx::Transform& transform,
                    std::size_t layer_index,
                    bool draw_render_layer,
                    bool draw_data_layer)
{
    const auto tile_width = map.GetTileWidth();
    const auto tile_height = map.GetTileHeight();
    const auto layer_tile_width  = tile_width * layer.GetTileSizeScaler();
    const auto layer_tile_height = tile_height * layer.GetTileSizeScaler();

    const unsigned layer_width_tiles  = layer.GetWidth();
    const unsigned layer_height_tiles = layer.GetHeight();
    const float layer_width_units  = layer_width_tiles  * layer_tile_width;
    const float layer_height_units = layer_height_tiles * layer_tile_height;

    const auto& model_to_world = transform.GetAsMatrix();
    const auto& world_to_view  = painter.GetViewMatrix();
    const auto& model_to_view  = world_to_view * model_to_world;
    const auto& map_box  = game::FBox(model_to_view, layer_width_units, layer_height_units);
    const auto& map_rect = map_box.GetBoundingRect();
    const auto& view_intersection = Intersect(map_rect, viewport);

    const auto& map_area = TransformRect(view_intersection, glm::inverse(model_to_view));

    const unsigned tile_row = map_area.GetTopLeft().y / layer_tile_height;
    const unsigned tile_col = map_area.GetTopLeft().x / layer_tile_width;
    const unsigned num_tile_rows = map_area.GetHeight() / layer_tile_height + 1;
    const unsigned num_tile_cols = map_area.GetWidth()/ layer_tile_width + 1;

    const unsigned max_draw_cols = std::min(tile_col+num_tile_cols, layer_width_tiles);
    const unsigned max_draw_rows = std::min(tile_row+num_tile_rows, layer_height_tiles);
    const auto tile_rect = game::URect(tile_col, tile_row, max_draw_cols, max_draw_rows);
    const auto tile_size = game::FSize(layer_tile_width, layer_tile_height);

    const auto type = layer.GetType();
    if (draw_render_layer && layer->HasRenderComponent())
    {
        if (type == game::TilemapLayer::Type::Render)
            DrawRenderLayer<game::TilemapLayer_Render>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataUInt4)
            DrawRenderLayer<game::TilemapLayer_Render_DataUInt4>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataSInt4)
            DrawRenderLayer<game::TilemapLayer_Render_DataSInt4>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataSInt8)
            DrawRenderLayer<game::TilemapLayer_Render_DataSInt8>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataUInt8)
            DrawRenderLayer<game::TilemapLayer_Render_DataUInt8>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataUInt24)
            DrawRenderLayer<game::TilemapLayer_Render_DataUInt24>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else if (type == game::TilemapLayer::Type::Render_DataSInt24)
            DrawRenderLayer<game::TilemapLayer_Render_DataSInt24>(map, layer, tile_rect, tile_size, painter, transform, layer_index);
        else BUG("Unknown render layer type.");
    }
    if (draw_data_layer && layer->HasDataComponent())
    {
        if (type == game::TilemapLayer::Type::Render_DataUInt4)
            DrawDataLayer<game::TilemapLayer_Render_DataUInt4>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::Render_DataSInt4)
            DrawDataLayer<game::TilemapLayer_Render_DataSInt4>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::Render_DataSInt8)
            DrawDataLayer<game::TilemapLayer_Render_DataSInt8>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::Render_DataUInt8)
            DrawDataLayer<game::TilemapLayer_Render_DataUInt8>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::Render_DataUInt24)
            DrawDataLayer<game::TilemapLayer_Render_DataUInt24>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::Render_DataSInt24)
            DrawDataLayer<game::TilemapLayer_Render_DataSInt24>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::DataSInt8)
            DrawDataLayer<game::TilemapLayer_Data_SInt8>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::DataUInt8)
            DrawDataLayer<game::TilemapLayer_Data_UInt8>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::DataSInt16)
            DrawDataLayer<game::TilemapLayer_Data_SInt16>(map, layer, tile_rect, tile_size, painter, transform);
        else if (type == game::TilemapLayer::Type::DataUInt16)
            DrawDataLayer<game::TilemapLayer_Data_UInt16>(map, layer, tile_rect, tile_size, painter, transform);
        else BUG("Unknown data layer type.");
    }
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
        transform.Push(p.node_to_scene);

        // draw when there's no scene hook or when the scene hook returns
        // true for the filtering operation.
        const bool should_draw = !scene_hook || (scene_hook && scene_hook->FilterEntity(*p.entity_object, painter, transform));

        if (should_draw)
        {
            if (scene_hook)
                scene_hook->BeginDrawEntity(*p.entity_object, painter, transform);

            if (p.visual_entity && p.entity_object->TestFlag(EntityType::Flags::VisibleInGame))
                Draw(*p.visual_entity, painter, transform, entity_hook);

            if (scene_hook)
                scene_hook->EndDrawEntity(*p.entity_object, painter, transform);
        }

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
                transform.Rotate(paint_node.world_rotation);
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
    transform.Rotate(paint_node.world_rotation);
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
void Renderer::DrawRenderLayer(const game::Tilemap& map,
                               const game::TilemapLayer& layer,
                               const game::URect& tile_rect,
                               const game::FSize& tile_size,
                               gfx::Painter& painter,
                               gfx::Transform& transform,
                               std::size_t layer_index)
{
    using TileType = typename LayerType::TileType;
    using LayerTraits = game::detail::TilemapLayerTraits<TileType>;

    const auto* ptr = game::TilemapLayerCast<LayerType>(&layer);

    const auto tile_row = tile_rect.GetY();
    const auto tile_col = tile_rect.GetX();
    const auto max_row = tile_rect.GetHeight();
    const auto max_col = tile_rect.GetWidth();

    // batches of tiles are indexed by the tile material.
    std::vector<gfx::TileBatch> tiles;

    for (unsigned row=tile_row; row<max_row; ++row)
    {
        for (unsigned col=tile_col; col<max_col; ++col)
        {
            // the tiles index value is an index into the tile map
            // sprite palette.
            const auto index = ptr->GetTile(row, col).index;
            // special index max means "no value". this is needed in order
            // to be able to have "holes" in some layer and let the layer
            // below show through. could have used zero but that would
            // make the palette indexing more inconvenient.
            if (index == LayerTraits::MaxPaletteIndex)
                continue;

            gfx::TileBatch::Tile tile;
            tile.pos.x = col;
            tile.pos.y = row;

            if (index >= tiles.size())
                tiles.resize(index+1);

            tiles[index].AddTile(std::move(tile));
        }
    }

    if (layer_index >= mTilemapPalette.size())
        mTilemapPalette.resize(layer_index+1);

    auto& layer_palette = mTilemapPalette[layer_index];

    // the index functions as an index into the layer's
    // material layer palette.
    for (size_t i=0; i<tiles.size(); ++i)
    {
        // find the material ID for this index from the layer.
        const auto& material_id = layer.GetPaletteMaterialId(i);

        // allocate new tile map material node.
        if (i == layer_palette.size())
            layer_palette.resize(i+1);

        auto& layer_node = layer_palette[i];

        if (layer_node.material_id != material_id)
        {
            layer_node.material_id = material_id;
            layer_node.material.reset();
            if (layer_node.material_id.empty())
            {
                WARN("Tilemap has no material set for layer material palette index. [map='%1', layer='%2', index=%3]",
                     map.GetClassName(), layer.GetClassName(), i);
                continue;
            }
            auto klass = mClassLib->FindMaterialClassById(material_id);
            if (!klass)
            {
                WARN("No such tilemap material class found. [map='%1', layer='%2', class='%2']",
                     map.GetClassName(), layer.GetClassName(), material_id);
                continue;
            }
            layer_node.material = gfx::CreateMaterialInstance(klass);
        }
        if (!layer_node.material)
            continue;

        auto& batch = tiles[i];
        batch.SetTileWidth(tile_size.GetWidth());
        batch.SetTileHeight(tile_size.GetHeight());
        painter.Draw(batch, transform, *layer_node.material);
    }
}

template<typename LayerType>
void Renderer::DrawDataLayer(const game::Tilemap& map,
                             const game::TilemapLayer& layer,
                             const game::URect& tile_rect,
                             const game::FSize& tile_size,
                             gfx::Painter& painter,
                             gfx::Transform& transform)
{
    using TileType = typename LayerType::TileType;
    using LayerTraits = game::detail::TilemapLayerTraits<TileType>;

    const auto* ptr = game::TilemapLayerCast<LayerType>(&layer);

    const auto tile_row = tile_rect.GetY();
    const auto tile_col = tile_rect.GetX();
    const auto max_row = tile_rect.GetHeight();
    const auto max_col = tile_rect.GetWidth();

    // batches of tiles are indexed by the tile material.
    std::vector<gfx::TileBatch> tiles;

    for (unsigned row=tile_row; row<max_row; ++row)
    {
        for (unsigned col=tile_col; col<max_col; ++col)
        {
            // the tiles index value is an index into the tile map
            // sprite palette.
            const auto value = NormalizeValue(ptr->GetTile(row, col));
            const auto index = value * 255u;

            gfx::TileBatch::Tile tile;
            tile.pos.x = col;
            tile.pos.y = row;

            if (index >= tiles.size())
                tiles.resize(index+1);
            tiles[index].AddTile(std::move(tile));
        }
    }
    for (size_t i=0; i<tiles.size(); ++i)
    {
        auto& batch = tiles[i];
        batch.SetTileWidth(tile_size.GetWidth());
        batch.SetTileHeight(tile_size.GetHeight());

        const auto color_val = i / 255.0f;
        const auto r = color_val;
        const auto g = color_val;
        const auto b = color_val;
        const auto a = 1.0f;
        const gfx::Color4f color(r, g, b, a);
        painter.Draw(batch, transform, gfx::CreateMaterialFromColor(color));
    }
}

} // namespace

