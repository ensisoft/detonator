// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <unordered_set>
#include <stack>

#include "base/format.h"
#include "base/logging.h"
#include "base/threadpool.h"
#include "base/trace.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/util.h"
#include "game/scene.h"
#include "game/entity.h"
#include "game/treeop.h"
#include "game/transform.h"
#include "game/entity_node_spatial_node.h"

namespace {
template<typename Result, typename Entity, typename Node>
class SceneEntityCollector : public game::RenderTree<Entity>::template TVisitor<Node> {
public:
    virtual void EnterNode(Node* node) override
    {
        if (!node)
            return;

        glm::mat4 parent_node_transform(1.0f);
        if (const auto* parent = GetParent())
        {
            const auto* parent_node = parent->FindNodeByClassId(node->GetParentNodeClassId());
            parent_node_transform   = parent->FindNodeTransform(parent_node);
        }
        mParents.push(node);
        mTransform.Push(std::move(parent_node_transform));
        Result ret;
        ret.node_to_scene = mTransform.GetAsMatrix();
        ret.entity = node;
        ret.placement = node;
        mResult.push_back(std::move(ret));
    }
    virtual void LeaveNode(Node* node) override
    {
        if (!node)
            return;
        mTransform.Pop();
        mParents.pop();
    }
    std::vector<Result> GetResult() &&
    { return std::move(mResult); }
private:
    Node* GetParent() const
    {
        if (mParents.empty())
            return nullptr;
        return mParents.top();
    }
private:
    std::stack<Node*> mParents;
    std::vector<Result> mResult;
    game::Transform mTransform;
};
} // namespace

