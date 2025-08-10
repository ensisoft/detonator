// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#include <memory>

#include "base/allocator.h"
#include "base/bitflag.h"
#include "data/fwd.h"

namespace game
{
    class SplineMoverClass;
    class SplineMover;
    class LinearMoverClass;
    class LinearMover;
    class RigidBodyClass;
    class RigidBody;
    class DrawableItemClass;
    class DrawableItem;
    class TextItemClass;
    class TextItem;
    class SpatialNodeClass;
    class SpatialNode;
    class FixtureClass;
    class Fixture;
    class MapNodeClass;
    class MapNode;
    class BasicLightClass;
    class BasicLight;
    class Entity;

    class EntityNodeClass
    {
    public:
        using DrawableItemType = DrawableItemClass;
        using TextItemType = TextItemClass;
        using LightClassType = BasicLightClass;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor
        };

        EntityNodeClass();
        EntityNodeClass(const EntityNodeClass& other);
        EntityNodeClass(EntityNodeClass&& other);

        // Get the class id.
        const std::string& GetId() const noexcept
        { return mClassId; }
        // Get the human-readable name for this class.
        const std::string& GetName() const noexcept
        { return mName; }
        // Get the human readable tag string for this node class.
        const std::string& GetTag() const noexcept
        { return mTag; }
        // Get the hash value based on the class object properties.
        std::size_t GetHash() const;

