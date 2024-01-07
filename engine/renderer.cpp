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
#include <limits>

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

void Renderer::CreateRenderStateFromScene(const game::Scene& scene)
{
    mPaintNodes.clear();

    const auto& nodes = scene.CollectNodes();

    gfx::Transform transform;

    for (const auto& p : nodes)
    {
        const Entity* entity = p.entity;
        if (!entity->HasRenderableItems())
            continue;
        transform.Push(p.node_to_scene);
            MapEntity<Entity, EntityNode>(*p.entity, transform);
        transform.Pop();
    }

}

void Renderer::UpdateRenderStateFromScene(const game::Scene& scene)
{
    const auto& nodes = scene.CollectNodes();

    gfx::Transform transform;

    for (const auto& p : nodes)
    {
        const Entity* entity = p.entity;

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
            MapEntity<Entity, EntityNode>(*p.entity, transform);
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

void Renderer::Draw(gfx::Device& device, const game::Tilemap* map)
{
    std::vector<DrawPacket> packets;

    if (map)
    {
        std::vector<TileBatch> batches;

        constexpr auto obey_klass_flags   = false;
        constexpr auto draw_render_layers = true;
        constexpr auto draw_data_layers   = false;
        constexpr auto use_tile_batching = true;
        PrepareMapTileBatches(*map, batches, draw_render_layers, draw_data_layers, obey_klass_flags, use_tile_batching);
        GenerateMapDrawPackets(*map, batches, packets);
    }

    const auto scene_packet_start_index = packets.size();

    // create the draw packets based on the scene's paint nodes.
    for (auto& [key, paint] : mPaintNodes)
    {
        CreateDrawResources<Entity, EntityNode>(paint);
        GenerateDrawPackets<Entity, EntityNode>(paint, packets, nullptr);
        paint.visited = true;
    }

    if (map)
    {
        // Compute map coordinates for all packets coming from scene rendering.
        // In other words, whenever a draw packet is generated from a scene's paint
        // node map that node to a position on the map tile grid for combining it
        // with the map's tile rendering.
        ComputeTileCoordinates(*map, scene_packet_start_index, packets);
        // Sort all packets based on the map based sorting criteria
        SortTilePackets(packets);
    }
    else
    {
        OffsetPacketLayers(packets);
    }

    DrawScenePackets(device, packets);
}

void Renderer::Draw(const Entity& entity,
                    gfx::Device& device, gfx::Transform& model,
                    EntityInstanceDrawHook* hook)
{
    DrawEntity<Entity, EntityNode>(entity, device, model, hook);
}

void Renderer::Draw(const EntityClass& entity,
                    gfx::Device& device, gfx::Transform& model,
                    EntityClassDrawHook* hook)
{
    DrawEntity<EntityClass, EntityNodeClass>(entity, device, model, hook);
}

void Renderer::Draw(const Scene& scene,
                    gfx::Device& device,
                    SceneInstanceDrawHook* scene_hook)
{
    DrawScene<Scene, Entity, Entity, EntityNode>(scene, nullptr, device, scene_hook);
}

void Renderer::Draw(const SceneClass& scene, const game::Tilemap* map,
                    gfx::Device& device,
                    SceneClassDrawHook* scene_hook)
{

    DrawScene<SceneClass, EntityPlacement, EntityClass, EntityNodeClass>(scene, map, device, scene_hook);
}

void Renderer::Draw(const game::Tilemap& map,
                    gfx::Device& device,
                    TileBatchDrawHook* hook,
                    bool draw_render_layer,
                    bool draw_data_layer)
{
    constexpr auto obey_klass_flags = true;
    constexpr auto use_tile_batching = false;

    std::vector<TileBatch> batches;
    PrepareMapTileBatches(map, batches, draw_render_layer, draw_data_layer, obey_klass_flags, use_tile_batching);

    std::vector<DrawPacket> packets;
    GenerateMapDrawPackets(map, batches,packets);

    SortTilePackets(packets);

    // put the data packets (indicated by editor) first
    std::stable_partition(packets.begin(), packets.end(),
                          [](const auto& packet) {
        return packet.domain == DrawPacket::Domain::Editor;
    });

    DrawTilemapPackets(device, packets, map, hook);
}

void Renderer::GenerateMapDrawPackets(const game::Tilemap& map,
                                      const std::vector<TileBatch>& batches,
                                      std::vector<DrawPacket>& packets) const
{
    const auto map_view = map.GetPerspective();
    const auto& map_view_to_clip    = CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport);
    const auto& map_world_to_view   = CreateModelViewMatrix(map_view, mCamera.position, mCamera.scale,
                                                            mCamera.rotation);
    const auto& scene_view_to_clip  = CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport);
    const auto& scene_world_to_view = CreateModelViewMatrix(GameView::AxisAligned, mCamera.position, mCamera.scale,
                                                            mCamera.rotation);

    // this matrix will transform coordinates from scene's coordinate space
    // into map coordinate space. but keep in mind that the scene world coordinate
    // is a coordinate in a 3D space not on the tile plane.
    const auto& from_map_to_scene = GetProjectionTransformMatrix(map_view_to_clip,
                                                                 map_world_to_view,
                                                                 scene_view_to_clip,
                                                                 scene_world_to_view);

    // Create draw packets out of tile batches
    for (auto& batch : batches)
    {
        if (batch.material == nullptr)
            continue;

        if (batch.type == TileBatch::Type::Render)
        {
            const auto tile_render_size = ComputeTileRenderSize(from_map_to_scene, batch.tile_size, map_view);
            const auto tile_render_width_scale = map->GetTileRenderWidthScale();
            const auto tile_render_height_scale = map->GetTileRenderHeightScale();
            const auto tile_width_render_units = tile_render_size.x * tile_render_width_scale;
            const auto tile_height_render_units = tile_render_size.y * tile_render_height_scale;

            auto tiles = std::make_unique<gfx::TileBatch>(std::move(batch.tiles));
            tiles->SetTileWorldSize(batch.tile_size);
            tiles->SetTileRenderWidth(tile_width_render_units);
            tiles->SetTileRenderHeight(tile_height_render_units);
            tiles->SetTileShape(gfx::TileBatch::TileShape::Automatic);
            if (map_view == game::Tilemap::Perspective::AxisAligned)
                tiles->SetProjection(gfx::TileBatch::Projection::AxisAligned);
            else if (map_view == game::Tilemap::Perspective::Dimetric)
                tiles->SetProjection(gfx::TileBatch::Projection::Dimetric);
            else BUG("unknown projection");

            DrawPacket packet;
            packet.source       = DrawPacket::Source::Map;
            packet.domain       = DrawPacket::Domain::Scene;
            packet.projection   = DrawPacket::Projection::Orthographic;
            packet.material     = batch.material;
            packet.drawable     = std::move(tiles);
            packet.transform    = from_map_to_scene;
            packet.map_row      = batch.row;
            packet.map_col      = batch.col;
            packet.map_layer    = batch.layer;
            packet.render_layer = 0;
            packet.packet_index = 0;
            packet.pass         = RenderPass::DrawColor;
            packets.push_back(std::move(packet));
        }
        else if (batch.type == TileBatch::Type::Data)
        {
            auto tiles = std::make_unique<gfx::TileBatch>(std::move(batch.tiles));
            tiles->SetTileWorldSize(batch.tile_size);
            tiles->SetTileRenderSize(batch.tile_size);
            tiles->SetTileShape(gfx::TileBatch::TileShape::Rectangle);
            tiles->SetProjection(gfx::TileBatch::Projection::AxisAligned);

            DrawPacket packet;
            packet.source       = DrawPacket::Source::Map;
            packet.domain       = DrawPacket::Domain::Editor;
            packet.projection   = DrawPacket::Projection::Orthographic;
            packet.material     = batch.material;
            packet.drawable     = std::move(tiles);
            packet.transform    = glm::mat4(1.0f);
            packet.map_row      = batch.row;
            packet.map_col      = batch.col;
            packet.map_layer    = batch.layer;
            packet.render_layer = 0;
            packet.packet_index = 0;
            packet.pass         = RenderPass::DrawColor;
            packets.push_back(std::move(packet));
        } else BUG("Unhandled tile batch type.");
    }
}

