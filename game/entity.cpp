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
    hash = base::hash_combine(hash, mLinearVelocity);
    hash = base::hash_combine(hash, mAngularVelocity);
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
    data.Write("linear_velocity", mLinearVelocity);
    data.Write("angular_velocity", mAngularVelocity);
}
// static
std::optional<RigidBodyItemClass> RigidBodyItemClass::FromJson(const data::Reader& data)
{
    RigidBodyItemClass ret;
    if (!data.Read("simulation",      &ret.mSimulation)  ||
        !data.Read("shape",           &ret.mCollisionShape)  ||
        !data.Read("flags",           &ret.mBitFlags)  ||
        !data.Read("polygon",         &ret.mPolygonShapeId)  ||
        !data.Read("friction",        &ret.mFriction)  ||
        !data.Read("restitution",     &ret.mRestitution)  ||
        !data.Read("angular_damping", &ret.mAngularDamping)  ||
        !data.Read("linear_damping",  &ret.mLinearDamping)  ||
        !data.Read("density",         &ret.mDensity) ||
        !data.Read("linear_velocity", &ret.mLinearVelocity) ||
        !data.Read("angular_velocity",&ret.mAngularVelocity))
        return std::nullopt;
    return ret;
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
        if (const auto* ptr = std::get_if<float>(param))
            hash = base::hash_combine(hash, *ptr);
        else if (const auto* ptr = std::get_if<int>(param))
            hash = base::hash_combine(hash, *ptr);
        else if (const auto* ptr = std::get_if<glm::vec2>(param))
            hash = base::hash_combine(hash, *ptr);
        else if (const auto* ptr = std::get_if<glm::vec3>(param))
            hash = base::hash_combine(hash, *ptr);
        else if (const auto* ptr = std::get_if<glm::vec4>(param))
            hash = base::hash_combine(hash, *ptr);
        else if (const auto* ptr = std::get_if<Color4f>(param))
            hash = base::hash_combine(hash, *ptr);
        else BUG("Unhandled material param type.");
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
    for (const auto& param : mMaterialParams)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", param.first);
        if (const auto* ptr = std::get_if<float>(&param.second))
            chunk->Write("value", *ptr);
        else if (const auto* ptr = std::get_if<int>(&param.second))
            chunk->Write("value", *ptr);
        else if (const auto* ptr = std::get_if<glm::vec2>(&param.second))
            chunk->Write("value", *ptr);
        else if (const auto* ptr = std::get_if<glm::vec3>(&param.second))
            chunk->Write("value", *ptr);
        else if (const auto* ptr = std::get_if<glm::vec4>(&param.second))
            chunk->Write("value", *ptr);
        else if (const auto* ptr = std::get_if<Color4f>(&param.second))
            chunk->Write("value", *ptr);
        else BUG("Unhandled uniform type.");
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

// static
std::optional<DrawableItemClass> DrawableItemClass::FromJson(const data::Reader& data)
{
    DrawableItemClass ret;
    if (!data.Read("flags",       &ret.mBitFlags) ||
        !data.Read("material",    &ret.mMaterialId) ||
        !data.Read("drawable",    &ret.mDrawableId) ||
        !data.Read("layer",       &ret.mLayer) ||
        !data.Read("linewidth",   &ret.mLineWidth) ||
        !data.Read("renderpass",  &ret.mRenderPass) ||
        !data.Read("renderstyle", &ret.mRenderStyle) ||
        !data.Read("timescale",   &ret.mTimeScale))
        return std::nullopt;

    for (unsigned i=0; i<data.GetNumChunks("material_params"); ++i)
    {
        MaterialParam param;
        const auto& chunk = data.GetReadChunk("material_params", i);
        std::string name;
        if (!chunk->Read("name", &name))
            return std::nullopt;

        // note: the order of vec2/3/4 reading is important since
        // vec4 parses as vec3/2 and vec3 parses as vec2.
        // this should be fixed in data/json.cpp
        if (ReadMaterialParam<float>(*chunk, param) ||
            ReadMaterialParam<int>(*chunk, param) ||
            ReadMaterialParam<glm::vec4>(*chunk, param) ||
            ReadMaterialParam<glm::vec3>(*chunk, param) ||
            ReadMaterialParam<glm::vec2>(*chunk, param) ||
            ReadMaterialParam<Color4f>(*chunk, param))
        {
            ret.mMaterialParams[std::move(name)] = std::move(param);
        } else return std::nullopt;
    }
    return ret;
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

// static
std::optional<TextItemClass> TextItemClass::FromJson(const data::Reader& data)
{
    TextItemClass ret;
    if (!data.Read("flags",            &ret.mBitFlags) ||
        !data.Read("horizontal_align", &ret.mHAlign) ||
        !data.Read("vertical_align",   &ret.mVAlign) ||
        !data.Read("layer",            &ret.mLayer) ||
        !data.Read("text",             &ret.mText) ||
        !data.Read("font_name",        &ret.mFontName) ||
        !data.Read("font_size",        &ret.mFontSize) ||
        !data.Read("raster_width",     &ret.mRasterWidth) ||
        !data.Read("raster_height",    &ret.mRasterHeight) ||
        !data.Read("line_height",      &ret.mLineHeight) ||
        !data.Read("text_color",       &ret.mTextColor))
        return std::nullopt;
    return ret;
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
}

EntityNodeClass::EntityNodeClass(EntityNodeClass&& other)
{
    mClassId   = std::move(other.mClassId);
    mName      = std::move(other.mName);
    mPosition  = std::move(other.mPosition);
    mScale     = std::move(other.mScale);
    mSize      = std::move(other.mSize);
    mRotation  = std::move(other.mRotation);
    mRigidBody = std::move(other.mRigidBody);
    mDrawable  = std::move(other.mDrawable);
    mTextItem  = std::move(other.mTextItem);
    mBitFlags  = std::move(other.mBitFlags);
}

std::size_t EntityNodeClass::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mSize);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mBitFlags.value());
    if (mRigidBody)
        hash = base::hash_combine(hash, mRigidBody->GetHash());
    if (mDrawable)
        hash = base::hash_combine(hash, mDrawable->GetHash());
    if (mTextItem)
        hash = base::hash_combine(hash, mTextItem->GetHash());
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

