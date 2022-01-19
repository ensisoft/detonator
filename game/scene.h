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
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <set>

#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/index.h"
#include "game/entity.h"
#include "game/tree.h"
#include "game/types.h"
#include "game/enum.h"
#include "game/scriptvar.h"

namespace game
{
    // SceneNodeClass holds the SceneClass node data.
    // I.e. the nodes in the scene class act as the placeholders
    // for the initial/static content in the scene. When a new
    // scene instance is created the initial entities in the scene
    // are created and positioned based on the SceneClass and its nodes.
    // For each SceneNodeClass a new entity object is then created.
    class SceneNodeClass
    {
    public:
        using Flags = Entity::Flags;

        struct ScriptVarValue {
            std::string id;
            ScriptVar::VariantType value;
        };

        SceneNodeClass()
        {
            mClassId = base::RandomString(10);
            SetFlag(Flags::VisibleInGame, true);
            SetFlag(Flags::VisibleInEditor, true);
        }
        // class setters.
        void SetFlag(Flags flag, bool on_off)
        {
            mFlagValBits.set(flag, on_off);
            mFlagSetBits.set(flag, true);
        }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetRotation(float rotation)
        { mRotation = rotation; }
        void SetEntityId(const std::string& id)
        { mEntityId = id; }
        void SetName(const std::string& name)
        { mName = name; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void SetIdleAnimationId(const std::string& id)
        { mIdleAnimationId = id; }
        void SetParentRenderTreeNodeId(const std::string& id)
        { mParentRenderTreeNodeId = id; }
        void SetEntity(std::shared_ptr<const EntityClass> klass)
        {
            mEntityId = klass->GetId();
            mEntity   = klass;
        }
        void ResetEntity()
        {
            mEntityId.clear();
            mEntity.reset();
        }
        void ResetEntityParams()
        {
            mIdleAnimationId.clear();
            mLifetime.reset();
            mFlagSetBits.clear();
            mFlagValBits.clear();
            mScriptVarValues.clear();
        }
        void ResetLifetime()
        { mLifetime.reset(); }
        void SetLifetime(double lifetime)
        { mLifetime = lifetime; }

        // class getters.
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetScale() const
        { return mScale; }
        float GetRotation() const
        { return mRotation; }
        std::string GetName() const
        { return mName; }
        std::string GetId() const
        { return mClassId; }
        std::string GetEntityId() const
        { return mEntityId; }
        std::string GetIdleAnimationId() const
        { return mIdleAnimationId; }
        std::string GetParentRenderTreeNodeId() const
        { return mParentRenderTreeNodeId; }
        std::shared_ptr<const EntityClass> GetEntityClass() const
        { return mEntity; }
        bool TestFlag(Flags flag) const
        { return mFlagValBits.test(flag); }
        int GetLayer() const
        { return mLayer; }
        double GetLifetime() const
        { return mLifetime.value_or(0.0); }
        bool HasSpecifiedParentNode() const
        { return !mParentRenderTreeNodeId.empty(); }
        bool HasIdleAnimationSetting() const
        { return !mIdleAnimationId.empty(); }
        bool HasLifetimeSetting() const
        { return mLifetime.has_value(); }
        bool HasFlagSetting(Flags flag) const
        { return mFlagSetBits.test(flag); }
        void ClearFlagSetting(Flags flag)
        { mFlagSetBits.set(flag, false); }
        std::size_t GetNumScriptVarValues() const
        { return mScriptVarValues.size(); }
        ScriptVarValue& GetGetScriptVarValue(size_t index)
        { return base::SafeIndex(mScriptVarValues, index); }
        const ScriptVarValue& GetScriptVarValue(size_t index) const
        { return base::SafeIndex(mScriptVarValues, index); }
        void AddScriptVarValue(const ScriptVarValue& value)
        { mScriptVarValues.push_back(value);}
        void AddScriptVarValue(ScriptVarValue&& value)
        { mScriptVarValues.push_back(std::move(value)); }
        ScriptVarValue* FindScriptVarValueById(const std::string& id);
        const ScriptVarValue* FindScriptVarValueById(const std::string& id) const;
        bool DeleteScriptVarValueById(const std::string& id);
        void SetScriptVarValue(const ScriptVarValue& value);

        void ClearStaleScriptValues(const EntityClass& klass);

        // Get the node hash value based on the properties.
        std::size_t GetHash() const;

        // Get this node's transform relative to its parent.
        glm::mat4 GetNodeTransform() const;

        // Make a clone of this node. The cloned node will
        // have all the same property values but a unique id.
        SceneNodeClass Clone() const;

        // Serialize node into JSON.
        void IntoJson(data::Writer& data) const;

        // Load node and its properties from JSON. Returns nullopt
        // if there was a problem.
        static std::optional<SceneNodeClass> FromJson(const data::Reader& data);
    private:
        // The node's unique class id.
        std::string mClassId;
        // The id of the entity this node contains.
        std::string mEntityId;
        // When the scene node (entity) is linked (parented)
        // to another scene node (entity) this id is the
        // node in the parent entity's render tree that is to
        // be used as the parent of this entity's nodes.
        std::string mParentRenderTreeNodeId;
        // The human readable name for the node.
        std::string mName;
        // The position of the node relative to its parent.
        glm::vec2   mPosition = {0.0f, 0.0f};
        // The scale of the node relative to its parent.
        glm::vec2   mScale = {1.0f, 1.0f};
        // The rotation of the node relative to its parent.
        float mRotation = 0.0f;
        // Node bitflags. the bits are doubled because a bit
        // is needed to indicate whether a bit is set or not
        base::bitflag<Flags> mFlagValBits;
        base::bitflag<Flags> mFlagSetBits;
        // the relative render order (layer index)
        int mLayer = 0;
        // the track id of the idle animation if any.
        // this setting will override the entity class idle
        // track designation if set.
        std::string mIdleAnimationId;
        std::optional<double> mLifetime;
        std::vector<ScriptVarValue> mScriptVarValues;
    private:
        // This is the runtime class reference to the
        // entity class that this node uses. Before creating
        // a scene instance it's important that this entity
        // reference is resolved to a class object instance.
        std::shared_ptr<const EntityClass> mEntity;
    };