void Renderer::PrepareMapTileBatches(const game::Tilemap& map,
                                     std::vector<TileBatch>& batches,
                                     bool draw_render_layer,
                                     bool draw_data_layer,
                                     bool obey_klass_flags,
                                     bool use_batching)

{
    // The logical game world is mapped inside the device viewport
    // through projection and clip transformations. thus it should
    // be possible to map the viewport back to the world plane already
    // with MapFromWindowToWorldPlane.
    const auto device_viewport_width   = mSurface.viewport.GetWidth();
    const auto device_viewport_height  = mSurface.viewport.GetHeight();
    const auto window_size = glm::vec2{device_viewport_width, device_viewport_height};

    const auto map_view = map.GetPerspective();
    const auto& view_to_clip = CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport);
    const auto& world_to_view = CreateModelViewMatrix(map_view, mCamera.position, mCamera.scale, mCamera.rotation);

    // map the corners of the viewport onto the map plane.
    const auto corners = MapFromWindowToWorldPlane(view_to_clip, world_to_view, window_size,
                                                   {window_size * glm::vec2{0.0f, 0.0f}, // top left
                                                    window_size * glm::vec2{1.0f, 0.0f}, // top right
                                                    window_size * glm::vec2{0.0f, 1.0f}, // bottom left
                                                    window_size * glm::vec2{1.0f, 1.0f}  // bottom right
                                                   });
    // if the map has dimetric projection then simply taking the top left
    // and bottom right corners isn't enough. rather the top left gives the left,
    // top right gives the top, bottom left gives the bottom and bottom right gives the right.
    const auto left   = corners[0].x; // top left
    const auto top    = corners[1].y; // top right
    const auto bottom = corners[2].y; // bottom left
    const auto right  = corners[3].x; // bottom right
    const auto& top_left  = glm::vec2{left, top};
    const auto& bot_right = glm::vec2{right, bottom};

    for (unsigned layer_index=0; layer_index<map.GetNumLayers(); ++layer_index)
    {
        const auto& layer = map.GetLayer(layer_index);
        if (!layer.IsLoaded())
            continue;

        if (obey_klass_flags)
        {
            const auto& klass = layer.GetClass();
            if (!klass.TestFlag(game::TilemapLayerClass::Flags::VisibleInEditor) ||
                !klass.TestFlag(game::TilemapLayerClass::Flags::Visible))
                continue;
        }

        // these are the tile sizes in units
        const auto map_tile_width_units    = map.GetTileWidth();
        const auto map_tile_height_units   = map.GetTileHeight();
        const auto layer_tile_width_units  = map_tile_width_units * layer.GetTileSizeScaler();
        const auto layer_tile_height_units = map_tile_height_units * layer.GetTileSizeScaler();

        const unsigned layer_width_tiles  = layer.GetWidth();
        const unsigned layer_height_tiles = layer.GetHeight();
        const float layer_width_units  = layer_width_tiles  * layer_tile_width_units;
        const float layer_height_units = layer_height_tiles * layer_tile_height_units;

        const auto layer_min = glm::vec2{0.0f,  0.0f};
        const auto layer_max = glm::vec2{layer_width_units, layer_height_units};

        const auto top_left_xy  = glm::clamp(glm::vec2{top_left},  layer_min, layer_max);
        const auto bot_right_xy = glm::clamp(glm::vec2{bot_right}, layer_min, layer_max);

        const auto top_left_tile_col = math::clamp(0u, layer_width_tiles, (unsigned)(top_left_xy.x / layer_tile_width_units));
        const auto top_left_tile_row = math::clamp(0u, layer_height_tiles, (unsigned)(top_left_xy.y / layer_tile_height_units));
        const auto bot_right_tile_col = math::clamp(0u, layer_width_tiles, (unsigned)(bot_right_xy.x / layer_tile_width_units));
        const auto bot_right_tile_row = math::clamp(0u, layer_height_tiles, (unsigned)(bot_right_xy.y / layer_tile_height_units));

        const auto max_col = std::min(layer_width_tiles, bot_right_tile_col+1);
        const auto max_row = std::min(layer_height_tiles, bot_right_tile_row+1);
        const auto visible_region = URect(top_left_tile_col, top_left_tile_row, max_col, max_row);
        //DEBUG("top left  row = %1, col = %2,  max_rows = %3, max_cols = %4", top_left_tile_row, top_left_tile_col, max_row, max_col);

        const auto type = layer.GetType();
        if (draw_render_layer && layer->HasRenderComponent())
        {
            if (type == game::TilemapLayer::Type::Render)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataUInt4)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataUInt4>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt4)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataSInt4>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt8)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataSInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataUInt8)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataUInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataUInt24)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataUInt24>(map, layer,visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt24)
                PrepareRenderLayerTileBatches<game::TilemapLayer_Render_DataSInt24>(map, layer,visible_region, batches, layer_index, use_batching);
            else BUG("Unknown render layer type.");
        }

        if (draw_data_layer && layer->HasDataComponent())
        {
            if (type == game::TilemapLayer::Type::Render_DataUInt4)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataUInt4>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt4)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataSInt4>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt8)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataSInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataUInt8)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataUInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataUInt24)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataUInt24>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::Render_DataSInt24)
                PrepareDataLayerTileBatches<game::TilemapLayer_Render_DataSInt24>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::DataSInt8)
                PrepareDataLayerTileBatches<game::TilemapLayer_Data_SInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::DataUInt8)
                PrepareDataLayerTileBatches<game::TilemapLayer_Data_UInt8>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::DataSInt16)
                PrepareDataLayerTileBatches<game::TilemapLayer_Data_SInt16>(map, layer, visible_region, batches, layer_index, use_batching);
            else if (type == game::TilemapLayer::Type::DataUInt16)
                PrepareDataLayerTileBatches<game::TilemapLayer_Data_UInt16>(map, layer, visible_region, batches, layer_index, use_batching);
            else BUG("Unknown data layer type.");
        }
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
        const auto& node = scene.GetPlacement(i);
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
        const auto horizontal_flip = item->TestFlag(DrawableItemType::Flags::FlipHorizontally);
        const auto vertical_flip   = item->TestFlag(DrawableItemType::Flags::FlipVertically);
        const auto& shape = paint_node.item_drawable;
        const auto view = item->GetRenderView();
        const auto size = node.GetSize();
        const auto is3d = Is3DShape(*shape);

        if (view == game::RenderView::Dimetric)
        {
            transform.Push(CreateModelMatrix(GameView::AxisAligned));
            transform.Push(CreateModelMatrix(GameView::Dimetric));
        }

        if (is3d)
        {
            // model transform. keep in mind 3D only applies to the
            // renderable item (so it's visual only) thus the 3rd dimension
            // comes from the renderable item instead of the node!
            transform.Push();
                transform.RotateAroundX(gfx::FDegrees(180.0f));
                if (horizontal_flip)
                    transform.Scale(-1.0f, 1.0f);
                if (vertical_flip)
                    transform.Scale(1.0f, -1.0f);

                transform.Scale(size.x, size.y, item->GetDepth());
                transform.Rotate(item->GetRotator());
                transform.Translate(item->GetOffset());

        }
        else
        {
            // model transform.
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                if (horizontal_flip)
                    transform.Scale(-1.0f, 1.0f);
                if (vertical_flip)
                    transform.Scale(1.0f, -1.0f);

                transform.Scale(size);
                transform.Rotate(item->GetRotator());
                transform.Translate(item->GetOffset());
        }

        glm::mat4 world(1.0f);
        if (view == game::RenderView::Dimetric)
        {
            world =  CreateModelMatrix(GameView::Dimetric) *
                     CreateModelMatrix(GameView::AxisAligned);
        }

        const auto& model = transform.GetAsMatrix();
        gfx::Drawable::Environment env;
        env.model_matrix = &model;
        env.world_matrix = &world;
        // todo: other env matrices?

        const auto time_scale = item->GetTimeScale();
        if (item->TestFlag(DrawableItemType::Flags::UpdateDrawable))
            paint_node.item_drawable->Update(env, dt * time_scale);
        if (item->TestFlag(DrawableItemType::Flags::RestartDrawable) && !paint_node.item_drawable->IsAlive())
            paint_node.item_drawable->Restart(env);

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            for (size_t i=0; i<item->GetNumCommands(); ++i)
            {
                const auto& src_cmd = item->GetCommand(i);
                gfx::Drawable::Command gfx_cmd;
                gfx_cmd.name = src_cmd.name;
                gfx_cmd.args = src_cmd.args;
                paint_node.item_drawable->Execute(env, gfx_cmd);
            }
            item->ClearCommands();
        }

        // pop model transform.
        transform.Pop();

        if (view == game::RenderView::Dimetric)
        {
            // pop view based model transform
            transform.Pop();
            transform.Pop();
        }
    }

    if (text && paint_node.text_material)
    {
        paint_node.text_material->Update(dt);
    }
    if (text && paint_node.text_drawable)
    {
        // push the actual model transform
        transform.Push(node.GetModelTransform());

        const auto& model = transform.GetAsMatrix();
        gfx::Drawable::Environment env;
        env.model_matrix = &model;
        // todo: other env matrices?

        paint_node.text_drawable->Update(env, dt);

        // pop model transform
        transform.Pop();
    }
}

