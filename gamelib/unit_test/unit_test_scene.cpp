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
#include "gamelib/scene.h"


bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

void unit_test_scene_node()
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

    game::SceneNodeClass node;
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
        auto ret = game::SceneNodeClass::FromJson(node.ToJson());
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
        TEST_REQUIRE(copy.GetClassId() == node.GetClassId());
        copy = node;
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetClassId() == node.GetClassId());
    }

    // test clone
    {
        auto clone = node.Clone();
        TEST_REQUIRE(clone.GetHash() != node.GetHash());
        TEST_REQUIRE(clone.GetClassId() != node.GetClassId());
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
        game::SceneNode instance(node);
        instance.SetName("foobar");
        TEST_REQUIRE(instance.GetInstanceId()   != node.GetClassId());
        TEST_REQUIRE(instance.GetInstanceName() == "foobar");
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

        instance.SetSize(glm::vec2(200.0f, 200.0f));
        instance.SetTranslation(glm::vec2(350.0f, -350.0f));
        instance.SetScale(glm::vec2(1.0f, 1.0f));
        instance.SetRotation(2.5f);
        TEST_REQUIRE(instance.GetSize()        == glm::vec2(200.0f, 200.0f));
        TEST_REQUIRE(instance.GetTranslation() == glm::vec2(350.0f, -350.0f));
        TEST_REQUIRE(instance.GetScale()       == glm::vec2(1.0f, 1.0f));
        TEST_REQUIRE(instance.GetRotation()    == real::float32(2.5f));
    }
}

