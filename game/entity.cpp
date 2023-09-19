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
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/treeop.h"
#include "game/entity.h"
#include "game/transform.h"

namespace {
    std::string FastId(std::size_t len)
    {
        static std::uint64_t counter = 1;
        return std::to_string(counter++);
    }
} // namespace

namespace game
{

size_t MapNodeClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(0, mMapSortPoint);
    return hash;
}

void MapNodeClass::IntoJson(data::Writer& data) const
{
    data.Write("map_sort_point", mMapSortPoint);
}
bool MapNodeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("map_sort_point", &mMapSortPoint);
    return ok;
}

SpatialNodeClass::SpatialNodeClass()
{
    mFlags.set(Flags::Enabled, true);
}
std::size_t SpatialNodeClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mShape);
    hash = base::hash_combine(hash, mFlags);
    return hash;
}
void SpatialNodeClass::IntoJson(data::Writer& data) const
{
    data.Write("shape", mShape);
    data.Write("flags", mFlags);
}

bool SpatialNodeClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("shape", &mShape);
    ok &= data.Read("flags", &mFlags);
    return ok;
}

std::size_t FixtureClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mCollisionShape);
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mPolygonShapeId);
    hash = base::hash_combine(hash, mRigidBodyNodeId);
    hash = base::hash_combine(hash, mFriction);
    hash = base::hash_combine(hash, mDensity);
    hash = base::hash_combine(hash, mRestitution);
    return hash;
}

void FixtureClass::IntoJson(data::Writer& data) const
{
    data.Write("shape",       mCollisionShape);
    data.Write("flags",       mBitFlags);
    data.Write("polygon",     mPolygonShapeId);
    data.Write("rigid_body",  mRigidBodyNodeId);
    data.Write("friction",    mFriction);
    data.Write("density",     mDensity);
    data.Write("restitution", mRestitution);
}

bool FixtureClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("shape",       &mCollisionShape);
    ok &= data.Read("flags",       &mBitFlags);
    ok &= data.Read("polygon",     &mPolygonShapeId);
    ok &= data.Read("rigid_body",  &mRigidBodyNodeId);
    ok &= data.Read("friction",    &mFriction);
    ok &= data.Read("density",     &mDensity);
    ok &= data.Read("restitution", &mRestitution);
    return ok;
}

std::size_t RigidBodyItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mSimulation);
    hash = base::hash_combine(hash, mCollisionShape);
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mPolygonShapeId);
    hash = base::hash_combine(hash, mFriction);
    hash = base::hash_combine(hash, mRestitution);
    hash = base::hash_combine(hash, mAngularDamping);
    hash = base::hash_combine(hash, mLinearDamping);
    hash = base::hash_combine(hash, mDensity);
    return hash;
}

void RigidBodyItemClass::IntoJson(data::Writer& data) const
{
    data.Write("simulation",      mSimulation);
    data.Write("shape",           mCollisionShape);
    data.Write("flags",           mBitFlags);
    data.Write("polygon",         mPolygonShapeId);
    data.Write("friction",        mFriction);
    data.Write("restitution",     mRestitution);
    data.Write("angular_damping", mAngularDamping);
    data.Write("linear_damping",  mLinearDamping);
    data.Write("density",         mDensity);
}

bool RigidBodyItemClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("simulation",      &mSimulation);
    ok &= data.Read("shape",           &mCollisionShape);
    ok &= data.Read("flags",           &mBitFlags);
    ok &= data.Read("polygon",         &mPolygonShapeId);
    ok &= data.Read("friction",        &mFriction);
    ok &= data.Read("restitution",     &mRestitution);
    ok &= data.Read("angular_damping", &mAngularDamping);
    ok &= data.Read("linear_damping",  &mLinearDamping);
    ok &= data.Read("density",         &mDensity);
    return ok;
}

DrawableItemClass::DrawableItemClass()
{
    mBitFlags.set(Flags::VisibleInGame,    true);
    mBitFlags.set(Flags::UpdateDrawable,   true);
    mBitFlags.set(Flags::UpdateMaterial,   true);
    mBitFlags.set(Flags::RestartDrawable,  true);
    mBitFlags.set(Flags::FlipHorizontally, false);
    mBitFlags.set(Flags::FlipVertically,   false);
    mBitFlags.set(Flags::PP_EnableBloom,   true);
}

std::size_t DrawableItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mMaterialId);
    hash = base::hash_combine(hash, mDrawableId);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mLineWidth);
    hash = base::hash_combine(hash, mRenderPass);
    hash = base::hash_combine(hash, mRenderStyle);
    hash = base::hash_combine(hash, mTimeScale);

    // remember the *unordered* nature of unordered_map
    std::set<std::string> keys;
    for (const auto& param : mMaterialParams)
        keys.insert(param.first);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mMaterialParams, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *param);
    }
    return hash;
}

void DrawableItemClass::IntoJson(data::Writer& data) const
{
    data.Write("flags",       mBitFlags);
    data.Write("material",    mMaterialId);
    data.Write("drawable",    mDrawableId);
    data.Write("layer",       mLayer);
    data.Write("linewidth",   mLineWidth);
    data.Write("renderpass",  mRenderPass);
    data.Write("renderstyle", mRenderStyle);
    data.Write("timescale",   mTimeScale);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> keys;
    for (const auto& [key, value] : mMaterialParams)
        keys.insert(key);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mMaterialParams, key);
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        chunk->Write("value", *param);
        data.AppendChunk("material_params", std::move(chunk));
    }
}

template<typename T>
bool ReadMaterialParam(const data::Reader& data, DrawableItemClass::MaterialParam& param)
{
    T value;
    if (!data.Read("value", &value))
        return false;
    param = value;
    return true;
}

bool DrawableItemClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("flags",       &mBitFlags);
    ok &= data.Read("material",    &mMaterialId);
    ok &= data.Read("drawable",    &mDrawableId);
    ok &= data.Read("layer",       &mLayer);
    ok &= data.Read("linewidth",   &mLineWidth);
    ok &= data.Read("renderpass",  &mRenderPass);
    ok &= data.Read("renderstyle", &mRenderStyle);
    ok &= data.Read("timescale",   &mTimeScale);

    for (unsigned i=0; i<data.GetNumChunks("material_params"); ++i)
    {
        const auto& chunk = data.GetReadChunk("material_params", i);

        MaterialParam param;
        std::string name;
        bool chunk_ok = true;
        chunk_ok &= chunk->Read("name",  &name);
        chunk_ok &= chunk->Read("value", &param);
        if (chunk_ok)
            mMaterialParams[std::move(name)] = param;

        ok &= chunk_ok;
    }
    return ok;
}