    // SceneClass provides the initial structure of the scene with 
    // initial placement of entities etc.
    class SceneClass
    {
    public:
        using RenderTree      = game::RenderTree<SceneNodeClass>;
        using RenderTreeNode  = SceneNodeClass;
        using RenderTreeValue = SceneNodeClass;

        enum class SpatialIndex {
            Disabled,
            QuadTree,
            DenseGrid
        };
        struct QuadTreeArgs {
            unsigned max_items = 4;
            unsigned max_levels = 3;
        };
        struct DenseGridArgs {
            unsigned num_rows = 1;
            unsigned num_cols = 1;
        };

        SceneClass();
        // Copy construct a deep copy of the scene.
        SceneClass(const SceneClass& other);

        // Add a new node to the scene.
        // Returns a pointer to the node that was added to the scene.
        // Note that the node is not yet added to the scene graph
        // and as such will not be considered for rendering etc.
        // You probably want to link the node to some other node.
        // See LinkChild.
        SceneNodeClass* AddNode(const SceneNodeClass& node);
        SceneNodeClass* AddNode(SceneNodeClass&& node);
        SceneNodeClass* AddNode(std::unique_ptr<SceneNodeClass> node);

        // Get the node by index. The index must be valid.
        SceneNodeClass& GetNode(size_t index);
        // Find scene node by name. Returns nullptr if
        // no such node could be found.
        SceneNodeClass* FindNodeByName(const std::string& name);
        // Find scene node by id. Returns nullptr if
        // no such node could be found.
        SceneNodeClass* FindNodeById(const std::string& id);
        // Get the scene node by index. The index must be valid.
        const SceneNodeClass& GetNode(size_t index) const;
        // Find scene node by class name. Returns nullptr if
        // no such node could be found.
        const SceneNodeClass* FindNodeByName(const std::string& name) const;
        // Find scene node by class id. Returns nullptr if
        // no such node could be found.
        const SceneNodeClass* FindNodeById(const std::string& id) const;

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity class object.
        void LinkChild(SceneNodeClass* parent, SceneNodeClass* child);
        // Break a child node away from its parent. The child node needs
        // to be a valid node and needs to point to a node that is added
        // to the render tree and belongs to this scene class object.
        // The child (and all of its children) that has been broken still
        // exists in the entity but is removed from the render tree.
        // You can then either DeleteNode to completely delete it or
        // LinkChild to insert it into another part of the render tree.
        void BreakChild(SceneNodeClass* child, bool keep_world_transform = true);
        // Re-parent a child node from its current parent to another parent.
        // Both the child node and the parent node to be a valid nodes and
        // need to point to nodes that are part of the render tree and belong
        // to this entity class object. This will move the whole hierarchy of
        // nodes starting from child under the new parent.
        // If keep_world_transform is true the child will be transformed such
        // that it's current world transformation remains the same. I.e  it's
        // position and rotation in the world don't change.
        void ReparentChild(SceneNodeClass* parent, SceneNodeClass* child, bool keep_world_transform = true);

        // Delete a node from the scene. The given node and all of its
        // children will be removed from the scene graph and then deleted.
        void DeleteNode(SceneNodeClass* node);

        // Duplicate an entire node hierarchy starting at the given node
        // and add the resulting hierarchy to node's parent.
        // Returns the root node of the new node hierarchy.
        SceneNodeClass* DuplicateNode(const SceneNodeClass* node);

