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
#include "graphics/simple_shape.h"
#include "graphics/particle_engine.h"
#include "graphics/tilebatch.h"
#include "graphics/debug_drawable.h"
#include "graphics/effect_drawable.h"
#include "graphics/text_material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "engine/classlib.h"
#include "game/entity.h"
#include "game/entity_class.h"
#include "game/scene.h"
#include "game/scene_class.h"
#include "game/types.h"
#include "game/tilemap.h"
#include "game/tilemap_layer_base.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_tilemap_node.h"
#include "game/entity_node_light.h"
#include "game/entity_node_mesh_effect.h"
#include "engine/renderer.h"
#include "engine/graphics.h"
#include "engine/camera.h"

using namespace game;

namespace engine
{

Renderer::Renderer(const ClassLibrary* classlib)
  : mClassLib(classlib)
{
}

void Renderer::CreateRendererState(const game::Scene& scene, const game::Tilemap* map)
{
    std::lock_guard<std::mutex> lock(mRendererLock);

    mPaintNodes.clear();

    const auto& nodes = scene.CollectNodes();

    for (const auto& p : nodes)
    {
        const Entity* entity = p.entity;
        if (!entity->HasRenderableItems() && !entity->HasLights())
            continue;

        gfx::Transform transform(p.node_to_scene);
        CreatePaintNodes<Entity, EntityNode>(*p.entity, transform);
    }
}

void Renderer::UpdateRendererState(const game::Scene& scene, const game::Tilemap* map)
{
    std::lock_guard<std::mutex> lock(mRendererLock);

    const auto& nodes = scene.CollectNodes();

    for (const auto& node : nodes)
    {
        const Entity* entity = node.entity;
        if (!entity->HasRenderableItems() && !entity->HasLights())
            continue;

        if (entity->HasBeenKilled())
        {
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);
                mPaintNodes.erase("drawable/" + node.GetId());
                mPaintNodes.erase("text-item/" + node.GetId());
                mLightNodes.erase("basic/" + node.GetId());
            }
        }
        else
        {
            gfx::Transform transform(node.node_to_scene);
            CreatePaintNodes<Entity, EntityNode>(*node.entity, transform);
        }
    }
}

void Renderer::Update(const Scene& scene, const game::Tilemap* map, double time, float dt)
{    
    // can't update the renderer state (paint nodes, lights etc)
    // concurrently.
    std::lock_guard<std::mutex> renderer_lock(mRendererLock);

    // can't update the materials/drawables concurrently
    // with frame rendering.
    std::lock_guard<std::mutex> frame_lock(mFrameLock);

    const auto& projection = scene->GetProjection();

    for (size_t i=0; i<scene.GetNumEntities(); ++i)
    {
        const auto& entity = scene.GetEntity(i);

        for (size_t i=0; i < entity.GetNumNodes(); ++i)
        {
            const auto& node = entity.GetNode(i);

            if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
            {
                UpdateDrawableResources<Entity, EntityNode>(entity, node, *paint, projection, time, dt);
                paint->visited = true;
            }

            if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
            {
                UpdateTextResources<Entity, EntityNode>(entity, node, *paint, projection, time, dt);
                paint->visited = true;
            }

            if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
            {
                UpdateLightResources<Entity, EntityNode>(entity, node, *light, projection, time, dt);
                light->visited = true;
            }
        }

    }
}

void Renderer::CreateFrame(const game::Scene& scene, const game::Tilemap* map, const FrameSettings& settings)
{
    std::lock_guard<std::mutex> renderer_lock(mRendererLock);

    const auto projection = scene->GetProjection();

    std::vector<DrawPacket> scene_draw_packets;
    std::vector<DrawPacket> map_draw_packets;
    std::vector<Light> lights;

    if (map)
    {
        std::vector<TileBatch> batches;

        constexpr auto obey_klass_flags   = false;
        constexpr auto draw_render_layers = true;
        constexpr auto draw_data_layers   = false;
        constexpr auto use_tile_batching  = true;
        TRACE_CALL("PrepareMapTileBatches", PrepareMapTileBatches(*map, settings, batches, draw_render_layers, draw_data_layers, obey_klass_flags, use_tile_batching));
        TRACE_CALL("GenerateMapDrawPackets", GenerateMapDrawPackets(*map, settings, batches, map_draw_packets));
    }

    const auto map_packet_count = map_draw_packets.size();

    {
        TRACE_SCOPE("CreateScenePackets");

        for (size_t i=0; i<scene.GetNumEntities(); ++i)
        {
            const auto& entity = scene.GetEntity(i);
            const auto entity_render_layer = entity.GetRenderLayer();
            const auto entity_belongs_to_tilemap = map && map->GetRenderLayer() == entity_render_layer;

            auto* packet_list = entity_belongs_to_tilemap ? &map_draw_packets : &scene_draw_packets;

            for (size_t j=0; j<entity.GetNumNodes(); ++j)
            {
                const auto& node = entity.GetNode(j);

                if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
                {
                    CreateDrawableDrawPackets<Entity, EntityNode>(entity, node, *paint, settings, projection, *packet_list, nullptr);
                    paint->visited = true;
                }

                if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
                {
                    CreateTextDrawPackets<Entity, EntityNode>(entity, node, *paint, settings, projection, *packet_list, nullptr);
                    paint->visited = true;
                }

                if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
                {
                    CreateLights<Entity, EntityNode>(entity, node, *light, settings, projection, lights);
                    light->visited = true;
                }
            }
        }

    } // update render state

    if (map)
    {
        // Compute map coordinates for all packets coming from scene rendering.
        // In other words, whenever a draw packet is generated from a scene's paint
        // node, map that node to a position on the map tile grid for combining it
        // with the map's tile rendering.
        TRACE_CALL("ComputeTileCoordinates", ComputeTileCoordinates(*map, map_packet_count, map_draw_packets));

        // Sort all packets based on the map based sorting criteria
        TRACE_CALL("SortTilePackets", SortTilePackets(map_draw_packets));
    }

    std::vector<DrawPacket> packets;
    base::AppendVector(packets, map_draw_packets);
    base::AppendVector(packets, scene_draw_packets);

    TRACE_CALL("OffsetPacketLayers", OffsetPacketLayers(packets, lights));

    // this is the outcome that the draw function will then actually draw
    std::lock_guard<std::mutex> lock(mFrameLock);
    std::swap(mRenderBuffer, packets);
    std::swap(mLightBuffer, lights);
    mFrameSettings = settings;
}