size_t TextItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mBitFlags.value());
    hash = base::hash_combine(hash, mHAlign);
    hash = base::hash_combine(hash, mVAlign);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mText);
    hash = base::hash_combine(hash, mFontName);
    hash = base::hash_combine(hash, mFontSize);
    hash = base::hash_combine(hash, mRasterWidth);
    hash = base::hash_combine(hash, mRasterHeight);
    hash = base::hash_combine(hash, mLineHeight);
    hash = base::hash_combine(hash, mTextColor);
    return hash;
}

void TextItemClass::IntoJson(data::Writer& data) const
{
    data.Write("flags",            mBitFlags);
    data.Write("horizontal_align", mHAlign);
    data.Write("vertical_align",   mVAlign);
    data.Write("layer",            mLayer);
    data.Write("text",             mText);
    data.Write("font_name",        mFontName);
    data.Write("font_size",        mFontSize);
    data.Write("raster_width",     mRasterWidth);
    data.Write("raster_height",    mRasterHeight);
    data.Write("line_height",      mLineHeight);
    data.Write("text_color",       mTextColor);
}


bool TextItemClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("flags",            &mBitFlags);
    ok &= data.Read("horizontal_align", &mHAlign);
    ok &= data.Read("vertical_align",   &mVAlign);
    ok &= data.Read("layer",            &mLayer);
    ok &= data.Read("text",             &mText);
    ok &= data.Read("font_name",        &mFontName);
    ok &= data.Read("font_size",        &mFontSize);
    ok &= data.Read("raster_width",     &mRasterWidth);
    ok &= data.Read("raster_height",    &mRasterHeight);
    ok &= data.Read("line_height",      &mLineHeight);
    ok &= data.Read("text_color",       &mTextColor);
    return ok;
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
        mRigidBody = std::make_shared<RigidBodyItemClass>(*other.mRigidBody);
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
    return hash;
}