namespace game
{




Scene::Scene(std::shared_ptr<const SceneClass> klass)
  : mClass(std::move(klass))
{
    std::unordered_map<const EntityPlacement*, const Entity*> entity_placement_map;

    bool spatial_nodes = false;

    // spawn an entity instance for each scene node class
    // in the scene class
    for (size_t i=0; i<mClass->GetNumNodes(); ++i)
    {
        const auto& placement = mClass->GetPlacement(i);
        const auto& entity_klass = placement.GetEntityClass();
        if (entity_klass == nullptr)
        {
            ERROR("Entity placement '%1' refers to an entity class that no longer exists.", placement.GetName());
            continue;
        }

        EntityArgs args;
        args.klass    = entity_klass;
        args.rotation = placement.GetRotation();
        args.position = placement.GetTranslation();
        args.scale    = placement.GetScale();
        args.name     = placement.GetName();
        args.id       = placement.GetId();

        auto entity   = CreateEntityInstance(args);

        for (size_t j=0; j<entity->GetNumNodes(); ++j)
        {
            const auto& node = entity->GetNode(j);
            if (node->HasSpatialNode())
                spatial_nodes = true;
        }

        // these need always be set for each entity spawned from scene
        // placement node.
        entity->SetParentNodeClassId(placement.GetParentRenderTreeNodeId());
        entity->SetRenderLayer(placement.GetRenderLayer());
        entity->SetMapLayer(placement.GetMapLayer());
        entity->SetScene(this);

        // optionally set instance settings, if these are not set then
        // entity class defaults apply.
        if (placement.HasIdleAnimationSetting())
            entity->SetIdleTrackId(placement.GetIdleAnimationId());
        if (placement.HasLifetimeSetting())
            entity->SetLifetime(placement.GetLifetime());
        if (placement.HasTag())
            entity->SetTag(*placement.GetTag());

        if (entity->HasIdleTrack())
            entity->PlayIdle();

        // check which flags the scene node has set and set those on the
        // entity instance. for any flag setting that is not set entity class
        // default will apply.
        for (const auto& flag : magic_enum::enum_values<Entity::Flags>())
        {
            if (placement.HasFlagSetting(flag))
                entity->SetFlag(flag, placement.TestFlag(flag));
        }

        // set the entity script variable values
        for (size_t i=0; i<placement.GetNumScriptVarValues(); ++i)
        {
            const auto& val = placement.GetScriptVarValue(i);
            auto* var = entity->FindScriptVarById(val.id);
            // deal with potentially stale data in the scene node.
            if (var == nullptr)
            {
                ERROR("Scene entity placement '%1' refers to entity script variable '%2' that no longer exists.", placement.GetName(), val.id);
                continue;
            }
            else if (ScriptVar::GetTypeFromVariant(val.value) != var->GetType())
            {
                ERROR("Scene entity placement '%1' refers to entity script variable '%2' with incorrect type.", placement.GetName(), val.id);
                continue;
            }
            else if (var->IsReadOnly())
            {
                ERROR("Scene entity placement '%1' tries to set a read only script variable '%1'.", placement.GetName(), var->GetName());
                continue;
            }
            std::visit([var](const auto& variant_value) {
                // eh what??
                const_cast<ScriptVar*>(var)->SetData(variant_value);
            }, val.value);
        }

        entity_placement_map[&placement] = entity.get();
        mIdMap[entity->GetId()] = entity.get();
        mNameMap[entity->GetName()] = entity.get();
        mEntities.push_back(std::move(entity));
    }
    mRenderTree.FromTree(mClass->GetRenderTree(), [&entity_placement_map](const EntityPlacement* placement) {
        return entity_placement_map[placement];
    });

    // make copies of mutable script variables.
    for (size_t i=0; i<mClass->GetNumScriptVars(); ++i)
    {
        auto var = mClass->GetScriptVar(i);
        if (!var.IsReadOnly())
            mScriptVars.push_back(std::move(var));
    }

    const auto index = mClass->GetDynamicSpatialIndex();
    if (index == SceneClass::SpatialIndex::QuadTree)
    {
        const auto* args = mClass->GetQuadTreeArgs();
        ASSERT(args);
        mSpatialIndex.reset(new QuadTreeIndex<EntityNode>(args->max_items, args->max_levels));
        DEBUG("Created scene spatial index. [type=%1, max_items=%2, max_levels=%3]",
              index, args->max_items, args->max_levels);
    }
    else if (index == SceneClass::SpatialIndex::DenseGrid)
    {
        const auto* args = mClass->GetDenseGridArgs();
        ASSERT(args);
        mSpatialIndex.reset(new DenseGridIndex<EntityNode>(args->num_rows, args->num_cols));
        DEBUG("Created scene spatial index. [type=%1, rows=%2, cols=%3]",
              index, args->num_rows, args->num_cols);
    }
    if (spatial_nodes && !mSpatialIndex)
    {
        WARN("Scene entities have spatial nodes but scene has no spatial index set.\n"
             "Spatial indexing and spatial queries will not work.\n"
             "You can enable spatial indexing in the scene editor.");
    }
}

Scene::Scene(const SceneClass& klass) : Scene(std::make_shared<SceneClass>(klass))
{}

Scene::~Scene() = default;

Entity& Scene::GetEntity(size_t index)
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
Entity* Scene::FindEntityByInstanceId(const std::string& id)
{
    auto it = mIdMap.find(id);
    if (it == mIdMap.end())
        return nullptr;
    return it->second;
}
Entity* Scene::FindEntityByInstanceName(const std::string& name)
{
    auto it = mNameMap.find(name);
    if (it == mNameMap.end())
        return nullptr;
    return it->second;
}
std::vector<Entity*> Scene::ListEntitiesByClassName(const std::string& name)
{
    std::vector<Entity*> ret;
    for (auto& entity : mEntities)
    {
        if (entity->GetClassName() == name)
            ret.push_back(entity.get());
    }
    return ret;
}

std::vector<const Entity*> Scene::ListEntitiesByClassName(const std::string& name) const
{
    std::vector<const Entity*> ret;
    for (auto& entity : mEntities)
    {
        if (entity->GetClassName() == name)
            ret.push_back(entity.get());
    }
    return ret;
}

std::vector<Entity*> Scene::ListEntitiesByTag(const std::string& tag)
{
    std::vector<Entity*> ret;
    for (auto& entity : mEntities)
    {
        const auto& tag_string = entity->GetTag();
        if (base::Contains(tag_string, tag))
            ret.push_back(entity.get());
    }
    return ret;
}
std::vector<const Entity*> Scene::ListEntitiesByTag(const std::string& tag) const
{
    std::vector<const Entity*> ret;
    for (auto& entity : mEntities)
    {
        const auto& tag_string = entity->GetTag();
        if (base::Contains(tag_string, tag))
            ret.push_back(entity.get());
    }
    return ret;
}

const Entity& Scene::GetEntity(size_t index) const
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
const Entity* Scene::FindEntityByInstanceId(const std::string& id) const
{
    auto it = mIdMap.find(id);
    if (it == mIdMap.end())
        return nullptr;
    return it->second;
}
const Entity* Scene::FindEntityByInstanceName(const std::string& name) const
{
    auto it = mNameMap.find(name);
    if (it == mNameMap.end())
        return nullptr;
    return it->second;
}

void Scene::KillEntity(Entity* entity)
{
    // if the entity still exists, but it has been killed
    // it means it'll be deleted at the end of this loop iteration
    // in EndLoop. Make sure not to add it again in the
    // kill set for next iteration even if the application tries
    // to kill it again.
    if (entity->HasBeenKilled())
        return;

    entity->Die();

    mKillSet.insert(entity);
}

Entity* Scene::SpawnEntity(const EntityArgs& args, bool link_to_root)
{
    TRACE_SCOPE("Scene::SpawnEntity");

    ASSERT(args.klass);

    auto* task_pool = args.async_spawn ? base::GetGlobalThreadPool() : nullptr;

    if (task_pool)
    {
        if (!mAsyncSpawnState)
            mAsyncSpawnState = std::make_shared<AsyncSpawnState>();

        class SpawnEntityTask : public base::ThreadTask {
        public:
            SpawnEntityTask(EntityArgs args, std::shared_ptr<AsyncSpawnState> state, double scene_time) noexcept
              : mSceneTime(scene_time)
              , mArgs(std::move(args))
              , mState(std::move(state))
            {}
        protected:
            virtual void DoTask() override
            {
                auto instance = CreateEntityInstance(mArgs);
                if (instance->HasIdleTrack())
                    instance->PlayIdle();

                if (mArgs.enable_logging)
                    DEBUG("New entity instance. [name='%1/%2']", mArgs.klass->GetName(), mArgs.name);

                std::lock_guard<std::mutex> lock(mState->mutex);
                SpawnRecord spawn;
                spawn.spawn_time = mSceneTime + mArgs.delay;
                spawn.instance   = std::move(instance);
                mState->spawn_list.push_back(std::move(spawn));
            }
        private:
            const double mSceneTime = 0.0f;
            const EntityArgs mArgs;
            std::shared_ptr<AsyncSpawnState> mState;
        };

        auto task = std::make_unique<SpawnEntityTask>(args, mAsyncSpawnState, mCurrentTime);
        task_pool->SubmitTask(std::move(task), base::ThreadPool::AnyWorkerThreadID);
        return nullptr;
    }

    // we must have the klass of the entity and an id.
    // the invariant that must hold is that entity IDs are
    // always going to be unique.
    auto instance = CreateEntityInstance(args);
    instance->SetScene(this);
    if (instance->HasIdleTrack())
    {
        instance->PlayIdle();
    }

    ASSERT(mIdMap.find(instance->GetId()) == mIdMap.end());

    SpawnRecord spawn;
    spawn.spawn_time = mCurrentTime + args.delay;
    spawn.instance   = std::move(instance);
    mSpawnList.push_back(std::move(spawn));
    if (args.enable_logging)
        DEBUG("New entity instance. [entity='%1/%2']", args.klass->GetName(), args.name);

    return mSpawnList.back().instance.get();
}

void Scene::BeginLoop()
{
    // turn on the kill flag for entities that were killed
    // during the last iteration of the game play.
    for (auto* entity : mKillSet)
    {
        // Set entity kill flag to indicate that it's been killed from the scene.
        // Note that this isn't the same as the c++ lifetime of the object!
        entity->SetFlag(Entity::ControlFlags::Killed, true);
        // propagate the kill flag to children when a parent
        // entity is killed. if this is not desired then one
        // should have unlinked the children first.
        mRenderTree.PreOrderTraverseForEach([](Entity* entity) {
            entity->SetFlag(Entity::ControlFlags::Killed, true);
            if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
                DEBUG("Entity was killed. [entity='%1/%2']", entity->GetClassName(), entity->GetName());
        }, entity);
    }

    if (mAsyncSpawnState)
    {
        std::lock_guard<std::mutex> lock(mAsyncSpawnState->mutex);
        base::AppendVector(mSpawnList, std::move(mAsyncSpawnState->spawn_list));
        mAsyncSpawnState->spawn_list.clear();
    }

    for (auto& spawn : mSpawnList)
    {
        if (mCurrentTime < spawn.spawn_time)
            continue;

        auto& entity = spawn.instance;

        if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
            DEBUG("Entity was spawned [entity=''%1/%2'].", entity->GetClassName(), entity->GetName());

        entity->SetFlag(Entity::ControlFlags::Spawned, true);
        entity->SetScene(this);

        ASSERT(!base::Contains(mIdMap, entity->GetId()));

        mIdMap[entity->GetId()]     = entity.get();
        mNameMap[entity->GetName()] = entity.get();
        mRenderTree.LinkChild(nullptr, entity.get());
        mEntities.push_back(std::move(entity));
    }

    base::EraseRemove(mSpawnList, [](auto& record) {
        return !record.instance;
    });

    mKillSet.clear();
}

void Scene::EndLoop()
{
    std::set<EntityNode*> killed_spatial_nodes;

    for (auto& entity : mEntities)
    {
        // turn off spawn flags.
        entity->SetFlag(Entity::ControlFlags::Spawned, false);
        // if the entity needs to be killed (i.e. Entity::Die was called instead
        // of Scene::KillEntity then we put the entity in the killset now
        // for killing it on next loop iteration
        if (entity->TestFlag(Entity::ControlFlags::WantsToDie) &&
            !entity->TestFlag(Entity::ControlFlags::Killed))
            mKillSet.insert(entity.get());

        if (!entity->TestFlag(Entity::ControlFlags::Killed))
            continue;

        if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
            DEBUG("Entity '%1/%2' was deleted.", entity->GetClassName(), entity->GetName());
        mRenderTree.DeleteNode(entity.get());
        mIdMap.erase(entity->GetId());
        mNameMap.erase(entity->GetName());

        if (mSpatialIndex)
        {
            for (size_t i = 0; i < entity->GetNumNodes(); ++i)
            {
                auto& node = entity->GetNode(i);
                if (node.HasSpatialNode())
                    killed_spatial_nodes.insert(&node);
            }
        }
    }
    if (mSpatialIndex)
        mSpatialIndex->Erase(killed_spatial_nodes);

    if (auto* task_pool = base::GetGlobalThreadPool())
    {
        // delete de-allocation to the task pool since allocation
        // is also there which means that the there might be a lock
        // on the entity node allocator which means the deletion
        // is blocked until the allocator is unlocked.
        std::vector<std::unique_ptr<Entity>> carcasses;

        auto it = std::remove_if(mEntities.begin(), mEntities.end(), [](const auto& entity) {
            return entity->TestFlag(Entity::ControlFlags::Killed);
        });
        std::move(it, mEntities.end(), std::back_inserter(carcasses));
        mEntities.erase(it, mEntities.end());

        class DeleteEntitiesTask : public base::ThreadTask {
        public:
            DeleteEntitiesTask(std::vector<std::unique_ptr<Entity>>&& carcasses)
              : mCarcasses(std::move(carcasses))
            {}
        protected:
            virtual void DoTask() override
            {
                // this is simple !
                mCarcasses.clear();
            }
        private:
            std::vector<std::unique_ptr<Entity>> mCarcasses;
        };
        auto task = std::make_unique<DeleteEntitiesTask>(std::move(carcasses));
        task_pool->SubmitTask(std::move(task), base::ThreadPool::AnyWorkerThreadID);
    }
    else
    {
        // delete the entities that were killed from the container
        mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [](const auto& entity) {
            return entity->TestFlag(Entity::ControlFlags::Killed);
        }), mEntities.end());
    }
}