        // Get the node's translation relative to its parent node.
        const glm::vec2& GetTranslation() const noexcept
        { return mPosition; }
        // Get the node's scale factor. The scale factor applies to
        // whole hierarchy of nodes.
        const glm::vec2& GetScale() const noexcept
        { return mScale; }
        // Get the node's box size.
        const glm::vec2& GetSize() const noexcept
        { return mSize;}
        // Get node's rotation relative to its parent node.
        float GetRotation() const noexcept
        { return mRotation; }
        // Set the human-readable node name.
        void SetName(std::string name) noexcept
        { mName = std::move(name); }
        // Set the entity node tag string.
        void SetTag(std::string tag) noexcept
        { mTag = std::move(tag); }
        // Set the node's scale. The scale applies to all
        // the subsequent hierarchy, i.e. all the nodes that
        // are in the tree under this node.
        void SetScale(const glm::vec2& scale) noexcept
        { mScale = scale; }
        void SetScale(float sx, float sy) noexcept
        { mScale = glm::vec2(sx, sy); }
        // Set the node's translation relative to the parent
        // of this node.
        void SetTranslation(const glm::vec2& vec) noexcept
        { mPosition = vec; }
        void SetTranslation(float x, float y) noexcept
        { mPosition = glm::vec2(x, y); }
        // Set the node's containing box size.
        // The size is used to for example to figure out
        // the dimensions of rigid body collision shape (if any)
        // and to resize the drawable object.
        void SetSize(const glm::vec2& size) noexcept
        { mSize = size; }
        void SetSize(float width, float height) noexcept
        { mSize = glm::vec2(width, height); }
        // Set the starting rotation in radians around the z axis.
        void SetRotation(float angle) noexcept
        { mRotation = angle;}
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }

        // Attach a rigid body to this node class.
        void SetRigidBody(const RigidBodyClass& body);
        // Attach a simple static drawable item to this node class.
        void SetDrawable(const DrawableItemClass& drawable);
        // Attach a text item to this node class.
        void SetTextItem(const TextItemClass& text);
        // Attach a spatial index node to this node class.
        void SetSpatialNode(const SpatialNodeClass& node);
        // Attach a rigid body fixture to this node class.
        void SetFixture(const FixtureClass& fixture);
        // Attach a tilemap node to this node class.
        void SetMapNode(const MapNodeClass& map);
        // Attach a transformer to this node class
        void SetLinearMover(const LinearMoverClass& mover);

        void SetSplineMover(const SplineMoverClass& mover);

        void SetBasicLight(const BasicLightClass& light);

        // Create and attach a rigid body with default settings.
        void CreateRigidBody();
        // Create and attach a drawable with default settings.
        void CreateDrawable();
        // Create and attach a text item with default settings.
        void CreateTextItem();
        // Create and attach spatial index  node with default settings.
        void CreateSpatialNode();
        // Create and attach a fixture with default settings.
        void CreateFixture();
        // Create and attach a map node with default settings.
        void CreateMapNode();
        // Create and attach a transformer with default settings.
        void CreateLinearMover();

        void CreateSplineMover();

        void CreateBasicLight();

        void RemoveDrawable() noexcept
        { mDrawable.reset(); }
        void RemoveRigidBody() noexcept
        { mRigidBody.reset(); }
        void RemoveTextItem() noexcept
        { mTextItem.reset(); }
        void RemoveSpatialNode() noexcept
        { mSpatialNode.reset(); }
        void RemoveFixture() noexcept
        { mFixture.reset(); }
        void RemoveMapNode() noexcept
        { mMapNode.reset(); }
        void RemoveLinearMover() noexcept
        { mLinearMover.reset(); }
        void RemoveSplineMover() noexcept
        { mSplineMover.reset(); }
        void RemoveBasicLight() noexcept
        { mBasicLight.reset(); }

        // Get the rigid body shared class object if any.
        auto GetSharedRigidBody() const noexcept
        { return mRigidBody; }
        // Get the drawable shared class object if any.
        auto GetSharedDrawable() const noexcept
        { return mDrawable; }
        // Get the text item class object if any.
        auto GetSharedTextItem() const noexcept
        { return mTextItem; }
        // Get the spatial index node if any.
        auto GetSharedSpatialNode() const noexcept
        { return mSpatialNode; }
        // Get the fixture class if any
        auto GetSharedFixture() const noexcept
        { return mFixture; }
        // Get the map node class if any.
        auto GetSharedMapNode() const noexcept
        { return mMapNode; }
        auto GetSharedLinearMover() const noexcept
        { return mLinearMover; }
        auto GetSharedSplineMover() const noexcept
        { return mSplineMover; }
        auto GetSharedBasicLight() const noexcept
        { return mBasicLight; }


        // Returns true if a rigid body has been set for this class.
        bool HasRigidBody() const noexcept
        { return !!mRigidBody; }
        // Returns true if a drawable object has been set for this class.
        bool HasDrawable() const noexcept
        { return !!mDrawable; }
        bool HasTextItem() const noexcept
        { return !!mTextItem; }
        bool HasSpatialNode() const noexcept
        { return !!mSpatialNode; }
        bool HasFixture() const noexcept
        { return !!mFixture; }
        bool HasMapNode() const noexcept
        { return !!mMapNode; }
        bool HasLinearMover() const noexcept
        { return !!mLinearMover; }
        bool HasSplineMover() const noexcept
        { return !!mSplineMover; }
        bool HasBasicLight() const noexcept
        { return !!mBasicLight; }

        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        RigidBodyClass* GetRigidBody() noexcept
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        DrawableItemClass* GetDrawable() noexcept
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        TextItemClass* GetTextItem() noexcept
        { return mTextItem.get(); }
        // Get the spatial index node if any. If no spatial index node object
        // has been set then returns nullptr.
        SpatialNodeClass* GetSpatialNode() noexcept
        { return mSpatialNode.get(); }
        // Get the fixture class object if any. If no fixture has been attached
        // then returns nullptr.
        FixtureClass* GetFixture() noexcept
        { return mFixture.get(); }
        MapNodeClass* GetMapNode() noexcept
        { return mMapNode.get(); }
        LinearMoverClass* GetLinearMover() noexcept
        { return mLinearMover.get(); }
        SplineMoverClass* GetSplineMover() noexcept
        { return mSplineMover.get(); }
        BasicLightClass* GetBasicLight() noexcept
        { return mBasicLight.get(); }

        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        const RigidBodyClass* GetRigidBody() const noexcept
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        const DrawableItemClass* GetDrawable() const noexcept
        { return mDrawable.get(); }
        // Get the text item object if any. If no text item class object
        // has been set then returns nullptr:
        const TextItemClass* GetTextItem() const noexcept
        { return mTextItem.get(); }
        // Get the spatial index node if any. If no spatial index node object
        // has been set then returns nullptr.
        const SpatialNodeClass* GetSpatialNode() const noexcept
        { return mSpatialNode.get(); }
        // Get the fixture class object if any. If no fixture has been attached
        // then returns nullptr.
        const FixtureClass* GetFixture() const noexcept
        { return mFixture.get(); }

        const LinearMoverClass* GetLinearMover() const noexcept
        { return mLinearMover.get(); }
        const SplineMoverClass* GetSplineMover() const noexcept
        { return mSplineMover.get(); }

        const MapNodeClass* GetMapNode() const noexcept
        { return mMapNode.get(); }

        const BasicLightClass* GetBasicLight() const noexcept
        { return mBasicLight.get(); }

        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        int GetLayer() const noexcept;

        void Update(float time, float dt);
        // Serialize the node into JSON.
        void IntoJson(data::Writer& data) const;
        // Load the node's properties from the given JSON object.
        bool FromJson(const data::Reader& data);
        // Make a new unique copy of this node class object
        // with all the same properties but with a different/unique ID.
        EntityNodeClass Clone() const;

        EntityNodeClass& operator=(const EntityNodeClass& other);

    private:
        // the resource id.
        std::string mClassId;
        // human-readable name of the class.
        std::string mName;
        // The node tag string.
        std::string mTag;
        // translation of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Nodes scaling factor. applies to all children.
        glm::vec2 mScale = {1.0f, 1.0f};
        // Size of the node's containing box.
        glm::vec2 mSize  = {1.0f, 1.0f};
        // Rotation around z axis in radians.
        float mRotation = 0.0f;
        // rigid body if any.
        std::shared_ptr<RigidBodyClass> mRigidBody;
        // drawable item if any.
        std::shared_ptr<DrawableItemClass> mDrawable;
        // text item if any.
        std::shared_ptr<TextItemClass> mTextItem;
        // spatial node if any.
        std::shared_ptr<SpatialNodeClass> mSpatialNode;
        // fixture if any.
        std::shared_ptr<FixtureClass> mFixture;
        std::shared_ptr<MapNodeClass> mMapNode;
        std::shared_ptr<LinearMoverClass> mLinearMover;
        std::shared_ptr<SplineMoverClass> mSplineMover;
        std::shared_ptr<BasicLightClass> mBasicLight;
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
    };

    struct EntityNodeTransform {
        // translation of the node relative to its parent.
        glm::vec2 translation = {0.0f, 0.0f};
        // Nodes scaling factor. applies to this node
        // and all of its children.
        glm::vec2 scale = {1.0f, 1.0f};
        // node's box size. used to generate collision shapes
        // and to resize the drawable shape (if any)
        glm::vec2 size = {1.0f, 1.0f};
        // Rotation around z axis in radians relative to parent.
        float rotation = 0.0f;

        EntityNodeTransform() = default;
        explicit EntityNodeTransform(const EntityNodeClass& klass)
          : translation(klass.GetTranslation())
          , scale(klass.GetScale())
          , size(klass.GetSize())
          , rotation(klass.GetRotation())
        {}

        inline void SetScale(glm::vec2 scale) noexcept
        { this->scale = scale; }
        inline void SetScale(float sx, float sy) noexcept
        { this->scale = glm::vec2(sx, sy); }
        inline void SetSize(glm::vec2 size) noexcept
        { this->size = size; }
        inline void SetSize(float width, float height) noexcept
        { this->size = glm::vec2(width, height); }
        inline void SetTranslation(glm::vec2 pos) noexcept
        { this->translation = pos; }
        inline void SetTranslation(float x, float y) noexcept
        { this->translation = glm::vec2(x, y); }
        inline void SetRotation(float rotation) noexcept
        { this->rotation = rotation; }
        inline void Translate(glm::vec2 vec) noexcept
        { this->translation += vec; }
        inline void Translate(float dx, float dy) noexcept
        { this->translation += glm::vec2(dx, dy); }
        inline void Rotate(float dr) noexcept
        { this->rotation += dr; }
        inline void Grow(glm::vec2 vec) noexcept
        { this->size += vec; }
        inline void Grow(float dx, float dy) noexcept
        { this->size += glm::vec2(dx, dy); }

        inline glm::vec2 GetXVector() const noexcept
        { return math::RotateVectorAroundZ(glm::vec2{1.0f, 0.0f}, this->rotation); }
        inline glm::vec2 GetYVector() const noexcept
        { return math::RotateVectorAroundZ(glm::vec2{0.0f, 1.0f}, this->rotation); }

        inline glm::vec2 GetForwardVector() const noexcept
        { return math::RotateVectorAroundZ(glm::vec2{1.0f, 0.0f}, this->rotation); }
        inline glm::vec2 GetUpVector() const noexcept
        { return math::RotateVectorAroundZ(glm::vec2{0.0f -1.0f}, this->rotation); }

        inline glm::vec2 GetTranslation() const noexcept
        { return this->translation; }
        inline glm::vec2 GetScale() const noexcept
        { return this->scale; }
        inline glm::vec2 GetSize() const noexcept
        { return this->size; }
        inline float GetRotation() const noexcept
        { return this->rotation; }

        inline float GetWidth() const noexcept
        { return this->size.x; }
        inline float GetHeight() const noexcept
        { return this->size.y; }
        inline float GetX() const noexcept
        { return this->translation.x; }
        inline float GetY() const noexcept
        { return this->translation.y; }
    };

    class EntityNode;

    class EntityNodeData {
    public:
        EntityNodeData(std::string id, std::string name) noexcept
           : mInstanceId(std::move(id))
           , mInstanceName(std::move(name))
        {}
        inline void SetName(std::string name) noexcept
        { mInstanceName = std::move(name); }
        inline std::string GetName() const noexcept
        { return mInstanceName; }
        inline std::string GetId() const noexcept
        { return mInstanceId; }
        inline Entity* GetEntity() noexcept
        { return mEntity; }
        inline EntityNode* GetNode() noexcept
        { return mNode; }
    private:
        friend class EntityNode;
        // the instance id.
        std::string mInstanceId;
        // the instance name.
        std::string mInstanceName;
        // The entity that owns this node.
        Entity* mEntity = nullptr;
        EntityNode* mNode = nullptr;
    };

    using EntityNodeAllocator = base::Allocator<EntityNodeTransform, EntityNodeData>;
    using EntityNodeTransformSequence = base::AllocatorSequence<EntityNodeTransform,
            EntityNodeTransform, EntityNodeData>;
    using EntityNodeDataSequence = base::AllocatorSequence<EntityNodeData,
            EntityNodeTransform, EntityNodeData>;

    class EntityNode
    {
    public:
        using Flags = EntityNodeClass::Flags;
        using DrawableItemType = DrawableItem;
        using TextItemType = TextItem;
        using LightClassType = BasicLight;

        explicit EntityNode(std::shared_ptr<const EntityNodeClass> klass, EntityNodeAllocator* allocator = nullptr);
        explicit EntityNode(const EntityNodeClass& klass, EntityNodeAllocator* allocator = nullptr);
        explicit EntityNode(EntityNode&& other);

        EntityNode(const EntityNode& other) = delete;
       ~EntityNode();

        void Release(EntityNodeAllocator* allocator);

        // transformation
        inline void SetScale(glm::vec2 scale) noexcept
        { mTransform->scale = scale; }
        inline void SetScale(float sx, float sy) noexcept
        { mTransform->scale = glm::vec2(sx, sy); }
        inline void SetSize(const glm::vec2& size) noexcept
        { mTransform->size = size; }
        inline void SetSize(float width, float height) noexcept
        { mTransform->size = glm::vec2(width, height); }
        inline void SetTranslation(glm::vec2 pos) noexcept
        { mTransform->translation = pos; }
        inline void SetTranslation(float x, float y) noexcept
        { mTransform->translation = glm::vec2(x, y); }
        inline void SetRotation(float rotation) noexcept
        { mTransform->rotation = rotation; }
        inline void Translate(const glm::vec2& vec) noexcept
        { mTransform->translation += vec; }
        inline void Translate(float dx, float dy) noexcept
        { mTransform->translation += glm::vec2(dx, dy); }
        inline void Rotate(float dr) noexcept
        { mTransform->rotation += dr; }
        inline void Grow(glm::vec2 vec) noexcept
        { mTransform->size += vec; }
        inline void Grow(float dx, float dy) noexcept
        { mTransform->size += glm::vec2(dx, dy); }
        inline glm::vec2 GetTranslation() const noexcept
        { return mTransform->translation; }
        inline glm::vec2 GetScale() const noexcept
        { return mTransform->scale; }
        inline glm::vec2 GetSize() const noexcept
        { return mTransform->size; }

        inline glm::vec2 GetXVector() const noexcept
        { return mTransform->GetXVector(); }
        inline glm::vec2 GetYVector() const noexcept
        { return mTransform->GetYVector(); }

        inline glm::vec2 GetForwardVector() const noexcept
        { return mTransform->GetForwardVector(); }
        inline glm::vec2 GetUpVector() const noexcept
        { return mTransform->GetUpVector(); }

        inline float GetRotation() const noexcept
        { return mTransform->rotation; }

        inline void SetName(std::string name) noexcept
        { mNodeData->mInstanceName = std::move(name); }
        inline std::string GetId() const noexcept
        { return mNodeData->mInstanceId; }
        inline std::string GetName() const noexcept
        { return mNodeData->mInstanceName; }
        inline Entity* GetEntity() noexcept
        { return mNodeData->mEntity; }
        inline const Entity* GetEntity() const noexcept
        { return mNodeData->mEntity; }

        inline std::string GetTag() const noexcept
        { return mClass->GetTag(); }
        inline bool TestFlag(Flags flags) const noexcept
        { return mClass->TestFlag(flags); }

        inline void SetEntity(Entity* entity) noexcept
        { mNodeData->mEntity = entity; }

        inline EntityNodeTransform* GetTransform() noexcept
        { return mTransform; }
        inline const EntityNodeTransform* GetTransform() const noexcept
        { return mTransform; }
        inline EntityNodeData* GetData() noexcept
        { return mNodeData; }
        inline const EntityNodeData* GetData() const noexcept
        { return mNodeData; }

        // Get the node's drawable item if any. If no drawable
        // item is set then returns nullptr.
        DrawableItem* GetDrawable();
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        RigidBody* GetRigidBody();
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        TextItem* GetTextItem();
        // Get the node's fixture if any. If no fixture is et
        // then returns nullptr.
        Fixture* GetFixture();

        SpatialNode* GetSpatialNode();

        MapNode* GetMapNode();

        LinearMover* GetLinearMover();

        SplineMover* GetSplineMover();

        BasicLight* GetBasicLight();

        // Get the node's drawable item if any. If now drawable
        // item is set then returns nullptr.
        const DrawableItem* GetDrawable() const;
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        const RigidBody* GetRigidBody() const;
        // Get the node's text item if any. If no text item
        // is set then returns nullptr.
        const TextItem* GetTextItem() const;
        // Get the node's spatial node if any. If no spatial node
        // is set then returns nullptr.
        const SpatialNode* GetSpatialNode() const;
        // Get the node's fixture if any. If no fixture is et
        // then returns nullptr.
        const Fixture* GetFixture() const;

        const MapNode* GetMapNode() const;

        const LinearMover* GetLinearMover() const;

        const SplineMover* GetSplineMover() const;

        const BasicLight* GetBasicLight() const;

        bool HasRigidBody() const noexcept
        { return !!mRigidBody; }
        bool HasDrawable() const noexcept
        { return !!mDrawable; }
        bool HasTextItem() const noexcept
        { return !!mTextItem; }
        bool HasSpatialNode() const noexcept
        { return !!mSpatialNode; }
        bool HasFixture() const noexcept
        { return !!mFixture; }
        bool HasMapNode() const noexcept
        { return !!mMapNode; }
        bool HasBasicLight() const noexcept
        { return !!mBasicLight; }
        bool HasLinearMover() const noexcept
        { return !!mLinearMover; }
        bool HasSplineMover() const noexcept
        { return !!mSplineMover; }

        // shortcut for class getters.
        const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        const std::string& GetClassTag() const noexcept
        { return mClass->GetTag(); }
        int GetLayer() const noexcept
        { return mClass->GetLayer(); }

        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        const EntityNodeClass& GetClass() const noexcept
        { return *mClass.get(); }
        const EntityNodeClass* operator->() const noexcept
        { return mClass.get(); }

        EntityNode& operator=(const EntityNode&) = delete;
    private:
        static constexpr auto InvalidAllocatorIndex = 0xFFFFFFFF;

        // the class object.
        std::shared_ptr<const EntityNodeClass> mClass;
        // Index of the data in the allocator.
        unsigned mAllocatorIndex = InvalidAllocatorIndex;
        // transformation object
        EntityNodeTransform* mTransform = nullptr;
        // data object
        EntityNodeData* mNodeData = nullptr;
        // rigid body if any.
        std::unique_ptr<RigidBody> mRigidBody;
        // drawable if any.
        std::unique_ptr<DrawableItem> mDrawable;
        // text item if any
        std::unique_ptr<TextItem> mTextItem;
        // spatial node if any
        std::unique_ptr<SpatialNode> mSpatialNode;
        // fixture if any
        std::unique_ptr<Fixture> mFixture;
        // map node if any.
        std::unique_ptr<MapNode> mMapNode;
        std::unique_ptr<LinearMover> mLinearMover;
        std::unique_ptr<SplineMover> mSplineMover;
        std::unique_ptr<BasicLight> mBasicLight;

    };

    std::string FastId(std::size_t len);

} // namespace