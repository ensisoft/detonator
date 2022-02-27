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

#include <algorithm>

#include "base/logging.h"
#include "base/math.h"
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/material.h"
#include "engine/physics.h"
#include "engine/loader.h"
#include "game/types.h"
#include "game/scene.h"
#include "game/transform.h"

using namespace game;

// ADL lookup for math::FindConvexHull in the global
// namespace for box2d
inline b2Vec2 GetPosition(const b2Vec2& vec2)
{ return vec2; }

inline b2Vec2 ToBox2D(const glm::vec2& vector)
{ return b2Vec2 {vector.x, vector.y}; }

inline glm::vec2 ToGlm(const b2Vec2& vector)
{ return glm::vec2 { vector.x, vector.y }; }

namespace engine
{

PhysicsEngine::PhysicsEngine(const ClassLibrary* loader)
  : mClassLib(loader)
{}

float PhysicsEngine::MapLengthFromGame(float length) const
{
    const auto& dir = glm::normalize(glm::vec2(1.0f, 1.0f));
    return glm::length(MapVectorFromGame(dir * length));
}

float PhysicsEngine::MapLengthToGame(float meters) const
{
    const auto dir = glm::normalize(glm::vec2(1.0f, 1.0f));
    return glm::length(MapVectorToGame(dir * meters));
}

void PhysicsEngine::UpdateWorld(const game::Scene& scene)
{
    // apply any pending state changes from *previous* game loop
    // iteration to the current state. this includes things such as
    // - velocity adjustments
    // - body flag state changes
    // - static body position changes
    // - culling killed entities
    // - creating new physics nodes for spawned entities

    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);

    const auto& nodes = scene.CollectNodes();
    for (const auto& node : nodes)
    {
        const auto* entity = node.entity_object;
        if (!entity->HasRigidBodies())
            continue;
        if (entity->HasBeenKilled())
        {
            KillEntity(*entity);
        }
        else if (entity->HasBeenSpawned())
        {
            transform.Push(node.node_to_scene);
                AddEntity(transform.GetAsMatrix(), *entity);
            transform.Pop();
        }
        else
        {
            transform.Push(node.node_to_scene);
                UpdateWorld(transform.GetAsMatrix(), *entity);
            transform.Pop();
        }
    }
}

void PhysicsEngine::UpdateWorld(const game::Entity& entity)
{
    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);
    UpdateWorld(transform.GetAsMatrix(), entity);
}

void PhysicsEngine::UpdateScene(game::Scene& scene)
{
    // two options here for updating entity nodes based on the
    // physics simulation.
    // 1. traverse the whole scene and look which entity nodes
    //    also exist in the physics world.
    //    a) use the render tree to traverse the scene.
    //    b) use the entity list to iterate over the scene and then
    //       see which entity's nodes have physics nodes.
    // 2. iterate over the physics nodes and find them in the scene
    //    and then update.
    // Currently, i'm not sure which strategy is more efficient. It'd seem
    // that if a lot of the entity nodes in the scene have physics bodies
    // then traversing the whole scene is a viable alternative. However, if
    // only a few nodes have physics bodies then likely only iterating over
    // the physics bodies and then looking up their transforms in the scene
    // is more efficient.
    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);

    const auto& nodes = scene.CollectNodes();
    for (const auto& node : nodes)
    {
        auto* entity = node.entity_object;
        if (entity->HasBeenKilled() || !entity->HasRigidBodies())
            continue;
        transform.Push(node.node_to_scene);
          UpdateEntity(transform.GetAsMatrix(), *entity);
        transform.Pop();
    }
}

void PhysicsEngine::UpdateEntity(Entity& entity)
{
    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);
    UpdateEntity(transform.GetAsMatrix(), entity);
}

