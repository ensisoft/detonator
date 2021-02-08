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

#include <algorithm>

#include "base/logging.h"
#include "base/math.h"
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "engine/physics.h"
#include "engine/loader.h"
#include "engine/types.h"
#include "engine/scene.h"
#include "engine/transform.h"

// ADL lookup for math::FindConvexHull in the global
// namespace for box2d
inline b2Vec2 GetPosition(const b2Vec2& vec2)
{ return vec2; }

namespace game
{

PhysicsEngine::PhysicsEngine(const ClassLibrary* loader)
  : mLoader(loader)
{}


void PhysicsEngine::UpdateScene(Scene& scene)
{
    using RenderTree = Scene::RenderTree ;

    for (auto& pair : mNodes)
        pair.second.alive = false;

    // two options here for updating entity nodes based on the
    // physics simulation.
    // 1. traverse the whole scene and look which entity nodes
    //    also exist in the physics world.
    //    a) use the render tree to traverse the scene.
    //    b) use the entity list to iterate over the scene and then
    //       see which entity's nodes have physics nodes.
    // 2. iterate over the physics nodes and find them in the scene
    //    and then update.
    // Currently i'm not sure which strategy is more efficient. It'd seem
    // that if a lot of the entity nodes in the scene have physics bodies
    // then traversing the whole scene is a viable alternative. However if
    // only a few nodes have physics bodies then likely only iterating over
    // the physics bodies and then looking up their transforms in the scene
    // is more efficient.
    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);

    const auto& nodes = scene.CollectNodes();
    for (const auto& node : nodes)
    {
        transform.Push(node.node_to_scene);
          UpdateEntity(transform.GetAsMatrix(), *node.entity);
        transform.Pop();
    }
    // cull physics nodes that were not touched.
    for (auto it = mNodes.begin(); it != mNodes.end();)
    {
        auto& node = it->second;
        if (node.alive)
        {
            ++it;
            continue;
        }
        DEBUG("Deleting physics node '%1'", node.debug_name);

        b2Fixture* fixture_list_head = node.world_body->GetFixtureList();
        while (fixture_list_head) {
            mFixtures.erase(fixture_list_head);
            fixture_list_head = fixture_list_head->GetNext();
        }
        mWorld->DestroyBody(node.world_body);

        it = mNodes.erase(it);
    }
}

void PhysicsEngine::UpdateEntity(Entity& entity)
{
    Transform transform;
    transform.Scale(glm::vec2(1.0f, 1.0f) / mScale);
    UpdateEntity(transform.GetAsMatrix(), entity);
}

void PhysicsEngine::Tick(std::vector<ContactEvent>* contacts)
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
            ASSERT(mEngine.mFixtures.find(A) != mEngine.mFixtures.end());
            ASSERT(mEngine.mFixtures.find(B) != mEngine.mFixtures.end());
            ContactEvent event;
            event.type = ContactEvent::Type::BeginContact;
            event.nodeA = mEngine.mFixtures[A];
            event.nodeB = mEngine.mFixtures[B];
            event.entityA = mEngine.mNodes[event.nodeA].entity;
            event.entityB = mEngine.mNodes[event.nodeB].entity;
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
            ASSERT(mEngine.mFixtures.find(A) != mEngine.mFixtures.end());
            ASSERT(mEngine.mFixtures.find(B) != mEngine.mFixtures.end());
            ContactEvent event;
            event.type = ContactEvent::Type::EndContact;
            event.nodeA = mEngine.mFixtures[A];
            event.nodeB = mEngine.mFixtures[B];
            event.entityA = mEngine.mNodes[event.nodeA].entity;
            event.entityB = mEngine.mNodes[event.nodeB].entity;
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

void PhysicsEngine::ApplyImpulseToCenter(const std::string& id, const glm::vec2& impulse) const
{
    auto it = mNodes.find(id);
    if (it == mNodes.end())
    {
        WARN("Applying impulse to a physics body that doesn't exist: '%1'", id);
        return;
    }
    auto& node = it->second;
    auto* body = node.world_body;
    if (body->GetType() != b2_dynamicBody)
    {
        WARN("Applying impulse to non dynamic body ('%1') will have no effect.", id);
        return;
    }
    body->ApplyLinearImpulseToCenter(b2Vec2(impulse.x, impulse.y), true /*wake */);
}

void PhysicsEngine::ApplyImpulseToCenter(const EntityNode& node, const glm::vec2& impulse) const
{
    ApplyImpulseToCenter(node.GetId(), impulse);
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
        transform.Push(node.node_to_scene);
          AddEntity(transform.GetAsMatrix(), *node.entity);
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

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view)
{
    auto mat = gfx::SolidColor(gfx::Color::HotPink);
    mat.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mat.SetBaseAlpha(0.6);

    view.Push();
    view.Scale(mScale);

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
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::RightTriangle)
            painter.Draw(gfx::RightTriangle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::IsoscelesTriangle)
            painter.Draw(gfx::IsoscelesTriangle(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Trapezoid)
            painter.Draw(gfx::Trapezoid(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Parallelogram)
            painter.Draw(gfx::Parallelogram(), view, mat);
        else if (physics_node.shape == (unsigned)RigidBodyItemClass::CollisionShape::Polygon) {
            const auto& klass = mLoader->FindDrawableClassById(physics_node.polygonId);
            if (klass == nullptr) {
                WARN("No polygon class found for node '%1'", physics_node.debug_name);
            } else {
                auto poly = gfx::CreateDrawableInstance(klass);
                painter.Draw(*poly, view, mat);
            }
        } else BUG("!unhandled collision shape for debug drawing.");

        view.Pop();
        view.Pop();
    }
    view.Pop();
}
#endif // GAMESTUDIO_ENABLE_PHYSICS_DEBUG