template<typename SceneType,typename SceneNodeType,
         typename EntityType, typename EntityNodeType>
void Renderer::DrawScene(const SceneType& scene, const game::Tilemap* map,
                         gfx::Device& device,
                         SceneDrawHook<SceneNodeType>* scene_hook)
{

    // When we're combining the map with a scene everything that is to be drawn
    // has to live in the same space and understand "depth" the same way.
    // Current implementation already uses scene layering so we're going to do
    // the same here and leverage the scene layer index value instead of using
    // floating points. Other option could be compute a floating distance from
    // camera and then sort the draw packets based on distance. It'd just mean
    // that scene layering would not work the same way regarding stencil masking
    // and how entities that map to the same scene layer interact when rendered.
    // I.e. each scene layer has the same render passes, mask cover/expose, color.
    // Layer Indexing is essentially the same thing but uses distinct buckets
    // instead of a continuous range.
    //
    // To compute the render layer index there could be two options (as far as I
    // can think of right now...)
    //
    // a) Use the relative height of objects in the clip space.
    // Project both entities and tiles from their own coordinate spaces into clip
    // space and use the clip space height value to to separate objects into different
    // render layer buckets.
    //
    //
    // b) Use the tilemap row index and render from back to front. Since rows grow towards
    // the "screen surface" and bigger row indices are closer to the viewer the row index
    // should map directly to a layer index. Then each entity needs to be mapped from
    // entity world into tilemap and xy tile position computed.

    std::vector<DrawPacket> packets;

    if (map)
    {
        std::vector<TileBatch> batches;

        constexpr auto obey_klass_flags   = false;
        constexpr auto draw_render_layers = true;
        constexpr auto draw_data_layers   = false;
        constexpr auto use_tile_batching = true;
        PrepareMapTileBatches(*map, batches, draw_render_layers, draw_data_layers, obey_klass_flags, use_tile_batching);
        GenerateMapDrawPackets(*map, batches, packets);
    }

    const auto& nodes = scene.CollectNodes();

    for (const auto& node : nodes)
    {
        // When we're drawing SceneClass object the placement is a EntityPlacement*
        // When we're drawing a Scene object the placement is n Entity* object
        auto placement = node.placement;
        // When we're drawing SceneClass object entity is shared_ptr<const EntityClass>
        auto entity = node.entity;

        // draw when there's no scene hook or when the scene hook returns
        // true for the filtering operation.
        const bool should_draw = !scene_hook || (scene_hook && scene_hook->FilterEntity(*placement));
        if (!should_draw)
            continue;

        if (scene_hook)
            scene_hook->BeginDrawEntity(*placement);

        // this is the model, aka model_to_world transform
        gfx::Transform transform(node.node_to_scene);

        if (placement->TestFlag(EntityType::Flags::VisibleInGame))
        {
            MapEntity<EntityType, EntityNodeType>(*entity, transform);

            std::vector<DrawPacket> entity_packets;

            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);
                if (auto* paint = base::SafeFind(mPaintNodes, node.GetId()))
                {
                    CreateDrawResources<EntityType, EntityNodeType>(*paint);
                    GenerateDrawPackets<EntityType, EntityNodeType>(*paint, entity_packets, nullptr /*entity hook */);
                }
            }
            // generate draw packets uses the entity to ask for the scene layer index.
            // When rendering a SceneClass the entity class doesn't have the layer index
            // (but only a stub function) and the real layer information is in the placement.
            for (auto& packet: entity_packets)
            {
                packet.render_layer = placement->GetLayer();
            }

            // Compute tile coordinates for each draw packet created by the entity.
            if (map)
            {
                ComputeTileCoordinates(*map, 0, entity_packets);
            }

            if (scene_hook)
            {
                for (auto& packet : entity_packets)
                {
                    if (scene_hook->InspectPacket(*placement, packet))
                        packets.push_back(packet);
                }

            }
            else base::AppendVector(packets, entity_packets);

        } // visible placement

        if (scene_hook)
        {
            scene_hook->AppendPackets(*placement, transform, packets);
            scene_hook->EndDrawEntity(*placement);
        }
    }

    if (map)
    {
        SortTilePackets(packets);
    }
    else
    {
        OffsetPacketLayers(packets);
    }

    DrawScenePackets(device, packets);
    DrawEditorPackets(device, packets);
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
                          gfx::Device& device,
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

    OffsetPacketLayers(packets);
    DrawScenePackets(device, packets);
    DrawEditorPackets(device, packets);
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
                WARN("No such drawable class found. [drawable='%1', entity='%2', node='%3']", drawable, entity.GetName(), node.GetName());
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
            if (paint_node.item_drawable->GetType() == gfx::Drawable::Type::SimpleShape)
            {
                auto simple = std::static_pointer_cast<gfx::SimpleShapeInstance>(paint_node.item_drawable);

                gfx::SimpleShapeStyle style;
                if (item->GetRenderStyle() == RenderStyle::Solid)
                    style = gfx::SimpleShapeStyle::Solid;
                else if (item->GetRenderStyle() == RenderStyle::Outline)
                    style = gfx::SimpleShapeStyle::Outline;
                else BUG("Unsupported rendering style.");

                simple->SetStyle(style);
            }
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

    glm::vec2 sort_point = {0.5f, 1.0f};
    if (const auto* map = node.GetMapNode())
        sort_point = map->GetSortPoint();

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
            // push the actual model transform
            transform.Push(node.GetModelTransform());

            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, text->TestFlag(TextItemClass::Flags::PP_EnableBloom));
            packet.source       = DrawPacket::Source::Scene;
            packet.domain       = DrawPacket::Domain::Scene;
            packet.drawable     = paint_node.text_drawable;
            packet.material     = paint_node.text_material;
            packet.transform    = transform;
            packet.sort_point   = sort_point;
            packet.pass         = RenderPass::DrawColor;
            packet.packet_index = text->GetLayer();
            packet.render_layer = entity.GetLayer();
            if (!hook || hook->InspectPacket(&node, packet))
                packets.push_back(std::move(packet));

            // pop model transform
            transform.Pop();
        }
    }

    if (const auto* item = node.GetDrawable())
    {
        const auto horizontal_flip = item->TestFlag(DrawableItemType::Flags::FlipHorizontally);
        const auto vertical_flip   = item->TestFlag(DrawableItemType::Flags::FlipVertically);
        const auto double_sided    = item->TestFlag(DrawableItemType::Flags::DoubleSided);
        const auto depth_test      = item->TestFlag(DrawableItemType::Flags::DepthTest);
        const auto& shape = paint_node.item_drawable;
        const auto size = node.GetSize();
        const auto is3d = Is3DShape(*shape);
        const auto view = item->GetRenderView();

        if (view == game::RenderView::Dimetric)
        {
            transform.Push(CreateModelMatrix(GameView::AxisAligned));
            transform.Push(CreateModelMatrix(GameView::Dimetric));
        }

        // The 2D and 3D shapes are different so that the 2D shapes are laid out in
        // the XY quadrant and one corner is aligned with local coordinate space origin,
        // but the 3D shapes are centered around the local origin.
        //
        // Entity node's model transform then assumes a 2D shape and does a small
        // translation step in order to center the shape's vertices around its local
        // origin. But when the shape is a 3D shape this step is actually wrong.
        //
        // Probably best way to fix this would be to make all the shapes consistent
        // and then fix the 2D drawing to do an offset adjustment to get simple 2D
        // drawing coordinates.
        if (is3d)
        {
            // model transform. keep in mind 3D only applies to the
            // renderable item (so it's visual only) thus the 3rd dimension
            // comes from the renderable item instead of the node!
            transform.Push();
               transform.RotateAroundX(gfx::FDegrees(180.0f));
               if (horizontal_flip)
                   transform.Scale(-1.0f, 1.0f);
               if (vertical_flip)
                   transform.Scale(1.0f, -1.0f);

               transform.Scale(size.x, size.y, item->GetDepth());
               transform.Rotate(item->GetRotator());
               transform.Translate(item->GetOffset());
        }
        else
        {
            // model transform.
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                if (horizontal_flip)
                    transform.Scale(-1.0f, 1.0f);
                if (vertical_flip)
                    transform.Scale(1.0f, -1.0f);

                transform.Scale(size);
                transform.Rotate(item->GetRotator());
                transform.Translate(item->GetOffset());
        }

        // if it doesn't render then no draw packets are generated
        if (item->TestFlag(DrawableItemType::Flags::VisibleInGame) && entity_visible)
        {
            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, item->TestFlag(DrawableItemType::Flags::PP_EnableBloom));
            packet.source       = DrawPacket::Source::Scene;
            packet.domain       = DrawPacket::Domain::Scene;
            packet.culling      = DrawPacket::Culling::Back;
            packet.depth_test   = DrawPacket::DepthTest::Disabled;
            packet.material     = paint_node.item_material;
            packet.drawable     = paint_node.item_drawable;
            packet.transform    = transform;
            packet.sort_point   = sort_point;
            packet.render_layer = entity.GetLayer();
            packet.pass         = item->GetRenderPass();
            packet.projection   = item->GetRenderProjection();
            packet.packet_index = item->GetLayer();
            packet.line_width   = item->GetLineWidth();

            if (mEditingMode)
            {
                if (mStyle == RenderingStyle::Wireframe &&
                    packet.drawable->GetPrimitive() == gfx::Drawable::Primitive::Triangles)
                {
                    static auto wireframe_color = std::make_shared<gfx::MaterialInstance>(
                            gfx::CreateMaterialClassFromColor(gfx::Color::Gray));
                    packet.drawable = std::make_shared<gfx::WireframeInstance>(packet.drawable);
                    packet.material = wireframe_color;
                }
            }

            if (double_sided)
                packet.culling = DrawPacket::Culling::None;
            else if (horizontal_flip ^ vertical_flip)
                packet.culling = DrawPacket::Culling::Front;

            if (depth_test)
                packet.depth_test = DrawPacket::DepthTest::LessOrEQual;

            if (!hook || hook->InspectPacket(&node , packet))
                packets.push_back(std::move(packet));
        }

        // pop model transform
        transform.Pop();

        if (view == game::RenderView::Dimetric)
        {
            // pop view based model transform
            transform.Pop();
            transform.Pop();
        }
    }

    // append any extra packets if needed.
    if (hook)
    {
        // Allow the draw hook to append any extra draw packets for this node.
        hook->AppendPackets(&node, transform, packets);
    }
}

