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

#include <atomic>

#include "base/hash.h"
#include "base/logging.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/transform.h"
#include "game/entity_node.h"
#include "game/entity_node_spline_mover.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_tilemap_node.h"
#include "game/entity_node_light.h"
#include "game/entity_node_mesh_effect.h"

namespace game
{

std::string FastId(std::size_t len)
{
    static std::atomic<std::uint64_t> counter = 1;
    return std::to_string(counter++);
}

EntityNodeClass::EntityNodeClass()
{
    mClassId = base::RandomString(10);
    mBitFlags.set(Flags::VisibleInEditor, true);
}

EntityNodeClass::EntityNodeClass(const EntityNodeClass& other)
{
    mClassId  = other.mClassId;
    mName     = other.mName;
    mTag      = other.mTag;
    mPosition = other.mPosition;
    mScale    = other.mScale;
    mSize     = other.mSize;
    mRotation = other.mRotation;
    mBitFlags = other.mBitFlags;
    if (other.mRigidBody)
        mRigidBody = std::make_shared<RigidBodyClass>(*other.mRigidBody);
    if (other.mDrawable)
        mDrawable = std::make_shared<DrawableItemClass>(*other.mDrawable);
    if (other.mTextItem)
        mTextItem = std::make_shared<TextItemClass>(*other.mTextItem);
    if (other.mSpatialNode)
        mSpatialNode = std::make_shared<SpatialNodeClass>(*other.mSpatialNode);
    if (other.mFixture)
        mFixture = std::make_shared<FixtureClass>(*other.mFixture);
    if (other.mMapNode)
        mMapNode = std::make_shared<MapNodeClass>(*other.mMapNode);
    if (other.mLinearMover)
        mLinearMover = std::make_shared<LinearMoverClass>(*other.mLinearMover);
    if (other.mSplineMover)
        mSplineMover = std::make_shared<SplineMoverClass>(*other.mSplineMover);
    if (other.mBasicLight)
        mBasicLight = std::make_shared<BasicLightClass>(*other.mBasicLight);
    if (other.mMeshEffect)
        mMeshEffect = std::make_shared<MeshEffectClass>(*other.mMeshEffect);
}

EntityNodeClass::EntityNodeClass(EntityNodeClass&& other)
{
    mClassId     = std::move(other.mClassId);
    mName        = std::move(other.mName);
    mTag         = std::move(other.mTag);
    mPosition    = std::move(other.mPosition);
    mScale       = std::move(other.mScale);
    mSize        = std::move(other.mSize);
    mRotation    = std::move(other.mRotation);
    mRigidBody   = std::move(other.mRigidBody);
    mDrawable    = std::move(other.mDrawable);
    mTextItem    = std::move(other.mTextItem);
    mBitFlags    = std::move(other.mBitFlags);
    mSpatialNode = std::move(other.mSpatialNode);
    mFixture     = std::move(other.mFixture);
    mMapNode     = std::move(other.mMapNode);
    mLinearMover = std::move(other.mLinearMover);
    mSplineMover = std::move(other.mSplineMover);
    mBasicLight  = std::move(other.mBasicLight);
    mMeshEffect  = std::move(other.mMeshEffect);
}

std::size_t EntityNodeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mTag);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mBitFlags);
    if (mRigidBody)
        hash = base::hash_combine(hash, mRigidBody->GetHash());
    if (mDrawable)
        hash = base::hash_combine(hash, mDrawable->GetHash());
    if (mTextItem)
        hash = base::hash_combine(hash, mTextItem->GetHash());
    if (mSpatialNode)
        hash = base::hash_combine(hash, mSpatialNode->GetHash());
    if (mFixture)
        hash = base::hash_combine(hash, mFixture->GetHash());
    if (mMapNode)
        hash = base::hash_combine(hash, mMapNode->GetHash());
    if (mLinearMover)
        hash = base::hash_combine(hash, mLinearMover->GetHash());
    if (mSplineMover)
        hash = base::hash_combine(hash, mSplineMover->GetHash());
    if (mBasicLight)
        hash = base::hash_combine(hash, mBasicLight->GetHash());
    if (mMeshEffect)
        hash = base::hash_combine(hash, mMeshEffect->GetHash());
    return hash;
}