void EntityNodeClass::SetRigidBody(const RigidBodyItemClass &body)
{
    mRigidBody = std::make_shared<RigidBodyItemClass>(body);
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

void EntityNodeClass::CreateRigidBody()
{
    mRigidBody = std::make_shared<RigidBodyItemClass>();
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
    mBitFlags    = std::move(tmp.mBitFlags);
    return *this;
}

EntityNode::EntityNode(std::shared_ptr<const EntityNodeClass> klass)
  : mClass(klass)
  , mInstId(FastId(10))
  , mName(mClass->GetName())
  , mPosition(mClass->GetTranslation())
  , mScale(mClass->GetScale())
  , mSize(mClass->GetSize())
  , mRotation(mClass->GetRotation())
{
    if (mClass->HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(mClass->GetSharedDrawable());
    if (mClass->HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(mClass->GetSharedRigidBody());
    if (mClass->HasTextItem())
        mTextItem = std::make_unique<TextItem>(mClass->GetSharedTextItem());
    if (mClass->HasSpatialNode())
        mSpatialNode = std::make_unique<SpatialNode>(mClass->GetSharedSpatialNode());
    if (mClass->HasFixture())
        mFixture = std::make_unique<Fixture>(mClass->GetSharedFixture());
    if (mClass->HasMapNode())
        mMapNode = std::make_unique<MapNode>(mClass->GetSharedMapNode());
}

EntityNode::EntityNode(const EntityNode& other)
  : mClass(other.mClass)
  , mInstId(other.mInstId)
  , mName(other.mName)
  , mPosition(other.mPosition)
  , mScale(other.mScale)
  , mSize(other.mSize)
  , mRotation(other.mRotation)
{
    if (other.HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(*other.GetRigidBody());
    if (other.HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(*other.GetDrawable());
    if (other.HasTextItem())
        mTextItem = std::make_unique<TextItem>(*other.GetTextItem());
    if (other.HasSpatialNode())
        mSpatialNode = std::make_unique<SpatialNode>(*other.GetSpatialNode());
    if (other->HasFixture())
        mFixture = std::make_unique<Fixture>(*other.GetFixture());
    if (other->HasMapNode())
        mMapNode = std::make_unique<MapNode>(*other.GetMapNode());
}

EntityNode::EntityNode(EntityNode&& other)
{
    mClass       = std::move(other.mClass);
    mInstId      = std::move(other.mInstId);
    mName        = std::move(other.mName);
    mScale       = std::move(other.mScale);
    mSize        = std::move(other.mSize);
    mPosition    = std::move(other.mPosition);
    mRotation    = std::move(other.mRotation);
    mRigidBody   = std::move(other.mRigidBody);
    mDrawable    = std::move(other.mDrawable);
    mTextItem    = std::move(other.mTextItem);
    mSpatialNode = std::move(other.mSpatialNode);
    mFixture     = std::move(other.mFixture);
    mMapNode     = std::move(other.mMapNode);
}

EntityNode::EntityNode(const EntityNodeClass& klass)
  : EntityNode(std::make_shared<EntityNodeClass>(klass))
{}

DrawableItem* EntityNode::GetDrawable()
{ return mDrawable.get(); }

RigidBodyItem* EntityNode::GetRigidBody()
{ return mRigidBody.get(); }

TextItem* EntityNode::GetTextItem()
{ return mTextItem.get(); }

Fixture* EntityNode::GetFixture()
{ return mFixture.get(); }

MapNode* EntityNode::GetMapNode()
{ return mMapNode.get(); }

const DrawableItem* EntityNode::GetDrawable() const
{ return mDrawable.get(); }

const RigidBodyItem* EntityNode::GetRigidBody() const
{ return mRigidBody.get(); }

const TextItem* EntityNode::GetTextItem() const
{ return mTextItem.get(); }

const SpatialNode* EntityNode::GetSpatialNode() const
{ return mSpatialNode.get(); }

const Fixture* EntityNode::GetFixture() const
{ return mFixture.get(); }

const MapNode* EntityNode::GetMapNode() const
{ return mMapNode.get(); }

void EntityNode::Reset()
{
    mPosition = mClass->GetTranslation();
    mScale    = mClass->GetScale();
    mSize     = mClass->GetSize();
    mRotation = mClass->GetRotation();
    if (mClass->HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(mClass->GetSharedDrawable());
    if (mClass->HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(mClass->GetSharedRigidBody());
    if (mClass->HasTextItem())
        mTextItem = std::make_unique<TextItem>(mClass->GetSharedTextItem());
    if (mClass->HasSpatialNode())
        mSpatialNode = std::make_unique<SpatialNode>(mClass->GetSharedSpatialNode());
    if (mClass->HasFixture())
        mFixture = std::make_unique<Fixture>(mClass->GetSharedFixture());
    if (mClass->HasMapNode())
        mMapNode = std::make_unique<MapNode>(mClass->GetSharedMapNode());
}

glm::mat4 EntityNode::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.RotateAroundZ(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 EntityNode::GetModelTransform() const
{
    Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

EntityClass::EntityClass()
{
    mClassId = base::RandomString(10);
    mFlags.set(Flags::VisibleInEditor, true);
    mFlags.set(Flags::VisibleInGame, true);
    mFlags.set(Flags::LimitLifetime, false);
    mFlags.set(Flags::KillAtLifetime, true);
    mFlags.set(Flags::KillAtBoundary, true);
    mFlags.set(Flags::TickEntity, true);
    mFlags.set(Flags::UpdateEntity, true);
    mFlags.set(Flags::WantsKeyEvents, false);
    mFlags.set(Flags::WantsMouseEvents, false);
}

EntityClass::EntityClass(const EntityClass& other)
{
    mClassId     = other.mClassId;
    mName        = other.mName;
    mTag         = other.mTag;
    mScriptFile  = other.mScriptFile;
    mIdleTrackId = other.mIdleTrackId;
    mFlags       = other.mFlags;
    mLifetime    = other.mLifetime;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<EntityNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : other.mAnimations)
    {
        mAnimations.push_back(std::make_unique<AnimationClass>(*track));
    }

    // make a deep copy of the script variables
    for (const auto& var : other.mScriptVars)
    {
        mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
    }

    // make a deep copy of the joints
    for (const auto& joint : other.mJoints)
    {
        mJoints.push_back(std::make_shared<PhysicsJoint>(*joint));
    }

    // Deepy copy animators
    for (const auto& animator : other.mAnimators)
    {
        mAnimators.push_back(std::make_shared<AnimatorClass>(*animator));
    }

    mRenderTree.FromTree(other.GetRenderTree(), [&map](const EntityNodeClass* node) {
        return map[node];
    });
}

EntityNodeClass* EntityClass::AddNode(const EntityNodeClass& node)
{
    mNodes.emplace_back(new EntityNodeClass(node));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(EntityNodeClass&& node)
{
    mNodes.emplace_back(new EntityNodeClass(std::move(node)));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(std::unique_ptr<EntityNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

void EntityClass::MoveNode(size_t src_index, size_t dst_index)
{
    ASSERT(src_index < mNodes.size());
    ASSERT(dst_index < mNodes.size());
    std::swap(mNodes[src_index], mNodes[dst_index]);
}

EntityClass::PhysicsJoint* EntityClass::AddJoint(const PhysicsJoint& joint)
{
    mJoints.push_back(std::make_shared<PhysicsJoint>(joint));
    return mJoints.back().get();
}

EntityClass::PhysicsJoint* EntityClass::AddJoint(PhysicsJoint&& joint)
{
    mJoints.push_back(std::make_shared<PhysicsJoint>(std::move(joint)));
    return mJoints.back().get();
}

void EntityClass::SetJoint(size_t index, const PhysicsJoint& joint)
{
    *base::SafeIndex(mJoints, index) = joint;
}
void EntityClass::SetJoint(size_t index, PhysicsJoint&& joint)
{
    *base::SafeIndex(mJoints, index) = std::move(joint);
}

EntityClass::PhysicsJoint& EntityClass::GetJoint(size_t index)
{
    return *base::SafeIndex(mJoints, index);
}
EntityClass::PhysicsJoint* EntityClass::FindJointById(const std::string& id)
{
    for (auto& joint : mJoints)
        if (joint->id == id) return joint.get();

    return nullptr;
}
EntityClass::PhysicsJoint* EntityClass::FindJointByNodeId(const std::string& id)
{
    for (auto& joint : mJoints)
    {
        if (joint->src_node_id == id ||
            joint->dst_node_id == id)
            return joint.get();
    }
    return nullptr;
}

const EntityClass::PhysicsJoint& EntityClass::GetJoint(size_t index) const
{
    return *base::SafeIndex(mJoints, index);
}
const EntityClass::PhysicsJoint* EntityClass::FindJointById(const std::string& id) const
{
    for (auto& joint : mJoints)
        if (joint->id == id) return joint.get();

    return nullptr;
}
const EntityClass::PhysicsJoint* EntityClass::FindJointByNodeId(const std::string& id) const
{
    for (auto& joint : mJoints)
    {
        if (joint->src_node_id == id ||
            joint->dst_node_id == id)
            return joint.get();
    }
    return nullptr;
}

void EntityClass::DeleteJointById(const std::string& id)
{
    for (auto it = mJoints.begin(); it != mJoints.end(); ++it)
    {
        if ((*it)->id == id)
        {
            mJoints.erase(it);
            return;
        }
    }
}

void EntityClass::DeleteJoint(std::size_t index)
{
    base::SafeErase(mJoints, index);
}

void EntityClass::DeleteInvalidJoints()
{
    mJoints.erase(std::remove_if(mJoints.begin(), mJoints.end(),
        [this](const auto& joint) {
            const auto* dst_node = FindNodeById(joint->dst_node_id);
            const auto* src_node = FindNodeById(joint->src_node_id);
            if (!dst_node || !src_node || (dst_node == src_node) ||
                !dst_node->HasRigidBody() ||
                !src_node->HasRigidBody())
                return true;
            return false;
    }), mJoints.end());
}

void EntityClass::FindInvalidJoints(std::vector<PhysicsJoint*>* invalid)
{
    // a joint is considered invalid when
    // - the src and dst nodes are the same.
    // - either dst or src node doesn't exist
    // - either dst or src node doesn't have rigid body
    for (auto& joint : mJoints)
    {
        const auto* dst_node = FindNodeById(joint->dst_node_id);
        const auto* src_node = FindNodeById(joint->src_node_id);
        if (!src_node || !dst_node || (dst_node == src_node) ||
            !dst_node->HasRigidBody() ||
            !src_node->HasRigidBody())
        {
            invalid->push_back(joint.get());
        }
    }
}

void EntityClass::DeleteInvalidFixtures()
{
    for (auto& node : mNodes)
    {
        if (auto* fixture = node->GetFixture())
        {
            if (!FindNodeById(fixture->GetRigidBodyNodeId()))
                node->RemoveFixture();
        }
    }
}

EntityNodeClass& EntityClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityNodeClass* EntityClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
EntityNodeClass* EntityClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const EntityNodeClass& EntityClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityNodeClass* EntityClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const EntityNodeClass* EntityClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

AnimationClass* EntityClass::AddAnimation(AnimationClass&& track)
{
    mAnimations.push_back(std::make_shared<AnimationClass>(std::move(track)));
    return mAnimations.back().get();
}
AnimationClass* EntityClass::AddAnimation(const AnimationClass& track)
{
    mAnimations.push_back(std::make_shared<AnimationClass>(track));
    return mAnimations.back().get();
}
AnimationClass* EntityClass::AddAnimation(std::unique_ptr<AnimationClass> track)
{
    mAnimations.push_back(std::move(track));
    return mAnimations.back().get();
}
void EntityClass::DeleteAnimation(size_t index)
{
    base::SafeErase(mAnimations, index);
}
bool EntityClass::DeleteAnimationByName(const std::string& name)
{
    return EraseByName(mAnimations, name);
}
bool EntityClass::DeleteAnimationById(const std::string& id)
{
    return EraseById(mAnimations, id);
}
AnimationClass& EntityClass::GetAnimation(size_t i)
{
    ASSERT(i < mAnimations.size());
    return *mAnimations[i].get();
}
AnimationClass* EntityClass::FindAnimationByName(const std::string& name)
{
    return FindByName(mAnimations, name);
}
const AnimationClass& EntityClass::GetAnimation(size_t i) const
{
    ASSERT(i < mAnimations.size());
    return *mAnimations[i].get();
}
const AnimationClass* EntityClass::FindAnimationByName(const std::string& name) const
{
    return FindByName(mAnimations, name);
}

AnimatorClass* EntityClass::AddAnimator(AnimatorClass&& animator)
{
    mAnimators.push_back(std::make_shared<AnimatorClass>(std::move(animator)));
    return mAnimators.back().get();
}
AnimatorClass* EntityClass::AddAnimator(const AnimatorClass& animator)
{
    mAnimators.push_back(std::make_shared<AnimatorClass>(animator));
    return mAnimators.back().get();
}
AnimatorClass* EntityClass::AddAnimator(const std::shared_ptr<AnimatorClass>& animator)
{
    mAnimators.push_back(animator);
    return mAnimators.back().get();
}

void EntityClass::DeleteAnimator(size_t index)
{
    base::SafeErase(mAnimators, index);
}
bool EntityClass::DeleteAnimatorByName(const std::string& name)
{
    return EraseByName(mAnimators, name);
}

bool EntityClass::DeleteAnimatorById(const std::string& id)
{
    return EraseById(mAnimators, id);
}

AnimatorClass& EntityClass::GetAnimator(size_t index)
{
    return *base::SafeIndex(mAnimators, index);
}
AnimatorClass* EntityClass::FindAnimatorByName(const std::string& name)
{
    return FindByName(mAnimators, name);
}
AnimatorClass* EntityClass::FindAnimatorById(const std::string& id)
{
    return FindById(mAnimators, id);
}

const AnimatorClass& EntityClass::GetAnimator(size_t index) const
{
    return *base::SafeIndex(mAnimators, index);
}
const AnimatorClass* EntityClass::FindAnimatorByName(const std::string& name) const
{
    return FindByName(mAnimators, name);
}
const AnimatorClass* EntityClass::FindAnimatorById(const std::string& id) const
{
    return FindById(mAnimators, id);
}

void EntityClass::LinkChild(EntityNodeClass* parent, EntityNodeClass* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void EntityClass::BreakChild(EntityNodeClass* child, bool keep_world_transform)
{
    game::BreakChild(mRenderTree, child, keep_world_transform);
}

void EntityClass::ReparentChild(EntityNodeClass* parent, EntityNodeClass* child, bool keep_world_transform)
{
    game::ReparentChild(mRenderTree, parent, child, keep_world_transform);
}

void EntityClass::DeleteNode(EntityNodeClass* node)
{
    // Erase joints that refer to this node in order to maintain
    // the invariant that the joints are always valid.
    mJoints.erase(std::remove_if(mJoints.begin(), mJoints.end(),
                                 [node](const auto& joint) {
        return joint->src_node_id == node->GetId() ||
               joint->dst_node_id == node->GetId();
    }), mJoints.end());
    game::DeleteNode(mRenderTree, node, mNodes);
}

EntityNodeClass* EntityClass::DuplicateNode(const EntityNodeClass* node)
{
    std::vector<std::unique_ptr<EntityNodeClass>> clones;

    auto* ret = game::DuplicateNode(mRenderTree, node, &clones);
    for (auto& clone : clones)
        mNodes.push_back(std::move(clone));
    return ret;

}

void EntityClass::CoarseHitTest(float x, float y, std::vector<EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void EntityClass::CoarseHitTest(const glm::vec2& pos, std::vector<EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, pos.x,pos.y, hits, hitbox_positions);
}
void EntityClass::CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void EntityClass::CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}
glm::vec2 EntityClass::MapCoordsFromNodeBox(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, pos.x, pos.y, node);
}
glm::vec2 EntityClass::MapCoordsToNodeBox(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsToNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, pos.x, pos.y, node);
}

FRect EntityClass::FindNodeBoundingRect(const EntityNodeClass* node) const
{
    return game::FindBoundingRect(mRenderTree, node);
}
FRect EntityClass::GetBoundingRect() const
{
    return game::FindBoundingRect(mRenderTree);
}

FBox EntityClass::FindNodeBoundingBox(const EntityNodeClass* node) const
{
    return game::FindBoundingBox(mRenderTree, node);
}

size_t EntityClass::FindNodeIndex(const EntityNodeClass* node) const
{
    for (size_t i=0; i<mNodes.size(); ++i)
    {
        if (mNodes[i].get() == node)
            return i;
    }
    return mNodes.size();
}

glm::mat4 EntityClass::FindNodeTransform(const EntityNodeClass* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}

glm::mat4 EntityClass::FindNodeModelTransform(const EntityNodeClass* node) const
{
    return game::FindNodeModelTransform(mRenderTree, node);
}

void EntityClass::AddScriptVar(const ScriptVar& var)
{
    mScriptVars.push_back(std::make_shared<ScriptVar>(var));
}
void EntityClass::AddScriptVar(ScriptVar&& var)
{
    mScriptVars.push_back(std::make_shared<ScriptVar>(std::move(var)));
}
void EntityClass::DeleteScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    auto it = mScriptVars.begin();
    mScriptVars.erase(it + index);
}
void EntityClass::SetScriptVar(size_t index, const ScriptVar& var)
{
    ASSERT(index <mScriptVars.size());
    *mScriptVars[index] = var;
}
void EntityClass::SetScriptVar(size_t index, ScriptVar&& var)
{
    ASSERT(index <mScriptVars.size());
    *mScriptVars[index] = std::move(var);
}
ScriptVar& EntityClass::GetScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    return *mScriptVars[index];
}
ScriptVar* EntityClass::FindScriptVarByName(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}
ScriptVar* EntityClass::FindScriptVarById(const std::string& id)
{
    for (auto& var : mScriptVars)
        if (var->GetId() == id)
            return var.get();
    return nullptr;
}

const ScriptVar& EntityClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return *mScriptVars[index];
}
const ScriptVar* EntityClass::FindScriptVarByName(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}
const ScriptVar* EntityClass::FindScriptVarById(const std::string& id) const
{
    for  (auto& var : mScriptVars)
        if (var->GetId() == id)
            return var.get();
    return nullptr;
}

std::size_t EntityClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mTag);
    hash = base::hash_combine(hash, mIdleTrackId);
    hash = base::hash_combine(hash, mScriptFile);
    hash = base::hash_combine(hash, mFlags.value());
    hash = base::hash_combine(hash, mLifetime);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const EntityNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });

    for (const auto& track : mAnimations)
        hash = base::hash_combine(hash, track->GetHash());

    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var->GetHash());

    for (const auto& joint : mJoints)
    {
        size_t jh = 0;
        jh = base::hash_combine(jh, joint->id);
        jh = base::hash_combine(jh, joint->type);
        jh = base::hash_combine(jh, joint->src_node_id);
        jh = base::hash_combine(jh, joint->dst_node_id);
        jh = base::hash_combine(jh, joint->dst_node_anchor_point);
        jh = base::hash_combine(jh, joint->src_node_anchor_point);
        jh = base::hash_combine(jh, joint->name);
        if (const auto* ptr = std::get_if<DistanceJointParams>(&joint->params))
        {
            jh = base::hash_combine(jh, ptr->min_distance.has_value());
            jh = base::hash_combine(jh, ptr->max_distance.has_value());
            jh = base::hash_combine(jh, ptr->max_distance.value_or(0.0f));
            jh = base::hash_combine(jh, ptr->min_distance.value_or(0.0f));
            jh = base::hash_combine(jh, ptr->stiffness);
            jh = base::hash_combine(jh, ptr->damping);
        }
        hash = base::hash_combine(hash, jh);
    }

    for (const auto& animator : mAnimators)
    {
        hash = base::hash_combine(hash, animator->GetHash());
    }

    return hash;
}

void EntityClass::IntoJson(data::Writer& data) const
{
    data.Write("id",          mClassId);
    data.Write("name",        mName);
    data.Write("tag",         mTag);
    data.Write("idle_track",  mIdleTrackId);
    data.Write("script_file", mScriptFile);
    data.Write("flags",       mFlags);
    data.Write("lifetime",    mLifetime);

    for (const auto& node : mNodes)
    {
        auto chunk = data.NewWriteChunk();
        node->IntoJson(*chunk);
        data.AppendChunk("nodes", std::move(chunk));
    }

    for (const auto& track : mAnimations)
    {
        auto chunk = data.NewWriteChunk();
        track->IntoJson(*chunk);
        data.AppendChunk("tracks", std::move(chunk));
    }

    for (const auto& var : mScriptVars)
    {
        auto chunk = data.NewWriteChunk();
        var->IntoJson(*chunk);
        data.AppendChunk("vars", std::move(chunk));
    }

    for (const auto& joint : mJoints)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("id", joint->id);
        chunk->Write("type", joint->type);
        chunk->Write("src_node_id", joint->src_node_id);
        chunk->Write("dst_node_id", joint->dst_node_id);
        chunk->Write("src_node_anchor_point", joint->src_node_anchor_point);
        chunk->Write("dst_node_anchor_point", joint->dst_node_anchor_point);
        chunk->Write("name", joint->name);
        if (const auto* ptr = std::get_if<DistanceJointParams>(&joint->params))
        {
            if (ptr->min_distance.has_value())
                chunk->Write("min_dist", ptr->min_distance.value());
            if (ptr->max_distance.has_value())
                chunk->Write("max_dist", ptr->max_distance.value());
            chunk->Write("damping", ptr->damping);
            chunk->Write("stiffness", ptr->stiffness);
        }
        data.AppendChunk("joints", std::move(chunk));
    }

    for (const auto& animator : mAnimators)
    {
        auto chunk = data.NewWriteChunk();
        animator->IntoJson(*chunk);
        data.AppendChunk("animators", std::move(chunk));
    }

    auto chunk = data.NewWriteChunk();
    RenderTreeIntoJson(mRenderTree, game::TreeNodeToJson<EntityNodeClass>, *chunk);
    data.Write("render_tree", std::move(chunk));
}