void Renderer::OffsetPacketLayers(std::vector<DrawPacket>& packets) const
{
    // the layer values can be negative but for the bucket sorting,
    // sorting packets into layers the indices must be positive.
    int32_t first_packet_index = std::numeric_limits<int>::max();
    int32_t first_render_layer = std::numeric_limits<int>::max();
    for (auto &packet : packets)
    {
        first_packet_index = std::min(first_packet_index, packet.packet_index);
        first_render_layer = std::min(first_render_layer, packet.render_layer);
    }
    // offset the layers.
    for (auto &packet : packets)
    {
        packet.packet_index -= first_packet_index;
        packet.render_layer -= first_render_layer;
        ASSERT(packet.packet_index >= 0 && packet.render_layer >= 0);
    }
}
void Renderer::DrawTilemapPackets(gfx::Device& device,
                                  const std::vector<DrawPacket>& packets,
                                  const game::Tilemap& map,TileBatchDrawHook* hook) const
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

    const auto window_size = glm::vec2{mSurface.viewport.GetWidth(), mSurface.viewport.GetHeight()};
    const auto logical_viewport_width = mCamera.viewport.GetWidth();
    const auto logical_viewport_height = mCamera.viewport.GetHeight();
    const auto& orthographic = CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport);

    gfx::Painter scene_painter(&device);
    scene_painter.SetProjectionMatrix(orthographic);
    scene_painter.SetViewMatrix(CreateModelViewMatrix(GameView::AxisAligned, mCamera.position, mCamera.scale, mCamera.rotation));
    scene_painter.SetViewport(mSurface.viewport);
    scene_painter.SetSurfaceSize(mSurface.size);
    scene_painter.SetEditingMode(mEditingMode);
    scene_painter.SetPixelRatio(window_size / glm::vec2{logical_viewport_width, logical_viewport_height} * mCamera.scale);

    // Setup painter to draw in whatever is the map perspective.
    gfx::Painter tile_painter(&device);
    tile_painter.SetProjectionMatrix(orthographic);
    tile_painter.SetViewMatrix(CreateModelViewMatrix(map.GetPerspective(), mCamera.position, mCamera.scale, mCamera.rotation));
    tile_painter.SetPixelRatio({1.0f, 1.0f});
    tile_painter.SetViewport(mSurface.viewport);
    tile_painter.SetSurfaceSize(mSurface.size);

    for (const auto& packet : packets)
    {
        if (packet.source != DrawPacket::Source::Map)
            continue;

        if (hook && !hook->FilterBatch(packet))
            continue;

        gfx::Painter* painter = nullptr;
        if (packet.domain == DrawPacket::Domain::Scene)
            painter = &scene_painter;
        else if (packet.domain == DrawPacket::Domain::Editor)
            painter = &tile_painter;
        else BUG("Unhandled draw packet domain.");

        if (hook)
            hook->BeginDrawBatch(packet, *painter);

        painter->Draw(*packet.drawable, packet.transform, *packet.material);

        if (hook)
            hook->EndDrawBatch(packet, *painter);
    }
}

