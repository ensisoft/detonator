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
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <mutex>

#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/index.h"
#include "game/entity.h"
#include "game/tree.h"
#include "game/types.h"
#include "game/enum.h"
#include "game/scene_class.h"
#include "game/scriptvar.h"

namespace game
{
    class Tilemap;

    // Scene is the runtime representation of a scene based on some scene class
    // object instance. When a new Scene instance is created the scene class
    // and its scene graph (render tree) is traversed. Each EntityPlacement is
    // then used to as the initial data for a new Entity instance. I.e. Entities
    // are created using the parameters of the corresponding EntityPlacement.
    // While the game runs entities can then be created/destroyed dynamically
    // as part of the game play.
    class Scene
    {
    public:
        using RenderTree      = game::RenderTree<Entity>;
        using RenderTreeNode  = Entity;
        using RenderTreeValue = Entity;
        using SpatialIndex    = game::SpatialIndex<EntityNode>;
        using BloomFilter     = SceneClass::BloomFilter;

        explicit Scene(std::shared_ptr<const SceneClass> klass);
        explicit Scene(const SceneClass& klass);
        Scene(const Scene& other) = delete;
       ~Scene();

        // Get the entity by index. The index must be valid.
        Entity& GetEntity(size_t index);
        // Find entity by id. Returns nullptr if  no such node could be found.
        Entity* FindEntityByInstanceId(const std::string& id);
        // Find entity by name. Returns nullptr if no such entity could be found.
        // Note that if there are multiple entities with the same instance
        // name it's undefined which one is returned.
        Entity* FindEntityByInstanceName(const std::string& name);
        // List all entities of the given class identified by its class name.
        std::vector<Entity*> ListEntitiesByClassName(const std::string& name);
        // List all entities of the given class identified by its class name.
        std::vector<const Entity*> ListEntitiesByClassName(const std::string& name) const;
        // List entities that have a matching tag string.
        std::vector<Entity*> ListEntitiesByTag(const std::string& tag);
        // List entities that have a matching tag string.
        std::vector<const Entity*> ListEntitiesByTag(const std::string& tag) const;
        // Get the node by index. The index must be valid.
        const Entity& GetEntity(size_t index) const;
        // Find entity by id. Returns nullptr if  no such node could be found.
        const Entity* FindEntityByInstanceId(const std::string& id) const;
        // Find entity by name. Returns nullptr if no such entity could be found.
        // Note that if there are multiple entities with the same instance
        // name it's undefined which one is returned.
        const Entity* FindEntityByInstanceName(const std::string& name) const;
        // Kill entity and mark it for removal later. Note that this will propagate
        // the kill to all the entity's children as well. If this is not what
        // you want then unlink the appropriate children first.
        void KillEntity(Entity* entity);
        // Spawn a new entity in the scene. Returns a pointer to the spawned
        // entity or nullptr if spawning failed.
        // if link_to_root is true the new entity is automatically linked to the
        // root of the scene graph. Otherwise, you can use LinkEntity to link
        // to any other entity or later RelinkEntity to relink to some other
        // entity in the scene graph.
        Entity* SpawnEntity(const EntityArgs& args, bool link_to_root = true);

        // Prepare the scene for the next iteration of the game loop.
        void BeginLoop();
        // Perform end of game loop iteration cleanup etc.
        void EndLoop();
        // Find a scripting variable.
        // Returns nullptr if there was no variable by this name.
        // Note that the const here only implies that the object
        // may not change in terms of c++ semantics. The actual *value*
        // can still be changed as long as the variable is not read only.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        // Value aggregate for nodes (entities) in the scene.
        struct ConstSceneNode {
            // The transformation matrix for transforming the
            // entity into the scene.
            glm::mat4 node_to_scene;
            // Visual representation object of the entity in the scene.
            union {
                const Entity* entity = nullptr;
                // The entity object in the scene.x
                const Entity* placement; // = nullptr;
            };
        };
        // Collect the entities in the scene into a flat list.
        std::vector<ConstSceneNode> CollectNodes() const;

        // Value aggregate for nodes (entities) in the scene.
        // Keep in mind that mutating any entity data can invalidate
        // the matrices. I.e. when entities are linked mutating the
        // parent entity invalidates the child nodes' matrices.
        struct SceneNode {
            // The transformation matrix for transforming the
            // entity into the scene.
            glm::mat4 node_to_scene;
            // Visual representation object of the entity in the scene.
            union {
                Entity* entity = nullptr;
                // The entity object in the scene.
                Entity* placement; // = nullptr;
            };
        };
        // Collect the entities in the scene into a flat list.
        std::vector<SceneNode> CollectNodes();