void PhysicsEngine::Step(std::vector<ContactEvent>* contacts)
{
    class ContactListener : public b2ContactListener
    {
    public:
        ContactListener(PhysicsEngine& engine, std::vector<ContactEvent>* contacts)
            : mEngine(engine)
            , mContacts(contacts)
        {}
        // This is called when two fixtures begin to overlap.
        // This is called for sensors and non-sensors.
        // This event can only occur inside the time step.
        virtual void BeginContact(b2Contact* contact) override
        {
            //DEBUG("BeginContact");
            auto* A = contact->GetFixtureA();
            auto* B = contact->GetFixtureB();
            const auto* IdA = base::SafeFind(mEngine.mFixtures, A);
            const auto* IdB = base::SafeFind(mEngine.mFixtures, B);
            ASSERT(IdA && IdB);
            ContactEvent event;
            event.type = ContactEvent::Type::BeginContact;
            event.nodeA = mEngine.mNodes[*IdA].node;
            event.nodeB = mEngine.mNodes[*IdB].node;
            event.entityA = mEngine.mNodes[*IdA].entity;
            event.entityB = mEngine.mNodes[*IdB].entity;
            mContacts->push_back(std::move(event));
        }

        // This is called when two fixtures cease to overlap.
        // This is called for sensors and non-sensors.
        // This may be called when a body is destroyed,
        // so this event can occur outside the time step.
        virtual void EndContact(b2Contact* contact) override
        {
            //DEBUG("EndContact");
            auto* A = contact->GetFixtureA();
            auto* B = contact->GetFixtureB();
            const auto* IdA = base::SafeFind(mEngine.mFixtures, A);
            const auto* IdB = base::SafeFind(mEngine.mFixtures, B);
            ASSERT(IdA && IdB);
            ContactEvent event;
            event.type = ContactEvent::Type::EndContact;
            event.nodeA = mEngine.mNodes[*IdA].node;
            event.nodeB = mEngine.mNodes[*IdB].node;
            event.entityA = mEngine.mNodes[*IdA].entity;
            event.entityB = mEngine.mNodes[*IdB].entity;
            mContacts->push_back(std::move(event));
        }

        // This is called after collision detection, but before collision resolution.
        // This gives you a chance to disable the contact based on the current configuration.
        // For example, you can implement a one-sided platform using this callback and
        // calling b2Contact::SetEnabled(false). The contact will be re-enabled each
        // time through collision processing, so you will need to disable the contact
        // every time-step. The pre-solve event may be fired multiple times per time step
        // per contact due to continuous collision detection.
        virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override
        { }

        // The post solve event is where you can gather collision impulse results.
        // If you don't care about the impulses, you should probably just implement the pre-solve event.
        virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override
        { }
    private:
        PhysicsEngine& mEngine;
        std::vector<ContactEvent>* mContacts = nullptr;
    };
    ContactListener listener(*this, contacts);

    mWorld->SetContactListener(contacts ? &listener : (b2ContactListener*)nullptr);
    mWorld->Step(mTimestep, mNumVelocityIterations, mNumPositionIterations);
    mWorld->SetContactListener(nullptr);
}

void PhysicsEngine::DeleteAll()
{
    for (auto& node : mNodes)
    {
        mWorld->DestroyBody(node.second.world_body);
    }
    mNodes.clear();
    mFixtures.clear();
}

void PhysicsEngine::DeleteBody(const std::string& id)
{
    auto it = mNodes.find(id);
    if (it == mNodes.end())
        return;
    auto& node = it->second;

    b2Fixture* fixture = node.world_body->GetFixtureList();
    while (fixture)
    {
        mFixtures.erase(fixture);
        fixture = fixture->GetNext();
    }
    mWorld->DestroyBody(node.world_body);

    mNodes.erase(it);
}
void PhysicsEngine::DeleteBody(const EntityNode& node)
{
    DeleteBody(node->GetId());
}

bool PhysicsEngine::ApplyImpulseToCenter(const std::string& id, const glm::vec2& impulse)
{
    if (auto* ptr = FindPhysicsNode(id))
    {
        auto* body = ptr->world_body;
        if (body->GetType() != b2_dynamicBody)
        {
            WARN("Applying impulse to non-dynamic body has no effect. [node='%1']", id);
            return false;
        }
        body->ApplyLinearImpulseToCenter(b2Vec2(impulse.x, impulse.y), true /*wake */);
        return true;
    }
    WARN("No such physics node. [node='%1']", id);
    return false;
}

bool PhysicsEngine::ApplyImpulseToCenter(const EntityNode& node, const glm::vec2& impulse)
{
    return ApplyImpulseToCenter(node.GetId(), impulse);
}