void unit_test_scene()
{
    game::SceneClass scene;
    {
        game::SceneNodeClass node;
        node.SetName("root");
        scene.AddNode(std::move(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_1");
        scene.AddNode(std::move(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        scene.AddNode(std::move(node));
    }

    TEST_REQUIRE(scene.GetNumNodes() == 3);
    TEST_REQUIRE(scene.GetNode(0).GetName() == "root");
    TEST_REQUIRE(scene.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(scene.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(scene.FindNodeByName("root"));
    TEST_REQUIRE(scene.FindNodeByName("child_1"));
    TEST_REQUIRE(scene.FindNodeByName("child_2"));
    TEST_REQUIRE(scene.FindNodeByName("foobar") == nullptr);
    TEST_REQUIRE(scene.FindNodeById(scene.GetNode(0).GetClassId()));
    TEST_REQUIRE(scene.FindNodeById(scene.GetNode(1).GetClassId()));
    TEST_REQUIRE(scene.FindNodeById("asg") == nullptr);

    // serialization
    {
        auto ret = game::SceneClass::FromJson(scene.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNumNodes() == 3);
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->GetId()   == scene.GetId());
        TEST_REQUIRE(ret->GetHash() == scene.GetHash());
    }

    // copy construction and assignment
    {
        auto copy(scene);
        TEST_REQUIRE(copy.GetNumNodes() == 3);
        TEST_REQUIRE(copy.GetNode(0).GetName() == "root");
        TEST_REQUIRE(copy.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(copy.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(copy.GetId()   == scene.GetId());
        TEST_REQUIRE(copy.GetHash() == scene.GetHash());

        copy = scene;
        TEST_REQUIRE(copy.GetNumNodes() == 3);
        TEST_REQUIRE(copy.GetNode(0).GetName() == "root");
        TEST_REQUIRE(copy.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(copy.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(copy.GetId()   == scene.GetId());
        TEST_REQUIRE(copy.GetHash() == scene.GetHash());
    }

    // clone
    {
        auto clone(scene.Clone());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(clone.GetId()   != scene.GetId());
        TEST_REQUIRE(clone.GetHash() != scene.GetHash());
    }

    // instance
    {
        game::Scene instance(scene);
        TEST_REQUIRE(instance.GetNumNodes() == 3);
        TEST_REQUIRE(instance.FindNodeByClassName("root"));
        TEST_REQUIRE(instance.FindNodeByClassId(scene.GetNode(0).GetClassId()));

        game::SceneNodeClass klass;
        klass.SetName("keke");
        game::SceneNode node(klass);
        node.SetName("my node");
        instance.AddNode(node);
        TEST_REQUIRE(instance.FindNodeByInstanceName("my node"));
        TEST_REQUIRE(instance.FindNodeByInstanceId(node.GetInstanceId()));
        TEST_REQUIRE(instance.DeleteNodeByInstanceId(node.GetInstanceId()));
        TEST_REQUIRE(instance.FindNodeByInstanceName("my node") == nullptr);
        TEST_REQUIRE(instance.FindNodeByInstanceId(node.GetInstanceId()) == nullptr);
    }

    scene.DeleteNode(0);
    TEST_REQUIRE(scene.GetNumNodes() == 2);
    TEST_REQUIRE(scene.DeleteNodeByName("child_1"));
    TEST_REQUIRE(scene.GetNumNodes() == 1);
    TEST_REQUIRE(scene.DeleteNodeById(scene.GetNode(0).GetClassId()));
    TEST_REQUIRE(scene.GetNumNodes() == 0);
}

void unit_test_scene_render_tree()
{
    game::SceneClass scene_class;

    // todo: rotational component in transform
    // todo: scaling component in transform

    {
        game::SceneNodeClass node_class;
        node_class.SetName("parent");
        node_class.SetTranslation(glm::vec2(10.0f, 10.0f));
        node_class.SetSize(glm::vec2(10.0f, 10.0f));
        node_class.SetRotation(0.0f);
        node_class.SetScale(glm::vec2(1.0f, 1.0f));
        auto* ret = scene_class.AddNode(node_class);
        auto& tree = scene_class.GetRenderTree();
        tree.AppendChild(ret);
    }

    // transforms relative to the parent
    {
        game::SceneNodeClass node_class;
        node_class.SetName("child");
        node_class.SetTranslation(glm::vec2(10.0f, 10.0f));
        node_class.SetSize(glm::vec2(2.0f, 2.0f));
        node_class.SetRotation(0.0f);
        node_class.SetScale(glm::vec2(1.0f, 1.0f));
        auto* ret = scene_class.AddNode(node_class);
        auto& tree = scene_class.GetRenderTree();
        tree.GetChildNode(0).AppendChild(ret);
    }

    // remember, the shape is aligned around the position

    // hit testing
    {
        std::vector<game::SceneNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        scene_class.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());
        TEST_REQUIRE(hitpos.empty());

        scene_class.CoarseHitTest(6.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "parent");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));

        hits.clear();
        hitpos.clear();
        scene_class.CoarseHitTest(20.0f, 20.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "child");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));
    }

    // whole bounding box.
    {
        const auto& box = scene_class.GetBoundingRect();
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(16.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(16.0f, box.GetHeight()));
    }

    // node bounding box
    {
        const auto* node = scene_class.FindNodeByName("parent");
        const auto& box = scene_class.GetBoundingRect(node);
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(10.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, box.GetHeight()));
    }
    // node bounding box
    {
        const auto* node = scene_class.FindNodeByName("child");
        const auto box = scene_class.GetBoundingRect(node);
        TEST_REQUIRE(math::equals(19.0f, box.GetX()));
        TEST_REQUIRE(math::equals(19.0f, box.GetY()));
        TEST_REQUIRE(math::equals(2.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(2.0f, box.GetHeight()));
    }

    // coordinate mapping
    {
        const auto* node = scene_class.FindNodeByName("child");
        auto vec = scene_class.MapCoordsFromNode(1.0f, 1.0f, node);
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = scene_class.MapCoordsToNode(20.0f, 20.0f, node);
        TEST_REQUIRE(math::equals(1.0f, vec.x));
        TEST_REQUIRE(math::equals(1.0f, vec.y));
    }

    // copying preserves the render tree.
    {
        game::SceneClass klass;
        game::SceneNodeClass parent;
        game::SceneNodeClass child0;
        game::SceneNodeClass child1;
        game::SceneNodeClass child2;
        parent.SetName("parent");
        child0.SetName("child0");
        child1.SetName("child1");
        child2.SetName("child2");
        auto* p  = klass.AddNode(parent);
        auto* c0 = klass.AddNode(child0);
        auto* c1 = klass.AddNode(child1);
        auto* c2 = klass.AddNode(child2);
        std::string names;
        {
            auto& tree = klass.GetRenderTree();
            auto& root = tree.AppendChild();
            root.SetValue(p);
            tree.GetChildNode(0).AppendChild(c0);
            tree.GetChildNode(0).AppendChild(c1);
            tree.GetChildNode(0).GetChildNode(0).AppendChild(c2);
            tree.PreOrderTraverseForEach([&names](const auto* node) {
                if (node)
                {
                    names.append(node->GetName());
                    names.append(" ");
                }
            });
        }
        std::cout << names << std::endl;
        TEST_REQUIRE(names == "parent child0 child2 child1 ");

        std::string test;
        auto copy(klass);
        copy.GetRenderTree().PreOrderTraverseForEach([&test](const auto* node) {
            if (node) {
                test.append(node->GetName());
                test.append(" ");
            }
        });
        TEST_REQUIRE(test == names);

        test.clear();
        copy = klass;
        copy.GetRenderTree().PreOrderTraverseForEach([&test](const auto* node) {
            if (node) {
                test.append(node->GetName());
                test.append(" ");
            }
        });
        TEST_REQUIRE(test == names);

        test.clear();
        copy = klass.Clone();
        copy.GetRenderTree().PreOrderTraverseForEach([&test](const auto* node) {
            if (node) {
                test.append(node->GetName());
                test.append(" ");
            }
        });
        TEST_REQUIRE(test == names);
    }

    // instance has the same render tree.
    {
        game::SceneClass klass;
        game::SceneNodeClass parent;
        game::SceneNodeClass child0;
        game::SceneNodeClass child1;
        game::SceneNodeClass child2;
        parent.SetName("parent");
        child0.SetName("child0");
        child1.SetName("child1");
        child2.SetName("child2");
        auto* p  = klass.AddNode(parent);
        auto* c0 = klass.AddNode(child0);
        auto* c1 = klass.AddNode(child1);
        auto* c2 = klass.AddNode(child2);
        std::string names;
        {
            auto& tree = klass.GetRenderTree();
            auto& root = tree.AppendChild();
            root.SetValue(p);
            tree.GetChildNode(0).AppendChild(c0);
            tree.GetChildNode(0).AppendChild(c1);
            tree.GetChildNode(0).GetChildNode(0).AppendChild(c2);
            tree.PreOrderTraverseForEach([&names](const auto* node) {
                if (node)
                {
                    names.append(node->GetName());
                    names.append(" ");
                }
            });
        }
        std::cout << names << std::endl;
        TEST_REQUIRE(names == "parent child0 child2 child1 ");

        std::string test;
        game::Scene instance(klass);
        instance.GetRenderTree().PreOrderTraverseForEach([&test](const auto* node) {
            if (node) {
                test.append(node->GetClassName());
                test.append(" ");
            }
        });
        TEST_REQUIRE(test == names);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_scene_node();
    unit_test_scene();
    unit_test_scene_render_tree();
    return 0;
}