void Renderer::DrawEditorPackets(gfx::Device& device, const std::vector<DrawPacket>& packets) const
{
    const auto window_size = glm::vec2{mSurface.viewport.GetWidth(), mSurface.viewport.GetHeight()};
    const auto logical_viewport_width = mCamera.viewport.GetWidth();
    const auto logical_viewport_height = mCamera.viewport.GetHeight();

    gfx::Painter painter(&device);
    painter.SetProjectionMatrix(CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport));
    painter.SetViewMatrix(CreateModelViewMatrix(GameView::AxisAligned, mCamera.position, mCamera.scale, mCamera.rotation));
    painter.SetViewport(mSurface.viewport);
    painter.SetSurfaceSize(mSurface.size);
    painter.SetEditingMode(mEditingMode);
    painter.SetPixelRatio(window_size / glm::vec2{logical_viewport_width, logical_viewport_height} * mCamera.scale);

    for (const auto& packet : packets)
    {
        if (packet.domain == DrawPacket::Domain::Editor)
            painter.Draw(*packet.drawable, packet.transform, *packet.material);
    }
}

void Renderer::DrawScenePackets(gfx::Device& device, std::vector<DrawPacket>& packets) const
{
    const auto window_size = glm::vec2{mSurface.viewport.GetWidth(), mSurface.viewport.GetHeight()};
    const auto logical_viewport_width = mCamera.viewport.GetWidth();
    const auto logical_viewport_height = mCamera.viewport.GetHeight();

    const auto& model_view = CreateModelViewMatrix(GameView::AxisAligned, mCamera.position, mCamera.scale, mCamera.rotation);
    const auto& orthographic = CreateProjectionMatrix(Projection::Orthographic, mCamera.viewport);
    const auto& perspective  = CreateProjectionMatrix(Projection::Perspective, mCamera.viewport);

    gfx::Painter painter(&device);
    painter.SetViewport(mSurface.viewport);
    painter.SetSurfaceSize(mSurface.size);
    painter.SetEditingMode(mEditingMode);
    painter.SetPixelRatio(window_size / glm::vec2{logical_viewport_width, logical_viewport_height} * mCamera.scale);

    // Each entity in the scene is assigned to a scene/entity layer and each
    // entity node within an entity is assigned to an entity layer.
    // Thus, to have the right ordering both indices of each
    // render packet must be considered!
    std::vector<std::vector<RenderLayer>> layers;

    for (auto& packet : packets)
    {
        if (!packet.material || !packet.drawable)
            continue;
        else if (packet.domain != DrawPacket::Domain::Scene)
            continue;

        const glm::mat4* projection = nullptr;
        if (packet.projection == DrawPacket::Projection::Orthographic)
            projection = &orthographic;
        else if (packet.projection == DrawPacket::Projection::Perspective)
            projection = &perspective;
        else BUG("Missing draw packet projection mapping.");

        if (CullDrawPacket(packet, *projection, model_view))
        {
            packet.flags.set(DrawPacket::Flags::CullPacket, true);
            //DEBUG("culled packet %1", packet.name);
        }

        if (mPacketFilter && !mPacketFilter->InspectPacket(packet))
            continue;

        if (packet.flags.test(DrawPacket::Flags::CullPacket))
            continue;

        gfx::Painter::DrawCommand draw;
        draw.user               = (void*)&packet;
        draw.model              = &packet.transform;
        draw.drawable           = packet.drawable.get();
        draw.material           = packet.material.get();
        draw.state.culling      = packet.culling;
        draw.state.line_width   = packet.line_width;
        draw.state.depth_test   = packet.depth_test;
        draw.state.write_color  = true;
        draw.state.stencil_func = gfx::Painter::StencilFunc ::Disabled;
        draw.view               = &model_view;
        draw.projection         = projection;
        painter.Prime(draw);

        const auto render_layer_index = packet.render_layer;
        if (render_layer_index >= layers.size())
            layers.resize(render_layer_index + 1);

        auto& render_layer = layers[render_layer_index];

        const auto packet_index = packet.packet_index;
        if (packet_index >= render_layer.size())
            render_layer.resize(packet_index + 1);

        RenderLayer& layer = render_layer[packet_index];
        if (packet.pass == RenderPass::DrawColor)
            layer.draw_color_list.push_back(draw);
        else if (packet.pass == RenderPass::MaskCover)
            layer.mask_cover_list.push_back(draw);
        else if (packet.pass == RenderPass::MaskExpose)
            layer.mask_expose_list.push_back(draw);
        else BUG("Missing packet render pass mapping.");
    }

    // set the state for each draw packet.
    for (auto& scene_layer : layers)
    {
        for (auto& entity_layer : scene_layer)
        {
            const auto needs_stencil = !entity_layer.mask_cover_list.empty() ||
                                       !entity_layer.mask_expose_list.empty();
            if (needs_stencil)
            {
                for (auto& draw : entity_layer.mask_expose_list)
                {
                    draw.state.write_color   = false;
                    draw.state.stencil_ref   = 1;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
                }
                for (auto& draw : entity_layer.mask_cover_list)
                {
                    draw.state.write_color   = false;
                    draw.state.stencil_ref   = 0;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::WriteRef;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::PassAlways;
                }
                for (auto& draw : entity_layer.draw_color_list)
                {
                    draw.state.write_color   = true;
                    draw.state.stencil_ref   = 1;
                    draw.state.stencil_mask  = 0xff;
                    draw.state.stencil_func  = gfx::Painter::StencilFunc::RefIsEqual;
                    draw.state.stencil_dpass = gfx::Painter::StencilOp::DontModify;
                    draw.state.stencil_dfail = gfx::Painter::StencilOp::DontModify;
                }
            }
        }
    }

    const gfx::Color4f bloom_color(mBloom.red, mBloom.green, mBloom.blue, 1.0f);
    const BloomPass bloom(mRendererName,  bloom_color, mBloom.threshold, painter);

    if (mEditingMode)
    {
        if (mStyle == RenderingStyle::Normal)
        {
            if (IsEnabled(Effects::Bloom))
                bloom.Draw(layers);
        }
    }
    else
    {
        if (IsEnabled(Effects::Bloom))
            bloom.Draw(layers);
    }

    MainRenderPass main(painter);
    main.Draw(layers);
    main.Composite(IsEnabled(Effects::Bloom) ? &bloom : nullptr);

}