bool EntityClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",          &mClassId);
    ok &= data.Read("name",        &mName);
    ok &= data.Read("tag",         &mTag);
    ok &= data.Read("idle_track",  &mIdleTrackId);
    ok &= data.Read("script_file", &mScriptFile);
    ok &= data.Read("flags",       &mFlags);
    ok &= data.Read("lifetime",    &mLifetime);

    for (unsigned i=0; i<data.GetNumChunks("nodes"); ++i)
    {
        const auto& chunk = data.GetReadChunk("nodes", i);
        auto klass = std::make_shared<EntityNodeClass>();
        mNodes.push_back(klass);
        if (!klass->FromJson(*chunk)) {
            WARN("Failed to load entity class node completely. [entity=%1', node='%1']", mName, klass->GetName());
            ok = false;
        }
    }
    for (unsigned i=0; i<data.GetNumChunks("tracks"); ++i)
    {
        const auto& chunk = data.GetReadChunk("tracks", i);
        auto klass = std::make_shared<AnimationClass>();
        mAnimations.push_back(klass);
        if (!klass->FromJson(*chunk)) {
            WARN("Failed to load entity animation track completely. [entity='%1', animation='%2']", mName, klass->GetName());
            ok = false;
        }
    }
    for (unsigned i=0; i<data.GetNumChunks("vars"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vars", i);
        auto var = std::make_shared<ScriptVar>();
        if (!var->FromJson(*chunk)) {
            WARN("Failed to load entity script variable completely. [entity='%1', var='%2']", mName, var->GetName());
            ok = false;
        } else mScriptVars.push_back(var);
    }
    for (unsigned i=0; i<data.GetNumChunks("joints"); ++i)
    {
        const auto& chunk = data.GetReadChunk("joints", i);
        auto joint = std::make_shared<PhysicsJoint>();
        auto& jref = *joint;
        ok &= chunk->Read("id",                    &jref.id);
        ok &= chunk->Read("type",                  &jref.type);
        ok &= chunk->Read("src_node_id",           &jref.src_node_id);
        ok &= chunk->Read("dst_node_id",           &jref.dst_node_id);
        ok &= chunk->Read("src_node_anchor_point", &jref.src_node_anchor_point);
        ok &= chunk->Read("dst_node_anchor_point", &jref.dst_node_anchor_point);
        ok &= chunk->Read("name",                  &jref.name);
        if (jref.type == PhysicsJointType::Distance)
        {
            DistanceJointParams params;
            ok &= chunk->Read("damping", &params.damping);
            ok &= chunk->Read("stiffness", &params.stiffness);
            if (chunk->HasValue("min_dist"))
            {
                float value = 0.0f;
                ok &= chunk->Read("min_dist", &value);
                params.min_distance = value;
            }
            if (chunk->HasValue("max_dist"))
            {
                float value = 0.0f;
                ok &= chunk->Read("max_dist", &value);
                params.max_distance = value;
            }
            joint->params = params;
        }
        mJoints.push_back(std::move(joint));
    }

    for (unsigned i=0; i<data.GetNumChunks("animators"); ++i)
    {
        const auto& chunk = data.GetReadChunk("animators", i);
        auto animator = std::make_shared<AnimatorClass>();
        ok &= animator->FromJson(*chunk);
        mAnimators.push_back(std::move(animator));
    }

    const auto& chunk = data.GetReadChunk("render_tree");
    if (!chunk)
        return false;
    RenderTreeFromJson(mRenderTree, game::TreeNodeFromJson(mNodes), *chunk);
    return ok;
}

