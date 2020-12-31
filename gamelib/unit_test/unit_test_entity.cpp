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

#include <string>
#include <cstddef>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/assert.h"
#include "base/math.h"
#include "graphics/color4f.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "gamelib/entity.h"


bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

// build easily comparable representation of the render tree
// by concatenating node names into a string in the order
// of traversal.
std::string WalkTree(const game::EntityClass& entity)
{
    std::string names;
    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* node)  {
        if (node) {
            names.append(node->GetName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

std::string WalkTree(const game::Entity& entity)
{
    std::string names;
    const auto& tree = entity.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* node)  {
        if (node) {
            names.append(node->GetName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

void unit_test_entity_node()
{
    game::DrawableItemClass draw;
    draw.SetDrawableId("rectangle");
    draw.SetMaterialId("test");
    draw.SetRenderPass(game::DrawableItemClass::RenderPass::Mask);
    draw.SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, true);
    draw.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);
    draw.SetLayer(10);
    draw.SetLineWidth(5.0f);

    game::RigidBodyItemClass body;
    body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
    body.SetSimulation(game::RigidBodyItemClass::Simulation::Dynamic);
    body.SetFlag(game::RigidBodyItemClass::Flags::Bullet, true);
    body.SetFriction(2.0f);
    body.SetRestitution(3.0f);
    body.SetAngularDamping(4.0f);
    body.SetLinearDamping(5.0f);
    body.SetDensity(-1.0f);
    body.SetPolygonShapeId("shape");

    game::EntityNodeClass node;
    node.SetName("root");
    node.SetSize(glm::vec2(100.0f, 100.0f));
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetDrawable(draw);
    node.SetRigidBody(body);

    TEST_REQUIRE(node.HasDrawable());
    TEST_REQUIRE(node.HasRigidBody());
    TEST_REQUIRE(node.GetName()         == "root");
    TEST_REQUIRE(node.GetSize()         == glm::vec2(100.0f, 100.0f));
    TEST_REQUIRE(node.GetTranslation()  == glm::vec2(150.0f, -150.0f));
    TEST_REQUIRE(node.GetScale()        == glm::vec2(4.0f, 5.0f));
    TEST_REQUIRE(node.GetRotation()     == real::float32(1.5f));
    TEST_REQUIRE(node.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
    TEST_REQUIRE(node.GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
    TEST_REQUIRE(node.GetDrawable()->GetLayer()        == 10);
    TEST_REQUIRE(node.GetDrawable()->GetDrawableId()   == "rectangle");
    TEST_REQUIRE(node.GetDrawable()->GetMaterialId()   == "test");
    TEST_REQUIRE(node.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
    TEST_REQUIRE(node.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
    TEST_REQUIRE(node.GetRigidBody()->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Circle);
    TEST_REQUIRE(node.GetRigidBody()->GetSimulation() == game::RigidBodyItemClass::Simulation::Dynamic);
    TEST_REQUIRE(node.GetRigidBody()->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
    TEST_REQUIRE(node.GetRigidBody()->GetFriction()       == real::float32(2.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetRestitution()    == real::float32(3.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetAngularDamping() == real::float32(4.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetLinearDamping()  == real::float32(5.0f));
    TEST_REQUIRE(node.GetRigidBody()->GetDensity()        == real::float32(-1.0));
    TEST_REQUIRE(node.GetRigidBody()->GetPolygonShapeId() == "shape");

    // to/from json
    {
        auto ret = game::EntityNodeClass::FromJson(node.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->HasDrawable());
        TEST_REQUIRE(ret->HasRigidBody());
        TEST_REQUIRE(ret->GetName()         == "root");
        TEST_REQUIRE(ret->GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(ret->GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret->GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret->GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret->GetDrawable()->GetDrawableId()   == "rectangle");
        TEST_REQUIRE(ret->GetDrawable()->GetMaterialId()   == "test");
        TEST_REQUIRE(ret->GetDrawable()->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(ret->GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(ret->GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(ret->GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
        TEST_REQUIRE(node.GetRigidBody()->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Circle);
        TEST_REQUIRE(node.GetRigidBody()->GetSimulation() == game::RigidBodyItemClass::Simulation::Dynamic);
        TEST_REQUIRE(node.GetRigidBody()->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
        TEST_REQUIRE(node.GetRigidBody()->GetFriction()       == real::float32(2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetRestitution()    == real::float32(3.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularDamping() == real::float32(4.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetLinearDamping()  == real::float32(5.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetDensity()        == real::float32(-1.0));
        TEST_REQUIRE(node.GetRigidBody()->GetPolygonShapeId() == "shape");
        TEST_REQUIRE(ret->GetHash() == node.GetHash());
    }

    // test copy and copy ctor
    {
        auto copy(node);
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
        copy = node;
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
    }

    // test clone
    {
        auto clone = node.Clone();
        TEST_REQUIRE(clone.GetHash() != node.GetHash());
        TEST_REQUIRE(clone.GetId() != node.GetId());
        TEST_REQUIRE(clone.GetName()         == "root");
        TEST_REQUIRE(clone.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(clone.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(clone.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(clone.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(clone.GetDrawable()->GetDrawableId()   == "rectangle");
        TEST_REQUIRE(clone.GetDrawable()->GetMaterialId()   == "test");
        TEST_REQUIRE(clone.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(clone.GetDrawable()->GetRenderPass()   == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
    }

    // test instance state.
    {
        // check initial state.
        game::EntityNode instance(node);
        TEST_REQUIRE(instance.GetId() != node.GetId());
        TEST_REQUIRE(instance.GetName()         == "root");
        TEST_REQUIRE(instance.GetClassName()    == "root");
        TEST_REQUIRE(instance.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(instance.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(instance.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(instance.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(instance.HasRigidBody());
        TEST_REQUIRE(instance.HasDrawable());
        TEST_REQUIRE(instance.GetDrawable()->GetLineWidth()   == real::float32(5.0f));
        TEST_REQUIRE(instance.GetDrawable()->GetRenderPass()  == game::DrawableItemClass::RenderPass::Mask);
        TEST_REQUIRE(instance.GetRigidBody()->GetPolygonShapeId() == "shape");

        instance.SetName("foobar");
        instance.SetSize(glm::vec2(200.0f, 200.0f));
        instance.SetTranslation(glm::vec2(350.0f, -350.0f));
        instance.SetScale(glm::vec2(1.0f, 1.0f));
        instance.SetRotation(2.5f);
        TEST_REQUIRE(instance.GetName()        == "foobar");
        TEST_REQUIRE(instance.GetSize()        == glm::vec2(200.0f, 200.0f));
        TEST_REQUIRE(instance.GetTranslation() == glm::vec2(350.0f, -350.0f));
        TEST_REQUIRE(instance.GetScale()       == glm::vec2(1.0f, 1.0f));
        TEST_REQUIRE(instance.GetRotation()    == real::float32(2.5f));
    }
}

void unit_test_entity_class()
{
    game::EntityClass entity;
    {
        game::EntityNodeClass node;
        node.SetName("root");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_1");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_2");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.AddNode(std::move(node));
    }

    {
        game::AnimationTrackClass track;
        track.SetName("test1");
        entity.AddAnimationTrack(track);
    }
    {
        game::AnimationTrackClass track;
        track.SetName("test2");
        entity.AddAnimationTrack(track);
    }

    TEST_REQUIRE(entity.GetNumNodes() == 3);
    TEST_REQUIRE(entity.GetNode(0).GetName() == "root");
    TEST_REQUIRE(entity.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(entity.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(entity.FindNodeByName("root"));
    TEST_REQUIRE(entity.FindNodeByName("child_1"));
    TEST_REQUIRE(entity.FindNodeByName("child_2"));
    TEST_REQUIRE(entity.FindNodeByName("foobar") == nullptr);
    TEST_REQUIRE(entity.FindNodeById(entity.GetNode(0).GetId()));
    TEST_REQUIRE(entity.FindNodeById(entity.GetNode(1).GetId()));
    TEST_REQUIRE(entity.FindNodeById("asg") == nullptr);
    TEST_REQUIRE(entity.GetNumTracks() == 2);
    TEST_REQUIRE(entity.FindAnimationTrackByName("test1"));
    TEST_REQUIRE(entity.FindAnimationTrackByName("sdgasg") == nullptr);

    // test linking.
    entity.LinkChild(nullptr, entity.FindNodeByName("root"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_1"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(entity) == "root child_1 child_2");

    // serialization
    {
        auto ret = game::EntityClass::FromJson(entity.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNumNodes() == 3);
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->GetId() == entity.GetId());
        TEST_REQUIRE(ret->GetHash() == entity.GetHash());
        TEST_REQUIRE(ret->GetNumTracks() == 2);
        TEST_REQUIRE(ret->FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(*ret) == "root child_1 child_2");
    }

    // copy construction and assignment
    {
        auto copy(entity);
        TEST_REQUIRE(copy.GetId() == entity.GetId());
        TEST_REQUIRE(copy.GetHash() == entity.GetHash());
        TEST_REQUIRE(copy.GetNumTracks() == 2);
        TEST_REQUIRE(copy.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");

        copy = entity;
        TEST_REQUIRE(copy.GetId() == entity.GetId());
        TEST_REQUIRE(copy.GetHash() == entity.GetHash());
        TEST_REQUIRE(copy.GetNumTracks() == 2);
        TEST_REQUIRE(copy.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");
    }

    // clone
    {
        auto clone(entity.Clone());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(clone.GetId() != entity.GetId());
        TEST_REQUIRE(clone.GetHash() != entity.GetHash());
        TEST_REQUIRE(clone.GetNumTracks() == 2);
        TEST_REQUIRE(clone.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");
    }

    // remember, the shape is aligned around the position

    // hit testing
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());
        TEST_REQUIRE(hitpos.empty());

        entity.CoarseHitTest(6.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "root");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));

        hits.clear();
        hitpos.clear();
        entity.CoarseHitTest(20.0f, 20.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "child_1");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));
    }

    // whole bounding box.
    {
        const auto& box = entity.GetBoundingRect();
        TEST_REQUIRE(math::equals(-11.0f, box.GetX()));
        TEST_REQUIRE(math::equals(-11.0f, box.GetY()));
        TEST_REQUIRE(math::equals(32.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(32.0f, box.GetHeight()));
    }


    // node bounding box
    {
        const auto* node = entity.FindNodeByName("root");
        const auto& box = entity.GetBoundingRect(node);
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(10.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, box.GetHeight()));
    }
    // node bounding box
    {
        const auto* node = entity.FindNodeByName("child_1");
        const auto box = entity.GetBoundingRect(node);
        TEST_REQUIRE(math::equals(19.0f, box.GetX()));
        TEST_REQUIRE(math::equals(19.0f, box.GetY()));
        TEST_REQUIRE(math::equals(2.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(2.0f, box.GetHeight()));
    }

    // coordinate mapping
    {
        const auto* node = entity.FindNodeByName("child_1");
        auto vec = entity.MapCoordsFromNode(1.0f, 1.0f, node);
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = entity.MapCoordsToNode(20.0f, 20.0f, node);
        TEST_REQUIRE(math::equals(1.0f, vec.x));
        TEST_REQUIRE(math::equals(1.0f, vec.y));
    }

    // test delete node
    {
        TEST_REQUIRE(entity.GetNumNodes() == 3);
        entity.DeleteNode(entity.FindNodeByName("child_2"));
        TEST_REQUIRE(entity.GetNumNodes() == 2);
        entity.DeleteNode(entity.FindNodeByName("root"));
        TEST_REQUIRE(entity.GetNumNodes() == 0);
    }
}


void unit_test_entity_instance()
{
    game::EntityClass klass;
    {
        game::EntityNodeClass node;
        node.SetName("root");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_1");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_2");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    {
        game::EntityNodeClass node;
        node.SetName("child_3");
        node.SetTranslation(glm::vec2(-20.0f, -20.0f));
        node.SetSize(glm::vec2(2.0f, 2.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        klass.AddNode(std::move(node));
    }
    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
    klass.LinkChild(klass.FindNodeByName("child_1"), klass.FindNodeByName("child_3"));
    TEST_REQUIRE(WalkTree(klass) == "root child_1 child_3 child_2");


    // create entity instance

    // test initial state.
    game::Entity instance(klass);
    TEST_REQUIRE(instance.GetNumNodes() == 4);
    TEST_REQUIRE(instance.GetNode(0).GetName() == "root");
    TEST_REQUIRE(instance.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(instance.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(instance.GetNode(3).GetName() == "child_3");
    TEST_REQUIRE(WalkTree(instance) == "root child_1 child_3 child_2");

    // todo: test more of the instance api
}

int test_main(int argc, char* argv[])
{
    unit_test_entity_node();
    unit_test_entity_class();
    unit_test_entity_instance();
    return 0;
}
