
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
#include "warnpop.h"

#include <algorithm>
#include <map>
#include <cmath>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "gamelib/treeop.h"
#include "gamelib/scene.h"

namespace game
{

SceneNodeClass::SceneNodeClass(const SceneNodeClass& other)
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

SceneNodeClass::SceneNodeClass(SceneNodeClass&& other)
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

std::size_t SceneNodeClass::GetHash() const
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

void SceneNodeClass::SetRigidBody(const RigidBodyItemClass &body)
{
    mRigidBody = std::make_shared<RigidBodyItemClass>(body);
}

void SceneNodeClass::SetDrawable(const DrawableItemClass &drawable)
{
    mDrawable = std::make_shared<DrawableItemClass>(drawable);
}

glm::mat4 SceneNodeClass::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}
glm::mat4 SceneNodeClass::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

void SceneNodeClass::Update(float time, float dt)
{
}

nlohmann::json SceneNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "class",    mClassId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "size",     mSize);
    base::JsonWrite(json, "rotation", mRotation);
    if (mRigidBody)
        json["rigid_body"] = mRigidBody->ToJson();
    if (mDrawable)
        json["drawable_item"] = mDrawable->ToJson();

    return json;
}

// statid
std::optional<SceneNodeClass> SceneNodeClass::FromJson(const nlohmann::json &json)
{
    SceneNodeClass ret;
    if (!base::JsonReadSafe(json, "class",    &ret.mClassId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "size",     &ret.mSize) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation))
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

SceneNodeClass SceneNodeClass::Clone() const
{
    SceneNodeClass ret(*this);
    ret.mClassId = base::RandomString(10);
    return ret;
}

SceneNodeClass& SceneNodeClass::operator=(const SceneNodeClass& other)
{
    if (this == &other)
        return *this;
    SceneNodeClass tmp(other);
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

SceneNode::SceneNode(std::shared_ptr<const SceneNodeClass> klass)
    : mClass(klass)
{
    mInstId = base::RandomString(10);
    Reset();
}

SceneNode::SceneNode(const SceneNode& other)
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

SceneNode::SceneNode(SceneNode&& other)
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

SceneNode::SceneNode(const SceneNodeClass& klass) : SceneNode(std::make_shared<SceneNodeClass>(klass))
{}

DrawableItem* SceneNode::GetDrawable()
{ return mDrawable.get(); }

RigidBodyItem* SceneNode::GetRigidBody()
{ return mRigidBody.get(); }

const DrawableItem* SceneNode::GetDrawable() const
{ return mDrawable.get(); }

const RigidBodyItem* SceneNode::GetRigidBody() const
{ return mRigidBody.get(); }

void SceneNode::Reset()
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

glm::mat4 SceneNode::GetNodeTransform() const
{
    gfx::Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

glm::mat4 SceneNode::GetModelTransform() const
{
    gfx::Transform transform;
    transform.Scale(mSize);
    // offset the object so that the center of the shape is aligned
    // with the position parameter.
    transform.Translate(-mSize.x * 0.5f, -mSize.y * 0.5f);
    return transform.GetAsMatrix();
}

SceneClass::SceneClass(const SceneClass& other)
{
    mClassId = other.mClassId;

    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        mNodes.emplace_back(new SceneNodeClass(*node));
    }

    // use JSON serialization to create a copy of the render tree.
    nlohmann::json json = other.mRenderTree.ToJson(other);

    mRenderTree = RenderTree::FromJson(json, *this).value();
}

SceneNodeClass* SceneClass::AddNode(const SceneNodeClass& node)
{
    mNodes.emplace_back(new SceneNodeClass(node));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(SceneNodeClass&& node)
{
    mNodes.emplace_back(new SceneNodeClass(std::move(node)));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(std::unique_ptr<SceneNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

SceneNodeClass& SceneClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
SceneNodeClass* SceneClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
SceneNodeClass* SceneClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
const SceneNodeClass& SceneClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const SceneNodeClass* SceneClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const SceneNodeClass* SceneClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}

void SceneClass::DeleteNode(size_t index)
{
    ASSERT(index < mNodes.size());
    mNodes.erase(mNodes.begin() + index);
}

bool SceneClass::DeleteNodeById(const std::string& id)
{
    auto it = std::find_if(mNodes.begin(), mNodes.end(), [&id](const auto& node) {
        return node->GetClassId() == id;
    });
    if (it == mNodes.end())
        return false;
    mNodes.erase(it);
    return true;
}

bool SceneClass::DeleteNodeByName(const std::string& name)
{
    auto it = std::find_if(mNodes.begin(), mNodes.end(), [&name](const auto& node) {
        return node->GetName() == name;
    });
    if (it == mNodes.end())
        return false;
    mNodes.erase(it);
    return true;
}

void SceneClass::CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    RenderTreeFunctions<SceneNodeClass>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void SceneClass::CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    RenderTreeFunctions<SceneNodeClass>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
glm::vec2 SceneClass::MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const
{
    return RenderTreeFunctions<SceneNodeClass>::MapCoordsFromNode(mRenderTree, x, y, node);
}
glm::vec2 SceneClass::MapCoordsToNode(float x, float y, const SceneNodeClass* node) const
{
    return RenderTreeFunctions<SceneNodeClass>::MapCoordsToNode(mRenderTree, x, y, node);
}
gfx::FRect SceneClass::GetBoundingRect(const SceneNodeClass* node) const
{
    return RenderTreeFunctions<SceneNodeClass>::GetBoundingRect(mRenderTree, node);
}
gfx::FRect SceneClass::GetBoundingRect() const
{
    return RenderTreeFunctions<SceneNodeClass>::GetBoundingRect(mRenderTree);
}

std::size_t SceneClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const SceneNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });
    return hash;
}