void Renderer::DrawFrame(gfx::Device& device) const
{
    std::lock_guard<std::mutex> lock(mFrameLock);

    // only take this shortcut when running for realz otherwise
    // (in the editor) we end up skipping doing low level render
    // hook operations such as drawing the guide grid
    if (mRenderBuffer.empty() && !mEditingMode)
        return;

    // surface (renderer) has not been configured yet.
    const auto width  = mFrameSettings.surface.size.GetWidth();
    const auto height = mFrameSettings.surface.size.GetHeight();
    if (width == 0 || height == 0)
        return;

    bool enable_bloom  = false;
    bool enable_lights = false;
    if (mFrameSettings.style == RenderingStyle::FlatColor ||
        mFrameSettings.style == RenderingStyle::BasicShading)
    {
        if (mFrameSettings.effects.test(Effects::Bloom))
            enable_bloom = true;
    }
    if (mFrameSettings.style == RenderingStyle::BasicShading)
        enable_lights = true;

    LowLevelRenderer low_level_renderer(&mRendererName, device);
    low_level_renderer.SetCamera(mFrameSettings.camera);
    low_level_renderer.SetEditingMode(mEditingMode);
    low_level_renderer.SetSurface(mFrameSettings.surface);
    low_level_renderer.SetBloom(mFrameSettings.bloom);
    low_level_renderer.SetFog(mFrameSettings.fog);
    low_level_renderer.EnableBloom(enable_bloom);
    low_level_renderer.EnableLights(enable_lights);
    low_level_renderer.EnableFog(mFrameSettings.enable_fog);
#if !defined(DETONATOR_ENGINE_BUILD)
    low_level_renderer.SetRenderHook(mLowLevelRendererHook);
    low_level_renderer.SetPacketFilter(mPacketFilter);
#endif

    TRACE_CALL("DrawPackets", low_level_renderer.DrawPackets(mRenderBuffer, mLightBuffer));
    TRACE_CALL("BlitImage", low_level_renderer.BlitImage());
}


#if !defined(DETONATOR_ENGINE_BUILD)
void Renderer::UpdateRendererState(const game::SceneClass& scene, const game::Tilemap* map)
{
    const auto& nodes = scene.CollectNodes();

    for (const auto& node : nodes)
    {
        const auto* placement = node.placement;
        // When we're drawing SceneClass object entity is shared_ptr<const EntityClass>
        // the entity class reference could be nullptr if for example the class was deleted.
        const auto& entity = node.entity;
        if (entity == nullptr)
            continue;

        gfx::Transform transform(node.node_to_scene);
        CreatePaintNodes<game::EntityClass, game::EntityNodeClass>(*entity, transform, placement->GetId());
    }
}

void Renderer::UpdateRendererState(const game::EntityClass& entity)
{
    gfx::Transform transform;
    CreatePaintNodes<EntityClass, EntityNodeClass>(entity, transform, "");
}

void Renderer::UpdateRendererState(const game::Entity& entity)
{
    gfx::Transform transform;
    CreatePaintNodes<Entity, EntityNode>(entity, transform, "");
}

void Renderer::UpdateRendererState(const game::Tilemap& map)
{
    // this API exists for the sake of completeness but currently
    // there's nothing here that needs to be done.
}

void Renderer::Update(const EntityClass& entity, double time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);

        if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
        {
            UpdateDrawableResources<EntityClass, EntityNodeClass>(entity, node, *paint, mProjection, time, dt);
            paint->visited = true;
        }

        if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
        {
            UpdateTextResources<EntityClass, EntityNodeClass>(entity, node, *paint, mProjection, time, dt);
            paint->visited = true;
        }

        if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
        {
            UpdateLightResources<EntityClass, EntityNodeClass>(entity, node, *light, mProjection, time, dt);
            light->visited = true;
        }
    }
}

void Renderer::Update(const SceneClass& scene, const game::Tilemap* map, double time, float dt)
{
    const auto projection = scene.GetProjection();

    for (size_t i=0; i<scene.GetNumNodes(); ++i)
    {
        const auto& placement = scene.GetPlacement(i);
        const auto& entity = placement.GetEntityClass();
        if (!entity)
            continue;

        for (size_t j=0; j<entity->GetNumNodes(); ++j)
        {
            const auto& node = entity->GetNode(j);
            if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + placement.GetId() + "/" + node.GetId()))
            {
                UpdateDrawableResources<EntityClass, EntityNodeClass>(*entity, node, *paint, projection, time, dt);
                paint->visited = true;
            }
            if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + placement.GetId() + "/" + node.GetId()))
            {
                UpdateTextResources<EntityClass, EntityNodeClass>(*entity, node, *paint, projection, time, dt);
                paint->visited = true;
            }
            if (auto* light = base::SafeFind(mLightNodes, "basic/" + placement.GetId() + "/" + node.GetId()))
            {
                UpdateLightResources<EntityClass, EntityNodeClass>(*entity, node, *light, projection, time, dt);
                light->visited = true;
            }
        }
    }
}

void Renderer::Update(const game::Tilemap& map, double time, float dt)
{
    // this API exists for the sake of completeness but currently
    // there's nothing here that needs to be done.
}

void Renderer::Update(const Entity& entity, double time, float dt)
{
    for (size_t i=0; i < entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);

        if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
        {
            UpdateDrawableResources<Entity, EntityNode>(entity, node, *paint, mProjection, time, dt);
            paint->visited = true;
        }

        if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
        {
            UpdateTextResources<Entity, EntityNode>(entity, node, *paint, mProjection, time, dt);
            paint->visited = true;
        }

        if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
        {
            UpdateLightResources<Entity, EntityNode>(entity, node, *light, mProjection, time, dt);
            light->visited = true;
        }
    }
}

