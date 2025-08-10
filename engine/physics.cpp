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
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "game/types.h"
#include "game/scene.h"
#include "game/entity.h"
#include "game/transform.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_fixture.h"
#include "engine/physics.h"
#include "engine/loader.h"

using namespace game;

// ADL lookup for math::FindConvexHull in the global
// namespace for box2d
inline b2Vec2 GetPosition(const b2Vec2& vec2)
{ return vec2; }

inline b2Vec2 ToBox2D(const glm::vec2& vector)
{ return b2Vec2 {vector.x, vector.y}; }

inline glm::vec2 ToGlm(const b2Vec2& vector) noexcept
{ return glm::vec2 { vector.x, vector.y }; }

inline float ToGlm(float val) noexcept
{ return val; }
inline bool ToGlm(bool val) noexcept
{ return val; }

namespace {
template<typename Joint, typename Value, b2JointType ExpectedType>
bool MaybeSetJointValue(b2Joint* joint, void (Joint::*SetFunction)(Value), const game::RigidBodyJoint::JointSettingValue& value)
{
    if (joint->GetType() != ExpectedType)
        return false;

    if (!std::holds_alternative<Value>(value))
        return false;

    auto* da_joint = static_cast<Joint*>(joint);
    (da_joint->*SetFunction)(std::get<Value>(value));
    return true;
}

template<typename Joint, b2JointType ExpectedType, typename Value>
bool MaybeGetJointValue(const b2Joint* joint, Value (Joint::*GetFunction)(void) const, engine::PhysicsEngine::JointValueType* out)
{
    if (joint->GetType() != ExpectedType)
        return false;

    const auto* da_joint = static_cast<const Joint*>(joint);

    *out = ToGlm((da_joint->*GetFunction)());
    return true;
}

template<typename Joint, b2JointType ExpectedType, typename Value>
bool MaybeGetJointValue(const b2Joint* joint, Value (Joint::*GetFunction)(float inv_dt) const, engine::PhysicsEngine::JointValueType* out, float inv_dt)
{
    if (joint->GetType() != ExpectedType)
        return false;

    const auto* da_joint = static_cast<const Joint*>(joint);

    *out = ToGlm((da_joint->*GetFunction)(inv_dt));
    return true;
}

void TransformVertices(const glm::vec2& shape_size,
                       const glm::vec2& shape_offset,
                       float shape_rotation,
                       b2Vec2* vertices, unsigned count)
{
    const auto t = shape_rotation;
    const auto& rot = glm::mat2(glm::vec2( std::cos(t), std::sin(t)),
                                glm::vec2(-std::sin(t), std::cos(t)));
    const auto& scale = glm::mat2(glm::vec2(shape_size.x, 0.0f),
                                  glm::vec2(0.0f, shape_size.y));
    for (unsigned i=0; i<count; ++i)
    {
        const auto& vec = ToGlm(vertices[i]);
        const auto& ret = (rot * scale * vec) + shape_offset;
        vertices[i] = ToBox2D(ret);
    }
}

std::unique_ptr<b2Shape> CreateCollisionShape(const engine::ClassLibrary& classlib,
                                              const std::string& polygon_shape_id,
                                              const std::string& debug_name,
                                              const glm::vec2& shape_size,
                                              const glm::vec2& shape_offset,
                                              float shape_rotation,
                                              game::CollisionShape shape)
{
    // collision shape used for collision resolver for the body.
    std::unique_ptr<b2Shape> collision_shape;
    if (shape == game::CollisionShape::Box)
    {
        b2Vec2 verts[4];
        verts[0] = b2Vec2(-0.5, -0.5);
        verts[1] = b2Vec2(-0.5,  0.5);
        verts[2] = b2Vec2( 0.5,  0.5);
        verts[3] = b2Vec2( 0.5, -0.5);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 4);

        auto box = std::make_unique<b2PolygonShape>();
        //box->SetAsBox(shape_size.x*0.5, shape_size.y*0.5, ToBox2D(shape_offset), shape_rotation);
        box->Set(verts, 4);
        collision_shape = std::move(box);
    }
    else if (shape == RigidBodyClass::CollisionShape::Circle)
    {
        auto circle = std::make_unique<b2CircleShape>();
        circle->m_radius = std::min(shape_size.x * 0.5, shape_size.y * 0.5);
        circle->m_p = ToBox2D(shape_offset);
        collision_shape = std::move(circle);
    }
    else if (shape == RigidBodyClass::CollisionShape::SemiCircle)
    {
        b2Vec2 verts[7];
        verts[0] = b2Vec2( 0.0,      -0.5);
        verts[1] = b2Vec2(-0.5*0.50, -0.5*0.86);
        verts[2] = b2Vec2(-0.5*0.86, -0.5*0.50);
        verts[3] = b2Vec2(-0.5*1.00, -0.5*0.00);
        verts[4] = b2Vec2( 0.5*1.00, -0.5*0.00);
        verts[5] = b2Vec2( 0.5*0.86, -0.5*0.50);
        verts[6] = b2Vec2( 0.5*0.50, -0.5*0.86);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 7);

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 7);
        collision_shape = std::move(poly);
    }
    else if (shape == RigidBodyClass::CollisionShape::RightTriangle)
    {
        b2Vec2 verts[3];
        verts[0] = b2Vec2(-0.5, -0.5);
        verts[1] = b2Vec2(-0.5,  0.5);
        verts[2] = b2Vec2( 0.5,  0.5);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 3);

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 3);
        collision_shape = std::move(poly);
    }
    else if (shape == RigidBodyClass::CollisionShape::IsoscelesTriangle)
    {
        b2Vec2 verts[3];
        verts[0] = b2Vec2( 0.0, -0.5);
        verts[1] = b2Vec2(-0.5,  0.5);
        verts[2] = b2Vec2( 0.5,  0.5);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 3);

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 3);
        collision_shape = std::move(poly);
    }
    else if (shape == RigidBodyClass::CollisionShape::Trapezoid)
    {
        b2Vec2 verts[4];
        verts[0] = b2Vec2(-0.3, -0.5);
        verts[1] = b2Vec2(-0.5,  0.5);
        verts[2] = b2Vec2( 0.5,  0.5);
        verts[3] = b2Vec2( 0.3, -0.5);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 4);

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 4);
        collision_shape = std::move(poly);
    }
    else if (shape == RigidBodyClass::CollisionShape::Parallelogram)
    {
        b2Vec2 verts[4];
        verts[0] = b2Vec2(-0.3, -0.5);
        verts[1] = b2Vec2(-0.5, 0.5);
        verts[2] = b2Vec2( 0.3, 0.5);
        verts[3] = b2Vec2( 0.5, -0.5);
        TransformVertices(shape_size, shape_offset, shape_rotation, verts, 4);

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(verts, 4);
        collision_shape = std::move(poly);
    }
    else if (shape == RigidBodyClass::CollisionShape::Polygon)
    {
        if (polygon_shape_id.empty())
        {
            WARN("Rigid body has no polygon shape id set. [node='%1']", debug_name);
            return collision_shape;
        }
        const auto& drawable = classlib.FindDrawableClassById(polygon_shape_id);
        if (!drawable || drawable->GetType() != gfx::DrawableClass::Type::Polygon)
        {
            WARN("No polygon class found for rigid body. [node='%1']", debug_name);
            return collision_shape;
        }
        const auto& polygon = std::static_pointer_cast<const gfx::PolygonMeshClass>(drawable);
        if (!polygon->HasInlineData())
        {
            WARN("Polygon class has no inline vertex data. [node='%1']", debug_name);
            return collision_shape;
        }
        if (*polygon->GetVertexLayout() != gfx::GetVertexLayout<gfx::Vertex2D>())
        {
            WARN("Polygon has non 2D vertex type.");
            return collision_shape;
        }

        const gfx::VertexStream stream(*polygon->GetVertexLayout(),
                                        polygon->GetVertexBufferPtr(),
                                        polygon->GetVertexBufferSize());

        std::vector<b2Vec2> verts;
        for (size_t i=0; i<stream.GetCount(); ++i)
        {
            const auto* vertex = stream.GetVertex<gfx::Vertex2D>(i);
            // polygon vertices are in normalized coordinate space in the lower
            // right quadrant, i.e. x = [0, 1] and y = [0, -1], flip about x axis
            const auto x = vertex->aPosition.x *  1.0;
            const auto y = vertex->aPosition.y * -1.0;
            b2Vec2 vert;
            // offset the vertices to be around origin.
            // the vertices must be relative to the body when the shape
            // is attached to a body.
            vert.x = x - 0.5f;
            vert.y = y - 0.5f;
            verts.push_back(vert);
        }
        TransformVertices(shape_size, shape_offset, shape_rotation, &verts[0], verts.size());
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
        if (verts.size() > b2_maxPolygonVertices)
        {
            // todo: deal with situation when we have more than b2_maxPolygonVertices (8?)
            WARN("The convex hull for rigid body has too many vertices. [node='%1']", debug_name);
        }

        auto poly = std::make_unique<b2PolygonShape>();
        poly->Set(&verts[0], verts.size());
        // todo: radius??
        collision_shape = std::move(poly);
    }
    return collision_shape;
}

} // namespace


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
        const auto* entity = node.entity;
        if (!entity->HasRigidBodies())
            continue;
        if (entity->HasBeenKilled())
        {
            KillEntity(*entity);
        }
        else
        {
            if (entity->HasBeenSpawned())
            {
                AddEntity(transform.GetAsMatrix(), *entity);
            }

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
        auto* entity = node.entity;
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
            const auto* fixtureA = base::SafeFind(mEngine.mFixtures, A);
            const auto* fixtureB = base::SafeFind(mEngine.mFixtures, B);
            ASSERT(fixtureA && fixtureB);
            ContactEvent event;
            event.type = ContactEvent::Type::BeginContact;
            event.nodeA = fixtureA->node;
            event.nodeB = fixtureB->node;
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
            const auto* fixtureA = base::SafeFind(mEngine.mFixtures, A);
            const auto* fixtureB = base::SafeFind(mEngine.mFixtures, B);
            ASSERT(fixtureA && fixtureB);
            ContactEvent event;
            event.type = ContactEvent::Type::EndContact;
            event.nodeA = fixtureA->node;
            event.nodeB = fixtureB->node;
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
    if (auto* ptr = base::SafeFind(mNodes, id))
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
    if (auto* ptr = base::SafeFind(mNodes, node))
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
    if (auto* ptr = base::SafeFind(mNodes, id))
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

void PhysicsEngine::RayCast(const glm::vec2& start, const glm::vec2& end,
    std::vector<RayCastResult>* result, RayCastMode mode) const
{
    class RayCastCallback : public b2RayCastCallback {
    public:
        RayCastCallback(const PhysicsEngine& engine, RayCastMode mode,
            std::vector<RayCastResult>& result)
                : mEngine(engine)
                , mMode(mode)
                , mResult(result)
        {}
        virtual float ReportFixture(b2Fixture* fixture,
                                    const b2Vec2& point,
                                    const b2Vec2& normal, float fraction) override
        {
            const auto* fixture_data = base::SafeFind(mEngine.mFixtures, fixture);
            RayCastResult result;
            result.node     = fixture_data->node;
            result.point    = ToGlm(point);
            result.normal   = ToGlm(normal);
            result.fraction = fraction;

            // ffs, the Box2D documentation on the semantics of the return value
            // are f*n garbage.

            // this document here https://www.iforce2d.net/b2dtut/world-querying says that
            // To find only the closest intersection:
            // - return the fraction value from the callback
            // - use the most recent intersection as the result
            // To find all intersections along the ray:
            // - return 1 from the callback
            // - store the intersections in a list
            // To simply find if the ray hits anything:
            // - if you get a callback, something was hit (but it may not be the closest)
            // - return 0 from the callback for efficiency
            if (mMode == RayCastMode::Closest)
            {
                mResult.clear();
                mResult.push_back(result);
                return fraction;
            }
            else if (mMode == RayCastMode::All)
            {
                mResult.push_back(result);
                return 1.0;
            }
            mResult.push_back(result);
            return 0.0f;
        }
    private:
        const PhysicsEngine& mEngine;
        const RayCastMode mMode = RayCastMode::All;
        std::vector<RayCastResult>& mResult;
    };
    RayCastCallback cb(*this, mode, *result);
    mWorld->RayCast(&cb, ToBox2D(start), ToBox2D(end));
}

void PhysicsEngine::CreateWorld(const Scene& scene)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mFixtures.clear();
    mNodes.clear();
    mWorld.reset();
    mWorld = std::make_unique<b2World>(gravity);

    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);

    const auto& nodes = scene.CollectNodes();
    for (const auto& node : nodes)
    {
        const auto* entity = node.entity;
        if (!entity->HasRigidBodies())
            continue;

        transform.Push(node.node_to_scene);
          AddEntity(transform.GetAsMatrix(), *node.entity);
        transform.Pop();
    }
}