bool PhysicsEngine::ApplyForceToCenter(const game::EntityNode& node, const glm::vec2& force)
{
    return ApplyForceToCenter(node.GetId(), force);
}
bool PhysicsEngine::ApplyForceToCenter(const std::string& node, const glm::vec2& force)
{
    if (auto* ptr = FindPhysicsNode(node))
    {
        auto* body = ptr->world_body;
        if (body->GetType() != b2_dynamicBody)
        {
            WARN("Applying force to non-dynamic body has no effect. [node='%1']", node);
            return false;
        }
        body->ApplyForceToCenter(ToBox2D(force), true /* wake*/);
        return true;
    }
    WARN("No such physics node. [node='%1']", node);
    return false;
}

bool PhysicsEngine::SetLinearVelocity(const EntityNode& node, const glm::vec2& velocity)
{
    return SetLinearVelocity(node.GetId(), velocity);
}
bool PhysicsEngine::SetLinearVelocity(const std::string& id, const glm::vec2& velocity)
{
    if (auto* ptr = FindPhysicsNode(id))
    {
        auto* body = ptr->world_body;
        if (body->GetType() == b2_staticBody)
        {
            WARN("Setting linear velocity on a static body has no effect. [node='%1']", id);
            return false;
        }
        body->SetLinearVelocity(b2Vec2(velocity.x, velocity.y));
        return true;
    }
    WARN("No such physics node. [node='%1']", id);
    return false;
}

void PhysicsEngine::CreateWorld(const Scene& scene)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mNodes.clear();
    mWorld.reset();
    mWorld = std::make_unique<b2World>(gravity);

    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);

    const auto& nodes = scene.CollectNodes();
    for (const auto& node : nodes)
    {
        const auto* entity = node.entity_object;
        if (!entity->HasRigidBodies())
            continue;

        transform.Push(node.node_to_scene);
          AddEntity(transform.GetAsMatrix(), *node.entity_object);
        transform.Pop();
    }
}

void PhysicsEngine::CreateWorld(const Entity& entity)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mNodes.clear();
    mWorld.reset();
    mWorld = std::make_unique<b2World>(gravity);

    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);
    AddEntity(transform.GetAsMatrix(), entity);
}

void PhysicsEngine::DestroyWorld()
{
    DeleteAll();
    mWorld.reset();
}