void Renderer::CreateFrame(const game::SceneClass& scene, const game::Tilemap* map, SceneClassDrawHook* scene_hook)
{
    const auto projection = scene.GetProjection();

    std::vector<DrawPacket> scene_draw_packets;
    std::vector<DrawPacket> map_draw_packets;

    //std::vector<Light> map_lights;
    std::vector<Light> scene_lights;

    if (map)
    {
        std::vector<TileBatch> batches;

        constexpr auto obey_klass_flags   = false;
        constexpr auto draw_render_layers = true;
        constexpr auto draw_data_layers   = false;
        // disable batching because of single tile highlights
        constexpr auto use_tile_batching = false;
        PrepareMapTileBatches(*map, mFrameSettings, batches, draw_render_layers, draw_data_layers, obey_klass_flags, use_tile_batching);
        GenerateMapDrawPackets(*map, mFrameSettings, batches, map_draw_packets);
    }

    const auto& nodes = scene.CollectNodes();

    for (const auto& node : nodes)
    {
        // When we're drawing SceneClass object the placement is a EntityPlacement*
        // When we're drawing a Scene object the placement is n Entity* object
        const auto& placement = node.placement;
        // When we're drawing SceneClass object entity is shared_ptr<const EntityClass>
        const auto& entity = node.entity;

        // draw when there's no scene hook or when the scene hook returns
        // true for the filtering operation.
        const bool should_draw = !scene_hook || (scene_hook->FilterEntity(*placement));
        if (!should_draw)
            continue;

        if (scene_hook)
            scene_hook->BeginDrawEntity(*placement);

        const auto entity_render_layer = placement->GetRenderLayer();
        const bool entity_belongs_to_tilemap = map && map->GetRenderLayer() == entity_render_layer;

        // choose a packet list for the entity. if the entity has been assigned to the same
        // rendering layer that has a tilemap then the rendering will require sorting the
        // entity packets into the tilemap packets
        auto* packet_list = entity_belongs_to_tilemap ? &map_draw_packets : &scene_draw_packets;

        if (placement->TestFlag(game::EntityPlacement::Flags::VisibleInGame))
        {
            std::vector<DrawPacket> entity_packets;
            std::vector<Light> entity_lights;

            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);

                if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + placement->GetId() + "/" + node.GetId()))
                {
                    CreateDrawableDrawPackets<game::EntityClass, game::EntityNodeClass>(*entity, node, *paint, mFrameSettings, projection, entity_packets, nullptr);
                    paint->visited = true;
                }

                if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + placement->GetId() + "/" + node.GetId()))
                {
                    CreateTextDrawPackets<game::EntityClass, game::EntityNodeClass>(*entity, node, *paint, mFrameSettings, projection, entity_packets, nullptr);
                    paint->visited = true;
                }
                if (auto* light = base::SafeFind(mLightNodes, "basic/" + placement->GetId() + "/" + node.GetId()))
                {
                    CreateLights<game::EntityClass, game::EntityNodeClass>(*entity, node, *light, mFrameSettings, projection, entity_lights);
                    light->visited = true;
                }
            }
            // generate draw packets uses the entity to ask for the scene layer index.
            // When rendering a SceneClass the entity class doesn't have the layer index
            // (but only a stub function) and the real layer information is in the placement.
            for (auto& packet: entity_packets)
            {
                packet.render_layer = entity_render_layer;
            }
            for (auto& light : entity_lights)
            {
                light.render_layer = entity_render_layer;
            }

            // Compute tile coordinates for each draw packet created by the entity
            // if the entity is in the same render layer as the map.
            // We must do the tilemap coordinate computation here so that the scene
            // hook (if any) can use the tile col/row information when doing
            // for example tile highlighting
            if (entity_belongs_to_tilemap)
            {
                ComputeTileCoordinates(*map, 0, entity_packets);
            }

            // If there's a scene hook let the scene hook inspect each and every
            // entity draw packet in order to perform things such as visualizations
            // of selected object(s) in the Editor.
            if (scene_hook)
            {
                for (auto& packet : entity_packets)
                {
                    if (scene_hook->InspectPacket(*placement, packet))
                        packet_list->push_back(packet);
                }
            }
            else base::AppendVector(*packet_list, entity_packets);

            // todo: solve lights for map
            base::AppendVector(scene_lights, entity_lights);

        } // visible placement

        if (scene_hook)
        {
            // this is the model, aka model_to_world transform
            gfx::Transform transform(node.node_to_scene);

            scene_hook->AppendPackets(*placement, transform, *packet_list);
            scene_hook->EndDrawEntity(*placement);
        }
    }

    // If we have any tilemap packets they must be sorted to the tile
    // specific rendering order in order to solve tile occlusion.
    SortTilePackets(map_draw_packets);

    std::vector<DrawPacket> packets;
    base::AppendVector(packets, map_draw_packets);
    base::AppendVector(packets, scene_draw_packets);

    // All rendering layers must be shifted so that there are no
    // negative values. and all layer offsets are positive. The
    // low level renderer uses a bucket sort for the render layers
    // and that cannot tolerate negative indices
    OffsetPacketLayers(packets, scene_lights);

    // this is the outcome that the draw function will then actually draw
    std::swap(mRenderBuffer, packets);
    std::swap(mLightBuffer, scene_lights);
}

void Renderer::CreateFrame(const game::EntityClass& entity, EntityClassDrawHook* hook)
{
    std::vector<DrawPacket> packets;
    std::vector<Light> lights;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);

        bool did_paint = false;
        if (node.HasDrawable())
        {
            if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
            {
                CreateDrawableDrawPackets<game::EntityClass, game::EntityNodeClass>(entity, node, *paint, mFrameSettings, mProjection, packets, hook);
                paint->visited = true;
                did_paint = true;
            }
        }

        if (node.HasTextItem())
        {
            if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
            {
                CreateTextDrawPackets<game::EntityClass, game::EntityNodeClass>(entity, node, *paint, mFrameSettings, mProjection, packets, hook);
                paint->visited = true;
                did_paint = true;
            }
        }

        if (node.HasBasicLight())
        {
            if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
            {
                CreateLights<game::EntityClass, game::EntityNodeClass>(entity, node, *light, mFrameSettings, mProjection, lights);
                light->visited = true;
            }
        }

        if (hook && !did_paint)
        {
            gfx::Transform transform(entity.FindNodeTransform(&node));
            hook->AppendPackets(&node, transform, packets);
        }
    }

    OffsetPacketLayers(packets, lights);

    std::swap(mRenderBuffer, packets);
    std::swap(mLightBuffer, lights);
}

void Renderer::CreateFrame(const game::Entity& entity, EntityInstanceDrawHook* hook)
{
    std::vector<DrawPacket> packets;
    std::vector<Light> lights;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);

        bool did_paint = false;
        if (auto* paint = base::SafeFind(mPaintNodes, "drawable/" + node.GetId()))
        {
            CreateDrawableDrawPackets<game::Entity, game::EntityNode>(entity, node, *paint, mFrameSettings, mProjection, packets, hook);
            paint->visited = true;
            did_paint = true;
        }

        if (auto* paint = base::SafeFind(mPaintNodes, "text-item/" + node.GetId()))
        {
            CreateTextDrawPackets<game::Entity, game::EntityNode>(entity, node, *paint, mFrameSettings, mProjection, packets, hook);
            paint->visited = true;
            did_paint = true;
        }
        if (auto* light = base::SafeFind(mLightNodes, "basic/" + node.GetId()))
        {
            CreateLights<game::Entity, game::EntityNode>(entity, node, *light, mFrameSettings, mProjection, lights);
            light->visited = true;
        }

        if (hook && !did_paint)
        {
            gfx::Transform transform(entity.FindNodeTransform(&node));
            hook->AppendPackets(&node, transform, packets);
        }
    }

    OffsetPacketLayers(packets, lights);

    std::swap(mRenderBuffer, packets);
    std::swap(mLightBuffer, lights);
}

void Renderer::CreateFrame(const game::Tilemap& map, bool draw_render_layer, bool draw_data_layer, TileBatchDrawHook* hook)
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

    constexpr auto obey_klass_flags = true;
    constexpr auto use_tile_batching = false;

    std::vector<TileBatch> batches;
    PrepareMapTileBatches(map, mFrameSettings, batches, draw_render_layer, draw_data_layer, obey_klass_flags, use_tile_batching);

    std::vector<DrawPacket> packets;
    GenerateMapDrawPackets(map, mFrameSettings, batches,packets);

    std::vector<Light> lights;

    OffsetPacketLayers(packets, lights);
    SortTilePackets(packets);

    // put the data packets (indicated by editor) first
    std::stable_partition(packets.begin(), packets.end(), [](const auto& packet) {
        return packet.domain == DrawPacket::Domain::Editor;
    });

    std::swap(mRenderBuffer, packets);
}

void Renderer::BeginFrame()
{
    if (mEditingMode)
    {
        for (auto& node: mPaintNodes)
            node.second.visited = false;

        for (auto& light : mLightNodes)
            light.second.visited = false;
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

        for (auto it = mLightNodes.begin(); it != mLightNodes.end(); )
        {
            const auto& light_node = it->second;
            if (light_node.visited)
                ++it;
            else it = mLightNodes.erase(it);
        }
    }
}

void Renderer::ClearPaintState()
{
    mPaintNodes.clear();
    mLightNodes.clear();
    mTilemapPalette.clear();
}

#endif // DETONATOR_ENGINE_BUILD