void PhysicsEngine::CreateWorld(const Entity& entity)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mFixtures.clear();
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
    if (const auto* ptr = base::SafeFind(mNodes, node.GetId()))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, ToGlm(body->GetLinearVelocity()));
    }
    return std::make_tuple(false, glm::vec2(0.0f, 0.0f));
}

std::tuple<bool, float> PhysicsEngine::FindCurrentAngularVelocity(const game::EntityNode& node) const
{
    if (const auto* ptr = base::SafeFind(mNodes, node.GetId()))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetAngularVelocity());
    }
    return std::make_tuple(false, 0.0f);
}

std::tuple<bool, float> PhysicsEngine::FindMass(const game::EntityNode& node) const
{
    if (const auto* ptr = base::SafeFind(mNodes, node.GetId()))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetMass());
    }
    return std::make_tuple(false, 0.0f);
}

std::tuple<bool, PhysicsEngine::JointValueType> PhysicsEngine::FindJointValue(const game::RigidBodyJoint& game_joint, JointValue value) const
{
    // any of these nodes should connect us to the rigid body which
    // is connected to the joint list which has the joint we're looking for
    const auto* src_node = game_joint.GetSrcNode();
    const auto* dst_node = game_joint.GetDstNode();
    ASSERT(src_node->HasRigidBody());
    ASSERT(dst_node->HasRigidBody());

    const auto* world_joint = FindJoint(*src_node, game_joint);
    if (!world_joint)
        return {false, 0.0f};

    auto ret = false;
   JointValueType val;

    if (value == JointValue::MinimumLength)
        ret = MaybeGetJointValue<b2DistanceJoint, e_distanceJoint, float>(world_joint, &b2DistanceJoint::GetMinLength, &val);
    else if (value == JointValue::MaximumLength)
        ret = MaybeGetJointValue<b2DistanceJoint, e_distanceJoint, float>(world_joint, &b2DistanceJoint::GetMaxLength, &val);
    else if (value == JointValue::CurrentLength)
        ret = MaybeGetJointValue<b2DistanceJoint, e_distanceJoint, float>(world_joint, &b2DistanceJoint::GetCurrentLength, &val);
    else if (value == JointValue::JointTranslation)
        ret = MaybeGetJointValue<b2PrismaticJoint, e_prismaticJoint, float>(world_joint, &b2PrismaticJoint::GetJointTranslation, &val);
    else if (value == JointValue::JointSpeed)
    {
        ret = MaybeGetJointValue<b2PrismaticJoint, e_prismaticJoint, float>(world_joint, &b2PrismaticJoint::GetJointSpeed, &val) ||
              MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, float>(world_joint, &b2RevoluteJoint::GetJointSpeed, &val);
    }
    else if (value == JointValue::JointAngle)
    {
        ret = MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, float>(world_joint, &b2RevoluteJoint::GetJointAngle, &val);
    }
    else if (value == JointValue::MotorSpeed)
    {
        ret = MaybeGetJointValue<b2PrismaticJoint, e_prismaticJoint, float>(world_joint, &b2PrismaticJoint::GetMotorSpeed, &val) ||
              MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, float>(world_joint, &b2RevoluteJoint::GetMotorSpeed, &val);
    }
    else if (value == JointValue::MotorTorque)
    {
        const auto inv_dt = 1.0 / mTimestep;
        ret = MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, float>(world_joint, &b2RevoluteJoint::GetMotorTorque, &val, inv_dt);
    }

    else if (value == JointValue::IsMotorEnabled)
    {
        ret = MaybeGetJointValue<b2PrismaticJoint, e_prismaticJoint, bool>(world_joint, &b2PrismaticJoint::IsMotorEnabled, &val) ||
              MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, bool>(world_joint, &b2RevoluteJoint::IsMotorEnabled, &val);
    }
    else if (value == JointValue::IsLimitEnabled)
    {
        ret = MaybeGetJointValue<b2PrismaticJoint, e_prismaticJoint, bool>(world_joint, &b2PrismaticJoint::IsLimitEnabled, &val) ||
              MaybeGetJointValue<b2RevoluteJoint, e_revoluteJoint, bool>(world_joint, &b2RevoluteJoint::IsLimitEnabled, &val);
    }
    else if (value == JointValue::Stiffness)
    {
        ret = MaybeGetJointValue<b2DistanceJoint, e_distanceJoint, float>(world_joint, &b2DistanceJoint::GetStiffness, &val) ||
              MaybeGetJointValue<b2WeldJoint, e_weldJoint, float>(world_joint, &b2WeldJoint::GetStiffness, &val);
    }
    else if (value == JointValue::Damping)
    {
        ret = MaybeGetJointValue<b2DistanceJoint, e_distanceJoint, float>(world_joint, &b2DistanceJoint::GetDamping, &val) ||
              MaybeGetJointValue<b2WeldJoint, e_weldJoint, float>(world_joint, &b2WeldJoint::GetDamping, &val);
    }
    else if (value == JointValue::LinearOffset)
    {
        if (world_joint->GetType() == e_motorJoint)
        {
            val = ToGlm(static_cast<const b2MotorJoint*>(world_joint)->GetLinearOffset());
            ret = true;
        }
    }
    else if (value == JointValue::AngularOffset)
    {
        ret = MaybeGetJointValue<b2MotorJoint, e_motorJoint, float>(world_joint, &b2MotorJoint::GetAngularOffset, &val);
    }
    else if (value == JointValue::SegmentLengthA)
    {
        ret = MaybeGetJointValue<b2PulleyJoint, e_pulleyJoint, float>(world_joint, &b2PulleyJoint::GetCurrentLengthA, &val);
    }
    else if (value == JointValue::SegmentLengthB)
    {
        ret = MaybeGetJointValue<b2PulleyJoint, e_pulleyJoint, float>(world_joint, &b2PulleyJoint::GetCurrentLengthB, &val);
    }

    return std::make_tuple(ret, val);
}

