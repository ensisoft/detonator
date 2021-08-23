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

#include <string>
#include <cstddef>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "base/assert.h"
#include "base/math.h"
#include "data/json.h"
#include "engine/scene.h"
#include "engine/entity.h"

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
            names.append(entity->GetName());
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
        data::JsonObject json;
        node.IntoJson(json);
        auto ret = game::SceneNodeClass::FromJson(json);
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
    klass.SetName("my scene");
    klass.SetScriptFileId("script.lua");
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

    {
        game::ScriptVar foo("foo", 123, game::ScriptVar::ReadOnly);
        game::ScriptVar bar("bar", 1.0f, game::ScriptVar::ReadWrite);
        klass.AddScriptVar(foo);
        klass.AddScriptVar(std::move(bar));
    }

    TEST_REQUIRE(klass.GetNumNodes() == 3);
    TEST_REQUIRE(klass.GetNode(0).GetName() == "root");
    TEST_REQUIRE(klass.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(klass.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(klass.FindNodeByName("root"));
    TEST_REQUIRE(klass.FindNodeById(klass.GetNode(0).GetId()));
    TEST_REQUIRE(klass.FindNodeById("asgas") == nullptr);
    TEST_REQUIRE(klass.FindNodeByName("foasg") == nullptr);
    TEST_REQUIRE(klass.GetNumScriptVars() == 2);
    TEST_REQUIRE(klass.GetScriptVar(0).GetName() == "foo");
    TEST_REQUIRE(klass.GetScriptVar(1).GetName() == "bar");

    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(klass) == "root child_1 child_2");

    // to/from json
    {
        data::JsonObject json;
        klass.IntoJson(json);
        auto ret = game::SceneClass::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName() == "my scene");
        TEST_REQUIRE(ret->GetScriptFileId() == "script.lua");
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->FindNodeByName("root"));
        TEST_REQUIRE(ret->FindNodeById(klass.GetNode(0).GetId()));
        TEST_REQUIRE(ret->FindNodeById("asgas") == nullptr);
        TEST_REQUIRE(ret->FindNodeByName("foasg") == nullptr);
        TEST_REQUIRE(ret->GetHash() == klass.GetHash());
        TEST_REQUIRE(ret->GetScriptVar(0).GetName() == "foo");
        TEST_REQUIRE(ret->GetScriptVar(1).GetName() == "bar");
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
        TEST_REQUIRE(clone.GetName() == klass.GetName());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");
    }

    // test breaking node away from the render tree.
    {
        klass.BreakChild(klass.FindNodeByName("root"), false);
        klass.BreakChild(klass.FindNodeByName("child_1"), false);
        klass.BreakChild(klass.FindNodeByName("child_2"), false);
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
        auto vec = klass.MapCoordsFromNodeModel(0.0f , 0.0f , node);
        TEST_REQUIRE(math::equals(100.0f, vec.x));
        TEST_REQUIRE(math::equals(100.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = klass.MapCoordsToNodeModel(100.0f , 100.0f , node);
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

void unit_test_scene_instance_create()
{
    auto entity = std::make_shared<game::EntityClass>();
    entity->SetFlag(game::EntityClass::Flags::TickEntity, true);
    entity->SetFlag(game::EntityClass::Flags::UpdateEntity, false);
    entity->SetLifetime(5.0f);

    game::SceneClass klass;
    // set some entity nodes in the scene class.
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
        node.SetFlag(game::SceneNodeClass::Flags::TickEntity, false);
        node.SetFlag(game::SceneNodeClass::Flags::UpdateEntity, true);
        klass.AddNode(node);
    }
    {
        game::SceneNodeClass node;
        node.SetName("child_2");
        node.SetEntity(entity);
        node.SetLifetime(3.0f);
        klass.AddNode(node);
    }
    // link to the scene graph
    klass.LinkChild(nullptr, klass.FindNodeByName("root"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_1"));
    klass.LinkChild(klass.FindNodeByName("root"), klass.FindNodeByName("child_2"));

    // set class scripting variables.
    {
        game::ScriptVar foo("foo", 123, game::ScriptVar::ReadWrite);
        game::ScriptVar bar("bar", 1.0f, game::ScriptVar::ReadOnly);
        klass.AddScriptVar(foo);
        klass.AddScriptVar(std::move(bar));
    }


    // the scene instance has the initial state based on the scene class object.
    // i.e. the initial entities are created based on the scene class nodes and
    // their properties.
    game::Scene instance(klass);
    TEST_REQUIRE(instance.GetNumEntities() == 3);
    TEST_REQUIRE(instance.GetEntity(0).GetName() == "root");
    TEST_REQUIRE(instance.GetEntity(1).GetName() == "child_1");
    TEST_REQUIRE(instance.GetEntity(2).GetName() == "child_2");
    TEST_REQUIRE(instance.GetEntity(0).GetId() == klass.GetNode(0).GetId());
    TEST_REQUIRE(instance.GetEntity(1).GetId() == klass.GetNode(1).GetId());
    TEST_REQUIRE(instance.GetEntity(2).GetId() == klass.GetNode(2).GetId());
    TEST_REQUIRE(instance.FindEntityByInstanceName("root"));
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_1"));
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_2"));
    TEST_REQUIRE(instance.FindEntityByInstanceName("blaal") == nullptr);
    TEST_REQUIRE(instance.FindEntityByInstanceId(klass.GetNode(0).GetId()));
    TEST_REQUIRE(instance.FindEntityByInstanceId(klass.GetNode(1).GetId()));
    TEST_REQUIRE(instance.FindEntityByInstanceId(klass.GetNode(2).GetId()));
    TEST_REQUIRE(instance.FindEntityByInstanceId("asegsa") == nullptr);
    TEST_REQUIRE(WalkTree(instance) == "root child_1 child_2");
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_1")->TestFlag(game::EntityClass::Flags::TickEntity) == false);
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_1")->TestFlag(game::EntityClass::Flags::UpdateEntity) == true);
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_1")->GetLifetime() == real::float32(5.0f));
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_2")->TestFlag(game::EntityClass::Flags::TickEntity) == true);
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_2")->TestFlag(game::EntityClass::Flags::UpdateEntity) == false);
    TEST_REQUIRE(instance.FindEntityByInstanceName("child_2")->GetLifetime() == real::float32(3.0f));

    // the scene instance has the initial values of scripting variables based on the
    // values set in the scene class object.
    TEST_REQUIRE(instance.FindScriptVar("foo"));
    TEST_REQUIRE(instance.FindScriptVar("bar"));
    TEST_REQUIRE(instance.FindScriptVar("foo")->IsReadOnly() == false);
    TEST_REQUIRE(instance.FindScriptVar("bar")->IsReadOnly() == true);
    instance.FindScriptVar("foo")->SetValue(444);
    TEST_REQUIRE(instance.FindScriptVar("foo")->GetValue<int>() == 444);
}

void unit_test_scene_instance_spawn()
{
    auto entity = std::make_shared<game::EntityClass>();

    game::SceneClass klass;

    // basic spawn cycle
    {
        game::Scene scene(klass);
        scene.BeginLoop();
            game::EntityArgs args;
            args.klass = entity;
            args.name  = "foo";
            auto* ret = scene.SpawnEntity(args);
            TEST_REQUIRE(ret);
            TEST_REQUIRE(ret->GetName() == "foo");
            TEST_REQUIRE(ret->GetId() == args.id);
            TEST_REQUIRE(ret->HasBeenSpawned() == false);
            TEST_REQUIRE(scene.FindEntityByInstanceName("foo") == nullptr);
            TEST_REQUIRE(scene.FindEntityByInstanceId(args.id) == nullptr);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(ret->HasBeenSpawned());
            TEST_REQUIRE(scene.FindEntityByInstanceName("foo") == ret);
            TEST_REQUIRE(scene.FindEntityByInstanceId(args.id) == ret);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(ret->HasBeenSpawned() == false);
            TEST_REQUIRE(scene.FindEntityByInstanceName("foo") == ret);
            TEST_REQUIRE(scene.FindEntityByInstanceId(args.id) == ret);
        scene.EndLoop();
    }

    // spawn while iterating over the entities. typical usage case for example
    // lua integration code is looping over the entities in order to invoke
    // entity callbacks which might then call back into the scene to modify the
    // scene state. special care must be taken to make sure that this is well defined
    // behaviour.
    {
        game::Scene scene(klass);

        scene.BeginLoop();
            game::EntityArgs args;
            args.klass = entity;
            args.name  = "0";
            args.id    = "0";
            scene.SpawnEntity(args);
            args.klass = entity;
            args.name  = "1";
            args.id    = "1";
            scene.SpawnEntity(args);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 2);
            TEST_REQUIRE(scene.GetEntity(0).GetName() == "0");
            TEST_REQUIRE(scene.GetEntity(1).GetName() == "1");
            for (size_t i=0; i<scene.GetNumEntities(); ++i)
            {
                game::EntityArgs args;
                args.klass = entity;
                args.name  = std::to_string(2+i);
                args.id    = std::to_string(2+i);
                scene.SpawnEntity(args);
            }
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 4);
            TEST_REQUIRE(scene.GetEntity(0).GetName() == "0");
            TEST_REQUIRE(scene.GetEntity(1).GetName() == "1");
            TEST_REQUIRE(scene.GetEntity(2).GetName() == "2");
            TEST_REQUIRE(scene.GetEntity(3).GetName() == "3");
        scene.EndLoop();
    }
}

void unit_test_scene_instance_kill()
{
    auto entity = std::make_shared<game::EntityClass>();

    game::SceneClass klass;

    // basic kill
    {
        game::Scene scene(klass);
        scene.BeginLoop();
           game::EntityArgs args;
           args.klass = entity;
           args.name  = "foo";
           auto* ret = scene.SpawnEntity(args);
        scene.EndLoop();

        scene.BeginLoop();
            scene.KillEntity(ret);
            TEST_REQUIRE(ret->HasBeenKilled() == false);
            TEST_REQUIRE(scene.FindEntityByInstanceName("foo") == ret);
            TEST_REQUIRE(scene.FindEntityByInstanceId(args.id) == ret);
            TEST_REQUIRE(scene.GetNumEntities() == 1);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(ret->HasBeenKilled() == true);
            TEST_REQUIRE(scene.GetNumEntities() == 1);
            TEST_REQUIRE(scene.FindEntityByInstanceId(args.id) == ret);
            TEST_REQUIRE(scene.FindEntityByInstanceName("foo") == ret);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 0);
        scene.EndLoop();
    }

    // kill flag propagation to children
    {
        // todo: need the linking api first
    }

    // kill while iterating over the entities.
    {
        game::Scene scene(klass);

        scene.BeginLoop();
            game::EntityArgs args;
            args.klass = entity;
            args.name  = "0";
            args.id    = "0";
            scene.SpawnEntity(args);
            args.klass = entity;
            args.name  = "1";
            args.id    = "1";
            scene.SpawnEntity(args);
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 2);
            TEST_REQUIRE(scene.GetEntity(0).GetName() == "0");
            TEST_REQUIRE(scene.GetEntity(1).GetName() == "1");
            for (size_t i=0; i<scene.GetNumEntities(); ++i)
            {
                auto* ret = &scene.GetEntity(i);
                scene.KillEntity(ret);
            }
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 2);
            TEST_REQUIRE(scene.GetEntity(0).GetName() == "0");
            TEST_REQUIRE(scene.GetEntity(1).GetName() == "1");
            TEST_REQUIRE(scene.GetEntity(0).HasBeenKilled());
            TEST_REQUIRE(scene.GetEntity(1).HasBeenKilled());
            TEST_REQUIRE(scene.FindEntityByInstanceName("0"));
            TEST_REQUIRE(scene.FindEntityByInstanceId("1"));
        scene.EndLoop();

        scene.BeginLoop();
            TEST_REQUIRE(scene.GetNumEntities() == 0);
        scene.EndLoop();
    }
}