void Renderer::GenerateMapDrawPackets(const game::Tilemap& map,
                                      const FrameSettings& settings,
                                      const std::vector<TileBatch>& batches,
                                      std::vector<DrawPacket>& packets) const
{
    const auto& camera = settings.camera;

    const auto map_view = map.GetPerspective();
    const auto& map_view_to_clip    = CreateProjectionMatrix(Projection::Orthographic, camera.viewport);
    const auto& map_world_to_view   = CreateModelViewMatrix(map_view, camera.position, camera.scale,
                                                            camera.rotation);
    const auto& scene_view_to_clip  = CreateProjectionMatrix(Projection::Orthographic, camera.viewport);
    const auto& scene_world_to_view = CreateModelViewMatrix(GameView::AxisAligned, camera.position, camera.scale,
                                                            camera.rotation);

    // this matrix will transform coordinates from map's coordinate space to
    // scene coordinate space. (the scene world coordinate is a coordinate
    // in the 3D rendering space)
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
            const auto tile_width_render_units = tile_render_size.x * tile_render_width_scale + settings.tile_size_fudge;
            const auto tile_height_render_units = tile_render_size.y * tile_render_height_scale + settings.tile_size_fudge;

            auto tiles = std::make_unique<gfx::TileBatch>(std::move(batch.tiles));
            tiles->SetTileWorldSize(batch.tile_size);
            tiles->SetTileRenderWidth(tile_width_render_units);
            tiles->SetTileRenderHeight(tile_height_render_units);
            tiles->SetTileShape(gfx::TileBatch::TileShape::Automatic);
            if (map_view == game::Tilemap::Perspective::AxisAligned)
                tiles->SetProjection(gfx::TileBatch::Projection::AxisAligned);
            else if (map_view == game::Tilemap::Perspective::Dimetric)
                tiles->SetProjection(gfx::TileBatch::Projection::Dimetric);
            else if (map_view == game::Tilemap::Perspective::Isometric)
                tiles->SetProjection(gfx::TileBatch::Projection::Isometric);
            else BUG("unknown projection");

            DrawPacket packet;
            packet.source       = DrawPacket::Source::Map;
            packet.domain       = DrawPacket::Domain::Scene;
            packet.projection   = DrawPacket::Projection::Orthographic;
            packet.pass         = DrawPacket::RenderPass::DrawColor;
            packet.material     = batch.material;
            packet.drawable     = std::move(tiles);
            packet.transform    = from_map_to_scene;
            packet.map_row      = batch.row;
            packet.map_col      = batch.col;
            packet.map_sort_key = batch.sort_key;
            packet.map_layer    = batch.layer_index;
            packet.render_layer = batch.render_layer;
            packet.packet_index = 0;
            packets.push_back(std::move(packet));
        }
        else if (batch.type == TileBatch::Type::Data && mEditingMode)
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
            packet.pass         = DrawPacket::RenderPass::DrawColor;
            packet.material     = batch.material;
            packet.drawable     = std::move(tiles);
            packet.transform    = glm::mat4(1.0f);
            packet.map_row      = batch.row;
            packet.map_col      = batch.col;
            packet.map_sort_key = batch.sort_key;
            packet.map_layer    = batch.layer_index;
            packet.render_layer = batch.render_layer;
            packet.packet_index = 0;
            packets.push_back(std::move(packet));
        } else BUG("Unhandled tile batch type.");
    }
}

void Renderer::PrepareMapTileBatches(const game::Tilemap& map,
                                     const FrameSettings& settings,
                                     std::vector<TileBatch>& batches,
                                     bool draw_render_layer,
                                     bool draw_data_layer,
                                     bool obey_klass_flags,
                                     bool use_batching)