std::tuple<bool, glm::vec2> PhysicsEngine::FindCurrentLinearVelocity(const game::EntityNode& node) const
{
    return FindCurrentLinearVelocity(node.GetId());
}
std::tuple<bool, glm::vec2> PhysicsEngine::FindCurrentLinearVelocity(const std::string& node) const
{
    if (const auto* ptr = FindPhysicsNode(node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, ToGlm(body->GetLinearVelocity()));
    }
    WARN("No such physics node. [node='%1']", node);
    return std::make_tuple(false, glm::vec2(0.0f, 0.0f));
}
std::tuple<bool, float> PhysicsEngine::FindCurrentAngularVelocity(const game::EntityNode& node) const
{
    return FindCurrentAngularVelocity(node.GetId());
}
std::tuple<bool, float> PhysicsEngine::FindCurrentAngularVelocity(const std::string& node) const
{
    if (const auto* ptr = FindPhysicsNode(node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetAngularVelocity());
    }
    WARN("No such physics node. [node='%1']", node);
    return std::make_tuple(false, 0.0f);
}
std::tuple<bool, float> PhysicsEngine::FindMass(const game::EntityNode& node) const
{
    return FindMass(node.GetId());
}
std::tuple<bool, float> PhysicsEngine::FindMass(const std::string& node) const
{
    if (const auto* ptr = FindPhysicsNode(node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetMass());
    }
    WARN("No such physics node. [node='%1']", node);
    return std::make_tuple(false, 0.0f);
}

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view)
{
    static auto klass = gfx::CreateMaterialClassFromColor(gfx::Color::HotPink);
    klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    klass.SetBaseAlpha(0.6);
    static gfx::MaterialClassInst mat(klass);

    view.Push();
    view.Scale(mScale);

    std::unordered_set<b2Joint*> joints;

    // there's b2Draw api for debug drawing but it seems that
    // when wanting to debug the *game* (not the physics engine
    // integration issues itself) this is actually more straightforward
    // than using the b2Draw way of visualizing the physics world.
    for (const auto& p : mNodes)
    {
        const auto& physics_node = p.second;
        const float angle = physics_node.world_body->GetAngle();
        const b2Vec2& pos = physics_node.world_body->GetPosition();

        view.Push();
        view.Rotate(angle);
        view.Translate(pos.x, pos.y);

        view.Push();
        view.Scale(physics_node.world_extents);
        view.Translate(physics_node.world_extents * -0.5f);

        if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Box)
            painter.Draw(gfx::Rectangle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Circle)
            painter.Draw(gfx::Circle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::SemiCircle)
            painter.Draw(gfx::SemiCircle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::RightTriangle)
            painter.Draw(gfx::RightTriangle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::IsoscelesTriangle)
            painter.Draw(gfx::IsoscelesTriangle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Trapezoid)
            painter.Draw(gfx::Trapezoid(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Parallelogram)
            painter.Draw(gfx::Parallelogram(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Polygon) {
            const auto& klass = mClassLib->FindDrawableClassById(physics_node.polygonId);
            if (klass == nullptr) {
                WARN("No polygon class found for node '%1'", physics_node.debug_name);
            } else {
                auto poly = gfx::CreateDrawableInstance(klass);
                painter.Draw(*poly, view, mat);
            }
        } else BUG("!unhandled collision shape for debug drawing.");

        b2JointEdge* joint_list = physics_node.world_body->GetJointList();
        while (joint_list)
        {
            b2Joint* joint = joint_list->joint;
            if (base::Contains(joints, joint))
            {
                joint_list = joint_list->next;
                continue;
            }

            if (joint->GetType() == b2JointType::e_distanceJoint)
            {
                b2DistanceJoint* dj = static_cast<b2DistanceJoint*>(joint);
                const auto& src_world_anchor = MapVectorToGame(ToGlm(dj->GetAnchorA()));
                const auto& dst_world_anchor = MapVectorToGame(ToGlm(dj->GetAnchorB()));
                gfx::DrawLine(painter,
                    gfx::FPoint(src_world_anchor.x, src_world_anchor.y),
                    gfx::FPoint(dst_world_anchor.x, dst_world_anchor.y), gfx::Color::HotPink, 2.0f);
            }
            joints.insert(joint);
            joint_list = joint_list->next;
        }

        view.Pop();
        view.Pop();
    }
    view.Pop();
}
#endif // GAMESTUDIO_ENABLE_PHYSICS_DEBUG

void PhysicsEngine::UpdateWorld(const glm::mat4& entity_to_world, const game::Entity& entity)
{
    using RenderTree = Entity::RenderTree;
    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const glm::mat4& mat, const Entity& entity, PhysicsEngine& engine)
          : mEntity(entity)
          , mEngine(engine)
          , mTransform(mat)
        {}
        virtual void EnterNode(const EntityNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());
            if (!node->HasRigidBody())
                return;

            auto* phys_node  = base::SafeFind(mEngine.mNodes, node->GetId());
            auto* rigid_body = node->GetRigidBody();
            auto* world_body = phys_node->world_body;

            if (phys_node->flags != rigid_body->GetFlags().value())
            {
                world_body->SetEnabled(rigid_body->TestFlag(RigidBodyItem::Flags::Enabled));
                world_body->SetBullet(rigid_body->TestFlag(RigidBodyItem::Flags::Bullet));
                world_body->SetFixedRotation(rigid_body->TestFlag(RigidBodyItem::Flags::DiscardRotation));
                world_body->SetSleepingAllowed(rigid_body->TestFlag(RigidBodyItem::Flags::CanSleep));
                b2Fixture* fixture_list_head = world_body->GetFixtureList();
                while (fixture_list_head) {
                    fixture_list_head->SetSensor(rigid_body->TestFlag(RigidBodyItem::Flags::Sensor));
                    fixture_list_head = fixture_list_head->GetNext();
                }
            }
            phys_node->flags = rigid_body->GetFlags().value();

            if (world_body->GetType() == b2BodyType::b2_staticBody)
            {
                // static bodies are not moved by the physics engine.
                // they may be moved by the user.
                // update the world transform from the scene to the physics world.
                mTransform.Push(node->GetModelTransform());
                    const FBox box(mTransform.GetAsMatrix());
                    const auto& node_pos_in_world   = box.GetCenter();
                    world_body->SetTransform(b2Vec2(node_pos_in_world.x, node_pos_in_world.y), box.GetRotation());
                mTransform.Pop();

                if (rigid_body->HasAngularVelocityAdjustment())
                    WARN("Angular velocity adjustment on static body will not work. [node='%1']", phys_node->debug_name);
                if (rigid_body->HasLinearVelocityAdjustment())
                    WARN("Linear velocity adjustment on static body will not work. [node='%1']", phys_node->debug_name);
            }
            else
            {
                // apply any adjustment done by the animation/game to the physics body.
                if (rigid_body->HasAngularVelocityAdjustment())
                    world_body->SetAngularVelocity(rigid_body->GetAngularVelocityAdjustment());
                if (rigid_body->HasLinearVelocityAdjustment())
                    world_body->SetLinearVelocity(ToBox2D(rigid_body->GetLinearVelocityAdjustment()));
                rigid_body->ClearVelocityAdjustments();
            }
        }
        virtual void LeaveNode(const EntityNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const Entity& mEntity;
        PhysicsEngine& mEngine;
        Transform mTransform;
    };
    Visitor visitor(entity_to_world, entity, *this);

    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