glm::mat4 EntityNodeClass::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
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
}

// static
std::optional<EntityNodeClass> EntityNodeClass::FromJson(const data::Reader& data)
{
    EntityNodeClass ret;
    if (!data.Read("class",    &ret.mClassId) ||
        !data.Read("name",     &ret.mName) ||
        !data.Read("position", &ret.mPosition) ||
        !data.Read("scale",    &ret.mScale) ||
        !data.Read("size",     &ret.mSize) ||
        !data.Read("rotation", &ret.mRotation) ||
        !data.Read("flags",    &ret.mBitFlags))
        return std::nullopt;

    if (const auto& chunk = data.GetReadChunk("rigid_body"))
    {
        auto body = RigidBodyItemClass::FromJson(*chunk);
        if (!body.has_value())
            return std::nullopt;
        ret.mRigidBody = std::make_shared<RigidBodyItemClass>(std::move(body.value()));
    }

    if (const auto& chunk = data.GetReadChunk("drawable_item"))
    {
        auto draw = DrawableItemClass::FromJson(*chunk);
        if (!draw.has_value())
            return std::nullopt;
        ret.mDrawable = std::make_shared<DrawableItemClass>(std::move(draw.value()));
    }

    if (const auto& chunk = data.GetReadChunk("text_item"))
    {
        auto text = TextItemClass::FromJson(*chunk);
        if (!text.has_value())
            return std::nullopt;
        ret.mTextItem = std::make_shared<TextItemClass>(std::move(text.value()));
    }
    return ret;
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
    mClassId   = std::move(tmp.mClassId);
    mName      = std::move(tmp.mName);
    mPosition  = std::move(tmp.mPosition);
    mScale     = std::move(tmp.mScale);
    mSize      = std::move(tmp.mSize);
    mRotation  = std::move(tmp.mRotation);
    mRigidBody = std::move(tmp.mRigidBody);
    mDrawable  = std::move(tmp.mDrawable);
    mTextItem  = std::move(tmp.mTextItem);
    mBitFlags  = std::move(tmp.mBitFlags);
    return *this;
}

EntityNode::EntityNode(std::shared_ptr<const EntityNodeClass> klass)
    : mClass(klass)
{
    mInstId = FastId(10);
    mName   = klass->GetName();
    Reset();
}