        // Value aggregate for scene nodes that represent placement
        // of entities in the scene.
        struct ConstSceneNode {
            // The transform matrix that applies to this entity (node)
            // in order to transform it to the scene.
            glm::mat4 node_to_scene;
            // The entity representation in the scene.
            std::shared_ptr<const EntityClass> visual_entity;
            // The entity object in the scene.
            const SceneNodeClass* entity_object = nullptr;
        };
        // Collect nodes from the scene into a flat list.
        std::vector<ConstSceneNode> CollectNodes() const;

        // Value aggregate for the nodes that represent placement
        // of entities in the scene.
        struct SceneNode {
            // The transform matrix that applies to this entity (node)
            // in order to transform it to the scene.
            glm::mat4 node_to_scene;
            // The entity representation in the scene.
            std::shared_ptr<const EntityClass> visual_entity;
            // The entity object in the scene.
            SceneNodeClass* entity_object = nullptr;
        };
        // Collect nodes from the scene into a flat list.
        std::vector<SceneNode> CollectNodes();

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node in the scene.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const glm::vec2& pos, std::vector<SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;
        void CoarseHitTest(const glm::vec2& pos, std::vector<const SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        glm::vec2 MapCoordsFromNodeBox(float x, float y, const SceneNodeClass* node) const;
        glm::vec2 MapCoordsFromNodeBox(const glm::vec2& pos, const SceneNodeClass* node) const;
        // Map coordinates in scene coordinate space into node's OOB coordinate space.
        glm::vec2 MapCoordsToNodeBox(float x, float y, const SceneNodeClass* node) const;
        glm::vec2 MapCoordsToNodeBox(const glm::vec2& pos, const SceneNodeClass* node) const;

        glm::mat4 FindNodeTransform(const SceneNodeClass* node) const;

        // Add a new scripting variable to the list of variables.
        // No checks are made to whether a variable by that name
        // already exists.
        void AddScriptVar(const ScriptVar& var);
        void AddScriptVar(ScriptVar&& var);
        // Delete the scripting variable at the given index.
        // The index must be a valid index.
        void DeleteScriptVar(size_t index);
        // Set the properties (copy over) the scripting variable at the given index.
        // The index must be a valid index.
        void SetScriptVar(size_t index, const ScriptVar& var);
        void SetScriptVar(size_t index, ScriptVar&& var);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        ScriptVar& GetScriptVar(size_t index);
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        ScriptVar* FindScriptVarByName(const std::string& name);
        ScriptVar* FindScriptVarById(const std::string& id);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        const ScriptVar& GetScriptVar(size_t index) const;
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        // Get the object hash value based on the property values.
        std::size_t GetHash() const;
        // Return number of scene nodes contained in the scene.
        std::size_t GetNumNodes() const
        { return mNodes.size();}
        std::size_t GetNumScriptVars() const
        { return mScriptVars.size(); }
        // Get the scene class object id.
        std::string GetId() const
        { return mClassId; }
        // Get the human-readable name of the class.
        std::string GetName() const
        { return mName; }
        // Get the associated script file ID.
        std::string GetScriptFileId() const
        { return mScriptFile; }
        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }
        SpatialIndex GetDynamicSpatialIndex() const
        { return mDynamicSpatialIndex; }
        FRect GetDynamicSpatialRect() const
        { return mDynamicSpatialRect; }
        const QuadTreeArgs& GetQuadTreeArgs() const
        { return mQuadTreeArgs; }
        const DenseGridArgs& GetDenseGridArgs() const
        { return mDenseGridArgs; }

        void SetDynamicSpatialIndexArgs(const DenseGridArgs& args)
        { mDenseGridArgs = args; }
        void SetDynamicSpatialIndexArgs(const QuadTreeArgs& args)
        { mQuadTreeArgs = args; }
        void SetDynamicSpatialIndex(SpatialIndex index)
        { mDynamicSpatialIndex = index; }
        void SetDynamicSpatialRect(const FRect& rect)
        { mDynamicSpatialRect = rect; }
        void SetName(const std::string& name)
        { mName = name; }
        void SetScriptFileId(const std::string& file)
        { mScriptFile = file; }
        bool HasScriptFile() const
        { return !mScriptFile.empty(); }
        bool IsDynamicSpatialIndexEnabled() const
        { return mDynamicSpatialIndex != SpatialIndex::Disabled; }
        void ResetScriptFile()
        { mScriptFile.clear(); }

        // Serialize the scene into JSON.
        void IntoJson(data::Writer& data) const;

        // Load the SceneClass from JSOn. Returns std::nullopt if there was a problem.
        static std::optional<SceneClass> FromJson(const data::Reader& data);

        // Make a clone of this scene. The cloned scene will
        // have all the same property values as its source
        // but a unique class id.
        SceneClass Clone() const;