std::tuple<bool, glm::vec2> PhysicsEngine::FindJointConnectionPoint(const game::EntityNode& node,
                                                                    const game::RigidBodyJoint& game_joint) const

{
    glm::vec2 ret = {0.0f, 0.0f};

    const auto* world_joint = FindJoint(node, game_joint);
    if (!world_joint)
        return {false, ret};

    if (game_joint.GetDstNode() == &node)
    {
        ret = ToGlm(world_joint->GetAnchorB());
    }
    else if (game_joint.GetSrcNode() == &node)
    {
        ret = ToGlm(world_joint->GetAnchorA());
    }
    else return {false, ret};

    return {true, ret};
}


#if defined(ENGINE_ENABLE_PHYSICS_DEBUG)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter) const
{
    static gfx::MaterialInstance mat(gfx::CreateMaterialClassFromColor(gfx::Color4f(gfx::Color::HotPink, 0.6)));

    gfx::Transform model;
    model.Scale(mScale);

    std::unordered_set<const b2Joint*> joints;

    // there's b2Draw api for debug drawing but it seems that
    // when wanting to debug the *game* (not the physics engine
    // integration issues itself) this is actually more straightforward
    // than using the b2Draw way of visualizing the physics world.
    for (const auto& p : mNodes)
    {
        const auto& rigid_body_data = p.second;
        const auto* world_body = rigid_body_data.world_body;
        const float angle = world_body->GetAngle();
        const b2Vec2& pos = world_body->GetPosition();

        model.Push();
        model.RotateAroundZ(angle);
        model.Translate(pos.x, pos.y);

        // visualize each fixture attached to the body.
        const b2Fixture* fixture = world_body->GetFixtureList();
        while (fixture)
        {
            const auto* fixture_data = base::SafeFind(mFixtures, (b2Fixture*)fixture);
            game::CollisionShape shape;
            std::string polygon;
            if (const auto* ptr = fixture_data->node->GetRigidBody()) {
                shape   = ptr->GetCollisionShape();
                polygon = ptr->GetPolygonShapeId();
            } else if (const auto* ptr = fixture_data->node->GetFixture()) {
                shape   = ptr->GetCollisionShape();
                polygon = ptr->GetPolygonShapeId();
            } else BUG("Unexpected fixture without node attachment.");

            auto shape_size = fixture_data->shape_size;
            if (shape == RigidBodyClass::CollisionShape::Circle)
                shape_size.x = shape_size.y = std::min(shape_size.x, shape_size.y);

            model.Push();
            model.Scale(shape_size);
            model.Translate(shape_size * -0.5f);
            model.RotateAroundZ(fixture_data->shape_rotation);
            model.Translate(fixture_data->shape_offset);

            if (shape == RigidBodyClass::CollisionShape::Box)
                painter.Draw(gfx::Rectangle(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::Circle)
                painter.Draw(gfx::Circle(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::SemiCircle)
                painter.Draw(gfx::SemiCircle(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::RightTriangle)
                painter.Draw(gfx::RightTriangle(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::IsoscelesTriangle)
                painter.Draw(gfx::IsoscelesTriangle(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::Trapezoid)
                painter.Draw(gfx::Trapezoid(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::Parallelogram)
                painter.Draw(gfx::Parallelogram(), model, mat);
            else if (shape == RigidBodyClass::CollisionShape::Polygon)
            {
                const auto& klass = mClassLib->FindDrawableClassById(polygon);
                if (klass == nullptr) {
                    WARN("No polygon class found for node '%1'", fixture_data->debug_name);
                } else {
                    auto poly = gfx::CreateDrawableInstance(klass);
                    painter.Draw(*poly, model, mat);
                }
            } else BUG("Unhandled collision shape for debug drawing.");

            model.Pop();
            fixture = fixture->GetNext();
        }

        const b2JointEdge* joint_list = world_body->GetJointList();
        while (joint_list)
        {
            const b2Joint* joint = joint_list->joint;
            if (base::Contains(joints, joint))
            {
                joint_list = joint_list->next;
                continue;
            }
            if (joint->GetType() == b2JointType::e_motorJoint)
            {
                const auto* mj = static_cast<const b2MotorJoint*>(joint);
                const auto& src_world_anchor = MapVectorToGame(ToGlm(mj->GetAnchorA()));
                const auto& dst_world_anchor = MapVectorToGame(ToGlm(mj->GetAnchorB()));
                gfx::DebugDrawLine(painter, src_world_anchor, dst_world_anchor, gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(src_world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(dst_world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_distanceJoint)
            {
                const auto* dj = static_cast<const b2DistanceJoint*>(joint);
                const auto& src_world_anchor = MapVectorToGame(ToGlm(dj->GetAnchorA()));
                const auto& dst_world_anchor = MapVectorToGame(ToGlm(dj->GetAnchorB()));
                gfx::DebugDrawLine(painter, src_world_anchor, dst_world_anchor, gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(src_world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(dst_world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_revoluteJoint)
            {
                const auto* rj = static_cast<const b2RevoluteJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(rj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_weldJoint)
            {
                const auto* wj = static_cast<const b2WeldJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(wj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_prismaticJoint)
            {
                const auto* pj = static_cast<const b2PrismaticJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(pj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_pulleyJoint)
            {
                const auto* pj = static_cast<const b2PulleyJoint*>(joint);
                const auto& world_ground_anchor_a = MapVectorToGame(ToGlm(pj->GetGroundAnchorA()));
                const auto& world_ground_anchor_b = MapVectorToGame(ToGlm(pj->GetGroundAnchorB()));
                const auto& world_body_anchor_a = MapVectorToGame(ToGlm(pj->GetAnchorA()));
                const auto& world_body_anchor_b = MapVectorToGame(ToGlm(pj->GetAnchorB()));
                gfx::DebugDrawLine(painter, world_body_anchor_a, world_ground_anchor_a, gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawLine(painter, world_body_anchor_b, world_ground_anchor_b, gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawLine(painter, world_ground_anchor_a, world_ground_anchor_b, gfx::Color::HotPink, 2.0f);

                gfx::DebugDrawCircle(painter, gfx::FCircle(world_ground_anchor_a, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_ground_anchor_b, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_body_anchor_a, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_body_anchor_b, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            joints.insert(joint);
            joint_list = joint_list->next;
        }

        model.Pop();
    }
}
#endif // ENGINE_ENABLE_PHYSICS_DEBUG

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
                world_body->SetEnabled(rigid_body->TestFlag(RigidBody::Flags::Enabled));
                world_body->SetBullet(rigid_body->TestFlag(RigidBody::Flags::Bullet));
                world_body->SetFixedRotation(rigid_body->TestFlag(RigidBody::Flags::DiscardRotation));
                world_body->SetSleepingAllowed(rigid_body->TestFlag(RigidBody::Flags::CanSleep));
                b2Fixture* fixture_list_head = world_body->GetFixtureList();
                while (fixture_list_head)
                {
                    auto* fixture_data = base::SafeFind(mEngine.mFixtures, fixture_list_head);
                    if (auto* ptr = fixture_data->node->GetRigidBody())
                        fixture_list_head->SetSensor(rigid_body->TestFlag(RigidBody::Flags::Sensor));

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

                if (rigid_body->HasAnyPhysicsAdjustment())
                    WARN("Can't apply any dynamic change (impulse, velocity, or force) to a static body. [node='%1']", phys_node->debug_name);
            }
            else
            {
                // if the body has a flag set we'll let the physics world body be moved based on the
                // transformation set on the node.  The idea is that this lets the game to do some small
                // changes such as make sure that some object is at the right place after some physics
                // movement. For example in a physics platformer an elevator might be moved over some time
                // in one direction by setting its velocity and then moved back over some time by setting
                // its velocity to the inverse value. The problem with this is that of course this isn't
                // exact and the objects position might change slightly depending on how many time steps
                // are taken in each direction.
                if (rigid_body->HasTransformReset())
                {
                    mTransform.Push(node->GetModelTransform());
                    const FBox box(mTransform);
                    const auto& node_pos_in_world = box.GetCenter();
                    world_body->SetTransform(b2Vec2(node_pos_in_world.x, node_pos_in_world.y), box.GetRotation());
                    mTransform.Pop();
                }

                // apply any adjustment done by the animation/game to the physics body.
                if (rigid_body->HasAngularVelocityAdjustment())
                    world_body->SetAngularVelocity(rigid_body->GetAngularVelocityAdjustment());
                if (rigid_body->HasLinearVelocityAdjustment())
                    world_body->SetLinearVelocity(ToBox2D(rigid_body->GetLinearVelocityAdjustment()));
                if (rigid_body->HasCenterImpulse())
                    world_body->ApplyLinearImpulseToCenter(ToBox2D(rigid_body->GetLinearImpulseToCenter()), true /*wake*/);
                if (rigid_body->HasCenterForce())
                    world_body->ApplyForceToCenter(ToBox2D(rigid_body->GetForceToCenter()), true /* wake */);

                rigid_body->ClearPhysicsAdjustments();
            }

            // the set of joints connected to the body should be the same
            // whether we look at the physics body object or the game rigid
            // body object. But the world body object already stores a direct
            // pointer to the game joint object so it's faster and simpler
            // that way to access the joint data.
            // we clear the joint after processing so in case we see
            // the same joint multiple times the settings are cleared
            // so no double change will take place.
            const b2JointEdge* joint_list = world_body->GetJointList();
            while (joint_list)
            {
                b2Joint* joint = joint_list->joint;

                const auto type = joint->GetType();

                auto* game_joint = (RigidBodyJoint*)joint->GetUserData().pointer;
                if (game_joint->CanSettingsChangeRuntime())
                {
                    for (size_t i=0; i<game_joint->GetNumPendingAdjustments(); ++i)
                    {
                        const auto* setting = game_joint->GetPendingAdjustment(i);

                        bool ok = true;

                        if (setting->setting == RigidBodyJoint::JointSetting::EnableMotor)
                        {
                            ok = MaybeSetJointValue<b2PrismaticJoint, bool, e_prismaticJoint>(joint, &b2PrismaticJoint::EnableMotor, setting->value) ||
                                 MaybeSetJointValue<b2RevoluteJoint,  bool, e_revoluteJoint>(joint, &b2RevoluteJoint::EnableMotor, setting->value);
                        }
                        else if (setting->setting == RigidBodyJoint::JointSetting::EnableLimit)
                        {
                            ok = MaybeSetJointValue<b2PrismaticJoint, bool, e_prismaticJoint>(joint, &b2PrismaticJoint::EnableLimit, setting->value) ||
                                 MaybeSetJointValue<b2RevoluteJoint,  bool, e_revoluteJoint>(joint, &b2RevoluteJoint::EnableLimit, setting->value);
                        }
                        else if (setting->setting == RigidBodyJoint::JointSetting::MotorTorque)
                        {
                            ok = MaybeSetJointValue<b2RevoluteJoint, float, e_revoluteJoint>(joint, &b2RevoluteJoint::SetMaxMotorTorque, setting->value) ||
                                 MaybeSetJointValue<b2PrismaticJoint, float, e_prismaticJoint>(joint, &b2PrismaticJoint::SetMaxMotorForce, setting->value) || // this seems actually it's torque (according to b2_prismatic_joint header)
                                 MaybeSetJointValue<b2MotorJoint, float, e_motorJoint>(joint, &b2MotorJoint::SetMaxTorque, setting->value);
                        }
                        else if (setting->setting == RigidBodyJoint::JointSetting::MotorSpeed)
                        {
                            ok = MaybeSetJointValue<b2PrismaticJoint, float, e_prismaticJoint>(joint, &b2PrismaticJoint::SetMotorSpeed, setting->value) ||
                                 MaybeSetJointValue<b2RevoluteJoint,  float, e_revoluteJoint>(joint, &b2RevoluteJoint::SetMotorSpeed, setting->value);
                        }
                        else if (setting->setting == RigidBodyJoint::JointSetting::MotorForce)
                        {
                            ok = MaybeSetJointValue<b2MotorJoint, float, e_motorJoint>(joint, &b2MotorJoint::SetMaxForce, setting->value);
                        }
                        else if(setting->setting == RigidBodyJoint::JointSetting::Stiffness)
                        {
                            ok = MaybeSetJointValue<b2WeldJoint, float, e_weldJoint>(joint, &b2WeldJoint::SetStiffness, setting->value) ||
                                 MaybeSetJointValue<b2DistanceJoint, float, e_distanceJoint>(joint, &b2DistanceJoint::SetStiffness, setting->value);
                        }
                        else if (setting->setting == RigidBodyJoint::JointSetting::Damping)
                        {
                            ok = MaybeSetJointValue<b2WeldJoint, float, e_weldJoint>(joint, &b2WeldJoint::SetDamping, setting->value) ||
                                 MaybeSetJointValue<b2DistanceJoint, float, e_distanceJoint>(joint, &b2DistanceJoint::SetDamping, setting->value);

                        } else BUG("Missing joint setting.");

                        if (!ok)
                        {
                            WARN("Failed to apply change on physics joint setting(s). [entity='%1', joint='%2', setting='%3']",
                                 phys_node->debug_name,  game_joint->GetName(), setting->setting);
                        }
                    }
                }
                else if (game_joint->HasPendingAdjustments())
                {
                    WARN("Physics joint settings are ignored since joint is statically configured. [entity='%1', joint='%2']",
                         phys_node->debug_name, game_joint->GetName());

                }
                // the pending adjustments now become the current values.
                game_joint->RealizePendingAdjustments();
                game_joint->ClearPendingAdjustments();

                joint_list = joint_list->next;
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
            transform.RotateAroundZ(physics_world_angle);
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

            // in order to support joint animation we must make the current
            // joint values available in the rigid body joint so the
            // animation system can change the values properly.

            // we don't actually need this block right now since the only
            // way to change joint settings is by using the adjust joint
            // settings API in the joint class. That means that when the
            // adjustments are realized they become the current values
            // and they remain current until any other adjustment is done
            // thus there's no need to update the current values dynamically
            // continuously. Only if something else would be allowed to
            // change the settings then we'd need this.
#if 0
            const b2JointEdge* joint_list = world_body->GetJointList();
            while (joint_list)
            {
                const b2Joint* joint = joint_list->joint;

                const auto type = joint->GetType();

                auto* dst = (RigidBodyJoint*)joint->GetUserData().pointer;
                if (dst->CanSettingsChangeRuntime())
                {
                    if (type == e_revoluteJoint)
                    {
                        const auto* src = static_cast<const b2RevoluteJoint*>(joint);
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::EnableMotor, src->IsMotorEnabled());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::EnableLimit, src->IsLimitEnabled());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorTorque, src->GetMaxMotorTorque());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorSpeed, src->GetMotorSpeed());
                    }
                    else if (type == e_prismaticJoint)
                    {
                        const auto* src = static_cast<const b2PrismaticJoint*>(joint);
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::EnableMotor, src->IsMotorEnabled());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::EnableLimit, src->IsLimitEnabled());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorTorque, src->GetMaxMotorForce());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorSpeed, src->GetMotorSpeed());
                    }
                    else if (type == e_motorJoint)
                    {
                        const auto* src = static_cast<const b2MotorJoint*>(joint);
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorForce, src->GetMaxForce());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::MotorTorque, src->GetMaxTorque());
                    }
                    else if (type == e_distanceJoint)
                    {
                        const auto* src = static_cast<const b2DistanceJoint*>(joint);
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::Stiffness, src->GetStiffness());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::Damping, src->GetDamping());
                    }
                    else if (type == e_weldJoint)
                    {
                        const auto* src = static_cast<const b2WeldJoint*>(joint);
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::Stiffness, src->GetStiffness());
                        dst->UpdateCurrentJointValue(RigidBodyJointSetting::Damping, src->GetDamping());
                    }
                }
            }
#endif

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

            if (!node->HasRigidBody() && !node->HasFixture())
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

    // create joints between physics bodies based on the
    // entity joint definitions
    for (size_t i=0; i<entity.GetNumJoints(); ++i)
    {
        const auto& joint = entity.GetJoint(i);
        const auto* src_node = joint.GetSrcNode();
        const auto* dst_node = joint.GetDstNode();
        if (src_node == dst_node)
        {
            ERROR("Cannot create a rigid body physics joint that would connect a rigid body to itself. "
                  "[entity='%1', joint='%2', node='%3']", entity.GetClassName(), joint.GetName(), src_node);
            continue;
        }

        RigidBodyData* src_physics_node = base::SafeFind(mNodes, src_node->GetId());
        RigidBodyData* dst_physics_node = base::SafeFind(mNodes, dst_node->GetId());
        if (!src_physics_node || !dst_physics_node)
        {
            ERROR("Failed to find a rigid body for physics joint creation.");
            continue;
        }

        // the local anchor points are relative to the node itself.
        const auto& src_local_anchor = joint.GetSrcAnchor();
        const auto& dst_local_anchor = joint.GetDstAnchor();

        // transform the anchor points into the physics world.
        Transform transform(entity_to_world);
        transform.Push(entity.FindNodeTransform(src_node));
            const auto& src_world_anchor = transform.GetAsMatrix() * glm::vec4(src_local_anchor, 1.0f, 1.0f);
        transform.Pop();

        transform.Push(entity.FindNodeTransform(dst_node));
            const auto& dst_world_anchor = transform.GetAsMatrix() * glm::vec4(dst_local_anchor, 1.0f, 1.0f);
        transform.Pop();

        const auto type = joint.GetType();

        if (type == RigidBodyJoint::JointType::Distance)
        {
            const auto& params = std::get<RigidBodyJointClass::DistanceJointParams>(joint.GetParams());

            // distance between the anchor points is the same as the distance
            // between the anchor points in the physics world.
            const auto distance = glm::length(dst_world_anchor - src_world_anchor);

            b2DistanceJointDef def = {};
            def.collideConnected = joint->CollideConnected();
            def.bodyA            = src_physics_node->world_body;
            def.bodyB            = dst_physics_node->world_body;
            def.localAnchorA     = ToBox2D(MapVectorFromGame(src_local_anchor));
            def.localAnchorB     = ToBox2D(MapVectorFromGame(dst_local_anchor));
            if (params.min_distance.has_value())
                def.minLength = params.min_distance.value();
            else def.minLength = distance;
            if (params.max_distance.has_value())
                def.maxLength = params.max_distance.value();
            else def.maxLength = distance;
            def.stiffness    = params.stiffness;
            def.damping      = params.damping;
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            if (def.minLength > def.maxLength)
            {
                def.minLength = def.maxLength;
            }
            // the joint is deleted whenever either body is deleted.
            mWorld->CreateJoint(&def);

            DEBUG("Distance joint info: [min distance=%1, max distance=%2, stiffness=%3, damping=%4]",
                  def.minLength, def.maxLength, params.stiffness, params.damping);

        }
        else if (type == RigidBodyJoint::JointType::Revolute)
        {
            const auto& params = std::get<RigidBodyJointClass::RevoluteJointParams>(joint.GetParams());
            // the revolute joint is a hinge like joint with one single point of rotation
            // around which the bodies rotate.
            b2RevoluteJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body, ToBox2D(src_world_anchor));
            def.collideConnected = joint->CollideConnected();
            def.enableLimit      = params.enable_limit;
            def.enableMotor      = params.enable_motor;
            def.upperAngle       = params.upper_angle_limit.ToRadians();
            def.lowerAngle       = params.lower_angle_limit.ToRadians() * -1.0f;
            def.motorSpeed       = params.motor_speed;
            def.maxMotorTorque   = params.motor_torque;
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            mWorld->CreateJoint(&def);

            DEBUG("Revolute joint info: [limit=%1, motor=%2, range=%3-%4, speed=%5, torque=%6",
                  params.enable_limit, params.enable_motor,
                  params.lower_angle_limit.ToDegrees(),
                  params.upper_angle_limit.ToDegrees(),
                  params.motor_speed, params.motor_torque);
        }
        else if (type == RigidBodyJoint::JointType::Weld)
        {
            const auto& params = std::get<RigidBodyJointClass::WeldJointParams>(joint.GetParams());
            b2WeldJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body, ToBox2D(src_world_anchor));
            def.collideConnected = joint->CollideConnected();
            def.damping          = params.damping;
            def.stiffness        = params.stiffness;
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            mWorld->CreateJoint(&def);

            DEBUG("Weld joint info: [damping=%1, stiffness=%2]", params.damping, params.stiffness);
        }
        else if (type == RigidBodyJoint::JointType::Prismatic)
        {
            Transform transform(entity_to_world);
            transform.Push(entity.FindNodeTransform(src_node));

            const auto& params = std::get<RigidBodyJointClass::PrismaticJointParams>(joint.GetParams());
            const auto& local_direction_vector = RotateVectorAroundZ(glm::vec2(1.0f, 0.0f), params.direction_angle.ToRadians());
            const auto& world_direction_vector = TransformVector(transform, local_direction_vector);

            b2PrismaticJointDef def {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body,
                           ToBox2D(src_world_anchor),
                           ToBox2D(world_direction_vector));
            def.collideConnected = joint->CollideConnected();

            def.enableLimit = params.enable_limit;
            def.enableMotor = params.enable_motor;
            def.motorSpeed  = params.motor_speed;
            // called force but the comments in the b2_prismatic_joint cay it's *torque*
            def.maxMotorForce = params.motor_torque;
            def.lowerTranslation = params.lower_limit * -1.0f;
            def.upperTranslation = params.upper_limit;
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            mWorld->CreateJoint(&def);

            DEBUG("Prismatic joint info: [limit=%1, motor=%2, range=%3-%4, speed=%5, torque=%6]",
                  params.enable_limit, params.enable_motor,
                  def.lowerTranslation, def.upperTranslation,
                  params.motor_speed, params.motor_torque);
        }
        else if (type == RigidBodyJoint::JointType ::Motor)
        {
            const auto& params = std::get<RigidBodyJointClass::MotorJointParams>(joint.GetParams());

            b2MotorJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body);
            def.collideConnected = joint->CollideConnected();
            def.maxForce = params.max_force;
            def.maxTorque = params.max_torque;
            def.correctionFactor = 0.3f;
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            mWorld->CreateJoint(&def);

            DEBUG("Motor joint info: [max_force=%1, max_torque=%2]", def.maxForce, def.maxTorque);
        }
        else if (type == RigidBodyJoint::JointType::Pulley)
        {
            const auto& params = std::get<RigidBodyJointClass::PulleyJointParams>(joint.GetParams());
            const auto* world_anchor_node_a = entity.FindNodeByClassId(params.anchor_nodes[0]);
            const auto* world_anchor_node_b = entity.FindNodeByClassId(params.anchor_nodes[1]);
            if (!world_anchor_node_a || !world_anchor_node_b)
            {
                ERROR("Failed to find pulley joint world anchor node.");
                continue;
            }

            // we're taking the center point of the node as the point that
            // will be transformed into world coordinates as the ground
            // anchor point for the pulley joint. This could also be use
            // configurable, similar to the regular joint anchors that the
            // user can adjust.

            Transform trans(entity_to_world);
            trans.Push(entity.FindNodeTransform(world_anchor_node_a));
                const auto world_ground_anchor_a = trans * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            trans.Pop();
            trans.Push(entity.FindNodeTransform(world_anchor_node_b));
               const auto world_ground_anchor_b = trans * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            trans.Pop();

            b2PulleyJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body,
                           ToBox2D(world_ground_anchor_a),
                           ToBox2D(world_ground_anchor_b),
                           ToBox2D(src_world_anchor),
                           ToBox2D(dst_world_anchor),
                           params.ratio);
            /// UGLY UGLY DANGEROUS
            // Store a reference to our joint data object in the joint itself
            // so that it's quick and easy to check if the joint parameters/settings
            // have been changed.
            def.userData.pointer = (uintptr_t)&joint;

            mWorld->CreateJoint(&def);

            DEBUG("Pulley joint info: [ratio=%1]", def.ratio);
        }
        else BUG("Unhandled physics joint type.");

        DEBUG("Created new physics joint. [entity='%1', type=%2, joint='%3', node='%4', node='%5']",
              entity.GetDebugName(), joint.GetType(), joint.GetName(), src_node->GetName(), dst_node->GetName());

        // If the joint supports changing the settings at runtime
        // dynamically after the joint has been created then fill
        // out the current (same as initial) values in the current
        // joint setting values.
        joint.InitializeCurrentValues();
    }
}

void PhysicsEngine::AddEntityNode(const glm::mat4& model_to_world, const Entity& entity, const EntityNode& node)
{
    FBox box(model_to_world);

    const auto& debug_name = entity.GetName() + "/" + node.GetName();

    if (const auto* body = node.GetRigidBody())
    {
        const auto& node_world_position = box.GetCenter();
        const auto& node_world_size     = box.GetSize();
        const auto  node_world_rotation = box.GetRotation();
        // this is the offset of the collision shape (fixture)
        // relative to the center of the physics body. I.e. there's
        // no offset and the collision shape is centered around the
        // physics body.
        const auto& shape_size   = node_world_size;
        const auto& shape_offset = glm::vec2(0.0f, 0.0f);

        auto collision_shape = CreateCollisionShape(*mClassLib,
            body->GetPolygonShapeId(),
            debug_name,
            shape_size,
            shape_offset,
            0.0f,
            body->GetCollisionShape());
        if (!collision_shape)
        {
            WARN("No collision shape. Skipping physics body creation. [node='%1']", debug_name);
            return;
        }

        // body def is used to define a new physics body in the world.
        b2BodyDef body_def;
        if (body->GetSimulation() == RigidBodyClass::Simulation::Static)
            body_def.type = b2_staticBody;
        else if (body->GetSimulation() == RigidBodyClass::Simulation::Dynamic)
            body_def.type = b2_dynamicBody;
        else if (body->GetSimulation() == RigidBodyClass::Simulation::Kinematic)
            body_def.type = b2_kinematicBody;

        body_def.position.Set(node_world_position.x, node_world_position.y);
        body_def.angle          = node_world_rotation;
        body_def.angularDamping = body->GetAngularDamping();
        body_def.linearDamping  = body->GetLinearDamping();
        body_def.enabled        = body->TestFlag(RigidBody::Flags::Enabled);
        body_def.bullet         = body->TestFlag(RigidBody::Flags::Bullet);
        body_def.fixedRotation  = body->TestFlag(RigidBody::Flags::DiscardRotation);
        body_def.allowSleep     = body->TestFlag(RigidBody::Flags::CanSleep);
        b2Body* world_body = mWorld->CreateBody(&body_def);

        // fixture attaches a collision shape to the body.
        b2FixtureDef fixture_def = {};
        fixture_def.shape       = collision_shape.get(); // cloned!?
        fixture_def.density     = body->GetDensity();
        fixture_def.friction    = body->GetFriction();
        fixture_def.restitution = body->GetRestitution();
        fixture_def.isSensor    = body->TestFlag(RigidBody::Flags::Sensor);
        b2Fixture* fixture = world_body->CreateFixture(&fixture_def);

        FixtureData fixture_data;
        fixture_data.node           = const_cast<game::EntityNode*>(&node);
        fixture_data.debug_name     = debug_name;
        fixture_data.shape_size     = shape_size;
        fixture_data.shape_offset   = shape_offset;
        fixture_data.shape_rotation = 0.0f;
        mFixtures[fixture] = fixture_data;

        RigidBodyData body_data;
        body_data.debug_name    = debug_name;
        body_data.world_body    = world_body;
        body_data.node          = const_cast<game::EntityNode*>(&node);
        body_data.world_extents = node_world_size;
        body_data.flags         = body->GetFlags().value();
        mNodes[node.GetId()]    = body_data;
        DEBUG("Created new physics body. [node='%1']", debug_name);
    }
    else if (const auto* fixture = node.GetFixture())
    {
        const auto* rigid_body_node = entity.FindNodeByClassId(fixture->GetRigidBodyNodeId());

        // the fixture attaches to the rigid body of another  entity node.
        if (auto* rigid_body_data = base::SafeFind(mNodes, rigid_body_node->GetId()))
        {
            b2Body* world_body = rigid_body_data->world_body;
            const auto world_body_rotation = world_body->GetAngle();
            const auto world_body_position = ToGlm(world_body->GetPosition());

            // The incoming transformation matrix transforms vertices
            // relative to entity node's local basis relative to the
            // physics world basis. That means that the FBox represents
            // the current node's box in the physics world. However,
            // the fixture (collision shape) needs to be expressed
            // relative to the physics rigid body.
            Transform transform;
            transform.RotateAroundZ(world_body_rotation);
            transform.Translate(world_body_position);
            // transformation for transforming vertices from the physics
            // world to the rigid body.
            const auto& world_to_body = glm::inverse(transform.GetAsMatrix());
            // Transform the box (its vertices) from world to rigid body.
            box.Transform(world_to_body);
            // collision shape parameters relative the rigid body
            const auto shape_size     = box.GetSize();
            const auto shape_offset   = box.GetCenter();
            const auto shape_rotation = box.GetRotation();

            auto collision_shape = CreateCollisionShape(*mClassLib,
                fixture->GetPolygonShapeId(),
                debug_name,
                shape_size,
                shape_offset,
                shape_rotation,
                fixture->GetCollisionShape());
            if (!collision_shape)
            {
                WARN("No Collision shape. Skipping fixture creation. [node='%1']", debug_name);
                return;
            }

            const auto* rigid_body  = rigid_body_data->node->GetRigidBody();
            const auto* density     = fixture->GetDensity();
            const auto* restitution = fixture->GetRestitution();
            const auto* friction    = fixture->GetFriction();

            b2FixtureDef fixture_def {};
            fixture_def.shape       = collision_shape.get();
            fixture_def.density     = density     ? *density     : rigid_body->GetDensity();
            fixture_def.friction    = friction    ? *friction    : rigid_body->GetFriction();
            fixture_def.restitution = restitution ? *restitution : rigid_body->GetRestitution();
            fixture_def.isSensor    = fixture->TestFlag(game::Fixture::Flags::Sensor);
            b2Fixture* fixture_ptr  = world_body->CreateFixture(&fixture_def);

            FixtureData fixture_data;
            fixture_data.node           = const_cast<game::EntityNode*>(&node);
            fixture_data.debug_name     = debug_name;
            fixture_data.shape_size     = shape_size;
            fixture_data.shape_offset   = shape_offset;
            fixture_data.shape_rotation = shape_rotation;
            mFixtures[fixture_ptr]      = fixture_data;
            DEBUG("Attached new fixture to a rigid body. [body='%1', fixture='%2']",
                  rigid_body_data->debug_name, node.GetName());
        }
        else
        {
            // todo: it's possible that we're visiting the nodes in the
            // wrong order, i.e. visiting the fixture node first before
            // visiting the rigid body node. in this case the rigid body
            // would not yet exist. This needs to be fixed later.
            WARN("Fixture refers to a physics body which isn't created yet. [entity='%1', body='%2', fixture='%3]",
                entity.GetName(), rigid_body_node->GetName(), node.GetName());
            return;
        }
    }
}

const b2Joint* PhysicsEngine::FindJoint(const game::EntityNode& node, const game::RigidBodyJoint& game_joint) const
{
    //const auto*
    const auto* src_world_node = base::SafeFind(mNodes, node.GetId());
    if (!src_world_node)
        return nullptr;

    const auto* src_world_body = src_world_node->world_body;
    const auto* src_joint_list = src_world_body->GetJointList();
    while (src_joint_list)
    {
        const b2Joint* joint = src_joint_list->joint;
        if ((const game::RigidBodyJoint*)joint->GetUserData().pointer == &game_joint)
        {
            return joint;
        }
        src_joint_list = src_joint_list->next;
    }
    return nullptr;
}

} // namespace