void PhysicsEngine::UpdateEntity(const glm::mat4& model_to_world, Entity& entity)
{
    using RenderTree = Entity::RenderTree;
    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(const glm::mat4& model_to_world, const Entity& entity, PhysicsEngine& engine)
          : mEntity(entity)
          , mEngine(engine)
          , mTransform(model_to_world)
        {}

        virtual void EnterNode(EntityNode* node) override
        {
            if (!node)
                return;

            const auto node_to_world = mTransform.GetAsMatrix();

            mTransform.Push(node->GetNodeTransform());
            if (!node->HasRigidBody())
                return;

            auto* phys_node  = base::SafeFind(mEngine.mNodes, node->GetId());
            // could have been killed.
            if (phys_node == nullptr)
                return;
            auto* rigid_body = node->GetRigidBody();
            auto* world_body = phys_node->world_body;
            if (world_body->GetType() == b2BodyType::b2_staticBody)
                return;

            // get the object's transform properties in the physics world.
            const auto physics_world_pos   = world_body->GetPosition();
            const auto physics_world_angle = world_body->GetAngle();

            // transform back into scene relative to the node's parent.
            // i.e. the world transform of the node is expressed as a transform
            // relative to its parent node.
            glm::mat4 mat;
            Transform transform;
            transform.Rotate(physics_world_angle);
            transform.Translate(physics_world_pos.x, physics_world_pos.y);
            transform.Push();
                transform.Scale(phys_node->world_extents);
                transform.Translate(phys_node->world_extents * -0.5f);
                mat = transform.GetAsMatrix();
            transform.Pop();

            FBox box(mat);
            box.Transform(glm::inverse(node_to_world));
            node->SetTranslation(box.GetCenter());
            node->SetRotation(box.GetRotation());

            const auto linear_velocity  = world_body->GetLinearVelocity();
            const auto angular_velocity = world_body->GetAngularVelocity();
            // update current instantaneous velocities for other subsystems/game to read
            // the velocities are in world space. i.e. not relative to the node parent
            // (except when the parent is the scene root)
            rigid_body->SetLinearVelocity(glm::vec2(linear_velocity.x, linear_velocity.y));
            rigid_body->SetAngularVelocity(angular_velocity);
        }
        virtual void LeaveNode(EntityNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const Entity& mEntity;
        PhysicsEngine& mEngine;
        Transform mTransform;
    };

    Visitor visitor(model_to_world,  entity,*this);

    auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

void PhysicsEngine::KillEntity(const game::Entity& entity)
{
    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& entity_node = entity.GetNode(i);
        auto it = mNodes.find(entity_node.GetId());
        if (it == mNodes.end())
            continue;
        auto& physics_node = it->second;

        DEBUG("Deleting physics body. [node='%1']", physics_node.debug_name);
        b2Fixture* fixture_list_head = physics_node.world_body->GetFixtureList();
        while (fixture_list_head) {
            mFixtures.erase(fixture_list_head);
            fixture_list_head = fixture_list_head->GetNext();
        }
        mWorld->DestroyBody(physics_node.world_body);
        mNodes.erase(it);
    }
}