EntityNode::EntityNode(const EntityNode& other)
{
    mClass    = other.mClass;
    mInstId   = other.mInstId;
    mName     = other.mName;
    mScale    = other.mScale;
    mSize     = other.mSize;
    mPosition = other.mPosition;
    mRotation = other.mRotation;
    if (other.HasRigidBody())
        mRigidBody = std::make_unique<RigidBodyItem>(*other.GetRigidBody());
    if (other.HasDrawable())
        mDrawable = std::make_unique<DrawableItem>(*other.GetDrawable());
    if (other.HasTextItem())
        mTextItem = std::make_unique<TextItem>(*other.GetTextItem());
}

EntityNode::EntityNode(EntityNode&& other)
{
    mClass     = std::move(other.mClass);
    mInstId    = std::move(other.mInstId);
    mName      = std::move(other.mName);
    mScale     = std::move(other.mScale);
    mSize      = std::move(other.mSize);
    mPosition  = std::move(other.mPosition);
    mRotation  = std::move(other.mRotation);
    mRigidBody = std::move(other.mRigidBody);
    mDrawable  = std::move(other.mDrawable);
    mTextItem  = std::move(other.mTextItem);
}

EntityNode::EntityNode(const EntityNodeClass& klass) : EntityNode(std::make_shared<EntityNodeClass>(klass))
{}

DrawableItem* EntityNode::GetDrawable()
{ return mDrawable.get(); }

RigidBodyItem* EntityNode::GetRigidBody()
{ return mRigidBody.get(); }

TextItem* EntityNode::GetTextItem()
{ return mTextItem.get(); }

const DrawableItem* EntityNode::GetDrawable() const
{ return mDrawable.get(); }

const RigidBodyItem* EntityNode::GetRigidBody() const
{ return mRigidBody.get(); }

const TextItem* EntityNode::GetTextItem() const
{ return mTextItem.get(); }

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
}

glm::mat4 EntityNode::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
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
    mFlags.set(Flags::TickEntity, true);
    mFlags.set(Flags::UpdateEntity, true);
    mFlags.set(Flags::WantsKeyEvents, false);
    mFlags.set(Flags::WantsMouseEvents, false);
}

EntityClass::EntityClass(const EntityClass& other)
{
    mClassId = other.mClassId;
    mName    = other.mName;
    mScriptFile  = other.mScriptFile;
    mIdleTrackId = other.mIdleTrackId;
    mFlags = other.mFlags;
    mLifetime = other.mLifetime;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<EntityNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : other.mAnimationTracks)
    {
        mAnimationTracks.push_back(std::make_unique<AnimationTrackClass>(*track));
    }

    for (const auto& var : other.mScriptVars)
    {
        mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
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

AnimationTrackClass* EntityClass::AddAnimationTrack(AnimationTrackClass&& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track)));
    return mAnimationTracks.back().get();
}
AnimationTrackClass* EntityClass::AddAnimationTrack(const AnimationTrackClass& track)
{
    mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(track));
    return mAnimationTracks.back().get();
}
AnimationTrackClass* EntityClass::AddAnimationTrack(std::unique_ptr<AnimationTrackClass> track)
{
    mAnimationTracks.push_back(std::move(track));
    return mAnimationTracks.back().get();
}
void EntityClass::DeleteAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    auto it = mAnimationTracks.begin();
    std::advance(it, i);
    mAnimationTracks.erase(it);
}
bool EntityClass::DeleteAnimationTrackByName(const std::string& name)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetName() == name) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}
bool EntityClass::DeleteAnimationTrackById(const std::string& id)
{
    for (auto it = mAnimationTracks.begin(); it != mAnimationTracks.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            mAnimationTracks.erase(it);
            return true;
        }
    }
    return false;
}
AnimationTrackClass& EntityClass::GetAnimationTrack(size_t i)
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}
AnimationTrackClass* EntityClass::FindAnimationTrackByName(const std::string& name)
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
}
const AnimationTrackClass& EntityClass::GetAnimationTrack(size_t i) const
{
    ASSERT(i < mAnimationTracks.size());
    return *mAnimationTracks[i].get();
}
const AnimationTrackClass* EntityClass::FindAnimationTrackByName(const std::string& name) const
{
    for (const auto& klass : mAnimationTracks)
    {
        if (klass->GetName() == name)
            return klass.get();
    }
    return nullptr;
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
void EntityClass::CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
glm::vec2 EntityClass::MapCoordsFromNodeModel(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsToNodeModel(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}
FRect EntityClass::FindNodeBoundingRect(const EntityNodeClass* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}
FRect EntityClass::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox EntityClass::FindNodeBoundingBox(const EntityNodeClass* node) const
{
    return game::GetBoundingBox(mRenderTree, node);
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
ScriptVar* EntityClass::FindScriptVar(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}
const ScriptVar& EntityClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return *mScriptVars[index];
}
const ScriptVar* EntityClass::FindScriptVar(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}

std::size_t EntityClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
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

    for (const auto& track : mAnimationTracks)
        hash = base::hash_combine(hash, track->GetHash());

    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var->GetHash());
    return hash;
}

void EntityClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mClassId);
    data.Write("name", mName);
    data.Write("idle_track", mIdleTrackId);
    data.Write("script_file", mScriptFile);
    data.Write("flags", mFlags);
    data.Write("lifetime", mLifetime);
    for (const auto& node : mNodes)
    {
        auto chunk = data.NewWriteChunk();
        node->IntoJson(*chunk);
        data.AppendChunk("nodes", std::move(chunk));
    }

    for (const auto& track : mAnimationTracks)
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

    auto chunk = data.NewWriteChunk();
    RenderTreeIntoJson(mRenderTree, game::TreeNodeToJson<EntityNodeClass>, *chunk);
    data.Write("render_tree", std::move(chunk));
}

// static
std::optional<EntityClass> EntityClass::FromJson(const data::Reader& data)
{
    EntityClass ret;
    if (!data.Read("id", &ret.mClassId) ||
        !data.Read("name", &ret.mName) ||
        !data.Read("idle_track", &ret.mIdleTrackId) ||
        !data.Read("script_file", &ret.mScriptFile) ||
        !data.Read("flags", &ret.mFlags) ||
        !data.Read("lifetime", &ret.mLifetime))
        return std::nullopt;

    for (unsigned i=0; i<data.GetNumChunks("nodes"); ++i)
    {
        const auto& chunk = data.GetReadChunk("nodes", i);
        std::optional<EntityNodeClass> node = EntityNodeClass::FromJson(*chunk);
        if (!node.has_value())
            return std::nullopt;
        ret.mNodes.push_back(std::make_shared<EntityNodeClass>(std::move(node.value())));
    }
    for (unsigned i=0; i<data.GetNumChunks("tracks"); ++i)
    {
        const auto& chunk = data.GetReadChunk("tracks", i);
        std::optional<AnimationTrackClass> track = AnimationTrackClass::FromJson(*chunk);
        if (!track.has_value())
            return std::nullopt;
        ret.mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track.value())));
    }
    for (unsigned i=0; i<data.GetNumChunks("vars"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vars", i);
        std::optional<ScriptVar> var = ScriptVar::FromJson(*chunk);
        if (!var.has_value())
            return std::nullopt;
        ret.mScriptVars.push_back(std::make_shared<ScriptVar>(var.value()));
    }

    const auto& chunk = data.GetReadChunk("render_tree");
    if (!chunk)
        return std::nullopt;
    RenderTreeFromJson(ret.mRenderTree, game::TreeNodeFromJson(ret.mNodes), *chunk);
    return ret;
}

EntityClass EntityClass::Clone() const
{
    EntityClass ret;
    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<EntityNodeClass>(node->Clone());
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : mAnimationTracks)
    {
        auto clone = std::make_unique<AnimationTrackClass>(track->Clone());
        if (track->GetId() == mIdleTrackId)
            ret.mIdleTrackId = clone->GetId();
        ret.mAnimationTracks.push_back(std::move(clone));
    }
    // remap the actuator node ids.
    for (auto& track : ret.mAnimationTracks)
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

    for (const auto& var : mScriptVars)
    {
        ret.mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
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
    mClassId     = std::move(tmp.mClassId);
    mName        = std::move(tmp.mName);
    mIdleTrackId = std::move(tmp.mIdleTrackId);
    mNodes       = std::move(tmp.mNodes);
    mScriptVars  = std::move(tmp.mScriptVars);
    mScriptFile  = std::move(tmp.mScriptFile);
    mFlags       = std::move(tmp.mFlags);
    mLifetime    = tmp.mLifetime;
    mRenderTree  = tmp.mRenderTree;
    mAnimationTracks = std::move(tmp.mAnimationTracks);
    return *this;
}