EntityClass EntityClass::Clone() const
{
    EntityClass ret;
    ret.mName = mName;
    ret.mFlags = mFlags;
    ret.mLifetime = mLifetime;
    ret.mScriptFile = mScriptFile;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep clone of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<EntityNodeClass>(node->Clone());
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // make a deep clone of the animation tracks
    for (const auto& track : mAnimations)
    {
        auto clone = std::make_unique<AnimationClass>(track->Clone());
        if (track->GetId() == mIdleTrackId)
            ret.mIdleTrackId = clone->GetId();
        ret.mAnimations.push_back(std::move(clone));
    }
    // remap the actuator node ids.
    for (auto& track : ret.mAnimations)
    {
        for (size_t i=0; i<track->GetNumActuators(); ++i)
        {
            auto& actuator = track->GetActuatorClass(i);
            const auto* source_node = FindNodeById(actuator.GetNodeId());
            if (source_node == nullptr)
                continue;
            const auto* cloned_node = map[source_node];
            actuator.SetNodeId(cloned_node->GetId());
        }
    }
    // make a deep copy of the scripting variables.
    for (const auto& var : mScriptVars)
    {
        // remap entity node references.
        if (var->GetType() == ScriptVar::Type::EntityNodeReference)
        {
            std::vector<ScriptVar::EntityNodeReference> refs;
            const auto& src_arr = var->GetArray<ScriptVar::EntityNodeReference>();
            for (const auto& src_ref : src_arr)
            {
                const auto* src_node = FindNodeById(src_ref.id);
                const auto* dst_node = map[src_node];
                ScriptVar::EntityNodeReference ref;
                ref.id = dst_node ? dst_node->GetId() : "";
                refs.push_back(std::move(ref));
            }
            auto clone = std::make_shared<ScriptVar>();
            clone->SetName(var->GetName());
            clone->SetReadOnly(var->IsReadOnly());
            clone->SetArray(var->IsArray());
            clone->SetNewArrayType(std::move(refs));
            ret.mScriptVars.push_back(clone);
        }
        else ret.mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
    }

    // make a deep clone of the joints.
    for (const auto& joint : mJoints)
    {
        auto clone   = std::make_shared<PhysicsJoint>(*joint);
        clone->id    = base::RandomString(10);
        // map the src and dst node IDs.
        const auto* old_src_node = FindNodeById(joint->src_node_id);
        const auto* old_dst_node = FindNodeById(joint->dst_node_id);
        ASSERT(old_src_node && old_dst_node);
        clone->src_node_id = map[old_src_node]->GetId();
        clone->dst_node_id = map[old_dst_node]->GetId();
        ret.mJoints.push_back(std::move(clone));
    }

    for (const auto& animator : mAnimators)
    {
        auto clone = std::make_shared<AnimatorClass>(animator->Clone());
        ret.mAnimators.push_back(std::move(clone));
    }

    ret.mRenderTree.FromTree(mRenderTree, [&map](const EntityNodeClass* node) {
        return map[node];
    });
    return ret;
}

