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
#include "gamelib/scene.h"
#include "gamelib/entity.h"

bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

// build easily comparable representation of the render tree
// by concatenating node names into a string in the order
// of traversal.
std::string WalkTree(const game::SceneClass& scene)
{
    std::string names;
    const auto& tree = scene.GetRenderTree();
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

std::string WalkTree(const game::Scene& scene)
{
    std::string names;
    const auto& tree = scene.GetRenderTree();
    tree.PreOrderTraverseForEach([&names](const auto* entity)  {
        if (entity) {
            names.append(entity->GetInstanceName());
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}

void unit_test_node()
{
    game::SceneNodeClass node;
    node.SetName("root");
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetEntityId("entity");

    // to/from json
    {
        auto ret = game::SceneNodeClass::FromJson(node.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName()         == "root");
        TEST_REQUIRE(ret->GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret->GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret->GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret->GetEntityId()     == "entity");
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
        TEST_REQUIRE(clone.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(clone.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(clone.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(clone.GetEntityId()     == "entity");
    }
}

void unit_test_scene_class()
{
    // make a small entity for testing.
    auto entity = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass node;
        node.SetName("node");
        node.SetSize(glm::vec2(20.0f, 20.0f));
        entity->LinkChild(nullptr, entity->AddNode(std::move(node)));
    }

    // build-up a test scene with some scene nodes.
    game::SceneClass klass;
    TEST_REQUIRE(klass.GetNumNodes() == 0);

    {
        game::SceneNodeClass node;
        node.SetName("root");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(0.0f, 0.0f));
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_1");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(100.0f, 100.0f));
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        node.SetEntity(entity);
        node.SetTranslation(glm::vec2(200.0f, 200.0f));
        klass.AddNode(node);
    }
    TEST_REQUIRE(klass.GetNumNodes() == 3);
    TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
    TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(klass.FindNodeByName("root"));
    TEST_REQUIRE(klass.FindNodeById(klass.GetNode(0).GetClassId()));
    TEST_REQUIRE(klass.FindNodeById("asgas") == nullptr);
    TEST_REQUIRE(klass.FindNodeByName("foasg") == nullptr);

    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2");

    // to/from json
    {
        auto ret = game::SceneClass::FromJson(klass.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->FindNodeByName("root"));
        TEST_REQUIRE(ret->FindNodeById(klass.GetNode(0).GetClassId()));
        TEST_REQUIRE(ret->FindNodeById("asgas") == nullptr);
        TEST_REQUIRE(ret->FindNodeByName("foasg") == nullptr);
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(WalkTree(*ret) == "root child_1 child_2");
    }

    // test copy and copy ctor
    {
        auto copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");
        copy = klass;
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");
    }

    // test clone
    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone.GetHash() != klass.GetHash());
        TEST_REQUIRE(clone.GetId() != klass.GetId());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");
    }

    // test breaking node away from the render tree.
    {
        klass.BreakChild(klass.FindNodeByName("child_1"));
        klass.BreakChild(klass.FindNodeByName("root"));
        TEST_REQUIRE(klass.GetNumNodes() == 3);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(WalkTree(klass) == "");

        klass.LinkChild(nullptr, klass.FindNodeByName("root"));
        klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
        klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2");

    }

    // test duplicate node
    {
        klass.DuplicateNode(klass.FindNodeByName("child_2"));
        TEST_REQUIRE(klass.GetNumNodes() == 4);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(klass.GetNode(3).GetName() == "Copy of child_2");
        klass.GetNode(3).SetName("child_3");
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2 child_3");
        klass.ReparentChild(klass.FindNodeByName("child_1"), klass.FindNodeByName("child_3"));
        TEST_REQUIRE(WalkTree(klass) == "root child_1 child_3 child_2");
    }

    // test bounding box
    {
        // todo:
    }

    // test hit testing
    {
        std::vector<game::SceneNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        klass.CoarseHitTest(50.0f, 50.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());

        klass.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(!hits.empty());
        TEST_REQUIRE(hits[0]->GetName() == "root");

        hits.clear();
        hitpos.clear();
        klass.CoarseHitTest(100.0f, 100.0f, &hits, &hitpos);
        TEST_REQUIRE(!hits.empty());
        TEST_REQUIRE(hits[0]->GetName() == "child_1");
    }

    // test coordinate mapping
    {
        const auto* node = klass.FindNodeByName("child_1");
        auto vec = klass.MapCoordsFromNode(0.0f, 0.0f, node);
        TEST_REQUIRE(math::equals(100.0f, vec.x));
        TEST_REQUIRE(math::equals(100.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = klass.MapCoordsToNode(100.0f, 100.0f, node);
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
    }

    // test delete node
    {
        klass.DeleteNode(klass.FindNodeByName(("child_3")));
        TEST_REQUIRE(klass.GetNumNodes() == 3);
        klass.DeleteNode(klass.FindNodeByName("child_1"));
        TEST_REQUIRE(klass.GetNumNodes() == 2);
        TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
        TEST_REQUIRE(klass.GetNode(1).GetName() == "child_2");
    }
}

void unit_test_scene_instance()
{
    auto entity = std::make_shared<game::EntityClass>();

    game::SceneClass klass;
    TEST_REQUIRE(klass.GetNumNodes() == 0);
    {
        game::SceneNodeClass node;
        node.SetName("root");
        node.SetEntity(entity);
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_1");
        node.SetEntity(entity);
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        node.SetEntity(entity);
        klass.AddNode(node);
    }
    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));


    // the scene instance has the initial state based on the scene class object.
    // i.e. the initial entities are created based on the scene class nodes and
    // their properties.
    game::Scene instance(klass);
    TEST_REQUIRE(instance.GetNumEntities() == 3);
    TEST_REQUIRE(instance.GetEntity(0).GetInstanceName() == "root");
    TEST_REQUIRE(instance.GetEntity(1).GetInstanceName() == "child_1");
    TEST_REQUIRE(instance.GetEntity(2).GetInstanceName() == "child_2");
    TEST_REQUIRE(instance.GetEntity(0).GetInstanceId() == klass.GetNode(0).GetClassId());
    TEST_REQUIRE(instance.GetEntity(1).GetInstanceId() == klass.GetNode(1).GetClassId());
    TEST_REQUIRE(instance.GetEntity(2).GetInstanceId() == klass.GetNode(2).GetClassId());
    TEST_REQUIRE(instance.FindEntityByInstanceName("root"));
    TEST_REQUIRE(instance.FindEntityByInstanceName("blaal") == nullptr);
    TEST_REQUIRE(instance.FindEntityByInstanceId(klass.GetNode(0).GetClassId()));
    TEST_REQUIRE(instance.FindEntityByInstanceId("asegsa") == nullptr);
    TEST_REQUIRE(WalkTree(instance) == "root child_1 child_2");
}

int test_main(int argc, char* argv[])
{
    unit_test_node();
    unit_test_scene_class();
    unit_test_scene_instance();
    return 0;
}