template<typename LayerType>
void Renderer::PrepareRenderLayerTileBatches(const game::Tilemap& map,
                                             const game::TilemapLayer& layer,
                                             const game::URect& visible_region,
                                             std::vector<TileBatch>& batches,
                                             std::uint16_t layer_index,
                                             bool use_batching)

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
    const auto map_tile_depth_units    = map.GetTileDepth();
    const auto layer_tile_width_units  = map_tile_width_units * layer.GetTileSizeScaler();
    const auto layer_tile_height_units = map_tile_height_units * layer.GetTileSizeScaler();
    const auto layer_tile_depth_units  = map_tile_depth_units * layer.GetTileSizeScaler();

    const auto cuboid_scale_factors = GetTileCuboidFactors(map.GetPerspective());
    const auto layer_tile_size = glm::vec3 { map_tile_width_units * cuboid_scale_factors.x,
                                             map_tile_height_units * cuboid_scale_factors.y,
                                             map_tile_depth_units * cuboid_scale_factors.z };

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
                batch.material  = GetTileMaterial(map, layer_index, material_index);
                batch.layer     = layer_index;
                batch.depth     = layer.GetDepth();
                batch.row       = row;
                batch.col       = col;
                batch.tile_size = layer_tile_size;
                batch.type      = TileBatch::Type::Render;
            }

            // append to tile to the current batch
            auto& batch = batches.back();
            gfx::TileBatch::Tile tile;
            tile.pos.x = col;
            tile.pos.y = row;
            tile.pos.z = layer.GetDepth();
            batch.tiles.push_back(tile);

            // keep track of the last material used.
            last_material_index  = use_batching ? material_index : LayerTraits::MaxPaletteIndex;
        }
    }
}