EntityClass& EntityClass::operator=(const EntityClass& other)
{
    if (this == &other)
        return *this;

    EntityClass tmp(other);
    mClassId         = std::move(tmp.mClassId);
    mName            = std::move(tmp.mName);
    mTag             = std::move(tmp.mTag);
    mIdleTrackId     = std::move(tmp.mIdleTrackId);
    mNodes           = std::move(tmp.mNodes);
    mJoints          = std::move(tmp.mJoints);
    mScriptVars      = std::move(tmp.mScriptVars);
    mScriptFile      = std::move(tmp.mScriptFile);
    mRenderTree      = std::move(tmp.mRenderTree);
    mAnimations      = std::move(tmp.mAnimations);
    mAnimators       = std::move(tmp.mAnimators);
    mFlags           = tmp.mFlags;
    mLifetime        = tmp.mLifetime;
    return *this;
}

Entity::Entity(const std::shared_ptr<const EntityClass>& klass)
  : mClass(klass)
  , mInstanceId(FastId(10))
  , mInstanceTag(klass->GetTag())
  , mLifetime(klass->GetLifetime())
  , mFlags(klass->GetFlags())
  , mIdleTrackId(klass->GetIdleTrackId())
{
    std::unordered_map<const EntityNodeClass*, EntityNode*> map;

    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        auto node_klass = mClass->GetSharedEntityNodeClass(i);
        auto node_inst  = CreateEntityNodeInstance(node_klass);
        node_inst->SetEntity(this);
        map[node_klass.get()] = node_inst.get();
        mNodes.push_back(std::move(node_inst));
    }
    // build render tree by mapping class entity node class objects
    // to entity node instance objects
    mRenderTree.FromTree(mClass->GetRenderTree(),
        [&map](const EntityNodeClass* node) {
            return map[node];
        });

    // assign the script variables.
    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        auto var = klass->GetSharedScriptVar(i);
        if (!var->IsReadOnly())
            mScriptVars.push_back(*var);
    }

    // create local joints by mapping the entity class joints from
    // entity class nodes to entity nodes in this entity instance.
    for (size_t i=0; i<mClass->GetNumJoints(); ++i)
    {
        const auto& joint_klass = mClass->GetSharedJoint(i);
        const auto* klass_src_node = mClass->FindNodeById(joint_klass->src_node_id);
        const auto* klass_dst_node = mClass->FindNodeById(joint_klass->dst_node_id);
        ASSERT(klass_src_node && klass_dst_node);
        auto* inst_src_node = map[klass_src_node];
        auto* inst_dst_node = map[klass_dst_node];
        ASSERT(inst_src_node && inst_dst_node);
        PhysicsJoint joint(joint_klass,
                           FastId(10),
                           inst_src_node, inst_dst_node);
        mJoints.push_back(std::move(joint));
    }

    if (mClass->GetNumAnimators())
    {
        mAnimator = Animator(mClass->GEtSharedAnimatorClass(0));
    }
}