void PhysicsEngine::AddEntity(const glm::mat4& entity_to_world, const Entity& entity)
{
    using RenderTree = Entity::RenderTree;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const glm::mat4& mat, const Entity& entity, PhysicsEngine& engine)
          : mEntity(entity)
          , mEngine(engine)
          , mTransform(mat)
        {}

        virtual void EnterNode(const EntityNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());

            if (!node->HasRigidBody())
                return;

            mTransform.Push(node->GetModelTransform());
                mEngine.AddEntityNode(mTransform.GetAsMatrix(), mEntity, *node);
            mTransform.Pop();
        }
        virtual void LeaveNode(const EntityNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const Entity& mEntity;
        PhysicsEngine& mEngine;
        Transform mTransform;
    };
    Visitor visitor(entity_to_world, entity, *this);

    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);

    Transform trans(entity_to_world);

    // create joints between physics bodies based on the
    // entity joint definitions
    for (size_t i=0; i<entity.GetNumJoints(); ++i)
    {
        const auto& joint = entity.GetJoint(i);
        const auto* src_node = joint.GetSrcNode();
        const auto* dst_node = joint.GetDstNode();
        PhysicsNode* src_physics_node = FindPhysicsNode(src_node->GetId());
        PhysicsNode* dst_physics_node = FindPhysicsNode(dst_node->GetId());
        ASSERT(src_physics_node && dst_physics_node);

        // the local anchor points are relative to the node itself.
        const auto& src_local_anchor = joint.GetSrcAnchorPoint();
        const auto& dst_local_anchor = joint.GetDstAnchorPoint();

        // transform the anchor points into the physics world.
        trans.Push(entity.FindNodeTransform(src_node));
            const auto& src_world_anchor = trans.GetAsMatrix() * glm::vec4(src_local_anchor, 1.0f, 1.0f);
        trans.Pop();
        trans.Push(entity.FindNodeTransform(dst_node));
            const auto& dst_world_anchor = trans.GetAsMatrix() * glm::vec4(dst_local_anchor, 1.0f, 1.0f);
        trans.Pop();
        // distance between the anchor points is the same as the distance
        // between the anchor points in the physics world.
        const auto distance = glm::length(dst_world_anchor - src_world_anchor);

        const auto type = joint.GetType();

        if (type == Entity::PhysicsJointType::Distance)
        {
            const auto& params = std::get<EntityClass::DistanceJointParams>(joint.GetParams());

            b2DistanceJointDef def = {};
            def.bodyA = src_physics_node->world_body;
            def.bodyB = dst_physics_node->world_body;
            def.localAnchorA = ToBox2D(src_local_anchor);
            def.localAnchorB = ToBox2D(dst_local_anchor);
            if (params.min_distance.has_value())
                def.minLength = MapLengthFromGame(params.min_distance.value());
            else def.minLength = distance;
            if (params.max_distance.has_value())
                def.maxLength = MapLengthFromGame(params.max_distance.value());
            else def.maxLength = distance;
            def.stiffness    = params.stiffness;
            def.damping      = params.damping;
            if (def.minLength > def.maxLength) {
                WARN("Entity distance joint min distance exceeds max distance. [entity='%1', joint='%2', min_dist=%3, max_dist=%4]",
                     entity.GetClassName(), entity.GetName(), joint.GetName(), def.minLength, def.maxLength);
                def.minLength = def.maxLength;
            }
            // the joint is deleted whenever either body is deleted.
            auto* ret = mWorld->CreateJoint(&def);
            DEBUG("Created new physics distance joint. [entity='%1/%2' joint='%3', src='%4', dst='%5', min=%6, max=%7]",
                  entity.GetClassName(), entity.GetName(), joint.GetName(), src_node->GetName(), dst_node->GetName(),
                  def.minLength, def.maxLength);
        }
        else BUG("Unhandled physics joint type.");
    }
}