template<typename LayerType>
void Renderer::PrepareDataLayerTileBatches(const game::Tilemap& map,
                                           const game::TilemapLayer& layer,
                                           const game::URect& visible_region,
                                           std::vector<TileBatch>& batches,
                                           std::uint16_t layer_index,
                                           bool use_batching)
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

    // these are the tile sizes in units
    const auto map_tile_width_units    = map.GetTileWidth();
    const auto map_tile_height_units   = map.GetTileHeight();
    const auto map_tile_depth_units    = map.GetTileDepth();
    const auto layer_tile_width_units  = map_tile_width_units * layer.GetTileSizeScaler();
    const auto layer_tile_height_units = map_tile_height_units * layer.GetTileSizeScaler();
    const auto layer_tile_depth_units  = map_tile_depth_units * layer.GetTileSizeScaler();

    const auto cuboid_scale_factors = GetTileCuboidFactors(map.GetPerspective());
    const auto layer_tile_size = glm::vec3 { map_tile_width_units * cuboid_scale_factors.x,
                                             map_tile_height_units * cuboid_scale_factors.y,
                                             map_tile_depth_units * cuboid_scale_factors.y };

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

            gfx::TileBatch::Tile tile;
            tile.pos.x = col;
            tile.pos.y = row;
            tile.pos.z = layer.GetDepth();

            TileBatch batch;
            batch.tiles.push_back(tile);
            batch.material  = gfx::CreateMaterialInstance(gfx::CreateMaterialClassFromColor(color));
            batch.depth     = layer.GetDepth();
            batch.row       = row;
            batch.col       = col;
            batch.layer     = layer_index;
            batch.type      = TileBatch::Type::Data;
            batch.tile_size = layer_tile_size;
            batches.push_back(std::move(batch));
        }
    }
}

void Renderer::SortTilePackets(std::vector<DrawPacket>& packets) const
{
    std::sort(packets.begin(), packets.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.map_row < rhs.map_row)
            return true;
        else if (lhs.map_row == rhs.map_row)
        {
            if (lhs.map_col < rhs.map_col)
                return true;
            else if (lhs.map_col == rhs.map_col)
                return lhs.map_layer < rhs.map_layer;
        }
        return false;
    });

    // assign draw packets to render layers while keeping the relative ordering the same

    uint32_t current_render_layer = 0;
    uint32_t current_row = 0;
    uint32_t current_col = 0;
    for (uint32_t i=0; i<packets.size(); ++i)
    {
        // if the row or the column changes start a new render layer
        if (packets[i].map_row != current_row || packets[i].map_col != current_col)
            ++current_render_layer;

        packets[i].render_layer = current_render_layer;
        current_row = packets[i].map_row;
        current_col = packets[i].map_col;
    }
}