void PhysicsEngine::UpdateEntity(const glm::mat4& model_to_world, Entity& entity)
{
    using RenderTree = Entity::RenderTree;

    class Visitor : public RenderTree::Visitor
    {
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

            // look if we have a physics node for this entity node.
            auto it = mEngine.mNodes.find(node->GetId());
            if (it == mEngine.mNodes.end())
            {
                // no node, no rigid body, nothing to do.
                if (!node->HasRigidBody())
                    return;
                // add a new rigid body based on this node.
                mTransform.Push(node->GetModelTransform());
                    mEngine.AddEntityNode(mTransform.GetAsMatrix(), mEntity, *node);
                mTransform.Pop();
            }
            else
            {
                // physics node has been created before but the rigid body
                // was removed from the node. erase the physics node.
                if (!node->HasRigidBody())
                {
                    mEngine.mWorld->DestroyBody(it->second.world_body);
                    mEngine.mNodes.erase(it);
                    return;
                }
            }

            auto& physics_node = it->second;
            physics_node.alive = true;
            if (physics_node.world_body->GetType() == b2BodyType::b2_staticBody)
            {
                auto* body = physics_node.world_body;
                // static bodies are not moved by the physics engine but they
                // may be moved by the user.
                // update the world transform from the scene to the physics world.
                mTransform.Push(node->GetModelTransform());
                    const FBox box(mTransform.GetAsMatrix());
                    const auto& node_pos_in_world   = box.GetPosition();
                    body->SetTransform(b2Vec2(node_pos_in_world.x, node_pos_in_world.y), box.GetRotation());
                mTransform.Pop();
            }
            else
            {
                if (physics_node.world_body->GetType() == b2BodyType::b2_kinematicBody)
                {
                    // set the instantaneous velocities from the animation system
                    // to the physics system in order to drive objects physically.
                    // the velocities are expressed relative to the world.
                    const auto* body = node->GetRigidBody();
                    const auto& velo = body->GetLinearVelocity();
                    physics_node.world_body->SetAngularVelocity(body->GetAngularVelocity());
                    physics_node.world_body->SetLinearVelocity(b2Vec2(velo.x, velo.y));
                }

                // get the object's transform properties in the physics world.
                const auto physics_world_pos = physics_node.world_body->GetPosition();
                const auto physics_world_angle = physics_node.world_body->GetAngle();

                // transform back into scene relative to the node's parent.
                // i.e. the world transform of the node is expressed as a transform
                // relative to its parent node.
                glm::mat4 mat;
                Transform transform;
                transform.Rotate(physics_world_angle);
                transform.Translate(physics_world_pos.x, physics_world_pos.y);
                transform.Push();
                    transform.Scale(physics_node.world_extents);
                    transform.Translate(physics_node.world_extents * -0.5f);
                    mat = transform.GetAsMatrix();
                transform.Pop();

                FBox box(mat);
                box.Transform(glm::inverse(node_to_world));
                node->SetTranslation(box.GetPosition());
                node->SetRotation(box.GetRotation());
                auto* body = node->GetRigidBody();
                const auto& linear_velocity = physics_node.world_body->GetLinearVelocity();
                const auto angular_velocity = physics_node.world_body->GetAngularVelocity();
                body->SetLinearVelocity(glm::vec2(linear_velocity.x, linear_velocity.y));
                body->SetAngularVelocity(angular_velocity);
            }
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


void PhysicsEngine::AddEntity(const glm::mat4& model_to_world, const Entity& entity)
{
    using RenderTree = Entity::RenderTree;

    class Visitor : public RenderTree::ConstVisitor
    {
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

            auto it = mEngine.mNodes.find(node->GetId());
            if (it != mEngine.mNodes.end())
                return;
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
    Visitor visitor(model_to_world, entity, *this);

    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

void PhysicsEngine::AddEntityNode(const glm::mat4& model_to_world, const Entity& entity, const EntityNode& node)
{
    const FBox box(model_to_world);
    const auto* body = node.GetRigidBody();
    const auto& node_pos_in_world   = box.GetPosition();
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
    const auto& velo = body->GetLinearVelocity();
    // set initial velocities.
    world_body->SetLinearVelocity(b2Vec2(velo.x, velo.y));
    world_body->SetAngularVelocity(body->GetAngularVelocity());

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
            WARN("Rigid body '%1' has no polygon shape id set.", debug_name);
            return;
        }
        const auto& drawable = mLoader->FindDrawableClassById(polygonId);
        if (!drawable || drawable->GetType() != gfx::DrawableClass::Type::Polygon) {
            WARN("No polygon class found for rigid body '%1'.", debug_name);
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
            WARN("The convex hull for rigid body '%1' has too many vertices.", debug_name);
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
    physics_node.entity        = entity.GetId();
    physics_node.node          = node.GetId();
    physics_node.world_extents = node_size_in_world;
    physics_node.polygonId     = polygonId;
    physics_node.shape         = (unsigned)body->GetCollisionShape();

    mNodes[node.GetId()]   = physics_node;
    mFixtures[fixture_ptr] = node.GetId();
    DEBUG("Created new physics body '%1'", debug_name);
}

} // namespace