SceneNodeClass* SceneClass::TreeNodeFromJson(const nlohmann::json &json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;

    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetClassId() == id) return it.get();

    BUG("No such node found.");
}

nlohmann::json SceneClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }
    using Serializer = SceneClass;
    json["render_tree"] = mRenderTree.ToJson<Serializer>();
    return json;
}

// static
nlohmann::json SceneClass::TreeNodeToJson(const SceneNodeClass *node)
{
    // do only shallow serialization of the animation node,
    // i.e. only record the id so that we can restore the node
    // later on load based on the ID.
    nlohmann::json ret;
    if (node)
        ret["id"] = node->GetClassId();
    return ret;
}

// static
std::optional<SceneClass> SceneClass::FromJson(const nlohmann::json& json)
{
    SceneClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mClassId))
        return std::nullopt;
    if (json.contains("nodes"))
    {
        for (const auto& json : json["nodes"].items())
        {
            std::optional<SceneNodeClass> node = SceneNodeClass::FromJson(json.value());
            if (!node.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_shared<SceneNodeClass>(std::move(node.value())));
        }
    }
    auto& serializer = ret;

    auto render_tree = RenderTree::FromJson(json["render_tree"], serializer);
    if (!render_tree.has_value())
        return std::nullopt;
    ret.mRenderTree = std::move(render_tree.value());
    return ret;
}

SceneClass SceneClass::Clone() const
{
    SceneClass ret;

    struct Serializer {
        SceneNodeClass* TreeNodeFromJson(const nlohmann::json& json)
        {
            if (!json.contains("id")) // root node has no id
                return nullptr;
            const std::string& old_id = json["id"];
            const std::string& new_id = idmap[old_id];
            auto* ret = nodes[new_id];
            ASSERT(ret != nullptr && "No such node found.");
            return ret;
        }
        std::unordered_map<std::string, std::string> idmap;
        std::unordered_map<std::string, SceneNodeClass*> nodes;
    };
    Serializer serializer;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<SceneNodeClass>(node->Clone());
        serializer.idmap[node->GetClassId()]  = clone->GetClassId();
        serializer.nodes[clone->GetClassId()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // use the json serialization setup the copy of the
    // render tree.
    nlohmann::json json = mRenderTree.ToJson(*this);
    // build our render tree.
    ret.mRenderTree = RenderTree::FromJson(json, serializer).value();
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId    = std::move(tmp.mClassId);
    mNodes      = std::move(tmp.mNodes);
    mRenderTree = tmp.mRenderTree;
    return *this;
}

Scene::Scene(std::shared_ptr<const SceneClass> klass)
    : mClass(klass)
{
    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        auto node = CreateSceneNodeInstance(mClass->GetSharedSceneNodeClass(i));
        mInstanceIdMap[node->GetInstanceId()] = node.get();
        mNodes.push_back(std::move(node));
    }

    // rebuild the render tree through JSON serialization
    nlohmann::json json = mClass->GetRenderTree().ToJson(*mClass);

    mRenderTree = RenderTree::FromJson(json, *this).value();
}

Scene::Scene(const SceneClass& klass)
  : Scene(std::make_shared<SceneClass>(klass))
{}

SceneNode* Scene::AddNode(const SceneNode& node)
{
    mNodes.emplace_back(new SceneNode(node));
    auto* back = mNodes.back().get();
    mInstanceIdMap[back->GetInstanceId()] = back;
    return back;
}
SceneNode* Scene::AddNode(SceneNode&& node)
{
    mNodes.emplace_back(new SceneNode(std::move(node)));
    auto* back = mNodes.back().get();
    mInstanceIdMap[back->GetInstanceId()] = back;
    return back;
}

SceneNode* Scene::AddNode(std::unique_ptr<SceneNode> node)
{
    mNodes.push_back(std::move(node));
    auto* back = mNodes.back().get();
    mInstanceIdMap[back->GetInstanceId()] = back;
    return back;
}