{
    const auto& surface = settings.surface;
    const auto& camera  = settings.camera;

    // The logical game world is mapped inside the device viewport
    // through projection and clip transformations. thus it should
    // be possible to map the viewport back to the world plane already
    // with MapFromWindowToWorldPlane.
    const auto device_viewport_width   = surface.viewport.GetWidth();
    const auto device_viewport_height  = surface.viewport.GetHeight();
    const auto window_size = glm::vec2{device_viewport_width, device_viewport_height};

    const auto map_view = map.GetPerspective();
    const auto& view_to_clip = CreateProjectionMatrix(Projection::Orthographic, camera.viewport);
    const auto& world_to_view = CreateModelViewMatrix(map_view, camera.position, camera.scale, camera.rotation);

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
        const auto layer_width_units  = float(layer_width_tiles)  * layer_tile_width_units;
        const auto layer_height_units = float(layer_height_tiles) * layer_tile_height_units;

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

        if (draw_data_layer && layer->HasDataComponent() && mEditingMode)
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

template<typename EntityType, typename EntityNodeType>
void Renderer::UpdateDrawableResources(const EntityType& entity, const EntityNodeType& entity_node, PaintNode& paint_node,
                                       SceneProjection projection, double time, float dt) const
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;
    const auto* item = entity_node.GetDrawable();

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    if (item && paint_node.material)
    {
        paint_node.material->ResetUniforms();
        if (const auto* params = item->GetMaterialParams())
            paint_node.material->SetUniforms(*params);

        const auto time_scale = item->GetTimeScale();
        if (item->TestFlag(DrawableItemType::Flags::UpdateMaterial))
            paint_node.material->Update(dt * time_scale);

        paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableBloom,
            item->TestFlag(DrawableItemType::Flags::PP_EnableBloom));

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            if (item->HasMaterialTimeAdjustment())
            {
                const float adjusted_time = item->GetMaterialTimeAdjustment();
                paint_node.material->SetRuntime(adjusted_time);
                item->ClearMaterialTimeAdjustment();
            }

            gfx::Material::Environment env;
            env.editing_mode   = mEditingMode;
            env.draw_primitive = paint_node.drawable->GetDrawPrimitive();
            env.draw_category  = paint_node.drawable->GetDrawCategory();
            env.render_pass    = gfx::RenderPass::ColorPass;

            for (size_t i=0; i<item->GetNumCommands(); ++i)
            {
                const auto& src_cmd = item->GetCommand(i);
                gfx::Material::Command gfx_cmd;
                gfx_cmd.name = src_cmd.name;
                gfx_cmd.args = src_cmd.args;
                paint_node.material->Execute(env, gfx_cmd);
            }
        }

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            item->SetCurrentMaterialTime(paint_node.material->GetRuntime());

            DrawableItem::SpriteCycle sprite_cycle;
            if (paint_node.material->GetValue("SpriteCycleName", &sprite_cycle.name) &&
                paint_node.material->GetValue("SpriteCycleTime", &sprite_cycle.time))
            {
                item->SetCurrentSpriteCycle(std::move(sprite_cycle));
            }
            else
            {
                item->ClearCurrentSpriteCycle();
            }
        }
    }
    if (item && paint_node.drawable)
    {
        const auto horizontal_flip = item->TestFlag(DrawableItemType::Flags::FlipHorizontally);
        const auto vertical_flip   = item->TestFlag(DrawableItemType::Flags::FlipVertically);
        const auto project_3d      = item->TestFlag(DrawableItemType::Flags::ProjectAs3D);
        const auto& shape = paint_node.drawable;
        const auto size = entity_node.GetSize();
        const auto is3d = Is3DShape(*shape);

        if (paint_node.material)
        {
            if (is3d || project_3d)
            {
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight,
                    item->TestFlag(DrawableItemType::Flags::Enable3DLight));
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableFog,
                    item->TestFlag(DrawableItemType::Flags::Enable3DFog));
            }
            else
            {
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight, false);
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableFog, false);
            }
        }

        const bool add_axonometric_transform = IsAxonometricProjection(projection) && (is3d || project_3d);
        if (add_axonometric_transform)
        {
            transform.Push(CreateModelMatrix(GameView::AxisAligned));
            transform.Push(CreateModelMatrix(projection));
        }

        if (is3d)
        {
            // model transform. keep in mind 3D only applies to the
            // renderable item (so it's visual only) thus the 3rd dimension
            // comes from the renderable item instead of the node!
            transform.Push();
                transform.RotateAroundX(gfx::FDegrees(180.0f));
                transform.Scale(size.x, size.y, item->GetDepth());
                transform.Rotate(item->GetRotator());
                transform.Translate(item->GetOffset());

        }
        else
        {
            // model transform.
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                transform.Scale(size);
                if (project_3d)
                {
                    transform.Rotate(item->GetRotator());
                    transform.Translate(item->GetOffset());
                }
        }

        glm::mat4 world(1.0f);
        if (add_axonometric_transform)
        {
            world =  CreateModelMatrix(projection) *
                     CreateModelMatrix(GameView::AxisAligned);
        }

        const auto& model = transform.GetAsMatrix();
        gfx::Drawable::Environment env;
        env.model_matrix = &model;
        env.world_matrix = &world;
        env.flip_uv_horizontally = horizontal_flip;
        env.flip_uv_vertically   = vertical_flip;
        // todo: other env matrices?

        const auto time_scale = item->GetTimeScale();
        if (item->TestFlag(DrawableItemType::Flags::UpdateDrawable))
            paint_node.drawable->Update(env, dt * time_scale);
        if (item->TestFlag(DrawableItemType::Flags::RestartDrawable) && !paint_node.drawable->IsAlive())
            paint_node.drawable->Restart(env);

        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            for (size_t i=0; i<item->GetNumCommands(); ++i)
            {
                const auto& src_cmd = item->GetCommand(i);
                gfx::Drawable::Command gfx_cmd;
                gfx_cmd.name = src_cmd.name;
                gfx_cmd.args = src_cmd.args;
                paint_node.drawable->Execute(env, gfx_cmd);
            }
        }

        // pop model transform.
        transform.Pop();

        if (add_axonometric_transform)
        {
            // pop view based model transform
            transform.Pop();
            transform.Pop();
        }
    }

    if (item)
    {
        if constexpr (std::is_same_v<EntityNodeType, game::EntityNode>)
        {
            item->ClearCommands();
        }
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::UpdateTextResources(const EntityType& entity, const EntityNodeType& entity_node, PaintNode& paint_node,
                                   SceneProjection mode, double time, float dt) const
{

    using TextItemType = typename EntityNodeType::TextItemType;
    const auto* text = entity_node.GetTextItem();

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    if (text && paint_node.material)
    {
        paint_node.material->Update(dt);
        paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableBloom,
            text->TestFlag(TextItemType::Flags::PP_EnableBloom));
        paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight,
            text->TestFlag(TextItemType::Flags::EnableLight));
    }
    if (text && paint_node.drawable)
    {
        // push the actual model transform
        transform.Push(entity_node.GetModelTransform());

        const auto& model = transform.GetAsMatrix();
        gfx::Drawable::Environment env;
        env.model_matrix = &model;
        // todo: other env matrices?

        paint_node.drawable->Update(env, dt);

        // pop model transform
        transform.Pop();
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::UpdateLightResources(const EntityType& entity, const EntityNodeType& entity_node, LightNode& light_node,
                                    SceneProjection mode, double time, float dt) const
{
    if (const auto* light = entity_node.GetBasicLight())
    {
        if (light_node.light)
        {
            const auto type = light->GetLightType();
            if (type == game::BasicLightType::Ambient)
                light_node.light->type = gfx::BasicLightType::Ambient;
            else if (type == game::BasicLightType::Directional)
                light_node.light->type = gfx::BasicLightType::Directional;
            else if (type == game::BasicLightType::Point)
                light_node.light->type = gfx::BasicLightType::Point;
            else if (type == game::BasicLightType::Spot)
                light_node.light->type = gfx::BasicLightType::Spot;
            else BUG("Bug on basic light type.");

            light_node.light->ambient_color = light->GetAmbientColor();
            light_node.light->diffuse_color = light->GetDiffuseColor();
            light_node.light->specular_color = light->GetSpecularColor();
            light_node.light->constant_attenuation = light->GetConstantAttenuation();
            light_node.light->linear_attenuation = light->GetLinearAttenuation();
            light_node.light->quadratic_attenuation = light->GetQuadraticAttenuation();
            light_node.light->spot_half_angle = light->GetSpotHalfAngle();
        }
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreatePaintNodes(const EntityType& entity, gfx::Transform& transform, std::string prefix)
{
    using RenderTree = game::RenderTree<EntityNodeType>;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const EntityType& entity, Renderer& renderer, gfx::Transform& transform, std::string prefix)
          : mEntity(entity)
          , mRenderer(renderer)
          , mTransform(transform)
          , mPrefix(std::move(prefix))
        {}
        virtual void EnterNode(const EntityNodeType* node) override
        {
            if (!node)
                return;

            // always push the node's transform, it might have children that
            // do render even if this node itself doesn't
            mTransform.Push(node->GetNodeTransform());

            game::FBox box;
            if  (node->HasDrawable() || node->HasTextItem() || node->HasBasicLight())
                box.Transform(mTransform.GetAsMatrix());

            if (const auto* item = node->GetDrawable())
            {
                auto& paint_node = mRenderer.mPaintNodes["drawable/" + mPrefix + node->GetId()];
                paint_node.visited        = true;
                paint_node.world_pos      = box.GetTopLeft();
                paint_node.world_scale    = box.GetSize();
                paint_node.world_rotation = box.GetRotation();
                mRenderer.CreateDrawableResources<EntityType, EntityNodeType>(mEntity, *node, paint_node);
            }

            if (const auto* text = node->GetTextItem())
            {
                auto& paint_node = mRenderer.mPaintNodes["text-item/" + mPrefix + node->GetId()];
                paint_node.visited        = true;
                paint_node.world_pos      = box.GetTopLeft();
                paint_node.world_scale    = box.GetSize();
                paint_node.world_rotation = box.GetRotation();
                mRenderer.CreateTextResources<EntityType, EntityNodeType>(mEntity, *node, paint_node);
            }

            if (const auto* light = node->GetBasicLight())
            {
                auto& light_node = mRenderer.mLightNodes["basic/" + mPrefix + node->GetId()];
                light_node.visited        = true;
                light_node.world_pos      = box.GetTopLeft();
                light_node.world_scale    = box.GetSize();
                light_node.world_rotation = box.GetRotation();
                mRenderer.CreateLightResources<EntityType, EntityNodeType>(mEntity, *node, light_node);
            }
        }
        virtual void LeaveNode(const EntityNodeType* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const EntityType& mEntity;
        Renderer& mRenderer;
        gfx::Transform& mTransform;
        const std::string mPrefix;
    } visitor(entity, *this, transform, prefix.empty() ? "" : prefix + "/");

    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateDrawableResources(const EntityType& entity, const EntityNodeType& entity_node, PaintNode& paint_node) const
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    if (const auto* item = entity_node.GetDrawable())
    {
        const auto* mesh_effect = entity_node.GetMeshEffect();

        const auto& materialId = item->GetMaterialId();
        const auto& drawableId = item->GetDrawableId() + (mesh_effect ? "+MeshEffect" : "");
        if (paint_node.materialId != materialId)
        {
            paint_node.material.reset();
            paint_node.materialId = materialId;
            auto klass = mClassLib->FindMaterialClassById(materialId);
            if (klass)
                paint_node.material = gfx::CreateMaterialInstance(klass);
            if (!paint_node.material)
                WARN("No such material class found. [material='%1', entity='%2', node='%3']", materialId,
                     entity.GetName(), entity_node.GetName());
            if (paint_node.material)
            {
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableBloom,
                    item->TestFlag(DrawableItemType::Flags::PP_EnableBloom));
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight, false);

                if (const auto* params = item->GetMaterialParams())
                    paint_node.material->SetUniforms(*params);
            }
        }
        if (paint_node.drawableId != drawableId)
        {
            paint_node.drawable.reset();
            paint_node.drawableId = drawableId;

            auto klass = mClassLib->FindDrawableClassById(item->GetDrawableId());
            if (klass)
            {
                if (mesh_effect)
                {
                    const auto effect_type = mesh_effect->GetEffectType();
                    auto effect_id = entity.GetId();
                    auto source_drawable = gfx::CreateDrawableInstance(klass);
                    auto effect_drawable = std::make_unique<gfx::EffectDrawable>(std::move(source_drawable), std::move(effect_id));
                    if (effect_type == game::MeshEffectClass::EffectType::MeshExplosion)
                    {
                        const auto* effect_args = mesh_effect->GetMeshExplosionEffectArgs();
                        gfx::EffectDrawable::MeshExplosionEffectArgs args;
                        args.mesh_subdivision_count = effect_args->mesh_subdivision_count;
                        args.shard_linear_speed     = effect_args->shard_linear_speed;
                        args.shard_linear_acceleration = effect_args->shard_linear_acceleration;
                        args.shard_rotational_speed    = effect_args->shard_rotational_speed;
                        args.shard_rotational_acceleration = effect_args->shard_rotational_acceleration;
                        effect_drawable->SetEffectType(gfx::EffectDrawable::EffectType::ShardedMeshExplosion);
                        effect_drawable->SetEffectArgs(args);
                    }
                    if (mesh_effect->HasEffectShapeId())
                    {
                        const auto& effect_shape_id = mesh_effect->GetEffectShapeId();
                        if (const auto& effect_shape_klass = mClassLib->FindDrawableClassById(effect_shape_id))
                        {
                            const auto type = effect_shape_klass->GetType();
                            if (type == gfx::DrawableClass::Type::Polygon)
                            {
                                auto effect_shape = gfx::CreateDrawableInstance(effect_shape_klass);
                                effect_drawable->SetEffectDrawable(std::move(effect_shape));
                            } else WARN("Mesh effect shape has unexpected/unsupported type. (Expected user defined shape class). [shape='%1', entity='%2', node='%3']",
                                effect_shape_klass->GetName(), entity.GetName(), entity_node.GetName());
                        } else WARN("No such effect shape class found. [effect='%1', entity='%2', node='%3']",
                                    effect_shape_id, entity.GetName(), entity_node.GetName());
                    }
                    paint_node.drawable = std::move(effect_drawable);
                }
                else
                {
                    paint_node.drawable = gfx::CreateDrawableInstance(klass);
                }
            }

            if (!paint_node.drawable)
                WARN("No such drawable class found. [drawable='%1', entity='%2', node='%3']", item->GetMaterialId(),
                     entity.GetName(), entity_node.GetName());
            if (paint_node.drawable)
            {
                gfx::Transform transform;
                transform.Scale(paint_node.world_scale);
                transform.RotateAroundZ(paint_node.world_rotation);
                transform.Translate(paint_node.world_pos);

                transform.Push(entity_node.GetModelTransform());

                const auto& model = transform.GetAsMatrix();
                gfx::Drawable::Environment env; // todo:
                env.model_matrix = &model;
                paint_node.drawable->Restart(env);
            }
        }
        if (paint_node.drawable &&
            paint_node.drawable->GetType() == gfx::Drawable::Type::SimpleShape)
        {
            auto simple = std::static_pointer_cast<gfx::SimpleShapeInstance>(paint_node.drawable);
            simple->SetStyle(gfx::SimpleShapeStyle::Solid);
        }

        if (paint_node.drawable && paint_node.material)
        {
            const auto is_3d = gfx::Is3DShape(*paint_node.drawable);
            const auto project_as_3d = item->TestFlag(DrawableItemType::Flags::ProjectAs3D);
            if (is_3d || project_as_3d)
            {
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight,
                        item->TestFlag(DrawableItemType::Flags::Enable3DLight));
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableFog,
                    item->TestFlag(DrawableItemType::Flags::Enable3DFog));
            }
            else
            {
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight, false);
                paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableFog, false);
            }
        }
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateTextResources(const EntityType& entity, const EntityNodeType& entity_node, PaintNode& paint_node) const
{
    using TextItemType = typename EntityNodeType::TextItemType;

    if (const auto* text = entity_node.GetTextItem())
    {
        const auto& node_size = entity_node.GetSize();
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
        if (paint_node.materialId != material)
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
            paint_node.material = std::move(mat);
            paint_node.materialId = material;
            paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableBloom,
                text->TestFlag(TextItemType::Flags::PP_EnableBloom));
            paint_node.material->SetFlag(gfx::MaterialInstance::Flags::EnableLight,
                text->TestFlag(TextItemType::Flags::EnableLight));
        }
        if (!paint_node.drawable)
        {
            auto klass = mClassLib->FindDrawableClassById(drawable);
            paint_node.drawable = gfx::CreateDrawableInstance(klass);
        }
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateLightResources(const EntityType& entity, const EntityNodeType& entity_node, LightNode& light_node) const
{
    // do not touch the light here if it already exists,
    // only create a new light if/when needed. The light
    // object itself can be used by the current frame that
    // is being drawn out in another thread that is currently
    // calling DrawFrame.

    // The light state can be updated in the UpdateLightResources
    // which is called after frame rendering has finished.

    if (const auto* light = entity_node.GetBasicLight())
    {
       if (!light_node.light)
       {
           const auto type = light->GetLightType();
           light_node.light = std::make_shared<gfx::BasicLight>();

           if (type == game::BasicLightType::Ambient)
               light_node.light->type = gfx::BasicLightType::Ambient;
           else if (type == game::BasicLightType::Directional)
               light_node.light->type = gfx::BasicLightType::Directional;
           else if (type == game::BasicLightType::Point)
               light_node.light->type = gfx::BasicLightType::Point;
           else if (type == game::BasicLightType::Spot)
               light_node.light->type = gfx::BasicLightType::Spot;
           else BUG("Bug on basic light type.");

           light_node.light->ambient_color = light->GetAmbientColor();
           light_node.light->diffuse_color = light->GetDiffuseColor();
           light_node.light->specular_color = light->GetSpecularColor();
           light_node.light->constant_attenuation = light->GetConstantAttenuation();
           light_node.light->linear_attenuation = light->GetLinearAttenuation();
           light_node.light->quadratic_attenuation = light->GetQuadraticAttenuation();
           light_node.light->spot_half_angle = light->GetSpotHalfAngle();
       }
   }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateDrawableDrawPackets(const EntityType& entity,
                                         const EntityNodeType& entity_node,
                                         const PaintNode& paint_node,
                                         const FrameSettings& settings,
                                         SceneProjection projection,
                                         std::vector<DrawPacket>& packets,
                                         EntityDrawHook<EntityNodeType>* hook) const
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    const bool entity_visible = entity.TestFlag(EntityType::Flags::VisibleInGame);

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    auto map_sort_point  = glm::vec2 {0.5f, 1.0f};
    auto map_layer       = uint16_t(0);
    auto map_sort_key    = static_cast<uint8_t>(game::TileOcclusion::None);
    if (const auto* map = entity_node.GetMapNode())
    {
        map_sort_point = map->GetSortPoint();
        map_layer      = map->GetMapLayer();
        map_sort_key   = static_cast<uint8_t>(map->GetTileOcclusion());
    }

    transform.Push(entity_node.GetModelTransform());
       map_sort_point = transform * glm::vec4{map_sort_point, 0.0f, 1.0};
     transform.Pop();

    const auto* item = entity_node.GetDrawable();
    if (item && paint_node.drawable && paint_node.material)
    {
        const auto horizontal_flip = item->TestFlag(DrawableItemType::Flags::FlipHorizontally);
        const auto vertical_flip   = item->TestFlag(DrawableItemType::Flags::FlipVertically);
        const auto double_sided    = item->TestFlag(DrawableItemType::Flags::DoubleSided);
        const auto depth_test      = item->TestFlag(DrawableItemType::Flags::DepthTest);
        const auto project_3d      = item->TestFlag(DrawableItemType::Flags::ProjectAs3D);
        const auto& shape = paint_node.drawable;
        const auto size = entity_node.GetSize();
        const auto is3d = Is3DShape(*shape);

        const auto add_axonometric_projection = IsAxonometricProjection(projection) && (is3d || project_3d);
        if (add_axonometric_projection)
        {
            transform.Push(CreateModelMatrix(GameView::AxisAligned));
            transform.Push(CreateModelMatrix(projection));
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
               transform.Scale(size.x, size.y, item->GetDepth());
               transform.Rotate(item->GetRotator());
               transform.Translate(item->GetOffset());
        }
        else
        {
            // model transform.
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                transform.Scale(size);
                if (project_3d)
                {
                    transform.Rotate(item->GetRotator());
                    transform.Translate(item->GetOffset());
                }
        }

        // if it doesn't render then no draw packets are generated
        if (item->TestFlag(DrawableItemType::Flags::VisibleInGame) && entity_visible)
        {
            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, item->TestFlag(DrawableItemType::Flags::PP_EnableBloom));
            packet.flags.set(DrawPacket::Flags::Flip_UV_Vertically, vertical_flip);
            packet.flags.set(DrawPacket::Flags::Flip_UV_Horizontally, horizontal_flip);
            packet.source           = DrawPacket::Source::Scene;
            packet.domain           = DrawPacket::Domain::Scene;
            packet.culling          = DrawPacket::Culling::Back;
            packet.depth_test       = DrawPacket::DepthTest::Disabled;
            packet.projection       = DrawPacket::Projection::Orthographic;
            packet.material         = paint_node.material;
            packet.drawable         = paint_node.drawable;
            packet.transform        = transform;
            packet.sort_point       = map_sort_point;
            packet.map_layer        = map_layer;
            packet.map_sort_key     = map_sort_key;
            packet.render_layer     = entity.GetRenderLayer();
            packet.pass             = item->GetRenderPass();
            packet.packet_index     = item->GetLayer();
            packet.coordinate_space = item->GetCoordinateSpace();

            if (projection == SceneProjection::AxisAlignedPerspective && (is3d || project_3d))
                packet.projection = DrawPacket::Projection::Perspective;

            if (mEditingMode)
            {
                if (settings.style == RenderingStyle::Wireframe &&
                        packet.drawable->GetDrawPrimitive() == gfx::Drawable::DrawPrimitive::Triangles)
                {
                    static auto wireframe_color = std::make_shared<gfx::MaterialInstance>(
                            gfx::CreateMaterialClassFromColor(gfx::Color::Gray));
                    packet.drawable = std::make_shared<gfx::WireframeInstance>(packet.drawable);
                    packet.material = wireframe_color;
                }
            }

            if (double_sided)
                packet.culling = DrawPacket::Culling::None;

            if (depth_test)
                packet.depth_test = DrawPacket::DepthTest::LessOrEQual;

            if (!hook || hook->InspectPacket(&entity_node , packet))
                packets.push_back(std::move(packet));
        }

        // pop model transform
        transform.Pop();

        if (add_axonometric_projection)
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
        hook->AppendPackets(&entity_node, transform, packets);
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateTextDrawPackets(const EntityType& entity,
                                     const EntityNodeType& entity_node,
                                     const PaintNode& paint_node,
                                     const FrameSettings& settings,
                                     SceneProjection mode,
                                     std::vector<DrawPacket>& packets,
                                     EntityDrawHook<EntityNodeType>* hook) const
{
    using DrawableItemType = typename EntityNodeType::DrawableItemType;

    const bool entity_visible = entity.TestFlag(EntityType::Flags::VisibleInGame);

    gfx::Transform transform;
    transform.Scale(paint_node.world_scale);
    transform.RotateAroundZ(paint_node.world_rotation);
    transform.Translate(paint_node.world_pos);

    auto map_sort_point = glm::vec2 {0.5f, 1.0f};
    auto map_layer      = uint16_t(0);
    auto map_sort_key   = static_cast<uint8_t>(game::TileOcclusion::None);
    if (const auto* map = entity_node.GetMapNode())
    {
        map_sort_point = map->GetSortPoint();
        map_layer      = map->GetMapLayer();
        map_sort_key   = static_cast<uint8_t>(map->GetTileOcclusion());
    }

    const auto* text = entity_node.GetTextItem();
    if (text && paint_node.drawable && paint_node.material)
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
            transform.Push(entity_node.GetModelTransform());

            DrawPacket packet;
            packet.flags.set(DrawPacket::Flags::PP_Bloom, text->TestFlag(TextItemClass::Flags::PP_EnableBloom));
            packet.source       = DrawPacket::Source::Scene;
            packet.domain       = DrawPacket::Domain::Scene;
            packet.pass         = DrawPacket::RenderPass::DrawColor;
            packet.drawable     = paint_node.drawable;
            packet.material     = paint_node.material;
            packet.transform    = transform;
            packet.sort_point   = transform * glm::vec4{map_sort_point, 0.0f, 1.0f};
            packet.map_layer    = map_layer;
            packet.map_sort_key = map_sort_key;
            packet.packet_index = text->GetLayer();
            packet.render_layer = entity.GetRenderLayer();
            packet.coordinate_space = text->GetCoordinateSpace();
            if (!hook || hook->InspectPacket(&entity_node, packet))
                packets.push_back(std::move(packet));

            // pop model transform
            transform.Pop();
        }
    }

    // append any extra packets if needed.
    if (hook)
    {
        // Allow the draw hook to append any extra draw packets for this node.
        hook->AppendPackets(&entity_node, transform, packets);
    }
}