Entity::Entity(const EntityArgs& args) : Entity(args.klass)
{
    mInstanceName = args.name;
    mLayer        = args.layer;
    if (!args.id.empty())
        mInstanceId = args.id;

    for (auto& node : mNodes)
    {
        if (mRenderTree.GetParent(node.get()))
            continue;
        // if this is a top level node (i.e. under the root node)
        // then bake the entity transform into this node.
        const auto& rotation    = node->GetRotation();
        const auto& translation = node->GetTranslation();
        const auto& scale       = node->GetScale();
        node->SetRotation(rotation + args.rotation);
        node->SetTranslation(translation + args.position);
        node->SetScale(scale * args.scale);
    }
    mControlFlags.set(ControlFlags::EnableLogging, args.enable_logging);
}

Entity::Entity(const EntityClass& klass)
  : Entity(std::make_shared<EntityClass>(klass))
{}

EntityNode& Entity::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityNode* Entity::FindNodeByClassName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByClassId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}

const EntityNode& Entity::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityNode* Entity::FindNodeByClassName(const std::string& name) const
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByClassId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}

void Entity::CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(const glm::vec2& pos, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void Entity::CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

glm::vec2 Entity::MapCoordsFromNodeBox(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, x, y, node);
}
glm::vec2 Entity::MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNode* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, pos.x, pos.y, node);
}

glm::vec2 Entity::MapCoordsToNodeBox(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, x, y, node);
}

glm::vec2 Entity::MapCoordsToNodeBox(const glm::vec2& pos, const EntityNode* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, pos.x, pos.y, node);
}

glm::mat4 Entity::FindNodeTransform(const EntityNode* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}
glm::mat4 Entity::FindNodeModelTransform(const EntityNode* node) const
{
    return game::FindNodeModelTransform(mRenderTree, node);
}

glm::mat4 Entity::FindRelativeTransform(const EntityNode* parent, const EntityNode* child) const
{
    const auto& parent_to_world = game::FindNodeTransform(mRenderTree, parent);
    const auto& child_to_world  = game::FindNodeTransform(mRenderTree, child);
    const auto& world_to_parent = glm::inverse(parent_to_world);
    return world_to_parent * child_to_world;
}

FRect Entity::FindNodeBoundingRect(const EntityNode* node) const
{
    return game::FindBoundingRect(mRenderTree, node);
}

FRect Entity::GetBoundingRect() const
{
    return game::FindBoundingRect(mRenderTree);
}

FBox Entity::FindNodeBoundingBox(const EntityNode* node) const
{
    return game::FindBoundingBox(mRenderTree, node);
}

void Entity::Die()
{
    SetFlag(ControlFlags::WantsToDie, true);
}
void Entity::DieIn(float seconds)
{
    mScheduledDeath = seconds;
}