Entity::Entity(std::shared_ptr<const EntityClass> klass)
    : mClass(klass)
{
    std::unordered_map<const EntityNodeClass*, const EntityNode*> map;

    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        auto node_klass = mClass->GetSharedEntityNodeClass(i);
        auto node_inst  = CreateEntityNodeInstance(node_klass);
        map[node_klass.get()] = node_inst.get();
        mNodes.push_back(std::move(node_inst));
    }

    mRenderTree.FromTree(mClass->GetRenderTree(), [&map](const EntityNodeClass* node) {
            return map[node];
        });

    // assign the script variables.
    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        auto var = klass->GetSharedScriptVar(i);
        if (!var->IsReadOnly())
            mScriptVars.push_back(*var);
    }

    mInstanceId  = FastId(10);
    mIdleTrackId = mClass->GetIdleTrackId();
    mFlags       = mClass->GetFlags();
    mLifetime    = mClass->GetLifetime();
}

Entity::Entity(const EntityArgs& args) : Entity(args.klass)
{
    mInstanceName = args.name;
    mInstanceId   = args.id.empty() ? FastId(10) : args.id;

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

EntityNode* Entity::AddNode(const EntityNode& node)
{
    mNodes.emplace_back(new EntityNode(node));
    return mNodes.back().get();
}
EntityNode* Entity::AddNode(EntityNode&& node)
{
    mNodes.emplace_back(new EntityNode(std::move(node)));
    return mNodes.back().get();
}

EntityNode* Entity::AddNode(std::unique_ptr<EntityNode> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

void Entity::LinkChild(EntityNode* parent, EntityNode* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

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

void Entity::DeleteNode(EntityNode* node)
{
    game::DeleteNode(mRenderTree, node, mNodes);
}

void Entity::CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

glm::vec2 Entity::MapCoordsFromNodeModel(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 Entity::MapCoordsToNodeModel(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}

glm::mat4 Entity::FindNodeTransform(const EntityNode* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}
glm::mat4 Entity::FindNodeModelTransform(const EntityNode* node) const
{
    return game::FindNodeModelTransform(mRenderTree, node);
}

FRect Entity::FindNodeBoundingRect(const EntityNode* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}

FRect Entity::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox Entity::FindNodeBoundingBox(const EntityNode* node) const
{
    return game::GetBoundingBox(mRenderTree, node);
}

void Entity::Update(float dt)
{
    mCurrentTime += dt;

    if (!mAnimationTrack)
        return;

    mAnimationTrack->Update(dt);
    for (auto& node : mNodes)
    {
        mAnimationTrack->Apply(*node);
    }

    if (!mAnimationTrack->IsComplete())
        return;

    // spams the log.
    //DEBUG("AnimationTrack '%1' completed.", mAnimationTrack->GetName());

    if (mAnimationTrack->IsLooping())
    {
        mAnimationTrack->Restart();
        for (auto& node : mNodes)
        {
            node->Reset();
        }
        return;
    }
    mAnimationTrack.reset();
}

void Entity::Play(std::unique_ptr<AnimationTrack> track)
{
    // todo: what to do if there's a previous track ?
    // possibilities: reset or queue?
    mAnimationTrack = std::move(track);
}
bool Entity::PlayAnimationByName(const std::string& name)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return true;
    }
    return false;
}
bool Entity::PlayAnimationById(const std::string& id)
{
    for (size_t i=0; i<mClass->GetNumTracks(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationTrackClass(i);
        if (klass->GetId() != id)
            continue;
        auto track = std::make_unique<AnimationTrack>(klass);
        Play(std::move(track));
        return true;
    }
    return false;
}

bool Entity::PlayIdle()
{
    if (mAnimationTrack)
        return false;

    if (!mIdleTrackId.empty())
        return PlayAnimationById(mIdleTrackId);
    else if(mClass->HasIdleTrack())
        return PlayAnimationById(mClass->GetIdleTrackId());

    return false;
}

bool Entity::IsPlaying() const
{
    return !!mAnimationTrack;
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

const ScriptVar* Entity::FindScriptVar(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVar(name);
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
