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

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>

#include "base/bitflag.h"
#include "gamelib/entity.h"
#include "gamelib/tree.h"
#include "gamelib/types.h"
#include "gamelib/enum.h"

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

        SceneNodeClass()
        {
            mClassId = base::RandomString(10);
            mFlags.set(Flags::VisibleInGame, true);
            mFlags.set(Flags::VisibleInEditor, true);
        }
        // class setters.
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
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
        std::shared_ptr<const EntityClass> GetEntityClass() const
        { return mEntity; }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        int GetLayer() const
        { return mLayer; }

        // Get the node hash value based on the properties.
        std::size_t GetHash() const;

        // Get this node's transform relative to its parent.
        glm::mat4 GetNodeTransform() const;

        // Make a clone of this node. The cloned node will
        // have all the same property values but a unique id.
        SceneNodeClass Clone() const;

        // Serialize node into JSON.
        nlohmann::json ToJson() const;

        // Load node and its properties from JSON. Returns nullopt
        // if there was a problem.
        static std::optional<SceneNodeClass> FromJson(const nlohmann::json& json);
    private:
        // The node's unique class id.
        std::string mClassId;
        // The id of the entity this node contains.
        std::string mEntityId;
        // The human readable name for the node.
        std::string mName;
        // The position of the node relative to its parent.
        glm::vec2   mPosition = {0.0f, 0.0f};
        // The scale of the node relative to its parent.
        glm::vec2   mScale = {1.0f, 1.0f};
        // The rotation of the node relative to its parent.
        float mRotation = 0.0f;
        // Node bitflags.
        base::bitflag<Flags> mFlags;
        // the relative render order (layer index)
        int mLayer = 0;
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

        SceneClass()
        { mClassId = base::RandomString(10); }
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
        void BreakChild(SceneNodeClass* child);
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

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node in the scene.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some node's (see SceneNode::GetNodeTransform) space
        // into entity coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const;
        // Map coordinates in scene's coordinate space into some node's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const SceneNodeClass* node) const;

        // Get the object hash value based on the property values.
        std::size_t GetHash() const;
        // Return number of scene nodes contained in the scene.
        std::size_t GetNumNodes() const
        { return mNodes.size();}
        // Get the scene class object id.
        std::string GetId() const
        { return mClassId; }

        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        // Lookup a SceneNodeClass based on the serialized ID in the JSON.
        SceneNodeClass* TreeNodeFromJson(const nlohmann::json& json);

        // Serialize the scene into JSON.
        nlohmann::json ToJson() const;

        // Serialize a scene node contained in the RenderTree to JSON by doing
        // a shallow (id only) based serialization.
        // Later in TreeNodeFromJson when reading back the render tree we simply
        // look up the node based on the ID.
        static nlohmann::json TreeNodeToJson(const SceneNodeClass* node);

        // Load the SceneClass from JSOn. Returns std::nullopt if there was a problem.
        static std::optional<SceneClass> FromJson(const nlohmann::json& json);

        // Make a clone of this scene. The cloned scene will
        // have all the same property values as its source
        // but a unique class id.
        SceneClass Clone() const;

        // Make a deep copy on assignment.
        SceneClass& operator=(const SceneClass& other);

    private:
        // the class / resource of this class.
        std::string mClassId;
        // storing by unique ptr so that the pointers
        // given to the render tree don't become invalid
        // when new nodes are added to the scene.
        std::vector<std::unique_ptr<SceneNodeClass>> mNodes;
        // scenegraph / render tree for hierarchical traversal
        // and transformation of the animation nodes. the tree defines
        // the parent-child transformation hierarchy.
        RenderTree mRenderTree;
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

        Scene(std::shared_ptr<const SceneClass> klass);
        Scene(const SceneClass& klass);
        Scene(const Scene& other) = delete;

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

        void Update(float dt);

        // Get the scene's render tree (scene graph). The render tree defines
        // the relative transformations and the transformation hierarchy of the
        // scene class nodes in the scene.
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        // Get the current number of entities in the scene.
        size_t GetNumEntities() const
        { return mEntities.size(); }

        // Get access to the scene class object.
        const SceneClass& GetClass() const
        { return *mClass.get(); }
        const SceneClass* operator->() const
        { return mClass.get(); }
        // Disabled.
        Scene& operator=(const Scene&) = delete;
    private:
        // the class object.
        std::shared_ptr<const SceneClass> mClass;
        // Entities currently in the scene.
        std::vector<std::unique_ptr<Entity>> mEntities;
        // The scene graph/render tree for hierarchical traversal
        // of the scene.
        RenderTree mRenderTree;
        // the current scene time.
        double mCurrentTime = 0.0;
    };

    std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass);
    std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass);

} // namespace