std::vector<Scene::ConstSceneNode> Scene::CollectNodes() const
{
    std::vector<ConstSceneNode> ret;
    for (auto& entity : mEntities)
    {
        ConstSceneNode node;
        node.node_to_scene = FindEntityTransform(entity.get());
        node.entity = entity.get();
        ret.push_back(std::move(node));
    }
    return ret;
}

std::vector<Scene::SceneNode> Scene::CollectNodes()
{
    std::vector<SceneNode> ret;
    for (auto& entity : mEntities)
    {
        SceneNode node;
        node.node_to_scene = FindEntityTransform(entity.get());
        node.entity = entity.get();
        ret.push_back(std::move(node));
    }
    return ret;
}

glm::mat4 Scene::FindEntityTransform(const Entity* entity) const
{
    // if the parent of this entity is the root node
    // then the matrix will be simply identity
    if (!mRenderTree.HasNode(entity) || !mRenderTree.GetParent(entity))
        return glm::mat4(1.0f);

    // search the render tree until we find the entity.
    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const Entity* entity) : mEntity(entity), mMatrix(1.0f)
        {}
        virtual void EnterNode(const Entity* entity) override
        {
            if (!entity)
                return;
            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(entity->GetParentNodeClassId());
                parent_node_transform   = parent->FindNodeTransform(parent_node);
            }
            mParents.push(entity);
            mTransform.Push(parent_node_transform);
            if (entity == mEntity)
            {
                mMatrix = mTransform.GetAsMatrix();
                mDone = true;
            }
        }
        virtual void LeaveNode(const Entity* entity) override
        {
            if (!entity)
                return;
            mTransform.Pop();
            mParents.pop();
        }
        virtual bool IsDone() const override
        { return mDone; }

        glm::mat4 GetMatrix() const
        { return mMatrix; }
    private:
        const Entity* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        const Entity* mEntity = nullptr;
        std::stack<const Entity*> mParents;
        glm::mat4 mMatrix;
        Transform mTransform;
        bool mDone = false;
    };
    Visitor visitor(entity);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetMatrix();
}