void Entity::Update(float dt, std::vector<Event>* events)
{
    mCurrentTime += dt;

    if (mScheduledDeath.has_value())
    {
        float& val = mScheduledDeath.value();
        val -= dt;
        if (val <= 0.0f)
            SetFlag(ControlFlags::WantsToDie, true);
    }

    mFinishedAnimation.reset();

    for (auto it = mTimers.begin(); it != mTimers.end();)
    {
        auto& timer = *it;
        timer.when -= dt;
        if (timer.when < 0.0f)
        {
            if (events)
            {
                TimerEvent event;
                event.name   = timer.name;
                event.jitter = timer.when;
                events->push_back(std::move(event));
            }
            it = mTimers.erase(it);
        } else ++it;
    }

    if (events)
    {
        for (auto& event: mEvents)
        {
            events->push_back(std::move(event));
        }
    }
    mEvents.clear();

    if (!mCurrentAnimation)
        return;

    // Update the animation state.
    mCurrentAnimation->Update(dt);

    // Apply animation state transforms/actions on the entity nodes.
    for (auto& node : mNodes)
    {
        mCurrentAnimation->Apply(*node);
    }

    if (!mCurrentAnimation->IsComplete())
        return;

    if (mCurrentAnimation->IsLooping())
    {
        mCurrentAnimation->Restart();
        for (auto& node : mNodes)
        {
            if (!mRenderTree.GetParent(node.get()))
                continue;

            const auto& klass = node->GetClass();
            const auto& rotation = klass.GetRotation();
            const auto& translation = klass.GetTranslation();
            const auto& scale = klass.GetScale();
            // if the node is a child node (i.e. has a parent) then reset the
            // node's transformation based on the node's transformation relative
            // to its parent in the entity class.
            node->SetTranslation(translation);
            node->SetRotation(rotation);
            node->SetScale(scale);
        }
        return;
    }
    // spams the log
    //DEBUG("Entity animation is complete. [entity='%1/%2', animation='%3']",
    //      mClass->GetName(), mInstanceName, mCurrentAnimation->GetName());
    std::swap(mCurrentAnimation, mFinishedAnimation);
    mCurrentAnimation.reset();
}

void Entity::UpdateAnimator(float dt, std::vector<AnimatorAction>* actions)
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        animator->Update(dt, actions);
    }
}
void Entity::UpdateAnimator(const game::Entity::AnimationTransition* transition,
                            const game::Entity::AnimationState* next)
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        animator->Update(transition, next);
    }
}

const Entity::AnimationState* Entity::GetCurrentAnimatorState() noexcept
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        return animator->GetCurrentState();
    }
    return nullptr;
}

const Entity::AnimationTransition* Entity::GetCurrentAnimationTransition() noexcept
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        return animator->GetTransition();
    }
    return nullptr;
}

Animation* Entity::PlayAnimation(std::unique_ptr<Animation> animation)
{
    // todo: what to do if there's a previous track ?
    // possibilities: reset or queue?
    mCurrentAnimation = std::move(animation);
    return mCurrentAnimation.get();
}
Animation* Entity::PlayAnimation(const Animation& animation)
{
    return PlayAnimation(std::make_unique<Animation>(animation));
}

Animation* Entity::PlayAnimation(Animation&& animation)
{
    return PlayAnimation(std::make_unique<Animation>(std::move(animation)));
}

Animation* Entity::PlayAnimationByName(const std::string& name)
{
    for (size_t i=0; i< mClass->GetNumAnimations(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<Animation>(klass);
        return PlayAnimation(std::move(track));
    }
    return nullptr;
}
Animation* Entity::PlayAnimationById(const std::string& id)
{
    for (size_t i=0; i< mClass->GetNumAnimations(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationClass(i);
        if (klass->GetId() != id)
            continue;
        auto track = std::make_unique<Animation>(klass);
        return PlayAnimation(std::move(track));
    }
    return nullptr;
}

Animation* Entity::PlayIdle()
{
    if (mCurrentAnimation)
        return nullptr;

    if (!mIdleTrackId.empty())
        return PlayAnimationById(mIdleTrackId);
    else if(mClass->HasIdleTrack())
        return PlayAnimationById(mClass->GetIdleTrackId());

    return nullptr;
}

bool Entity::IsDying() const
{
    return mControlFlags.test(ControlFlags::WantsToDie);
}

bool Entity::IsAnimating() const
{
    return !!mCurrentAnimation;
}

bool Entity::HasExpired() const
{
    if (!mFlags.test(EntityClass::Flags::LimitLifetime))
        return false;
    else if (mCurrentTime >= mLifetime)
        return true;
    return false;
}

bool Entity::HasBeenKilled() const
{ return TestFlag(ControlFlags::Killed); }

bool Entity::HasBeenSpawned() const
{ return TestFlag(ControlFlags::Spawned); }

bool Entity::HasRigidBodies() const
{
    for (auto& node : mNodes)
        if (node->HasRigidBody())
            return true;
    return false;
}

bool Entity::HasSpatialNodes() const
{
    for (auto& node : mNodes)
        if (node->HasSpatialNode())
            return true;
    return false;
}

bool Entity::KillAtBoundary() const
{ return TestFlag(Flags::KillAtBoundary); }

bool Entity::DidFinishAnimation() const
{ return !!mFinishedAnimation; }

bool Entity::HasRenderableItems() const
{
    for (auto& node : mNodes)
        if (node->HasDrawable() || node->HasTextItem())
            return true;
    return false;
}

const Entity::PhysicsJoint& Entity::GetJoint(std::size_t index) const
{
    return base::SafeIndex(mJoints, index);
}

const Entity::PhysicsJoint* Entity::FindJointById(const std::string& id) const
{
    for (const auto& joint : mJoints)
    {
        if (joint.GetId() == id)
            return &joint;
    }
    return nullptr;
}

const Entity::PhysicsJoint* Entity::FindJointByNodeId(const std::string& id) const
{
    for (const auto& joint : mJoints)
    {
        if (joint.GetSrcId() == id ||
            joint.GetDstId() == id)
            return &joint;
    }
    return nullptr;
}

const ScriptVar* Entity::FindScriptVarByName(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVarByName(name);
}
const ScriptVar* Entity::FindScriptVarById(const std::string& id) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetId() == id)
            return &var;
    }
    return mClass->FindScriptVarById(id);
}

std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass)
{ return std::make_unique<Entity>(klass); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass)
{ return CreateEntityInstance(std::make_shared<const EntityClass>(klass)); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args)
{ return std::make_unique<Entity>(args); }

std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass)
{ return std::make_unique<EntityNode>(klass); }

} // namespace
