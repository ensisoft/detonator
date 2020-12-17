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

#include "base/logging.h"
#include "graphics/transform.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "gamelib/physics.h"
#include "gamelib/scene.h"

namespace game
{

PhysicsEngine::PhysicsEngine(const ClassLibrary* loader)
  : mLoader(loader)
{}

void PhysicsEngine::UpdateScene(Scene& scene)
{
    for (auto it = mNodes.begin(); it != mNodes.end(); )
    {
        const auto& physics_node = it->second;
        auto* node = scene.FindNodeByInstanceId(physics_node.instance);
        if (node == nullptr) {
            it = mNodes.erase(it);
            continue;
        }
        const b2Vec2& pos = physics_node.world_body->GetPosition();
        const float angle = physics_node.world_body->GetAngle();
        node->SetTranslation(glm::vec2(pos.x, pos.y));
        node->SetRotation(angle);
        ++it;
    }
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

void PhysicsEngine::BuildPhysicsWorldFromScene(const Scene& scene)
{
    const b2Vec2 gravity(mGravity.x, mGravity.y);

    mNodes.clear();
    mWorld.reset();
    mWorld = std::make_unique<b2World>(gravity);

    using RenderTree = Scene::RenderTree;

    class Visitor : public RenderTree::ConstVisitor
    {
    public:
        Visitor(PhysicsEngine& engine) : mEngine(engine)
        {
            mTransform.Scale(mEngine.mScale);
        }
        virtual void EnterNode(const SceneNode* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());

            auto it = mEngine.mNodes.find(node->GetInstanceId());
            if (it != mEngine.mNodes.end())
                return;
            const auto* body = node->GetRigidBody();
            if (!body)
                return;

            mTransform.Push(node->GetModelTransform());

            FBox box;
            box.Transform(mTransform.GetAsMatrix());

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

            body_def.angle    = box.GetRotation();
            body_def.position.Set(node_pos_in_world.x, node_pos_in_world.y);
            b2Body* world_body = mEngine.mWorld->CreateBody(&body_def);

            // collision shape used for collision resolver for the body.
            b2PolygonShape collision_shape;
            collision_shape.SetAsBox(node_size_in_world.x * 0.5, node_size_in_world.y * 0.5);

            // fixture attaches a collision shape to the body.
            b2FixtureDef fixture;
            fixture.shape    = &collision_shape;
            fixture.density  = 1.0f;
            fixture.friction = 0.3;
            world_body->CreateFixture(&fixture);

            PhysicsNode physics_node;
            physics_node.world_body = world_body;
            physics_node.instance   = node->GetInstanceId();
            physics_node.world_extents = node_size_in_world;
            mEngine.mNodes[node->GetInstanceId()] = physics_node;

            mTransform.Pop();

            DEBUG("Created new physics body for scene node '%1' ('%2')",
                  node->GetInstanceId(), node->GetInstanceName());
        }
        virtual void LeaveNode(const SceneNode* node) override
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

#if defined(GAMELIB_ENABLE_PHYSICS_TEST)
void PhysicsEngine::DebugDrawObjects(gfx::Painter& painter, gfx::Transform& view)
{
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
}
#endif // GAMELIB_ENABLE_PHYSICS_TEST

} // namespace