glm::mat4 Scene::FindEntityNodeTransform(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityTransform(entity));
    transform.Push(entity->FindNodeTransform(node));
    return transform.GetAsMatrix();
}

FRect Scene::FindEntityBoundingRect(const Entity* entity) const
{
    FRect ret;
    Transform transform(FindEntityTransform(entity));
    for (size_t i=0; i<entity->GetNumNodes(); ++i)
    {
        const auto& node = entity->GetNode(i);
        transform.Push(entity->FindNodeTransform(&node));
          transform.Push(node->GetModelTransform());
          ret = Union(ret, ComputeBoundingRect(transform.GetAsMatrix()));
          transform.Pop();
        transform.Pop();
    }
    return ret;
}
FRect Scene::FindEntityNodeBoundingRect(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityNodeTransform(entity, node));
    transform.Push(node->GetModelTransform());
    return ComputeBoundingRect(transform.GetAsMatrix());
}

FBox Scene::FindEntityNodeBoundingBox(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityNodeTransform(entity, node));
    transform.Push(node->GetModelTransform());
    return FBox(transform.GetAsMatrix());
}

glm::vec2 Scene::MapVectorFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const
{
    const auto& from_entity_to_world = FindEntityNodeTransform(entity, node);
    return TransformVector(from_entity_to_world, vector);
}
glm::vec3 Scene::MapVectorFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec3& vector) const
{
    const auto& from_entity_to_world = FindEntityNodeTransform(entity, node);
    return TransformVector(from_entity_to_world, vector);
}

