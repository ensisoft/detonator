
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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp> // for glm::inverse
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>
#include <map>
#include <cmath>
#include <unordered_set>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "gamelib/treeop.h"
#include "gamelib/entity.h"
#include "gamelib/transform.h"

namespace game
{

std::size_t RigidBodyItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mSimulation);
    hash = base::hash_combine(hash, mCollisionShape);
    hash = base::hash_combine(hash, mBitFlags.value());
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

nlohmann::json RigidBodyItemClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "simulation",      mSimulation);
    base::JsonWrite(json, "shape",           mCollisionShape);
    base::JsonWrite(json, "flags",           mBitFlags);
    base::JsonWrite(json, "polygon",         mPolygonShapeId);
    base::JsonWrite(json, "friction",        mFriction);
    base::JsonWrite(json, "restitution",     mRestitution);
    base::JsonWrite(json, "angular_damping", mAngularDamping);
    base::JsonWrite(json, "linear_damping",  mLinearDamping);
    base::JsonWrite(json, "density",         mDensity);
    base::JsonWrite(json, "linear_velocity", mLinearVelocity);
    base::JsonWrite(json, "angular_velocity", mAngularVelocity);
    return json;
}
// static
std::optional<RigidBodyItemClass> RigidBodyItemClass::FromJson(const nlohmann::json& json)
{
    RigidBodyItemClass ret;
    if (!base::JsonReadSafe(json, "simulation",      &ret.mSimulation)  ||
        !base::JsonReadSafe(json, "shape",           &ret.mCollisionShape)  ||
        !base::JsonReadSafe(json, "flags",           &ret.mBitFlags)  ||
        !base::JsonReadSafe(json, "polygon",         &ret.mPolygonShapeId)  ||
        !base::JsonReadSafe(json, "friction",        &ret.mFriction)  ||
        !base::JsonReadSafe(json, "restitution",     &ret.mRestitution)  ||
        !base::JsonReadSafe(json, "angular_damping", &ret.mAngularDamping)  ||
        !base::JsonReadSafe(json, "linear_damping",  &ret.mLinearDamping)  ||
        !base::JsonReadSafe(json, "density",         &ret.mDensity) ||
        !base::JsonReadSafe(json, "linear_velocity", &ret.mLinearVelocity) ||
        !base::JsonReadSafe(json, "angular_velocity",&ret.mAngularVelocity))
        return std::nullopt;
    return ret;
}

std::size_t DrawableItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mBitFlags.value());
    hash = base::hash_combine(hash, mMaterialId);
    hash = base::hash_combine(hash, mDrawableId);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mAlpha);
    hash = base::hash_combine(hash, mLineWidth);
    hash = base::hash_combine(hash, mRenderPass);
    hash = base::hash_combine(hash, mRenderStyle);
    hash = base::hash_combine(hash, mTimeScale);
    return hash;
}

nlohmann::json DrawableItemClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "flags",       mBitFlags);
    base::JsonWrite(json, "material",    mMaterialId);
    base::JsonWrite(json, "drawable",    mDrawableId);
    base::JsonWrite(json, "layer",       mLayer);
    base::JsonWrite(json, "alpha",       mAlpha);
    base::JsonWrite(json, "linewidth",   mLineWidth);
    base::JsonWrite(json, "renderpass",  mRenderPass);
    base::JsonWrite(json, "renderstyle", mRenderStyle);
    base::JsonWrite(json, "timescale",   mTimeScale);
    return json;
}

// static
std::optional<DrawableItemClass> DrawableItemClass::FromJson(const nlohmann::json& json)
{
    DrawableItemClass ret;
    if (!base::JsonReadSafe(json, "flags",       &ret.mBitFlags) ||
        !base::JsonReadSafe(json, "material",    &ret.mMaterialId) ||
        !base::JsonReadSafe(json, "drawable",    &ret.mDrawableId) ||
        !base::JsonReadSafe(json, "layer",       &ret.mLayer) ||
        !base::JsonReadSafe(json, "alpha",       &ret.mAlpha) ||
        !base::JsonReadSafe(json, "linewidth",   &ret.mLineWidth) ||
        !base::JsonReadSafe(json, "renderpass",  &ret.mRenderPass) ||
        !base::JsonReadSafe(json, "renderstyle", &ret.mRenderStyle) ||
        !base::JsonReadSafe(json, "timescale",   &ret.mTimeScale))
        return std::nullopt;
    return ret;
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

nlohmann::json EntityNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "class",    mClassId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "size",     mSize);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "flags",    mBitFlags);
    if (mRigidBody)
        json["rigid_body"] = mRigidBody->ToJson();
    if (mDrawable)
        json["drawable_item"] = mDrawable->ToJson();

    return json;
}

