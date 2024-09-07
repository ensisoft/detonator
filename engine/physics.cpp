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
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_fixture.h"

using namespace game;

// ADL lookup for math::FindConvexHull in the global
// namespace for box2d
inline b2Vec2 GetPosition(const b2Vec2& vec2)
{ return vec2; }

inline b2Vec2 ToBox2D(const glm::vec2& vector)
{ return b2Vec2 {vector.x, vector.y}; }

inline glm::vec2 ToGlm(const b2Vec2& vector)
{ return glm::vec2 { vector.x, vector.y }; }

namespace {
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
        circle->m_radius = std::max(shape_size.x * 0.5, shape_size.y * 0.5);
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
    return FindCurrentLinearVelocity(node.GetId());
}
std::tuple<bool, glm::vec2> PhysicsEngine::FindCurrentLinearVelocity(const std::string& node) const
{
    if (const auto* ptr = base::SafeFind(mNodes, node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, ToGlm(body->GetLinearVelocity()));
    }
    return std::make_tuple(false, glm::vec2(0.0f, 0.0f));
}
std::tuple<bool, float> PhysicsEngine::FindCurrentAngularVelocity(const game::EntityNode& node) const
{
    return FindCurrentAngularVelocity(node.GetId());
}
std::tuple<bool, float> PhysicsEngine::FindCurrentAngularVelocity(const std::string& node) const
{
    if (const auto* ptr = base::SafeFind(mNodes, node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetAngularVelocity());
    }
    return std::make_tuple(false, 0.0f);
}
std::tuple<bool, float> PhysicsEngine::FindMass(const game::EntityNode& node) const
{
    return FindMass(node.GetId());
}
std::tuple<bool, float> PhysicsEngine::FindMass(const std::string& node) const
{
    if (const auto* ptr = base::SafeFind(mNodes, node))
    {
        const auto* body = ptr->world_body;
        return std::make_tuple(true, body->GetMass());
    }
    return std::make_tuple(false, 0.0f);
}

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter) const
{
    static gfx::MaterialInstance mat(gfx::CreateMaterialClassFromColor(gfx::Color4f(gfx::Color::HotPink, 0.6)));

    gfx::Transform model;
    model.Scale(mScale);

    std::unordered_set<b2Joint*> joints;

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

            model.Push();
            model.Scale(fixture_data->shape_size);
            model.Translate(fixture_data->shape_size * -0.5f);
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
                gfx::DebugDrawLine(painter,
                                   gfx::FPoint(src_world_anchor.x, src_world_anchor.y),
                                   gfx::FPoint(dst_world_anchor.x, dst_world_anchor.y), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(src_world_anchor.x, src_world_anchor.y, 5.0f), gfx::Color::HotPink, 2.0f);
                gfx::DebugDrawCircle(painter, gfx::FCircle(dst_world_anchor.x, dst_world_anchor.y, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_revoluteJoint)
            {
                b2RevoluteJoint* rj = static_cast<b2RevoluteJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(rj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor.x, world_anchor.y, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_weldJoint)
            {
                b2WeldJoint* wj = static_cast<b2WeldJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(wj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor.x, world_anchor.y, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_prismaticJoint)
            {
                b2PrismaticJoint* pj = static_cast<b2PrismaticJoint*>(joint);
                const auto& world_anchor = MapVectorToGame(ToGlm(pj->GetAnchorA()));
                gfx::DebugDrawCircle(painter, gfx::FCircle(world_anchor.x, world_anchor.y, 5.0f), gfx::Color::HotPink, 2.0f);
            }
            else if (joint->GetType() == b2JointType::e_pulleyJoint)
            {
                b2PulleyJoint* pj = static_cast<b2PulleyJoint*>(joint);
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
                if (rigid_body->HasCenterImpulse())
                    world_body->ApplyLinearImpulseToCenter(ToBox2D(rigid_body->GetLinearImpulseToCenter()), true /*wake*/);

                rigid_body->ClearPhysicsAdjustments();
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

        if (type == Entity::PhysicsJointType::Distance)
        {
            const auto& params = std::get<EntityClass::DistanceJointParams>(joint.GetParams());

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
                def.minLength = MapLengthFromGame(params.min_distance.value());
            else def.minLength = distance;
            if (params.max_distance.has_value())
                def.maxLength = MapLengthFromGame(params.max_distance.value());
            else def.maxLength = distance;
            def.stiffness    = params.stiffness;
            def.damping      = params.damping;

            if (def.minLength > def.maxLength)
            {
                def.minLength = def.maxLength;
            }
            // the joint is deleted whenever either body is deleted.
            mWorld->CreateJoint(&def);

            DEBUG("Distance joint info: [min distance=%1, max distance=%2, stiffness=%3, damping=%4]",
                  def.minLength, def.maxLength, params.stiffness, params.damping);

        }
        else if (type == Entity::PhysicsJointType::Revolute)
        {
            const auto& params = std::get<EntityClass::RevoluteJointParams>(joint.GetParams());
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
            mWorld->CreateJoint(&def);

            DEBUG("Revolute joint info: [limit=%1, motor=%2, range=%3-%4, speed=%5, torque=%6",
                  params.enable_limit, params.enable_motor,
                  params.lower_angle_limit.ToDegrees(),
                  params.upper_angle_limit.ToDegrees(),
                  params.motor_speed, params.motor_torque);
        }
        else if (type == Entity::PhysicsJointType::Weld)
        {
            const auto& params = std::get<EntityClass::WeldJointParams>(joint.GetParams());
            b2WeldJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body, ToBox2D(src_world_anchor));
            def.collideConnected = joint->CollideConnected();
            def.damping          = params.damping;
            def.stiffness        = params.stiffness;
            mWorld->CreateJoint(&def);

            DEBUG("Weld joint info: [damping=%1, stiffness=%2]", params.damping, params.stiffness);
        }
        else if (type == Entity::PhysicsJointType::Prismatic)
        {
            Transform transform(entity_to_world);
            transform.Push(entity.FindNodeTransform(src_node));

            const auto& params = std::get<EntityClass::PrismaticJointParams>(joint.GetParams());
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
            def.maxMotorForce = params.motor_torque;
            def.lowerTranslation = params.lower_limit * -1.0f;
            def.upperTranslation = params.upper_limit;
            mWorld->CreateJoint(&def);

            DEBUG("Prismatic joint info: [limit=%1, motor=%2, range=%3-%4, speed=%5, torque=%6]",
                  params.enable_limit, params.enable_motor,
                  def.lowerTranslation, def.upperTranslation,
                  params.motor_speed, params.motor_torque);
        }
        else if (type == Entity::PhysicsJointType ::Motor)
        {
            const auto& params = std::get<EntityClass::MotorJointParams>(joint.GetParams());

            b2MotorJointDef def = {};
            def.Initialize(src_physics_node->world_body, dst_physics_node->world_body);
            def.collideConnected = joint->CollideConnected();
            def.maxForce = params.max_force;
            def.maxTorque = params.max_torque;
            def.correctionFactor = 0.3f;
            mWorld->CreateJoint(&def);

            DEBUG("Motor joint info: [max_force=%1, max_torque=%2]", def.maxForce, def.maxTorque);
        }
        else if (type == Entity::PhysicsJointType::Pulley)
        {
            const auto& params = std::get<EntityClass::PulleyJointParams>(joint.GetParams());
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

            mWorld->CreateJoint(&def);

            DEBUG("Pulley joint info: [ratio=%1]", def.ratio);

        }

        else BUG("Unhandled physics joint type.");

        DEBUG("Created new physics joint. [entity='%1', type=%2, joint='%3', node='%4', node='%5']",
              entity.GetDebugName(), joint.GetType(), joint.GetName(), src_node->GetName(), dst_node->GetName());
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

} // namespace