glm::vec2 Scene::MapPointFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& point) const
{
    const auto& from_entity_to_world = FindEntityNodeTransform(entity, node);
    return TransformPoint(from_entity_to_world, point);
}

glm::vec2 Scene::MapVectorToEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const
{
    const auto& from_entity_to_world = FindEntityNodeTransform(entity, node);
    const auto& from_world_to_entity = glm::inverse(from_entity_to_world);
    return TransformVector(from_world_to_entity, vector);
}

glm::vec2 Scene::MapPointToEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& point) const
{
    const auto& from_entity_to_world = FindEntityNodeTransform(entity, node);
    const auto& from_world_to_entity = glm::inverse(from_entity_to_world);
    return TransformPoint(from_world_to_entity, point);
}

const ScriptVar* Scene::FindScriptVarByName(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVarByName(name);
}
const ScriptVar* Scene::FindScriptVarById(const std::string& id) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetId() == id)
            return &var;
    }
    return mClass->FindScriptVarById(id);
}

void Scene::Update(float dt, std::vector<Event>* events)
{
    mCurrentTime += dt;

    // todo: limit which entities are getting updated.
    for (auto& entity : mEntities)
    {
        std::vector<Entity::Event> entity_events;
        entity->Update(dt, events ? &entity_events : nullptr);
        for (auto& entity_event : entity_events)
        {
            if (auto* ptr = std::get_if<Entity::TimerEvent>(&entity_event))
            {
                EntityTimerEvent timer;
                timer.entity = entity.get();
                timer.event  = std::move(*ptr);
                events->push_back(std::move(timer));
            }
            else if (auto* ptr = std::get_if<Entity::PostedEvent>(&entity_event))
            {
                EntityEventPostedEvent posted_event;
                posted_event.entity  = entity.get();
                posted_event.event   = std::move(*ptr);
                events->push_back(std::move(posted_event));
            }
        }

        if (entity->HasExpired())
        {
            if (entity->TestFlag(Entity::Flags::KillAtLifetime))
                entity->SetFlag(Entity::ControlFlags::Killed , true);
            continue;
        }
        if (entity->IsAnimating())
            continue;

        if (!entity->HasIdleTrack())
            continue;

        const auto& finished_animations = entity->GetFinishedAnimations();
        bool play_idle = true;
        for (const auto* anim : finished_animations)
        {
            const auto& idle_track_id = entity->GetIdleTrackId();
            const auto& prev_track_id = anim->GetClassId();
            if (idle_track_id == prev_track_id)
            {
                play_idle = false;
                break;
            }
        }
        if (play_idle)
            entity->PlayIdle();
    }
}

