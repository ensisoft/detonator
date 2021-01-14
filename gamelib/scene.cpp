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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <unordered_set>

#include "base/format.h"
#include "base/logging.h"
#include "gamelib/scene.h"
#include "gamelib/entity.h"
#include "gamelib/treeop.h"
#include "gamelib/transform.h"

namespace game
{

std::size_t SceneNodeClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mEntityId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mFlags.value());
    hash = base::hash_combine(hash, mLayer);
    return hash;
}

glm::mat4 SceneNodeClass::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

SceneNodeClass SceneNodeClass::Clone() const
{
    SceneNodeClass copy(*this);
    copy.mClassId = base::RandomString(10);
    return copy;
}

nlohmann::json SceneNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",       mClassId);
    base::JsonWrite(json, "entity",   mEntityId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "flags", mFlags.value());
    base::JsonWrite(json, "layer",    mLayer);
    return json;
}

// static
std::optional<SceneNodeClass> SceneNodeClass::FromJson(const nlohmann::json& json)
{
    unsigned flags = 0;
    SceneNodeClass ret;
    if (!base::JsonReadSafe(json, "id",       &ret.mClassId)||
        !base::JsonReadSafe(json, "entity",   &ret.mEntityId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(json, "flags",    &flags) ||
        !base::JsonReadSafe(json, "layer",    &ret.mLayer))
        return std::nullopt;
    ret.mFlags.set_from_value(flags);
    return ret;
}

SceneClass::SceneClass(const SceneClass& other)
{
    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    mClassId    = other.mClassId;
    mScriptVars = other.mScriptVars;
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<SceneNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }
    mRenderTree.FromTree(other.mRenderTree, [&map](const SceneNodeClass* node) {
        return map[node];
    });
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
        if (node->GetId() == id)
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
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

void SceneClass::LinkChild(SceneNodeClass* parent, SceneNodeClass* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void SceneClass::BreakChild(SceneNodeClass* child)
{
    game::BreakChild(mRenderTree, child);
}

void SceneClass::ReparentChild(SceneNodeClass* parent, SceneNodeClass* child, bool keep_world_transform)
{
    game::ReparentChild(mRenderTree, parent, child, keep_world_transform);
}

void SceneClass::DeleteNode(SceneNodeClass* node)
{
    game::DeleteNode(mRenderTree, node, mNodes);
}

SceneNodeClass* SceneClass::DuplicateNode(const SceneNodeClass* node)
{
    return game::DuplicateNode(mRenderTree, node, &mNodes);
}

void SceneClass::CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    // todo: improve the implementation with some form of space partitioning.
    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(float x, float y,
                std::vector<SceneNodeClass*>& hit_nodes,
                std::vector<glm::vec2>* hit_pos)
          : mHitPos(x, y, 1.0f, 1.0f)
          , mHitNodes(hit_nodes)
          , mHitPositions(hit_pos)
        {}
        virtual void EnterNode(SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            auto klass = node->GetEntityClass();
            if (!klass) {
                WARN("Node '%1' has no entity class object!", node->GetName());
                return;
            }

            auto scene_to_entity = glm::inverse(mTransform.GetAsMatrix());
            auto entity_hit_pos  = scene_to_entity * mHitPos;
            std::vector<const EntityNodeClass*> nodes;
            klass->CoarseHitTest(entity_hit_pos.x, entity_hit_pos.y, &nodes);
            if (!nodes.empty())
            {
                mHitNodes.push_back(node);
                if (mHitPositions)
                    mHitPositions->push_back(glm::vec2(entity_hit_pos.x, entity_hit_pos.y));
            }
        }
        virtual void LeaveNode(SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPos;
        std::vector<SceneNodeClass*>& mHitNodes;
        std::vector<glm::vec2>* mHitPositions = nullptr;
        Transform mTransform;
    };
    Visitor visitor(x, y, *hits, hitbox_positions);
    mRenderTree.PreOrderTraverse(visitor);
}
void SceneClass::CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                       std::vector<glm::vec2>* hitbox_positions) const
{
    // todo: improve the implementation with some form of space partitioning.
    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(float x, float y,
                std::vector<const SceneNodeClass*>& hit_nodes,
                std::vector<glm::vec2>* hit_pos)
                : mHitPos(x, y, 1.0f, 1.0f)
                , mHitNodes(hit_nodes)
                , mHitPositions(hit_pos)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            auto klass = node->GetEntityClass();
            if (!klass) {
                WARN("Node '%1' has no entity class object!", node->GetName());
                return;
            }

            auto scene_to_entity = glm::inverse(mTransform.GetAsMatrix());
            auto entity_hit_pos  = scene_to_entity * mHitPos;
            std::vector<const EntityNodeClass*> nodes;
            klass->CoarseHitTest(entity_hit_pos.x, entity_hit_pos.y, &nodes);
            if (!nodes.empty())
            {
                mHitNodes.push_back(node);
                if (mHitPositions)
                    mHitPositions->push_back(glm::vec2(entity_hit_pos.x, entity_hit_pos.y));
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPos;
        std::vector<const SceneNodeClass*>& mHitNodes;
        std::vector<glm::vec2>* mHitPositions = nullptr;
        Transform mTransform;
    };
    Visitor visitor(x, y, *hits, hitbox_positions);
    mRenderTree.PreOrderTraverse(visitor);
}