template<typename EntityType, typename EntityNodeType>
void Renderer::CreateLights(const EntityType& entity,
                            const EntityNodeType& entity_node,
                            const LightNode& light_node,
                            const FrameSettings& settings,
                            SceneProjection projection,
                            std::vector<Light>& lights) const
{
    using LightClassType = typename EntityNodeType::LightClassType;

    if (!entity.TestFlag(EntityType::Flags::VisibleInGame))
        return;

    const auto* node_light = entity_node.GetBasicLight();
    if (!node_light || !node_light->IsEnabled() || !light_node.light)
        return;

    const auto add_axonometric_transform = IsAxonometricProjection(projection);

    gfx::Transform light_to_world;
    light_to_world.Scale(light_node.world_scale);
    light_to_world.RotateAroundZ(light_node.world_rotation);
    light_to_world.Translate(light_node.world_pos);

    if (add_axonometric_transform)
    {
        light_to_world.Push(CreateModelMatrix(GameView::AxisAligned));
        light_to_world.Push(CreateModelMatrix(projection));
    }

    light_to_world.Push();
         light_to_world.Translate(node_light->GetTranslation());

    auto map_sort_point = glm::vec2 {0.5f, 1.0f};
    auto map_layer      = std::uint16_t(0);
    auto map_sort_key   = static_cast<uint8_t>(game::TileOcclusion::None);
    if (const auto* map = entity_node.GetMapNode())
    {
        map_sort_point = map->GetSortPoint();
        map_layer      = map->GetMapLayer();
        map_sort_key   = static_cast<uint8_t>(map->GetTileOcclusion());
    }

    const auto& light_to_view  = CreateModelViewMatrix(GameView::AxisAligned,
        settings.camera.position,
        settings.camera.scale, settings.camera.rotation);

    const auto& light_direction = node_light->GetDirection();
    const auto& light_position  = glm::vec3(0.0f, 0.0f, 0.0f);

    auto light_copy = std::make_shared<gfx::BasicLight>(*light_node.light);
    light_copy->world_direction = math::TransformDirection(light_to_world, light_direction);
    light_copy->world_position  = math::TransformPosition(light_to_world, light_position);
    light_copy->view_direction  = math::TransformDirection(light_to_view * light_to_world, light_direction);
    light_copy->view_position   = math::TransformPosition(light_to_view * light_to_world, light_position);

    // todo: should we use the light map sort key somewhere?

    Light light;
    light.light        = std::move(light_copy);
    light.transform    = light_to_world;
    light.render_layer = entity.GetRenderLayer();
    light.map_layer    = map_layer;
    light.packet_index = node_light->GetLayer();
    light.sort_point   = light_to_world * glm::vec4{map_sort_point, 0.0f, 1.0};
    lights.push_back(std::move(light));
}