void PhysicsEngine::AddEntityNode(const glm::mat4& model_to_world, const Entity& entity, const EntityNode& node)
{
    const FBox box(model_to_world);
    const auto* body = node.GetRigidBody();
    const auto& node_pos_in_world   = box.GetCenter();
    const auto& node_size_in_world  = box.GetSize();
    const auto& rigid_body_class    = body->GetClass();
    const auto& debug_name          = entity.GetName() + "/" + node.GetName();

    // body def is used to define a new physics body in the world.
    b2BodyDef body_def;
    if (rigid_body_class.GetSimulation() == RigidBodyItemClass::Simulation::Static)
        body_def.type = b2_staticBody;
    else if (rigid_body_class.GetSimulation() == RigidBodyItemClass::Simulation::Dynamic)
        body_def.type = b2_dynamicBody;
    else if (rigid_body_class.GetSimulation() == RigidBodyItemClass::Simulation::Kinematic)
        body_def.type = b2_kinematicBody;

    body_def.position.Set(node_pos_in_world.x, node_pos_in_world.y);
    body_def.angle          = box.GetRotation();
    body_def.angularDamping = body->GetAngularDamping();
    body_def.linearDamping  = body->GetLinearDamping();
    body_def.enabled        = body->TestFlag(RigidBodyItem::Flags::Enabled);
    body_def.bullet         = body->TestFlag(RigidBodyItem::Flags::Bullet);
    body_def.fixedRotation  = body->TestFlag(RigidBodyItem::Flags::DiscardRotation);
    body_def.allowSleep     = body->TestFlag(RigidBodyItem::Flags::CanSleep);
    b2Body* world_body = mWorld->CreateBody(&body_def);

    // collision shape used for collision resolver for the body.
    std::unique_ptr<b2Shape> collision_shape;
    std::string polygonId;
    if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Box)
    {
        auto box = std::make_unique<b2PolygonShape>();
        box->SetAsBox(node_size_in_world.x * 0.5, node_size_in_world.y * 0.5);
        collision_shape = std::move(box);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Circle)
    {
        auto circle = std::make_unique<b2CircleShape>();
        circle->m_radius = std::max(node_size_in_world.x * 0.5, node_size_in_world.y * 0.5);
        collision_shape = std::move(circle);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::SemiCircle)
    {
        const float width  = node_size_in_world.x;
        const float height = node_size_in_world.y;
        b2Vec2 verts[7];
        verts[0] = b2Vec2(0.0, -height*0.5);
        verts[1] = b2Vec2(-width*0.5*0.50, -height*0.5*0.86);
        verts[2] = b2Vec2(-width*0.5*0.86, -height*0.5*0.50);
        verts[3] = b2Vec2(-width*0.5*1.00, -height*0.5*0.00);
        verts[4] = b2Vec2( width*0.5*1.00, -height*0.5*0.00);
        verts[5] = b2Vec2( width*0.5*0.86, -height*0.5*0.50);
        verts[6] = b2Vec2( width*0.5*0.50, -height*0.5*0.86);
        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 7);
        collision_shape = std::move(poly);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::RightTriangle)
    {
        const float width   = node_size_in_world.x;
        const float height  = node_size_in_world.y;
        b2Vec2 verts[3];
        verts[0] = b2Vec2(-width*0.5, -height*0.5);
        verts[1] = b2Vec2(-width*0.5f, height*0.5);
        verts[2] = b2Vec2( width*0.5,  height*0.5);
        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 3);
        collision_shape = std::move(poly);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::IsoscelesTriangle)
    {
        const float width   = node_size_in_world.x;
        const float height  = node_size_in_world.y;
        b2Vec2 verts[3];
        verts[0] = b2Vec2( 0.0f, -height*0.5);
        verts[1] = b2Vec2(-width*0.5f, height*0.5);
        verts[2] = b2Vec2( width*0.5,  height*0.5);
        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 3);
        collision_shape = std::move(poly);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Trapezoid)
    {
        const float width   = node_size_in_world.x;
        const float height  = node_size_in_world.y;
        b2Vec2 verts[4];
        verts[0] = b2Vec2(-width*0.3, -height*0.5);
        verts[1] = b2Vec2(-width*0.5, height*0.5);
        verts[2] = b2Vec2( width*0.5, height*0.5);
        verts[3] = b2Vec2( width*0.3, -height*0.5);
        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 4);
        collision_shape = std::move(poly);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Parallelogram)
    {
        const float width   = node_size_in_world.x;
        const float height  = node_size_in_world.y;
        b2Vec2 verts[4];
        verts[0] = b2Vec2(-width*0.3, -height*0.5);
        verts[1] = b2Vec2(-width*0.5, height*0.5);
        verts[2] = b2Vec2( width*0.3, height*0.5);
        verts[3] = b2Vec2( width*0.5, -height*0.5);
        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 4);
        collision_shape = std::move(poly);
    }
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Polygon)
    {
        polygonId = body->GetPolygonShapeId();
        if (polygonId.empty()) {
            WARN("Rigid body has no polygon shape id set. [node='%1']", debug_name);
            return;
        }
        const auto& drawable = mClassLib->FindDrawableClassById(polygonId);
        if (!drawable || drawable->GetType() != gfx::DrawableClass::Type::Polygon) {
            WARN("No polygon class found for rigid body. [node='%1']", debug_name);
            return;
        }
        const auto& polygon = std::static_pointer_cast<const gfx::PolygonClass>(drawable);
        const float width   = node_size_in_world.x;
        const float height  = node_size_in_world.y;
        std::vector<b2Vec2> verts;
        for (size_t i=0; i<polygon->GetNumVertices(); ++i)
        {
            const auto& vertex = polygon->GetVertex(i);
            // polygon vertices are in normalized coordinate space in the lower
            // right quadrant, i.e. x = [0, 1] and y = [0, -1], flip about x axis
            const auto x = vertex.aPosition.x * width;
            const auto y = vertex.aPosition.y * height * -1.0;
            b2Vec2 v2;
            // offset the vertices to be around origin.
            // the vertices must be relative to the body when the shape
            // is attached to a body.
            v2.x = x - width * 0.5f;
            v2.y = y - height * 0.5f;
            verts.push_back(v2);
        }
        // invert the order of polygons in order to invert the winding
        // oder. This is done because of the flip around axis inverts
        // the winding order
        std::reverse(verts.begin(), verts.end());

        // it's possible that the set of vertices for a convex hull has
        // less vertices than the polygon itself. I'm not sure how Box2D
        // will behave when the number of vertices exceeds b2_maxPolygonVertices.
        // Finding the convex hull here can at least help us discard some
        // irrelevant vertices already.
        verts = math::FindConvexHull(verts);
        // still too many?
        if (verts.size() > b2_maxPolygonVertices) {
            // todo: deal with situation when we have more than b2_maxPolygonVertices (8?)
            WARN("The convex hull for rigid body has too many vertices. [node='%1']", debug_name);
        }

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(&verts[0], verts.size());
        // todo: radius??
        collision_shape = std::move(poly);
    }

    // fixture attaches a collision shape to the body.
    b2FixtureDef fixture;
    fixture.shape       = collision_shape.get();
    fixture.density     = body->GetDensity();
    fixture.friction    = body->GetFriction();
    fixture.restitution = body->GetRestitution();
    fixture.isSensor    = body->TestFlag(RigidBodyItem::Flags::Sensor);
    b2Fixture* fixture_ptr = world_body->CreateFixture(&fixture);

    PhysicsNode physics_node;
    physics_node.debug_name    = debug_name;
    physics_node.world_body    = world_body;
    physics_node.entity        = const_cast<game::Entity*>(&entity); 
    physics_node.node          = const_cast<game::EntityNode*>(&node);
    physics_node.world_extents = node_size_in_world;
    physics_node.polygonId     = polygonId;
    physics_node.shape         = (unsigned)body->GetCollisionShape();
    physics_node.flags         = body->GetFlags().value();
    ASSERT(mNodes.find(node.GetId()) == mNodes.end());
    mNodes[node.GetId()]   = physics_node;
    mFixtures[fixture_ptr] = node.GetId();
    DEBUG("Created new physics body. [node='%1']", debug_name);
}

PhysicsEngine::PhysicsNode* PhysicsEngine::FindPhysicsNode(const std::string& id)
{
    auto it = mNodes.find(id);
    if (it == mNodes.end())
        return nullptr;
    return &it->second;
}

const PhysicsEngine::PhysicsNode* PhysicsEngine::FindPhysicsNode(const std::string& id) const
{
    auto it = mNodes.find(id);
    if (it == mNodes.end())
        return nullptr;
    return &it->second;
}

} // namespace
