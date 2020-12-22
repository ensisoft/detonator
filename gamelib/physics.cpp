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
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "gamelib/physics.h"
#include "gamelib/loader.h"
#include "gamelib/types.h"
#include "gamelib/entity.h"

namespace game
{

PhysicsEngine::PhysicsEngine(const ClassLibrary* loader)
  : mLoader(loader)
{}

void PhysicsEngine::UpdateScene(Entity& scene)
{
    using RenderTree = Entity::RenderTree;

    class Visitor : public RenderTree::Visitor
    {
    public:
        Visitor(PhysicsEngine& engine) : mEngine(engine)
        {
            mTransform.Scale(glm::vec2(1.0f, 1.0f) / mEngine.mScale);
        }
        virtual void EnterNode(EntityNode* node) override
        {
            if (!node)
                return;

            const auto node_to_world = mTransform.GetAsMatrix();

            mTransform.Push(node->GetNodeTransform());

            auto it = mEngine.mNodes.find(node->GetInstanceId());
            if (it == mEngine.mNodes.end())
                return;

            if (!node->HasRigidBody())
            {
                mEngine.mNodes.erase(it);
                return;
            }

            const auto& physics_node = it->second;
            const auto physics_world_pos   = physics_node.world_body->GetPosition();
            const auto physics_world_angle = physics_node.world_body->GetAngle();

            glm::mat4 mat;
            gfx::Transform transform;
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
        }
        virtual void LeaveNode(EntityNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        PhysicsEngine& mEngine;
        gfx::Transform mTransform;
    };

    Visitor visitor(*this);

    auto& tree = scene.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

void PhysicsEngine::Tick()
{
    mWorld->Step(mTimestep, mNumVelocityIterations, mNumPositionIterations);
}

void PhysicsEngine::DeleteAll()
{
    for (auto& node : mNodes)
    {
        mWorld->DestroyBody(node.second.world_body);
    }
    mNodes.clear();
}

void PhysicsEngine::DeleteBody(const std::string& id)
{
    auto it = mNodes.find(id);
    if (it == mNodes.end())
        return;
    auto& node = it->second;
    mWorld->DestroyBody(node.world_body);
    mNodes.erase(it);
}

void PhysicsEngine::BuildPhysicsWorldFromScene(const Entity& scene)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mNodes.clear();
    mWorld.reset();
    mWorld = std::make_unique<b2World>(gravity);

    using RenderTree = Entity::RenderTree;

    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(PhysicsEngine& engine) : mEngine(engine)
        {
            mTransform.Scale(glm::vec2(1.0f, 1.0f) / mEngine.mScale);
        }
        virtual void EnterNode(const EntityNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());

            auto it = mEngine.mNodes.find(node->GetInstanceId());
            if (it != mEngine.mNodes.end())
                return;
            if (!node->HasRigidBody())
                return;

            mTransform.Push(node->GetModelTransform());
            mEngine.AddPhysicsNode(mTransform.GetAsMatrix(), *node);
            mTransform.Pop();
        }
        virtual void LeaveNode(const EntityNode* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        PhysicsEngine& mEngine;
        gfx::Transform mTransform;
    };
    Visitor visitor(*this);

    const auto& tree = scene.GetRenderTree();
    tree.PreOrderTraverse(visitor);
}

#if defined(GAMESTUDIO_ENABLE_PHYSICS_DEBUG)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view)
{
    view.Push();
    view.Scale(mScale);

    // todo: use the box2d debug draw interface.
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
                painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), view, gfx::SolidColor(gfx::Color::HotPink));
            view.Pop();
        view.Pop();
    }

    view.Pop();
}
#endif // GAMESTUDIO_ENABLE_PHYSICS_DEBUG

void PhysicsEngine::AddPhysicsNode(const glm::mat4& model_to_world, const EntityNode& node)
{
    const FBox box(model_to_world);
    const auto* body = node.GetRigidBody();
    const auto& node_pos_in_world   = box.GetPosition();
    const auto& node_size_in_world  = box.GetSize();
    const auto& rigid_body_class    = body->GetClass();

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
    b2Body* world_body = mWorld->CreateBody(&body_def);

    // collision shape used for collision resolver for the body.
    std::unique_ptr<b2Shape> collision_shape;
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
    else if (body->GetCollisionShape() == RigidBodyItemClass::CollisionShape::Polygon)
    {
        const auto& polygonid = body->GetPolygonShapeId();
        if (polygonid.empty()) {
            WARN("Rigid body for node '%1' ('%2') has no polygon shape id set.", node.GetInstanceId(), node.GetInstanceName());
            return;
        }
        const auto& drawable = mLoader->FindDrawableClass(polygonid);
        if (!drawable || drawable->GetType() != gfx::DrawableClass::Type::Polygon) {
            WARN("No polygon class found for node '%1' ('%2')", node.GetInstanceId(), node.GetInstanceName());
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
        // todo: deal with situation when we have more than b2_maxPolygonVertices (8?)
        std::reverse(verts.begin(), verts.end());
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
    world_body->CreateFixture(&fixture);

    PhysicsNode physics_node;
    physics_node.world_body = world_body;
    physics_node.instance   = node.GetInstanceId();
    physics_node.world_extents = node_size_in_world;
    mNodes[node.GetInstanceId()] = physics_node;

    DEBUG("Created new physics body for scene node '%1' ('%2')",
          node.GetInstanceId(), node.GetInstanceName());
}

} // namespace