bool Renderer::CullDrawPacket(const DrawPacket& packet, const glm::mat4& projection, const glm::mat4& modelview) const
{
    // the draw packets for the map are only generated for the visible part
    // of the map already. so culling check is not needed.
    if (packet.source == DrawPacket::Source::Map)
        return false;

    const auto& shape = packet.drawable;

    // don't cull global particle engines since the particles can be "where-ever"
    if (shape->GetType() == gfx::Drawable::Type::ParticleEngine)
    {
        const auto& particle = std::static_pointer_cast<const gfx::ParticleEngineInstance>(shape);
        const auto& params   = particle->GetParams();
        if (params.coordinate_space == gfx::ParticleEngineClass::CoordinateSpace::Global)
            return false;
    }

    // take the model view bounding box (which we should probably get from the drawable)
    // and project all the 6 corners on the rendering plane. cull the packet if it's
    // outside the NDC the X, Y axis.
    std::vector<glm::vec4> corners;

    if (Is3DShape(*shape))
    {
        corners.resize(8);
        corners[0] = glm::vec4 { -0.5f,  0.5f, 0.5f, 1.0f };
        corners[1] = glm::vec4 { -0.5f, -0.5f, 0.5f, 1.0f };
        corners[2] = glm::vec4 {  0.5f,  0.5f, 0.5f, 1.0f };
        corners[3] = glm::vec4 {  0.5f, -0.5f, 0.5f, 1.0f };
        corners[4] = glm::vec4 { -0.5f,  0.5f, -0.5f, 1.0f };
        corners[5] = glm::vec4 { -0.5f, -0.5f, -0.5f, 1.0f };
        corners[6] = glm::vec4 {  0.5f,  0.5f, -0.5f, 1.0f };
        corners[7] = glm::vec4 {  0.5f, -0.5f, -0.5f, 1.0f };
    }
    else
    {
        // regarding the Y value, remember the complication
        // in the 2D vertex shader. huhu.. should really fix
        // this one soon...
        corners.resize(4);
        corners[0] = glm::vec4 { 0.0f,  0.0f, 0.0f, 1.0f };
        corners[1] = glm::vec4 { 0.0f,  1.0f, 0.0f, 1.0f };
        corners[2] = glm::vec4 { 1.0f,  1.0f, 0.0f, 1.0f };
        corners[3] = glm::vec4 { 1.0f,  0.0f, 0.0f, 1.0f };
    }

    const auto& transform = projection * modelview;

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    for (const auto& p0 : corners)
    {
        auto p1 = transform * packet.transform * p0;
        p1 /= p1.w;
        min_x = std::min(min_x, p1.x);
        max_x = std::max(max_x, p1.x);
        min_y = std::min(min_y, p1.y);
        max_y = std::max(max_y, p1.y);
    }
    // completely above or below the NDC
    if (max_y < -1.0f || min_y > 1.0f)
        return true;

    // completely to the left of to the right of the NDC
    if (max_x < -1.0f || min_x > 1.0f)
        return true;

    return false;

}

void Renderer::ComputeTileCoordinates(const game::Tilemap& map,
                                      std::size_t packet_start_index,
                                      std::vector<DrawPacket>& packets) const
{

    const auto map_view = map.GetPerspective();
    const unsigned map_width  = map->GetMapWidth(); // tiles
    const unsigned map_height = map->GetMapHeight(); // tiles
    const auto tile_width_units   = map->GetTileWidth();
    const auto tile_height_units  = map->GetTileHeight();

    for (size_t i=packet_start_index; i<packets.size(); ++i)
    {
        auto& packet = packets[i];

        // take a model space coordinate and transform by the packet's model transform
        // into world space in the scene.
        const auto scene_world_pos =  packet.transform * glm::vec4{packet.sort_point, 0.0f, 1.0f}; 
        //DEBUG("Scene pos = %1", scene_world_pos);
        // map the scene world pos to a tilemap plane position.
        const auto map_plane_pos = MapFromScenePlaneToTilePlane(scene_world_pos, map_view);
        const auto map_plane_pos_xy = glm::vec2{math::clamp(0.0f, map_width * tile_width_units, map_plane_pos.x),
                                                math::clamp(0.0f, map_height * tile_height_units, map_plane_pos.y)};
        const uint32_t map_row = math::clamp(0u, map_height-1, (unsigned)(map_plane_pos_xy.y / tile_height_units));
        const uint32_t map_col = math::clamp(0u, map_width-1,  (unsigned)(map_plane_pos_xy.x / tile_width_units));
        ASSERT(map_row < map_height && map_col < map_width);
        packet.map_row   = map_row;
        packet.map_col   = map_col;
        packet.map_layer = std::max(0, packet.render_layer);
        //DEBUG("map pos = %1", map_plane_pos);
        //DEBUG("map row = %1, col = %2", map_row, map_col);
    }
}


std::shared_ptr<const gfx::Material> Renderer::GetTileMaterial(const game::Tilemap& map, std::uint16_t layer_index, std::uint16_t material_index)
{
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
    const auto& material = layer.GetPaletteMaterialId(material_index);

    // create the material instances if the material Id has changed.
    if (layer_material_node.material_id != material)
    {
        layer_material_node.material_id = material;
        layer_material_node.material.reset();
        if (layer_material_node.material_id.empty())
        {
            WARN("Tilemap layer has no material ID set for material palette index. [layer='%1', index=%2]", layer.GetClassName(), material_index);
            return nullptr;
        }
        auto klass = mClassLib->FindMaterialClassById(material);
        if (!klass)
        {
            WARN("No such tilemap layer material class found. [layer='%1', material='%2']", layer.GetClassName(), material);
            return nullptr;
        }
        layer_material_node.material = gfx::CreateMaterialInstance(klass);
    }
    return layer_material_node.material;
}

} // namespace