void Renderer::OffsetPacketLayers(std::vector<DrawPacket>& packets, std::vector<Light>& lights) const
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
    for (auto& light : lights)
    {
        first_packet_index = std::min(first_packet_index, light.packet_index);
        first_render_layer = std::min(first_render_layer, light.render_layer);
    }

    // offset the layers.
    for (auto &packet : packets)
    {
        packet.packet_index -= first_packet_index;
        packet.render_layer -= first_render_layer;
        ASSERT(packet.packet_index >= 0 && packet.render_layer >= 0);
    }

    // offset the lights
    for (auto& light : lights)
    {
        light.packet_index -= first_packet_index;
        light.render_layer -= first_render_layer;
        ASSERT(light.packet_index >= 0 && light.render_layer >= 0);
    }
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

    const auto layer_tile_size = glm::vec3 { layer_tile_width_units,
                                             layer_tile_height_units,
                                             layer_tile_depth_units };
    const auto& klass = layer.GetClass();

    for (unsigned row=tile_row; row<max_row; ++row)
    {
        auto last_palette_index = LayerTraits::MaxPaletteIndex;

        for (unsigned col=tile_col; col<max_col; ++col)
        {
            // the tiles index value is an index into the tile map
            // sprite palette.
            const auto palette_index = ptr->GetTile(row, col).index;
            // special index max means "no value". this is needed in order
            // to be able to have "holes" in some layer and let the layer
            // below show through. could have used zero but that would
            // make the palette indexing more inconvenient.
            if (palette_index == LayerTraits::MaxPaletteIndex)
                continue;

            // if the material changes start a new tile batch.
            if (last_palette_index != palette_index)
            {
                batches.emplace_back();
                auto& batch = batches.back();
                batch.material     = GetTileMaterial(map, layer_index, palette_index);
                batch.sort_key     = static_cast<uint8_t>(klass.GetPaletteOcclusion(palette_index));
                batch.layer_index  = klass.GetLayer();
                batch.depth        = klass.GetDepth();
                batch.render_layer = map.GetRenderLayer();
                batch.row          = row;
                batch.col          = col;
                batch.tile_size    = layer_tile_size;
                batch.type         = TileBatch::Type::Render;
            }

            // append to tile to the current batch
            auto& batch = batches.back();
            gfx::TileBatch::Tile tile;
            tile.pos.x  = col;
            tile.pos.y  = row;
            tile.pos.z  = klass.GetDepth();
            tile.data.x = klass.GetPaletteMaterialTileIndex(palette_index);
            batch.tiles.push_back(tile);

            // keep track of the last material used.
            last_palette_index = use_batching ? palette_index : LayerTraits::MaxPaletteIndex;
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

    const auto layer_tile_size = glm::vec3 { layer_tile_width_units,
                                             layer_tile_height_units,
                                             layer_tile_depth_units };

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
            tile.pos.x = float(col);
            tile.pos.y = float(row);
            tile.pos.z = float(layer.GetDepth());

            TileBatch batch;
            batch.tiles.push_back(tile);
            batch.material     = gfx::CreateMaterialInstance(gfx::CreateMaterialClassFromColor(color));
            batch.depth        = layer.GetDepth();
            batch.layer_index  = layer.GetLayer();
            batch.row          = row;
            batch.col          = col;
            batch.render_layer = map.GetRenderLayer();
            batch.type         = TileBatch::Type::Data;
            batch.tile_size    = layer_tile_size;
            batches.push_back(std::move(batch));
        }
    }
}