void EntityNodeClass::SetRigidBody(const RigidBodyClass &body)
{
    mRigidBody = std::make_shared<RigidBodyClass>(body);
}

void EntityNodeClass::SetDrawable(const DrawableItemClass &drawable)
{
    mDrawable = std::make_shared<DrawableItemClass>(drawable);
}

void EntityNodeClass::SetTextItem(const TextItemClass& text)
{
    mTextItem = std::make_shared<TextItemClass>(text);
}

void EntityNodeClass::SetSpatialNode(const SpatialNodeClass& node)
{
    mSpatialNode = std::make_shared<SpatialNodeClass>(node);
}

void EntityNodeClass::SetFixture(const FixtureClass& fixture)
{
    mFixture = std::make_shared<FixtureClass>(fixture);
}

void EntityNodeClass::SetMapNode(const MapNodeClass& map)
{
    mMapNode = std::make_shared<MapNodeClass>(map);
}
void EntityNodeClass::SetLinearMover(const LinearMoverClass& mover)
{
    mLinearMover = std::make_shared<LinearMoverClass>(mover);
}

void EntityNodeClass::SetSplineMover(const SplineMoverClass& mover)
{
    mSplineMover = std::make_shared<SplineMoverClass>(mover);
}

void EntityNodeClass::SetBasicLight(const BasicLightClass& light)
{
    mBasicLight = std::make_shared<BasicLightClass>(light);
}

void EntityNodeClass::SetMeshEffect(const MeshEffectClass& effect)
{
    mMeshEffect = std::make_shared<MeshEffectClass>(effect);
}

void EntityNodeClass::CreateRigidBody()
{
    mRigidBody = std::make_shared<RigidBodyClass>();
}

void EntityNodeClass::CreateDrawable()
{
    mDrawable = std::make_shared<DrawableItemClass>();
}

void EntityNodeClass::CreateTextItem()
{
    mTextItem = std::make_shared<TextItemClass>();
}

void EntityNodeClass::CreateSpatialNode()
{
    mSpatialNode = std::make_shared<SpatialNodeClass>();
}

void EntityNodeClass::CreateFixture()
{
    mFixture = std::make_shared<FixtureClass>();
}

void EntityNodeClass::CreateMapNode()
{
    mMapNode = std::make_shared<MapNodeClass>();
}

void EntityNodeClass::CreateLinearMover()
{
    mLinearMover = std::make_shared<LinearMoverClass>();
}

void EntityNodeClass::CreateSplineMover()
{
    mSplineMover = std::make_shared<SplineMoverClass>();
}

void EntityNodeClass::CreateBasicLight()
{
    mBasicLight = std::make_shared<BasicLightClass>();
}

void EntityNodeClass::CreateMeshEffect()
{
    mMeshEffect = std::make_shared<MeshEffectClass>();
}