        // Make a deep copy on assignment.
        SceneClass& operator=(const SceneClass& other);

    private:
        // the class / resource of this class.
        std::string mClassId;
        // the human readable name of the class.
        std::string mName;
        // the id of the associated script file (if any)
        std::string mScriptFile;
        // storing by unique ptr so that the pointers
        // given to the render tree don't become invalid
        // when new nodes are added to the scene.
        std::vector<std::unique_ptr<SceneNodeClass>> mNodes;
        // scenegraph / render tree for hierarchical traversal
        // and transformation of the animation nodes. the tree defines
        // the parent-child transformation hierarchy.
        RenderTree mRenderTree;
        // Scripting variables.
        std::vector<ScriptVar> mScriptVars;
        SpatialIndex mDynamicSpatialIndex = SpatialIndex::Disabled;
        FRect mDynamicSpatialRect;
        QuadTreeArgs mQuadTreeArgs;
        DenseGridArgs mDenseGridArgs;
    };

    // Scene is the runtime representation of a scene based on some scene class
    // object instance. When a new Scene instance is created the scene class
    // and its scene graph (render tree) is traversed. Each SceneNodeClass is
    // then used to as the initial data for a new Entity instance. I.e. Entities
    // are created using the parameters of the corresponding SceneNodeClass.
    // While the game runs entities can then be created/destroyed dynamically
    // as part of the game play.
    class Scene
    {
    public:
        using RenderTree      = game::RenderTree<Entity>;
        using RenderTreeNode  = Entity;
        using RenderTreeValue = Entity;
        using SpatialIndex    = game::SpatialIndex<EntityNode>;

        Scene(std::shared_ptr<const SceneClass> klass);
        Scene(const SceneClass& klass);
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
            const Entity* visual_entity = nullptr;
            // The entity object in the scene.x
            const Entity* entity_object = nullptr;
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
            Entity* visual_entity = nullptr;
            // The entity object in the scene.
            Entity* entity_object = nullptr;
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
        glm::vec2 MapEntityNodeVector(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const;
        // Map a point relative to the entity node's local origin into a world space point.
        // If the entity is not in the scene or if the node is not part of the entity
        // the result is undefined.
        FPoint MapEntityNodePoint(const Entity* entity, const EntityNode* node, const FPoint& point) const;

        void Update(float dt);

        void UpdateSpatialIndex();

        inline void QuerySpatialNodes(const FRect& area_of_interest, std::set<EntityNode*>* result)
        { query_spatial_nodes(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::set<const EntityNode*>* result) const
        { query_spatial_nodes(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::vector<EntityNode*>* result)
        { query_spatial_nodes(area_of_interest, result); }
        inline void QuerySpatialNodes(const FRect& area_of_interest, std::vector<const EntityNode*>* result) const
        { query_spatial_nodes(area_of_interest, result); }
        inline void QuerySpatialNodes(const FPoint& point, std::set<EntityNode*>* result)
        { query_spatial_nodes(point, result); }
        inline void QuerySpatialNodes(const FPoint& point, std::set<const EntityNode*>* result) const
        { query_spatial_nodes(point, result); }
        inline void QuerySpatialNodes(const FPoint& point, std::vector<EntityNode*>* result)
        { query_spatial_nodes(point, result); }
        inline void QuerySpatialNodes(const FPoint& point, std::vector<const EntityNode*>* result) const
        { query_spatial_nodes(point, result); }

        const SpatialIndex* GetSpatialIndex() const
        { return mSpatialIndex.get(); }
        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }
        double GetTime() const
        { return mCurrentTime; }
        bool HasSpatialIndex() const
        { return !!mSpatialIndex; }
        // Get the current number of entities in the scene.
        size_t GetNumEntities() const
        { return mEntities.size(); }
        std::string GetClassName() const
        { return mClass->GetName(); }
        std::string GetClassId() const
        { return mClass->GetId(); }

        // Get access to the scene class object.
        const SceneClass& GetClass() const
        { return *mClass.get(); }
        const SceneClass* operator->() const
        { return mClass.get(); }
        // Disabled.
        Scene& operator=(const Scene&) = delete;
    private:
        template<typename Predicate, typename ResultContainer>
        void query_spatial_nodes(const Predicate& predicate, ResultContainer* result) const
        {
            if (mSpatialIndex)
                mSpatialIndex->Query(predicate, result);
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
        // spawn list of new entities that have been spawned
        // but not yet placed in the scene's entity list.
        std::vector<std::unique_ptr<Entity>> mSpawnList;
        // kill set of entities that were killed but have
        // not yet been removed from the scene.
        std::unordered_set<Entity*> mKillSet;
        // Spatial index for object (entity node) queries (if any)
        std::unique_ptr<SpatialIndex> mSpatialIndex;
    };

    std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass);
    std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass);

} // namespace