// static
std::optional<EntityNodeClass> EntityNodeClass::FromJson(const nlohmann::json &json)
{
    EntityNodeClass ret;
    if (!base::JsonReadSafe(json, "class",    &ret.mClassId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "size",     &ret.mSize) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(json, "flags",    &ret.mBitFlags))
        return std::nullopt;

    if (json.contains("rigid_body"))
    {
        auto body = RigidBodyItemClass::FromJson(json["rigid_body"]);
        if (!body.has_value())
            return std::nullopt;
        ret.mRigidBody = std::make_shared<RigidBodyItemClass>(std::move(body.value()));
    }

    if (json.contains("drawable_item"))
    {
        auto draw = DrawableItemClass::FromJson(json["drawable_item"]);
        if (!draw.has_value())
            return std::nullopt;
        ret.mDrawable = std::make_shared<DrawableItemClass>(std::move(draw.value()));
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
    mBitFlags  = std::move(tmp.mBitFlags);
    return *this;
}

EntityNode::EntityNode(std::shared_ptr<const EntityNodeClass> klass)
    : mClass(klass)
{
    mInstId = base::RandomString(10);
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
}

EntityNode::EntityNode(const EntityNodeClass& klass) : EntityNode(std::make_shared<EntityNodeClass>(klass))
{}

DrawableItem* EntityNode::GetDrawable()
{ return mDrawable.get(); }

RigidBodyItem* EntityNode::GetRigidBody()
{ return mRigidBody.get(); }

const DrawableItem* EntityNode::GetDrawable() const
{ return mDrawable.get(); }

const RigidBodyItem* EntityNode::GetRigidBody() const
{ return mRigidBody.get(); }

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

EntityClass::EntityClass(const EntityClass& other)
{
    mClassId = other.mClassId;
    mName    = other.mName;
    mIdleTrackId = other.mIdleTrackId;

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
glm::vec2 EntityClass::MapCoordsFromNode(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsToNode(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}
FRect EntityClass::GetBoundingRect(const EntityNodeClass* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}
FRect EntityClass::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox EntityClass::GetBoundingBox(const EntityNodeClass* node) const
{
    return game::GetBoundingBox(mRenderTree, node);
}

glm::mat4 EntityClass::GetNodeTransform(const EntityNodeClass* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
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

nlohmann::json EntityClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    base::JsonWrite(json, "name", mName);
    base::JsonWrite(json, "idle_track", mIdleTrackId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }

    for (const auto& track : mAnimationTracks)
    {
        json["tracks"].push_back(track->ToJson());
    }

    for (const auto& var : mScriptVars)
    {
        json["vars"].push_back(var->ToJson());
    }

    json["render_tree"] = mRenderTree.ToJson(game::TreeNodeToJson<EntityNodeClass>);
    return json;
}


// static
std::optional<EntityClass> EntityClass::FromJson(const nlohmann::json& json)
{
    EntityClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mClassId) ||
        !base::JsonReadSafe(json, "name", &ret.mName) ||
        !base::JsonReadSafe(json, "idle_track", &ret.mIdleTrackId))
        return std::nullopt;
    if (json.contains("nodes"))
    {
        for (const auto& json : json["nodes"].items())
        {
            std::optional<EntityNodeClass> node = EntityNodeClass::FromJson(json.value());
            if (!node.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_shared<EntityNodeClass>(std::move(node.value())));
        }
    }
    if (json.contains("tracks"))
    {
        for (const auto& js : json["tracks"].items())
        {
            std::optional<AnimationTrackClass> track = AnimationTrackClass::FromJson(js.value());
            if (!track.has_value())
                return std::nullopt;
            ret.mAnimationTracks.push_back(std::make_shared<AnimationTrackClass>(std::move(track.value())));
        }
    }
    if (json.contains("vars"))
    {
        for (const auto& js : json["vars"].items())
        {
            std::optional<ScriptVar> var = ScriptVar::FromJson(js.value());
            if (!var.has_value())
                return std::nullopt;
            ret.mScriptVars.push_back(std::make_shared<ScriptVar>(var.value()));
        }
    }

    ret.mRenderTree.FromJson(json["render_tree"], game::TreeNodeFromJson(ret.mNodes));
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

    mInstanceId = base::RandomString(10);
    mIdleTrackId = mClass->GetIdleTrackId();
    mFlags.set(Flags::VisibleInGame, true);
}

Entity::Entity(const EntityArgs& args) : Entity(args.klass)
{
    mInstanceName = args.name;
    mInstanceId   = args.id;
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

glm::vec2 Entity::MapCoordsFromNode(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 Entity::MapCoordsToNode(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsToNode(mRenderTree, x, y, node);
}

glm::mat4 Entity::GetNodeTransform(const EntityNode* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}

FRect Entity::GetBoundingRect(const EntityNode* node) const
{
    return game::GetBoundingRect(mRenderTree, node);
}

FRect Entity::GetBoundingRect() const
{
    return game::GetBoundingRect(mRenderTree);
}

FBox Entity::GetBoundingBox(const EntityNode* node) const
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