void Scene::LinkChild(SceneNode* parent, SceneNode* child)
{
    ASSERT(child);
    ASSERT(child != parent);

    if (parent == nullptr)
    {
        mRenderTree.AppendChild(child);
        return;
    }
    auto* tree_node = mRenderTree.FindNodeByValue(parent);
    ASSERT(tree_node);
    ASSERT(mRenderTree.FindNodeByValue(child) == nullptr);
    tree_node->AppendChild(child);
}

SceneNode& Scene::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
SceneNode* Scene::FindNodeByClassName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
SceneNode* Scene::FindNodeByClassId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
SceneNode* Scene::FindNodeByInstanceId(const std::string& id)
{
    auto it = mInstanceIdMap.find(id);
    if (it == mInstanceIdMap.end())
        return nullptr;
    return it->second;
}
SceneNode* Scene::FindNodeByInstanceName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node->GetInstanceName() == name)
            return node.get();
    return nullptr;
}

const SceneNode& Scene::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const SceneNode* Scene::FindNodeByClassName(const std::string& name) const
{
    for (auto& node : mNodes)
        if (node->GetClassName() == name)
            return node.get();
    return nullptr;
}
const SceneNode* Scene::FindNodeByClassId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node->GetClassId() == id)
            return node.get();
    return nullptr;
}
const SceneNode* Scene::FindNodeByInstanceId(const std::string& id) const
{
    auto it = mInstanceIdMap.find(id);
    if (it == mInstanceIdMap.end())
        return nullptr;
    return it->second;
}
const SceneNode* Scene::FindNodeByInstanceName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetInstanceName() == name)
            return node.get();
    return nullptr;
}

void Scene::DeleteNode(size_t index)
{
    ASSERT(index < mNodes.size());

    const auto& node = mNodes[index];
    DeleteNodeByInstanceId(node->GetInstanceId());
}

bool Scene::DeleteNodeByInstanceId(const std::string& id)
{
    auto it = mInstanceIdMap.find(id);
    if (it == mInstanceIdMap.end())
        return false;

    auto* scene_node = it->second;
    scene_node->mKilled = true;
    // if the node is in the render tree then  find the render tree node
    // that contains the node to be deleted.
    if (auto* tree_node = mRenderTree.FindNodeByValue(scene_node))
    {
        // traverse the tree starting from the node to be deleted
        // and capture the ids of the animation nodes that are part
        // of this hierarchy.
        tree_node->PreOrderTraverseForEach([](SceneNode *value) {
            value->mKilled = true;
        });

        // find the parent node and remove the node from the render tree.
        // this will remove all the render tree node's children too.
        auto *tree_parent = mRenderTree.FindParent(tree_node);
        tree_parent->DeleteChild(tree_node);
    }

    // mark each node for removal and and remove from the instance id map.
    for (auto it = mInstanceIdMap.begin(); it != mInstanceIdMap.end();)
    {
        auto* node = it->second;
        if (node->mKilled)
        {
            it = mInstanceIdMap.erase(it);
            continue;
        }
        ++it;
    }

    // remove each node from the node container
    mNodes.erase(std::remove_if(mNodes.begin(), mNodes.end(), [](const auto& node) {
        return node->mKilled;
    }), mNodes.end());

    // some nodes were removed.
    return true;
}

void Scene::CoarseHitTest(float x, float y, std::vector<SceneNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    RenderTreeFunctions<SceneNode>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Scene::CoarseHitTest(float x, float y, std::vector<const SceneNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    RenderTreeFunctions<SceneNode>::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

glm::vec2 Scene::MapCoordsFromNode(float x, float y, const SceneNode* node) const
{
    return RenderTreeFunctions<SceneNode>::MapCoordsFromNode(mRenderTree, x, y, node);
}

glm::vec2 Scene::MapCoordsToNode(float x, float y, const SceneNode* node) const
{
    return RenderTreeFunctions<SceneNode>::MapCoordsToNode(mRenderTree, x, y, node);
}

gfx::FRect Scene::GetBoundingRect(const SceneNode* node) const
{
    return RenderTreeFunctions<SceneNode>::GetBoundingRect(mRenderTree, node);
}

gfx::FRect Scene::GetBoundingRect() const
{
    return RenderTreeFunctions<SceneNode>::GetBoundingRect(mRenderTree);
}

FBox Scene::GetBoundingBox(const SceneNode* node) const
{
    return RenderTreeFunctions<SceneNode>::GetBoundingBox(mRenderTree, node);
}

SceneNode* Scene::TreeNodeFromJson(const nlohmann::json &json)
{
    if (!json.contains("id")) // root node has no id
        return nullptr;
    const std::string& id = json["id"];
    for (auto& it : mNodes)
        if (it->GetClassId() == id) return it.get();

    BUG("No such node found");
    return nullptr;
}

std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass)
{ return std::make_unique<Scene>(klass); }

std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass)
{ return CreateSceneInstance(std::make_shared<const SceneClass>(klass)); }

std::unique_ptr<SceneNode> CreateSceneNodeInstance(std::shared_ptr<const SceneNodeClass> klass)
{ return std::make_unique<SceneNode>(klass); }

} // namespace