        // Find the transformation for transforming nodes
        // from this entity's space into scene's coordinate space.
        // If the entity is not part of this scene the result is undefined.
        glm::mat4 FindEntityTransform(const Entity* entity) const;
        // Find the complete transformation for transforming the given entity node's model
        // into the scene's coordinate space.
        // If the entity is not part of this scene or if the node is not part
        // of the entity the result is undefined.
        glm::mat4 FindEntityNodeTransform(const Entity* entity, const EntityNode* node) const;
        // Find the bounding (axis aligned) rectangle that encloses
        // all the entity nodes' models. The returned rectangle is in
        // the scene coordinate space.
        // If the entity is not part of this scene the result is undefined.
        FRect FindEntityBoundingRect(const Entity* entity) const;
        // Find the axis-aligned bounding box (AABB) that encloses the
        // given entity node's model. The returned rectangle is in the scene
        // coordinate space. If the entity is not part of this scene or
        // if the node is not part of the entity the result is undefined.
        FRect FindEntityNodeBoundingRect(const Entity* entity, const EntityNode* node) const;
        // Find the oriented bounding box (OOB) that encloses the
        // given entity node's model. The returned box is in the scene coordinate space
        // and has the dimensions and orientation corresponding to the node's
        // model transformation. If the entity is not part of this scene or
        // the node is not part of the entity the result is undefined.
        FBox FindEntityNodeBoundingBox(const Entity* entity, const EntityNode* node) const;
        // Map a directional (unit length) vector relative to some entity node basis
        // vectors into world/scene basis vectors. Returns a normalized vector relative
        // to the scene/world basis vectors.
        // If the entity is not in the scene or if the node is not part of the
        // entity the result is undefined.
        glm::vec2 MapVectorFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const;
        glm::vec3 MapVectorFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec3& vector) const;
        // Map a point relative to the entity node's local origin into a world space point.
        // If the entity is not in the scene or if the node is not part of the entity
        // the result is undefined.
        glm::vec2 MapPointFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& point) const;

        glm::vec2 MapVectorToEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const;
        // Map a world point in world (scene) space to the entity node coordinate space.
        // In other words returns the vector that describes the point relative to the entity node's origin.
        // If the entity is not in the scene or if the node is not part of the entity
        // the result is undefined.
        glm::vec2 MapPointToEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& point) const;

        struct EntityTimerEvent {
            Entity* entity = nullptr;
            Entity::TimerEvent event;
        };
        struct EntityEventPostedEvent {
            Entity* entity = nullptr;
            Entity::PostedEvent event;
        };
        using Event = std::variant<EntityTimerEvent,
                EntityEventPostedEvent>;

        void Update(float dt, std::vector<Event>* events = nullptr);

        void Rebuild();

        inline void QuerySpatialNodes(const FRect& area_of_interest, std::set<EntityNode*>* result)
        { query_spatial_nodes_by_rect(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::set<const EntityNode*>* result) const
        { query_spatial_nodes_by_rect(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::vector<EntityNode*>* result)
        { query_spatial_nodes_by_rect(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::vector<const EntityNode*>* result) const
        { query_spatial_nodes_by_rect(area_of_interest, result); }

        using SpatialQueryMode = SpatialIndex::QueryMode;

        inline void QuerySpatialNodes(const FPoint& point, std::set<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_point(point, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, std::set<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_point(point, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, std::vector<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_point(point, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, std::vector<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_point(point, result, mode); }

        inline void QuerySpatialNodes(const FPoint& point, float radius, std::set<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_point_radius(point, radius, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, float radius, std::set<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_point_radius(point, radius, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, float radius, std::vector<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_point_radius(point, radius, result, mode); }
        inline void QuerySpatialNodes(const FPoint& point, float radius, std::vector<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_point_radius(point, radius, result, mode); }

        inline void QuerySpatialNodes(const FPoint& a, const FPoint& b, std::set<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_line(a, b, result, mode); }
        inline void QuerySpatialNodes(const FPoint& a, const FPoint& b, std::set<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_line(a, b, result, mode); }
        inline void QuerySpatialNodes(const FPoint& a, const FPoint& b, std::vector<EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All)
        { query_spatial_nodes_by_line(a, b, result, mode); }
        inline void QuerySpatialNodes(const FPoint& a, const FPoint& b, std::vector<const EntityNode*>* result, SpatialQueryMode mode = SpatialQueryMode::All) const
        { query_spatial_nodes_by_line(a, b, result, mode); }

        const Tilemap* GetMap() const noexcept
        { return mMap; }
        Tilemap* GetMap() noexcept
        { return mMap; }
        void SetMap(Tilemap* map) noexcept
        { mMap = map; }

        const SpatialIndex* GetSpatialIndex() const noexcept
        { return mSpatialIndex.get(); }
        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }
        // Get current scene runtime in seconds since the scene was first created.
        double GetTime() const noexcept
        { return mCurrentTime; }
        // Check whether the scene has a spatial index or not.
        bool HasSpatialIndex() const noexcept
        { return !!mSpatialIndex; }
        // Get the current number of entities in the scene.
        size_t GetNumEntities() const noexcept
        { return mEntities.size(); }
        // Get the scene's class name.
        const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        // Get the scenes' class ID.
        const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        // Check whether the scene has a script file ID or not.
        const std::string& GetScriptFileId() const noexcept
        { return mClass->GetScriptFileId(); }
        // Get the bloom filter settings if any.
        const BloomFilter* GetBloom() const noexcept
        { return mClass->GetBloom(); }
        // Get access to the scene class object.
        const SceneClass& GetClass() const noexcept
        { return *mClass.get(); }
        const SceneClass* operator->() const noexcept
        { return mClass.get(); }
        // Disabled.
        Scene& operator=(const Scene&) = delete;
    private:
        template<typename ResultContainer>
        void query_spatial_nodes_by_point(const FPoint& point, ResultContainer* result, SpatialQueryMode mode) const
        {
            if (mSpatialIndex)
                mSpatialIndex->Query(point, result, mode);
        }
        template<typename ResultContainer>
        void query_spatial_nodes_by_point_radius(const FPoint& point, float radius, ResultContainer* result, SpatialQueryMode mode) const
        {
            if (mSpatialIndex)
                mSpatialIndex->Query(point, radius, result, mode);
        }
        template<typename ResultContainer>
        void query_spatial_nodes_by_rect(const FRect& rect, ResultContainer* result) const
        {
            if (mSpatialIndex)
                mSpatialIndex->Query(rect, result);
        }
        template<typename ResultContainer>
        void query_spatial_nodes_by_line(const FPoint& a, const FPoint& b, ResultContainer* result, SpatialQueryMode mode) const
        {
            if (mSpatialIndex)
                mSpatialIndex->Query(a, b, result, mode);
        }

    private:
        // the class object.
        std::shared_ptr<const SceneClass> mClass;
        // Entities currently in the scene.
        std::vector<std::unique_ptr<Entity>> mEntities;
        // lookup table for mapping entity ids to entities.
        std::unordered_map<std::string, Entity*> mIdMap;
        // lookup table for mapping entity names to entities.
        // the names are *human-readable* and set by the designer
        // so it's possible that there could be name collisions.
        std::unordered_map<std::string, Entity*> mNameMap;
        // The list of script variables.
        std::vector<ScriptVar> mScriptVars;
        // The scene graph/render tree for hierarchical traversal
        // of the scene.
        RenderTree mRenderTree;
        // the current scene time.
        double mCurrentTime = 0.0;
        // kill set of entities that were killed but have
        // not yet been removed from the scene.
        std::unordered_set<Entity*> mKillSet;
        // Spatial index for object (entity node) queries (if any)
        std::unique_ptr<SpatialIndex> mSpatialIndex;
        // for convenience..
        Tilemap* mMap = nullptr;

        struct SpawnRecord {
            double spawn_time = 0.0f;
            std::unique_ptr<Entity> instance;
        };
        // spawn list of new entities that have been spawned
        // but not yet placed in the scene's entity list.
        std::vector<SpawnRecord> mSpawnList;

        struct AsyncSpawnState {
            std::mutex mutex;
            std::vector<SpawnRecord> spawn_list;
        };
        std::shared_ptr<AsyncSpawnState> mAsyncSpawnState;
    };

    std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass);
    std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass);

} // namespace