glm::vec2 SceneClass::MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const
{
    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(float x, float y, const SceneNodeClass* node)
          : mPos(x, y, 1.0f, 1.0f)
          , mNode(node)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            if (node == mNode)
            {
                auto mat = mTransform.GetAsMatrix();
                auto ret = mat * mPos;
                mResult.x = ret.x;
                mResult.y = ret.y;
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        glm::vec2 GetResult() const
        { return mResult; }
    private:
        const glm::vec4 mPos;
        const SceneNodeClass* mNode = nullptr;
        Transform mTransform;
        glm::vec2 mResult;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}
glm::vec2 SceneClass::MapCoordsToNode(float x, float y, const SceneNodeClass* node) const
{
    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(float x, float y, const SceneNodeClass* node)
                : mPos(x, y, 1.0f, 1.0f)
                , mNode(node)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            if (node == mNode)
            {
                auto mat = glm::inverse(mTransform.GetAsMatrix());
                auto ret = mat * mPos;
                mResult.x = ret.x;
                mResult.y = ret.y;
            }
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        glm::vec2 GetResult() const
        { return mResult; }
    private:
        const glm::vec4 mPos;
        const SceneNodeClass* mNode = nullptr;
        Transform mTransform;
        glm::vec2 mResult;
    };

    Visitor visitor(x, y, node);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}

void SceneClass::AddScriptVar(const ScriptVar& var)
{
    mScriptVars.push_back(var);
}
void SceneClass::AddScriptVar(ScriptVar&& var)
{
    mScriptVars.push_back(var);
}
void SceneClass::DeleteScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    auto it = mScriptVars.begin();
    mScriptVars.erase(it + index);
}
void SceneClass::SetScriptVar(size_t index, const ScriptVar& var)
{
    ASSERT(index <mScriptVars.size());
    mScriptVars[index] = var;
}
void SceneClass::SetScriptVar(size_t index, ScriptVar&& var)
{
    ASSERT(index <mScriptVars.size());
    mScriptVars[index] = std::move(var);
}
ScriptVar& SceneClass::GetScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    return mScriptVars[index];
}
ScriptVar* SceneClass::FindScriptVar(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}
const ScriptVar& SceneClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return mScriptVars[index];
}
const ScriptVar* SceneClass::FindScriptVar(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}

size_t SceneClass::GetHash() const
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
    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var.GetHash());
    return hash;
}