void Scene::Rebuild()
{
    const auto* left_boundary  = mClass->GetLeftBoundary();
    const auto* right_boundary = mClass->GetRightBoundary();
    const auto* top_boundary   = mClass->GetTopBoundary();
    const auto* bottom_boundary = mClass->GetBottomBoundary();

    const bool has_boundary_condition = left_boundary ||
                                        right_boundary ||
                                        top_boundary ||
                                        bottom_boundary;
    // If there's no spatial index and no boundary condition of any kind
    // we can skip all the work here because there's no dynamic spatial
    // index to update and nothing needs to be checked against any boundary
    // condition.
    if (!mSpatialIndex && !has_boundary_condition)
        return;

    // Iterate over the render tree and look for entity nodes that have
    // SpatialNode attachment. For the nodes with spatial node compute
    // the node's AABB and place that node into the spatial index.
    class Visitor final : public RenderTree::Visitor {
    public:
        Visitor(SpatialIndex* index, Scene* scene, float left, float right, float top, float bottom)
            : mLeftBound(left)
            , mRightBound(right)
            , mTopBound(top)
            , mBottomBound(bottom)
            , mLeft(std::numeric_limits<double>::max())
            , mRight(std::numeric_limits<double>::lowest())
            , mTop(std::numeric_limits<double>::max())
            , mBottom(std::numeric_limits<double>::lowest())
            , mIndex(index)
            , mScene(scene)
        {}

        virtual void EnterNode(Entity* entity) override
        {
            if (!entity)
                return;

            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(entity->GetParentNodeClassId());
                mTransform.Push(parent->FindNodeTransform(parent_node));
            }

            mParents.push(entity);

            // if the entity has no spatial nodes and is not expected
            // to be killed at the scene boundary then the rest of the
            // work can be skipped.
            if (!entity->HasSpatialNodes() &&
                (!entity->KillAtBoundary() || entity->HasBeenKilled()))
                return;

            FRect rect;
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);
                mTransform.Push(entity->FindNodeModelTransform(&node));
                const auto& aabb = ComputeBoundingRect(mTransform.GetAsMatrix());
                if (const auto* spatial = node.GetSpatialNode(); spatial && spatial->IsEnabled() && mIndex)
                {
                    const auto left   = (double)aabb.GetX();
                    const auto right  = (double)aabb.GetX() + aabb.GetWidth();
                    const auto top    = (double)aabb.GetY();
                    const auto bottom = (double)aabb.GetY() + aabb.GetHeight();
                    mLeft   = std::min(mLeft, left);
                    mRight  = std::max(mRight, right);
                    mTop    = std::min(mTop, top);
                    mBottom = std::max(mBottom, bottom);

                    if (spatial->GetShape() == SpatialNode::Shape::AABB)
                    {
                        mItems.push_back({const_cast<EntityNode*>(&node), aabb});
                    } else BUG("Unimplemented spatial shape insertion.");
                }
                rect = base::Union(rect, aabb);
                mTransform.Pop();
            }
            // if the entity has already been killed there's no point
            // to test whether it should be killed if it has gone
            // beyond the boundaries.
            if (entity->HasBeenKilled())
                return;
            // if the entity doesn't enable boundary killing skip boundary testing.
            if (!entity->KillAtBoundary())
                return;

            // check against the scene's boundary values.
            const auto left   = (double)rect.GetX();
            const auto right  = (double)rect.GetX() + rect.GetWidth();
            const auto top    = (double)rect.GetY();
            const auto bottom = (double)rect.GetY() + rect.GetHeight();
            if ((left > mRightBound) || (right < mLeftBound) ||
                (top > mBottomBound) || (bottom < mTopBound))
            {
                mScene->mKillSet.insert(entity);
            }
        }
        virtual void LeaveNode(Entity* entity) override
        {
            if (!entity)
                return;
            mParents.pop();
            if (const auto* parent = GetParent())
                mTransform.Pop();
        }
        void Update() const
        {
            if (mIndex)
            {
                // because of numerical stability with floats (precision loss)
                // we're going to artificially enlarge the spatial rectangle little
                // bit to make sure that all the spatial node rects will be enclosed
                // inside the main rect.
                // Todo: using a hard coded value here, the scale of the adjustment
                // should depend on scale of the floats min/max themselves.
                const auto xpos   = mLeft - 1.0;
                const auto ypos   = mTop -1.0;
                const auto width  = mRight - mLeft + 2.0f;
                const auto height = mBottom - mTop + 2.0f;
                mIndex->Insert(FRect(xpos, ypos, width, height), mItems);
            }
        }
    private:
        Entity* GetParent() const noexcept
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        using SpatialIndexItem = game::SpatialIndex<EntityNode>::Item;

        const double mLeftBound;
        const double mRightBound;
        const double mTopBound;
        const double mBottomBound;
        double mLeft;
        double mRight;
        double mTop;
        double mBottom;

        std::vector<SpatialIndexItem> mItems;
        std::stack<Entity*> mParents;
        Transform mTransform;
        SpatialIndex* mIndex = nullptr;
        Scene* mScene = nullptr;
    };

    const auto left_boundary_value   = left_boundary   ? *left_boundary   : std::numeric_limits<float>::lowest();
    const auto right_boundary_value  = right_boundary  ? *right_boundary  : std::numeric_limits<float>::max();
    const auto top_boundary_value    = top_boundary    ? *top_boundary    : std::numeric_limits<float>::lowest();
    const auto bottom_boundary_value = bottom_boundary ? *bottom_boundary : std::numeric_limits<float>::max();

    Visitor visitor(mSpatialIndex.get(), this,
        left_boundary_value, right_boundary_value,
        top_boundary_value, bottom_boundary_value);
    mRenderTree.PreOrderTraverse(visitor);

    visitor.Update();
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