void Renderer::SortTilePackets(std::vector<DrawPacket>& packets) const
{
    // layer is the layer index coming from the tilemap
    // the sorting order applies *inside* a render layer.
    // the render layer sorting happens later, so as long
    // as the relative ordering is correct the renderer
    // will then put the packets in render layers properly.

    // layer 0, row 0, col 0, sk = 0
    // layer 0, row 0, col 0, sk = 1
    // layer 0, row 0, col 1, sk = 0
    // layer 0, row 0, col 1, sk = 1
    // ...
    // layer 0, row 1, col 1, sk = 0
    // layer 0, row 1, col 1, sk = 1
    
    std::sort(packets.begin(), packets.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.map_layer < rhs.map_layer)
            return true;
        else if (lhs.map_layer == rhs.map_layer)
        {
            if (lhs.map_row < rhs.map_row)
                return true;
            else if (lhs.map_row == rhs.map_row)
            {
                if (lhs.map_col < rhs.map_col)
                    return true;
                else if (lhs.map_col == rhs.map_col)
                {
                    if (lhs.map_sort_key < rhs.map_sort_key)
                        return true;
                    else if (lhs.map_sort_key == rhs.map_sort_key)
                    {
                        if (static_cast<int>(lhs.source) < static_cast<int>(rhs.source))
                            return true;
                    }
                }
            }
        }
        return false;
    });
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
        const auto scene_world_pos = glm::vec4{packet.sort_point, 0.0, 1.0};
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