glm::mat4 EntityNodeClass::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.RotateAroundZ(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}
glm::mat4 EntityNodeClass::GetModelTransform() const
{
    Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

int EntityNodeClass::GetLayer() const noexcept
{
    return mDrawable ? mDrawable->GetLayer() : 0;
}

void EntityNodeClass::Update(float time, float dt)
{
}

void EntityNodeClass::IntoJson(data::Writer& data) const
{
    data.Write("class",    mClassId);
    data.Write("name",     mName);
    data.Write("tag",      mTag);
    data.Write("position", mPosition);
    data.Write("scale",    mScale);
    data.Write("size",     mSize);
    data.Write("rotation", mRotation);
    data.Write("flags",    mBitFlags);
    if (mRigidBody)
    {
        auto chunk = data.NewWriteChunk();
        mRigidBody->IntoJson(*chunk);
        data.Write("rigid_body", std::move(chunk));
    }
    if (mDrawable)
    {
        auto chunk = data.NewWriteChunk();
        mDrawable->IntoJson(*chunk);
        data.Write("drawable_item", std::move(chunk));
    }
    if (mTextItem)
    {
        auto chunk = data.NewWriteChunk();
        mTextItem->IntoJson(*chunk);
        data.Write("text_item", std::move(chunk));
    }
    if (mSpatialNode)
    {
        auto chunk = data.NewWriteChunk();
        mSpatialNode->IntoJson(*chunk);
        data.Write("spatial_node", std::move(chunk));
    }
    if (mFixture)
    {
        auto chunk = data.NewWriteChunk();
        mFixture->IntoJson(*chunk);
        data.Write("fixture", std::move(chunk));
    }
    if (mMapNode)
    {
        auto chunk = data.NewWriteChunk();
        mMapNode->IntoJson(*chunk);
        data.Write("map_node", std::move(chunk));
    }
    if (mLinearMover)
    {
        auto chunk = data.NewWriteChunk();
        mLinearMover->IntoJson(*chunk);
        data.Write("linear_mover", std::move(chunk));
    }
    if (mSplineMover)
    {
        auto chunk = data.NewWriteChunk();
        mSplineMover->IntoJson(*chunk);
        data.Write("spline_mover", std::move(chunk));
    }

    if (mBasicLight)
    {
        auto chunk = data.NewWriteChunk();
        mBasicLight->IntoJson(*chunk);
        data.Write("basic_light", std::move(chunk));
    }

    if (mMeshEffect)
    {
        auto chunk = data.NewWriteChunk();
        mMeshEffect->IntoJson(*chunk);
        data.Write("mesh_effect", std::move(chunk));
    }
}

template<typename T>
bool ComponentClassFromJson(const std::string& node, const char* name, const data::Reader& data, std::shared_ptr<T>& klass)
{
    if (const auto& chunk = data.GetReadChunk(name))
    {
        klass = std::make_shared<T>();
        if (!klass->FromJson(*chunk))
        {
            WARN("Entity node class component failed to load. [node=%1, component='%2']",  node, name);
            return false;
        }
    }
    return true;
}

bool EntityNodeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("class",    &mClassId);
    ok &= data.Read("name",     &mName);
    ok &= data.Read("tag",      &mTag);
    ok &= data.Read("position", &mPosition);
    ok &= data.Read("scale",    &mScale);
    ok &= data.Read("size",     &mSize);
    ok &= data.Read("rotation", &mRotation);
    ok &= data.Read("flags",    &mBitFlags);
    ok &= ComponentClassFromJson(mName, "rigid_body",    data, mRigidBody);
    ok &= ComponentClassFromJson(mName, "drawable_item", data, mDrawable);
    ok &= ComponentClassFromJson(mName, "text_item",     data, mTextItem);
    ok &= ComponentClassFromJson(mName, "spatial_node",  data, mSpatialNode);
    ok &= ComponentClassFromJson(mName, "fixture",       data, mFixture);
    ok &= ComponentClassFromJson(mName, "map_node",      data, mMapNode);
    ok &= ComponentClassFromJson(mName, "linear_mover",  data, mLinearMover);
    ok &= ComponentClassFromJson(mName, "spline_mover",  data, mSplineMover);
    ok &= ComponentClassFromJson(mName, "basic_light",   data, mBasicLight);
    ok &= ComponentClassFromJson(mName, "mesh_effect",   data, mMeshEffect);
    return ok;
}

EntityNodeClass EntityNodeClass::Clone() const
{
    EntityNodeClass ret(*this);
    ret.mClassId = base::RandomString(10);
    return ret;
}