nlohmann::json SceneClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }
    for (const auto& var : mScriptVars)
    {
        json["vars"].push_back(var.ToJson());
    }
    json["render_tree"] = mRenderTree.ToJson(&game::TreeNodeToJson<SceneNodeClass>);
    return json;
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
            ret.mNodes.push_back(std::make_unique<SceneNodeClass>(std::move(node.value())));
        }
    }
    if (json.contains("vars"))
    {
        for (const auto& js : json["vars"].items())
        {
            std::optional<ScriptVar> var = ScriptVar::FromJson(js.value());
            if (!var.has_value())
                return std::nullopt;
            ret.mScriptVars.push_back(std::move(var.value()));
        }
    }

    ret.mRenderTree.FromJson(json["render_tree"], game::TreeNodeFromJson(ret.mNodes));
    return ret;
}
SceneClass SceneClass::Clone() const
{
    SceneClass ret;

    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<SceneNodeClass>(*node);
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }
    ret.mScriptVars = mScriptVars;

    ret.mRenderTree.FromTree(mRenderTree, [&map](const SceneNodeClass* node) {
        return map[node];
    });
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId    = std::move(tmp.mClassId);
    mNodes      = std::move(tmp.mNodes);
    mScriptVars = std::move(tmp.mScriptVars);
    mRenderTree = tmp.mRenderTree;
    return *this;
}

Scene::Scene(std::shared_ptr<const SceneClass> klass)
  : mClass(klass)
{
    std::unordered_map<const SceneNodeClass*, const Entity*> map;

    for (size_t i=0; i<klass->GetNumNodes(); ++i)
    {
        const auto& node = klass->GetNode(i);
        EntityArgs args;
        args.klass    = node.GetEntityClass();
        args.rotation = node.GetRotation();
        args.position = node.GetTranslation();
        args.scale    = node.GetScale();
        args.name     = node.GetName();
        args.id       = node.GetId();
        args.layer    = node.GetLayer();
        ASSERT(args.klass);
        auto entity   = CreateEntityInstance(args);
        entity->SetFlag(Entity::Flags::VisibleInGame, node.TestFlag(SceneNodeClass::Flags::VisibleInGame));
        map[&node] = entity.get();
        mEntities.push_back(std::move(entity));
    }
    mRenderTree.FromTree(mClass->GetRenderTree(), [&map](const SceneNodeClass* node) {
        return map[node];
    });

    // make copies of mutable script variables.
    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        auto var = klass->GetScriptVar(i);
        if (!var.IsReadOnly())
            mScriptVars.push_back(std::move(var));
    }
}

Scene::Scene(const SceneClass& klass) : Scene(std::make_shared<SceneClass>(klass))
{}

Entity& Scene::GetEntity(size_t index)
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
Entity* Scene::FindEntityByInstanceId(const std::string& id)
{
    for (auto& e : mEntities)
        if (e->GetId() == id)
            return e.get();
    return nullptr;
}
Entity* Scene::FindEntityByInstanceName(const std::string& name)
{
    for (auto& e : mEntities)
        if (e->GetName() == name)
            return e.get();
    return nullptr;
}

const Entity& Scene::GetEntity(size_t index) const
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
const Entity* Scene::FindEntityByInstanceId(const std::string& id) const
{
    for (const auto& e : mEntities)
        if (e->GetId() == id)
            return e.get();
    return nullptr;
}
const Entity* Scene::FindEntityByInstanceName(const std::string& name) const
{
    for (const auto& e : mEntities)
        if (e->GetName() == name)
            return e.get();
    return nullptr;
}

const ScriptVar* Scene::FindScriptVar(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVar(name);
}

void Scene::Update(float dt)
{
    mCurrentTime += dt;

    // todo: limit which entities are getting updated.
    for (auto& entity : mEntities)
    {
        entity->Update(dt);
        if (entity->IsPlaying())
            continue;
        if (entity->HasIdleTrack())
            entity->PlayIdle();
    }
}

std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass)
{
    return std::make_unique<Scene>(klass);
}

std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass)
{
    return std::make_unique<Scene>(klass);
}

} // namespace