void unit_test_scene_instance_transform()
{
    auto entity0 = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass parent;
        parent.SetName("parent");
        parent.SetSize(glm::vec2(10.0f, 10.0f));
        parent.SetTranslation(glm::vec2(0.0f, 0.0f));
        entity0->LinkChild(nullptr,  entity0->AddNode(parent));

        game::EntityNodeClass child0;
        child0.SetName("child0");
        child0.SetSize(glm::vec2(16.0f, 6.0f));
        child0.SetTranslation(glm::vec2(20.0f, 20.0f));
        entity0->LinkChild(entity0->FindNodeByName("parent"), entity0->AddNode(child0));
    }
    auto entity1 = std::make_shared<game::EntityClass>();
    {
        game::EntityNodeClass node;
        node.SetName("node");
        node.SetSize(glm::vec2(5.0f, 5.0f));
        node.SetTranslation(glm::vec2(15.0f, 15.0f));
        entity1->LinkChild(nullptr,  entity1->AddNode(node));
    }

    game::SceneClass klass;
    // setup a scene with 2 entities where the second entity
    // is  linked to one of the nodes in the first entity
    {
        game::SceneNodeClass node;
        node.SetName("entity0");
        node.SetEntity(entity0);
        node.SetTranslation(glm::vec2(-10.0f, -10.0f));
        klass.LinkChild(nullptr, klass.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("entity1");
        node.SetEntity(entity1);
        // link this sucker to so that the nodes in entity1 are transformed relative
        // to child0 node in entity0
        node.SetParentRenderTreeNodeId(entity0->FindNodeByName("child0")->GetId());
        node.SetTranslation(glm::vec2(50.0f, 50.0f));
        klass.LinkChild(klass.FindNodeByName("entity0"), klass.AddNode(node));
    }

    auto scene = game::CreateSceneInstance(klass);

    // check entity nodes.
    // when the scene instance is created the scene nodes
    // are used to give the initial placement of entity nodes in the scene.
    {
        auto* entity0 = scene->FindEntityByInstanceName("entity0");
        auto box = scene->FindEntityNodeBoundingBox(entity0, entity0->FindNodeByInstanceName("parent"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(10.0f, 10.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // placement
                                       + glm::vec2(0.0f, 0.0f) // node's offset relative to entity root
                                       + glm::vec2(-5.0f, -5.0f)); // half the size for model offset

        auto rect = scene->FindEntityNodeBoundingRect(entity0, entity0->FindNodeByInstanceName("parent"));
        TEST_REQUIRE(rect.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(rect.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-10.0f + 0.0f -5.0f));
        TEST_REQUIRE(rect.GetY() == real::float32(-10.0f + 0.0f -5.0f));

        box = scene->FindEntityNodeBoundingBox(entity0, entity0->FindNodeByInstanceName("child0"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(16.0f, 6.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // entity placement
                                       + glm::vec2(0.0f, 0.0f) // parent offset relative to the entity root
                                       + glm::vec2(20.0f, 20.0f) // node's offset relative to parent
                                       + glm::vec2(-8.0f, -3.0f)); // half the size for model offset
        rect = scene->FindEntityNodeBoundingRect(entity0, entity0->FindNodeByInstanceName("child0"));
        TEST_REQUIRE(rect.GetWidth() == real::float32(16.0f));
        TEST_REQUIRE(rect.GetHeight() == real::float32(6.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-10.0f + 0.0f + 20.0f -8.0f));
        TEST_REQUIRE(rect.GetY() == real::float32(-10.0f + 0.0f + 20.0f -3.0f));

        // combined bounding rect for both nodes in entity0
        rect = scene->FindEntityBoundingRect(entity0);
        TEST_REQUIRE(rect.GetWidth() == real::float32(15.0 + 18.0f));
        TEST_REQUIRE(rect.GetHeight() == real::float32(15.0 + 13.0f));
        TEST_REQUIRE(rect.GetX() == real::float32(-15.0));
        TEST_REQUIRE(rect.GetY() == real::float32(-15.0));

        auto* entity1 = scene->FindEntityByInstanceName("entity1");
        box = scene->FindEntityNodeBoundingBox(entity1, entity1->FindNodeByInstanceName("node"));
        TEST_REQUIRE(box.GetSize() == glm::vec2(5.0f, 5.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // parent entity placement
                                            + glm::vec2(0.0f, 0.0f)     // parent entity parent node offset relative to entity root
                                            + glm::vec2(20.0f, 20.0f)   // child node offset relative to its entity parent node
                                            + glm::vec2(50.0f, 50.0f)  // this entity placement
                                            + glm::vec2(15.0f, 15.0f) // node placement relative to entity root
                                            + glm::vec2(-2.5, -2.5f)); // half the size for model offset
        // todo: bounding rects for entity1
    }


    {
        // entity0 is linked to the root of the scene graph, therefore the scene graph transform
        // for the nodes in entity0 is identity.
        auto* entity = scene->FindEntityByInstanceName("entity0");
        auto mat = scene->FindEntityTransform(entity);
        game::FBox box(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(1.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(1.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(0.0f , 0.0f));
        // when the scene instance is created the scene class nodes are used to give the
        // initial placement of entities and the scene class nodes' transforms are
        // baked into the transforms of the top level entity nodes.
        auto* node  = entity->FindNodeByInstanceName("parent");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        TEST_REQUIRE(box.GetWidth() == real::float32(10.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(10.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-15.0f, -15.0f));

        // 'child0' node's transform is relative to 'parent' node.
        node = entity->FindNodeByInstanceName("child0");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        TEST_REQUIRE(box.GetWidth() == real::float32(16.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(6.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0, -10.0f) +
                                              glm::vec2(20.0f, 20.0f)  -
                                              glm::vec2(8.0, 3.0));
    }

    {
        // entity1 is linked to entity0 with the link node being child0 in entity0.
        // that means that the nodes in entity1 have a transform that is relative
        // child0 node in entity0.
        auto* entity = scene->FindEntityByInstanceName("entity1");
        auto mat = scene->FindEntityTransform(entity);
        game::FBox box(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(1.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(1.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f , -10.0f) // initial placement
                                         + glm::vec2(20.0f, 20.0f));  // link node offset

        // when the scene instance is created the scene class nodes are used to give the
        // initial placement of entities and the scene class nodes' transforms are
        // baked into the transforms of the top level entity nodes.
        auto* node = entity->FindNodeByInstanceName("node");
        box.Reset();
        box.Transform(node->GetModelTransform());
        box.Transform(entity->FindNodeTransform(node));
        box.Transform(mat);
        TEST_REQUIRE(box.GetWidth() == real::float32(5.0f));
        TEST_REQUIRE(box.GetHeight() == real::float32(5.0f));
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(-10.0f, -10.0f) // parent entity placement translate
                                         + glm::vec2(0.0f, 0.0f) // parent entity parent node translate
                                         + glm::vec2(20.0f, 20.0f) // parent entity child node translate
                                         + glm::vec2(50.0f, 50.0f) // this entity placement translate
                                         + glm::vec2(15.0f, 15.0f) // this entity node translate
                                         + glm::vec2(-2.5f, -2.5f)); // half model size translate offset
    }

}

int test_main(int argc, char* argv[])
{
    unit_test_node();
    unit_test_scene_class();
    unit_test_scene_instance_create();
    unit_test_scene_instance_spawn();
    unit_test_scene_instance_kill();
    unit_test_scene_instance_transform();
    return 0;
}