EntityNodeClass& EntityNodeClass::operator=(const EntityNodeClass& other)
{
    if (this == &other)
        return *this;
    EntityNodeClass tmp(other);
    mClassId     = std::move(tmp.mClassId);
    mName        = std::move(tmp.mName);
    mTag         = std::move(tmp.mTag);
    mPosition    = std::move(tmp.mPosition);
    mScale       = std::move(tmp.mScale);
    mSize        = std::move(tmp.mSize);
    mRotation    = std::move(tmp.mRotation);
    mRigidBody   = std::move(tmp.mRigidBody);
    mDrawable    = std::move(tmp.mDrawable);
    mTextItem    = std::move(tmp.mTextItem);
    mSpatialNode = std::move(tmp.mSpatialNode);
    mFixture     = std::move(tmp.mFixture);
    mMapNode     = std::move(tmp.mMapNode);
    mLinearMover = std::move(tmp.mLinearMover);
    mSplineMover = std::move(tmp.mSplineMover);
    mBitFlags    = std::move(tmp.mBitFlags);
    mBasicLight  = std::move(tmp.mBasicLight);
    mMeshEffect  = std::move(tmp.mMeshEffect);
    return *this;
}

EntityNode::EntityNode(std::shared_ptr<const EntityNodeClass> klass, EntityNodeAllocator* allocator)
  : mClass(std::move(klass))
{
    if (allocator)
    {
        std::lock_guard<std::mutex> lock(allocator->GetMutex());

        mAllocatorIndex = allocator->GetNextIndex();
        mTransform = allocator->CreateObject<EntityNodeTransform>(mAllocatorIndex, *mClass);
        mNodeData  = allocator->CreateObject<EntityNodeData>(mAllocatorIndex, FastId(10), mClass->GetName());
        mNodeData->mNode = this;
    }
    else
    {
        mTransform = new EntityNodeTransform(*mClass);
        mNodeData  = new EntityNodeData(FastId(10), mClass->GetName());
        mNodeData->mNode = this;
    }

    if (mClass->HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(mClass->GetSharedDrawable());
    if (mClass->HasRigidBody())
        mRigidBody = std::make_unique<RigidBody>(mClass->GetSharedRigidBody());
    if (mClass->HasTextItem())
        mTextItem = std::make_unique<TextItem>(mClass->GetSharedTextItem());
    if (mClass->HasSpatialNode())
        mSpatialNode = std::make_unique<SpatialNode>(mClass->GetSharedSpatialNode());
    if (mClass->HasFixture())
        mFixture = std::make_unique<Fixture>(mClass->GetSharedFixture());
    if (mClass->HasMapNode())
        mMapNode = std::make_unique<MapNode>(mClass->GetSharedMapNode());
    if (mClass->HasLinearMover())
        mLinearMover = std::make_unique<LinearMover>(mClass->GetSharedLinearMover());
    if (mClass->HasSplineMover())
        mSplineMover = std::make_unique<SplineMover>(mClass->GetSharedSplineMover());
    if (mClass->HasBasicLight())
        mBasicLight = std::make_unique<BasicLight>(mClass->GetSharedBasicLight());
    if (mClass->HasMeshEffect())
        mMeshEffect = std::make_unique<MeshEffect>(mClass->GetSharedMeshEffect());
}

EntityNode::EntityNode(EntityNode&& other) noexcept
   : mClass         (std::move(other.mClass))
   , mAllocatorIndex(other.mAllocatorIndex)
   , mTransform     (other.mTransform)
   , mNodeData      (other.mNodeData)
   , mRigidBody     (std::move(other.mRigidBody))
   , mDrawable      (std::move(other.mDrawable))
   , mTextItem      (std::move(other.mTextItem))
   , mSpatialNode   (std::move(other.mSpatialNode))
   , mFixture       (std::move(other.mFixture))
   , mMapNode       (std::move(other.mMapNode))
   , mLinearMover   (std::move(other.mLinearMover))
   , mSplineMover   (std::move(other.mSplineMover))
   , mBasicLight    (std::move(other.mBasicLight))
   , mMeshEffect    (std::move(other.mMeshEffect))
{
    other.mTransform = nullptr;
    other.mNodeData  = nullptr;
    mNodeData->mNode = this;
}

EntityNode::EntityNode(const EntityNodeClass& klass, EntityNodeAllocator* allocator)
  : EntityNode(std::make_shared<EntityNodeClass>(klass), allocator)
{}

EntityNode::~EntityNode()
{
    if (mAllocatorIndex != InvalidAllocatorIndex)
    {
        // assert that Release was called
        ASSERT(mTransform == nullptr);
        ASSERT(mNodeData == nullptr);
    }
    else
    {
        delete mTransform;
        delete mNodeData;
    }
}

void EntityNode::Release(EntityNodeAllocator* allocator)
{
    if (mTransform)
    {
        ASSERT(mAllocatorIndex != InvalidAllocatorIndex);

        std::lock_guard<std::mutex> lock(allocator->GetMutex());

        allocator->DestroyObject(mAllocatorIndex, mTransform);
        allocator->DestroyObject(mAllocatorIndex, mNodeData);
        allocator->FreeIndex(mAllocatorIndex);

        mTransform = nullptr;
        mNodeData  = nullptr;
    }
}
EntityNodeTransform* EntityNode::GetTransform() noexcept
{
    return mTransform;
}

EntityNodeData* EntityNode::GetData() noexcept
{
    return mNodeData;
}

DrawableItem* EntityNode::GetDrawable() noexcept
{
    return mDrawable.get();
}

RigidBody* EntityNode::GetRigidBody() noexcept
{
    return mRigidBody.get();
}

TextItem* EntityNode::GetTextItem() noexcept
{
    return mTextItem.get();
}

Fixture* EntityNode::GetFixture() noexcept
{
    return mFixture.get();
}

MapNode* EntityNode::GetMapNode() noexcept
{
    return mMapNode.get();
}

SpatialNode* EntityNode::GetSpatialNode() noexcept
{
    return mSpatialNode.get();
}

LinearMover* EntityNode::GetLinearMover() noexcept
{
    return mLinearMover.get();
}

SplineMover* EntityNode::GetSplineMover() noexcept
{
    return mSplineMover.get();
}

BasicLight* EntityNode::GetBasicLight() noexcept
{
    return mBasicLight.get();
}

MeshEffect *EntityNode::GetMeshEffect() noexcept
{
    return mMeshEffect.get();
}

const EntityNodeTransform* EntityNode::GetTransform() const noexcept
{
    return mTransform;
}

const EntityNodeData* EntityNode::GetData() const noexcept
{
    return mNodeData;
}

const DrawableItem* EntityNode::GetDrawable() const noexcept
{
    return mDrawable.get();
}

const RigidBody* EntityNode::GetRigidBody() const noexcept
{
    return mRigidBody.get();
}

const TextItem* EntityNode::GetTextItem() const noexcept
{
    return mTextItem.get();
}

const SpatialNode* EntityNode::GetSpatialNode() const noexcept
{
    return mSpatialNode.get();
}

const Fixture* EntityNode::GetFixture() const noexcept
{
    return mFixture.get();
}

const MapNode* EntityNode::GetMapNode() const noexcept
{
    return mMapNode.get();
}

const LinearMover* EntityNode::GetLinearMover() const noexcept
{
    return mLinearMover.get();
}

const SplineMover* EntityNode::GetSplineMover() const noexcept
{
    return mSplineMover.get();
}

const BasicLight* EntityNode::GetBasicLight() const noexcept
{
    return mBasicLight.get();
}

const MeshEffect *EntityNode::GetMeshEffect() const noexcept
{
    return mMeshEffect.get();
}

glm::mat4 EntityNode::GetNodeTransform() const noexcept
{
    Transform transform;
    transform.Scale(mTransform->scale);
    transform.RotateAroundZ(mTransform->rotation);
    transform.Translate(mTransform->translation);
    return transform.GetAsMatrix();
}

glm::mat4 EntityNode::GetModelTransform() const noexcept
{
    const auto& size = mTransform->size;

    Transform transform;
    transform.Scale(size);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-size.x * 0.5f, -size.y * 0.5f);
    return transform.GetAsMatrix();
}


} // namespace
