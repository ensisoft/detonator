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
#include "base/memory.h"
#include "data/json.h"
#include "game/entity.h"

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
    TEST_CASE(test::Type::Feature)

    game::DrawableItemClass draw;
    draw.SetDrawableId("rectangle");
    draw.SetMaterialId("test");
    draw.SetRenderPass(game::DrawableItemClass::RenderPass::MaskCover);
    draw.SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, true);
    draw.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);
    draw.SetLayer(10);
    draw.SetLineWidth(5.0f);
    draw.SetMaterialParam("kFloat", 1.0f);
    draw.SetMaterialParam("kVec2", glm::vec2(1.0f, 2.0f));
    draw.SetMaterialParam("kVec3", glm::vec3(1.0f, 2.0f, 3.0f));
    draw.SetMaterialParam("kColor", game::Color::DarkCyan);

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

    game::TextItemClass text;
    text.SetText("jeesus ajaa mopolla");
    text.SetLineHeight(2.0f);
    text.SetFontSize(18);
    text.SetLayer(3);
    text.SetRasterWidth(100);
    text.SetRasterHeight(200);
    text.SetFontName("fontname.otf");
    text.SetFlag(game::TextItemClass::Flags::UnderlineText, true);
    text.SetAlign(game::TextItemClass::VerticalTextAlign::Top);
    text.SetAlign(game::TextItemClass::HorizontalTextAlign::Left);
    text.SetTextColor(game::Color::HotPink);

    game::SpatialNodeClass spatial;
    spatial.SetShape(game::SpatialNodeClass::Shape::AABB);
    spatial.SetFlag(game::SpatialNodeClass::Flags::ReportOverlap, true);

    game::FixtureClass fix;
    fix.SetCollisionShape(game::FixtureClass::CollisionShape::Circle);
    fix.SetDensity(2.0f);
    fix.SetFriction(3.0f);
    fix.SetRestitution(4.0f);
    fix.SetPolygonShapeId("123232ddd");
    fix.SetRigidBodyNodeId("81288");

    game::EntityNodeClass node;
    node.SetName("root");
    node.SetSize(glm::vec2(100.0f, 100.0f));
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetDrawable(draw);
    node.SetRigidBody(body);
    node.SetTextItem(text);
    node.SetSpatialNode(spatial);
    node.SetFixture(fix);

    TEST_REQUIRE(node.HasDrawable());
    TEST_REQUIRE(node.HasRigidBody());
    TEST_REQUIRE(node.HasTextItem());
    TEST_REQUIRE(node.HasSpatialNode());
    TEST_REQUIRE(node.HasFixture());
    TEST_REQUIRE(node.GetName()         == "root");
    TEST_REQUIRE(node.GetSize()         == glm::vec2(100.0f, 100.0f));
    TEST_REQUIRE(node.GetTranslation()  == glm::vec2(150.0f, -150.0f));
    TEST_REQUIRE(node.GetScale()        == glm::vec2(4.0f, 5.0f));
    TEST_REQUIRE(node.GetRotation()     == real::float32(1.5f));
    TEST_REQUIRE(node.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
    TEST_REQUIRE(node.GetDrawable()->GetRenderPass() == game::DrawableItemClass::RenderPass::MaskCover);
    TEST_REQUIRE(node.GetDrawable()->GetLayer()        == 10);
    TEST_REQUIRE(node.GetDrawable()->GetDrawableId()   == "rectangle");
    TEST_REQUIRE(node.GetDrawable()->GetMaterialId()   == "test");
    TEST_REQUIRE(*node.GetDrawable()->GetMaterialParamValue<float>("kFloat") == real::float32(1.0f));
    TEST_REQUIRE(*node.GetDrawable()->GetMaterialParamValue<glm::vec2>("kVec2") == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(*node.GetDrawable()->GetMaterialParamValue<glm::vec3>("kVec3") == glm::vec3(1.0f, 2.0f, 3.0f));
    TEST_REQUIRE(*node.GetDrawable()->GetMaterialParamValue<game::Color4f>("kColor") == game::Color::DarkCyan);
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
    TEST_REQUIRE(node.GetTextItem()->GetText() == "jeesus ajaa mopolla");
    TEST_REQUIRE(node.GetTextItem()->GetFontSize() == 18);
    TEST_REQUIRE(node.GetTextItem()->GetFontName() == "fontname.otf");
    TEST_REQUIRE(node.GetTextItem()->GetRasterWidth() == 100);
    TEST_REQUIRE(node.GetTextItem()->GetRasterHeight() == 200);
    TEST_REQUIRE(node.GetSpatialNode()->GetShape() == game::SpatialNodeClass::Shape::AABB);
    TEST_REQUIRE(node.GetFixture()->GetCollisionShape() == game::FixtureClass::CollisionShape::Circle);
    TEST_REQUIRE(*node.GetFixture()->GetDensity() == real::float32(2.0f));
    TEST_REQUIRE(*node.GetFixture()->GetFriction() == real::float32(3.0f));
    TEST_REQUIRE(*node.GetFixture()->GetRestitution() == real::float32(4.0f));
    TEST_REQUIRE(node.GetFixture()->GetPolygonShapeId() == "123232ddd");
    TEST_REQUIRE(node.GetFixture()->GetRigidBodyNodeId() == "81288");

    // to/from json
    {
        data::JsonObject json;
        node.IntoJson(json);
        game::EntityNodeClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.HasDrawable());
        TEST_REQUIRE(ret.HasRigidBody());
        TEST_REQUIRE(ret.HasTextItem());
        TEST_REQUIRE(ret.HasSpatialNode());
        TEST_REQUIRE(ret.GetName()         == "root");
        TEST_REQUIRE(ret.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(ret.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret.GetDrawable()->GetDrawableId()   == "rectangle");
        TEST_REQUIRE(ret.GetDrawable()->GetMaterialId()   == "test");
        TEST_REQUIRE(ret.GetDrawable()->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(ret.GetDrawable()->GetRenderPass() == game::DrawableItemClass::RenderPass::MaskCover);
        TEST_REQUIRE(ret.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(ret.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
        TEST_REQUIRE(ret.GetRigidBody()->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Circle);
        TEST_REQUIRE(ret.GetRigidBody()->GetSimulation() == game::RigidBodyItemClass::Simulation::Dynamic);
        TEST_REQUIRE(ret.GetRigidBody()->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
        TEST_REQUIRE(ret.GetRigidBody()->GetFriction()       == real::float32(2.0f));
        TEST_REQUIRE(ret.GetRigidBody()->GetRestitution()    == real::float32(3.0f));
        TEST_REQUIRE(ret.GetRigidBody()->GetAngularDamping() == real::float32(4.0f));
        TEST_REQUIRE(ret.GetRigidBody()->GetLinearDamping()  == real::float32(5.0f));
        TEST_REQUIRE(ret.GetRigidBody()->GetDensity()        == real::float32(-1.0));
        TEST_REQUIRE(ret.GetRigidBody()->GetPolygonShapeId() == "shape");
        TEST_REQUIRE(ret.GetTextItem()->GetText() == "jeesus ajaa mopolla");
        TEST_REQUIRE(ret.GetTextItem()->GetFontSize() == 18);
        TEST_REQUIRE(ret.GetTextItem()->GetFontName() == "fontname.otf");
        TEST_REQUIRE(ret.GetTextItem()->GetRasterWidth() == 100);
        TEST_REQUIRE(ret.GetTextItem()->GetRasterHeight() == 200);
        TEST_REQUIRE(ret.GetSpatialNode()->TestFlag(game::SpatialNodeClass::Flags::ReportOverlap));
        TEST_REQUIRE(ret.GetSpatialNode()->GetShape() == game::SpatialNodeClass::Shape::AABB);
        TEST_REQUIRE(ret.GetFixture()->GetCollisionShape() == game::FixtureClass::CollisionShape::Circle);
        TEST_REQUIRE(*ret.GetFixture()->GetDensity() == real::float32(2.0f));
        TEST_REQUIRE(*ret.GetFixture()->GetFriction() == real::float32(3.0f));
        TEST_REQUIRE(*ret.GetFixture()->GetRestitution() == real::float32(4.0f));
        TEST_REQUIRE(ret.GetFixture()->GetPolygonShapeId() == "123232ddd");
        TEST_REQUIRE(ret.GetFixture()->GetRigidBodyNodeId() == "81288");
        TEST_REQUIRE(ret.GetHash() == node.GetHash());
    }

    // test copy and copy ctor
    {
        auto copy(node);
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId() == node.GetId());
        game::EntityNodeClass temp;
        temp = copy;
        TEST_REQUIRE(temp.GetHash() == node.GetHash());
        TEST_REQUIRE(temp.GetId() == node.GetId());
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
        TEST_REQUIRE(clone.GetDrawable()->GetRenderPass() == game::DrawableItemClass::RenderPass::MaskCover);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(clone.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::RestartDrawable) == false);
        TEST_REQUIRE(clone.GetTextItem()->GetText() == "jeesus ajaa mopolla");
        TEST_REQUIRE(clone.GetTextItem()->GetFontSize() == 18);
        TEST_REQUIRE(clone.GetTextItem()->GetFontName() == "fontname.otf");
        TEST_REQUIRE(clone.GetTextItem()->GetRasterWidth() == 100);
        TEST_REQUIRE(clone.GetTextItem()->GetRasterHeight() == 200);
        TEST_REQUIRE(clone.GetSpatialNode()->TestFlag(game::SpatialNodeClass::Flags::ReportOverlap));
        TEST_REQUIRE(clone.GetSpatialNode()->GetShape() == game::SpatialNodeClass::Shape::AABB);
        TEST_REQUIRE(clone.GetFixture()->GetCollisionShape() == game::FixtureClass::CollisionShape::Circle);
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
        TEST_REQUIRE(instance.HasSpatialNode());
        TEST_REQUIRE(instance.GetDrawable()->GetLineWidth()   == real::float32(5.0f));
        TEST_REQUIRE(instance.GetDrawable()->GetRenderPass() == game::DrawableItemClass::RenderPass::MaskCover);
        TEST_REQUIRE(instance.GetRigidBody()->GetPolygonShapeId() == "shape");
        TEST_REQUIRE(instance->GetTextItem());
        TEST_REQUIRE(instance->GetTextItem()->GetText() == "jeesus ajaa mopolla");
        TEST_REQUIRE(instance->GetTextItem()->GetFontSize() == 18);
        TEST_REQUIRE(instance->GetTextItem()->GetFontName() == "fontname.otf");
        TEST_REQUIRE(instance->GetTextItem()->GetRasterWidth() == 100);
        TEST_REQUIRE(instance->GetTextItem()->GetRasterHeight() == 200);
        TEST_REQUIRE(instance->GetSpatialNode()->TestFlag(game::SpatialNodeClass::Flags::ReportOverlap));
        TEST_REQUIRE(instance->GetSpatialNode()->GetShape() == game::SpatialNodeClass::Shape::AABB);
        TEST_REQUIRE(instance->GetFixture()->GetCollisionShape() == game::FixtureClass::CollisionShape::Circle);

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
    TEST_CASE(test::Type::Feature)

    game::EntityClass entity;
    entity.SetName("TestEntityClass");
    entity.SetLifetime(5.0f);
    entity.SetFlag(game::EntityClass::Flags::UpdateEntity, false);
    entity.SetFlag(game::EntityClass::Flags::WantsMouseEvents, true);
    entity.SetSriptFileId("script_123.lua");
    entity.SetTag("foo bar");
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
        game::AnimationClass track;
        track.SetName("test1");
        entity.AddAnimation(track);
    }
    {
        game::AnimationClass track;
        track.SetName("test2");
        entity.AddAnimation(track);
        entity.SetIdleTrackId(track.GetId());
    }

    {
        game::ScriptVar foo("something", 123, game::ScriptVar::ReadOnly);
        game::ScriptVar bar("other_thing", std::string("jallukola"), game::ScriptVar::ReadWrite);
        game::ScriptVar arr("array", std::vector<int>{1, 8, -1}, game::ScriptVar::ReadWrite);
        entity.AddScriptVar(foo);
        entity.AddScriptVar(bar);
        entity.AddScriptVar(arr);

        auto* node = entity.FindNodeByName("child_2");
        // node reference variable.
        game::ScriptVar::EntityNodeReference  ref;
        ref.id = node->GetId();
        game::ScriptVar var("node", ref, false);
        entity.AddScriptVar(std::move(var));
    }

    // physics joint
    {
        game::EntityClass::PhysicsJoint joint;
        joint.name = "test";
        joint.dst_node_id = entity.GetNode(0).GetId();
        joint.src_node_id = entity.GetNode(1).GetId();
        joint.type        = game::EntityClass::PhysicsJointType::Distance;
        joint.id          = base::RandomString(10);
        joint.src_node_anchor_point = glm::vec2(-1.0f, 2.0f);
        joint.dst_node_anchor_point = glm::vec2(2.0f, -1.0f);
        game::EntityClass::DistanceJointParams params;
        params.damping = 2.0f;
        params.stiffness = 3.0f;
        params.min_distance = 4.0f;
        params.max_distance = 5.0f;
        joint.params = params;
        entity.AddJoint(std::move(joint));
    }

    TEST_REQUIRE(entity.GetName() == "TestEntityClass");
    TEST_REQUIRE(entity.GetLifetime() == real::float32(5.0f));
    TEST_REQUIRE(entity.GetScriptFileId() == "script_123.lua");
    TEST_REQUIRE(entity.GetTag() == "foo bar");
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
    TEST_REQUIRE(entity.GetNumAnimations() == 2);
    TEST_REQUIRE(entity.FindAnimationByName("test1"));
    TEST_REQUIRE(entity.FindAnimationByName("sdgasg") == nullptr);
    TEST_REQUIRE(entity.GetIdleTrackId() == entity.GetAnimation(1).GetId());
    TEST_REQUIRE(entity.GetNumScriptVars() == 4);
    TEST_REQUIRE(entity.GetScriptVar(0).GetName() == "something");
    TEST_REQUIRE(entity.GetScriptVar(0).GetValue<int>() == 123);
    TEST_REQUIRE(entity.GetScriptVar(0).IsReadOnly() == true);
    TEST_REQUIRE(entity.GetScriptVar(0).IsArray() == false);
    TEST_REQUIRE(entity.GetScriptVar(1).GetName() == "other_thing");
    TEST_REQUIRE(entity.GetScriptVar(1).GetValue<std::string>() == "jallukola");
    TEST_REQUIRE(entity.GetScriptVar(1).IsReadOnly() == false);
    TEST_REQUIRE(entity.GetScriptVar(1).IsArray() == false);
    TEST_REQUIRE(entity.GetScriptVar(2).GetName() == "array");
    TEST_REQUIRE(entity.GetScriptVar(2).IsReadOnly() == false);
    TEST_REQUIRE(entity.GetScriptVar(2).IsArray() == true);
    TEST_REQUIRE(entity.FindScriptVarByName("foobar") == nullptr);
    TEST_REQUIRE(entity.FindScriptVarByName("something"));
    TEST_REQUIRE(entity.GetNumJoints() == 1);
    TEST_REQUIRE(entity.GetJoint(0).dst_node_id == entity.GetNode(0).GetId());
    TEST_REQUIRE(entity.GetJoint(0).src_node_id == entity.GetNode(1).GetId());

    // test linking.
    entity.LinkChild(nullptr, entity.FindNodeByName("root"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_1"));
    entity.LinkChild(entity.FindNodeByName("root"), entity.FindNodeByName("child_2"));
    TEST_REQUIRE(WalkTree(entity) == "root child_1 child_2");

    // serialization
    {
        data::JsonObject json;
        entity.IntoJson(json);
        //std::cout << json.ToString();
        game::EntityClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetName() == "TestEntityClass");
        TEST_REQUIRE(ret.GetNumNodes() == 3);
        TEST_REQUIRE(ret.GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret.GetId() == entity.GetId());
        TEST_REQUIRE(ret.GetHash() == entity.GetHash());
        TEST_REQUIRE(ret.GetNumAnimations() == 2);
        TEST_REQUIRE(ret.FindAnimationByName("test1"));
        TEST_REQUIRE(ret.GetNumScriptVars() == 4);
        TEST_REQUIRE(ret.GetScriptVar(0).GetName() == "something");
        TEST_REQUIRE(ret.GetScriptVar(0).GetValue<int>() == 123);
        TEST_REQUIRE(ret.GetScriptVar(0).IsReadOnly() == true);
        TEST_REQUIRE(ret.GetScriptVar(0).IsArray() == false);
        TEST_REQUIRE(ret.GetScriptVar(1).GetName() == "other_thing");
        TEST_REQUIRE(ret.GetScriptVar(1).GetValue<std::string>() == "jallukola");
        TEST_REQUIRE(ret.GetScriptVar(1).IsReadOnly() == false);
        TEST_REQUIRE(ret.GetScriptVar(1).IsArray() == false);
        TEST_REQUIRE(ret.GetScriptVar(2).GetName() == "array");
        TEST_REQUIRE(ret.GetScriptVar(2).IsReadOnly() == false);
        TEST_REQUIRE(ret.GetScriptVar(2).IsArray() == true);
        TEST_REQUIRE(ret.GetNumJoints() == 1);
        TEST_REQUIRE(ret.GetJoint(0).name == "test");
        TEST_REQUIRE(ret.GetJoint(0).dst_node_id == entity.GetNode(0).GetId());
        TEST_REQUIRE(ret.GetJoint(0).src_node_id == entity.GetNode(1).GetId());
        TEST_REQUIRE(ret.GetJoint(0).src_node_anchor_point == glm::vec2(-1.0f, 2.0f));
        TEST_REQUIRE(ret.GetJoint(0).dst_node_anchor_point == glm::vec2(2.0f, -1.0f));
        const auto* joint_params = std::get_if<game::EntityClass::DistanceJointParams>(&entity.GetJoint(0).params);
        TEST_REQUIRE(joint_params);
        TEST_REQUIRE(joint_params->damping == real::float32(2.0f));
        TEST_REQUIRE(joint_params->stiffness == real::float32(3.0f));
        TEST_REQUIRE(joint_params->min_distance.has_value());
        TEST_REQUIRE(joint_params->max_distance.has_value());
        TEST_REQUIRE(joint_params->min_distance.value() == real::float32(4.0f));
        TEST_REQUIRE(joint_params->max_distance.value() == real::float32(5.0f));
        TEST_REQUIRE(WalkTree(ret) == "root child_1 child_2");
    }

    // copy construction and assignment
    {
        auto copy(entity);
        TEST_REQUIRE(copy.GetId() == entity.GetId());
        TEST_REQUIRE(copy.GetHash() == entity.GetHash());
        TEST_REQUIRE(copy.GetNumAnimations() == 2);
        TEST_REQUIRE(copy.FindAnimationByName("test1"));
        TEST_REQUIRE(WalkTree(copy) == "root child_1 child_2");

        game::EntityClass temp;
        temp = entity;
        TEST_REQUIRE(temp.GetId() == entity.GetId());
        TEST_REQUIRE(temp.GetHash() == entity.GetHash());
        TEST_REQUIRE(temp.GetNumAnimations() == 2);
        TEST_REQUIRE(temp.FindAnimationByName("test1"));
        TEST_REQUIRE(WalkTree(temp) == "root child_1 child_2");
    }

    // clone
    {
        auto clone(entity.Clone());
        TEST_REQUIRE(clone.GetName() == "TestEntityClass");
        TEST_REQUIRE(clone.GetScriptFileId() == "script_123.lua");
        TEST_REQUIRE(clone.GetLifetime() == real::float32(5.0f));
        TEST_REQUIRE(clone.TestFlag(game::EntityClass::Flags::UpdateEntity) == false);
        TEST_REQUIRE(clone.TestFlag(game::EntityClass::Flags::WantsMouseEvents) == true);
        TEST_REQUIRE(clone.GetNumAnimations() == 2);
        TEST_REQUIRE(clone.FindAnimationByName("test1"));
        TEST_REQUIRE(clone.FindAnimationByName("test2"));
        TEST_REQUIRE(clone.GetIdleTrackId() == clone.GetAnimation(1).GetId());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(clone.GetId() != entity.GetId());
        TEST_REQUIRE(clone.GetHash() != entity.GetHash());
        TEST_REQUIRE(clone.GetNumAnimations() == 2);
        TEST_REQUIRE(clone.FindAnimationByName("test1"));
        TEST_REQUIRE(WalkTree(clone) == "root child_1 child_2");

        const auto* node = clone.FindNodeByName("child_2");
        const auto& var = clone.GetScriptVar(3);
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::EntityNodeReference);
        TEST_REQUIRE(var.GetValue<game::ScriptVar::EntityNodeReference>().id == node->GetId());
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

    // node bounding rect/box
    {
        const auto* node = entity.FindNodeByName("root");
        const auto& rect = entity.FindNodeBoundingRect(node);
        TEST_REQUIRE(math::equals(5.0f, rect.GetX()));
        TEST_REQUIRE(math::equals(5.0f, rect.GetY()));
        TEST_REQUIRE(math::equals(10.0f, rect.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, rect.GetHeight()));

        const auto& box = entity.FindNodeBoundingBox(node);
        TEST_REQUIRE(box.GetTopLeft() == glm::vec2(5.0f, 5.0f));
        TEST_REQUIRE(math::equals(10.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, box.GetHeight()));

        auto ret = entity.MapCoordsToNodeBox(10.0f, 10.0f, node);
        TEST_REQUIRE(ret == glm::vec2(5.0f, 5.0f));
        ret = entity.MapCoordsToNodeBox(5.0f, 5.0f, node);
        TEST_REQUIRE(ret == glm::vec2(0.0f, 0.0f));
        ret = entity.MapCoordsToNodeBox(15.0f, 15.0f, node);
        TEST_REQUIRE(ret == glm::vec2(10.0f, 10.0f));

        ret = entity.MapCoordsFromNodeBox(5.0f, 5.0f, node);
        TEST_REQUIRE(ret == glm::vec2(10.0f, 10.0f));

        ret = entity.MapCoordsFromNodeBox(0.0f, 0.0f, node);
        TEST_REQUIRE(ret == glm::vec2(5.0f, 5.0f));

    }

    // node bounding box
    {
        const auto* node = entity.FindNodeByName("child_1");
        const auto box = entity.FindNodeBoundingRect(node);
        TEST_REQUIRE(math::equals(19.0f, box.GetX()));
        TEST_REQUIRE(math::equals(19.0f, box.GetY()));
        TEST_REQUIRE(math::equals(2.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(2.0f, box.GetHeight()));
    }

    // coordinate mapping
    {
        const auto* node = entity.FindNodeByName("child_1");
        auto vec = entity.MapCoordsFromNodeBox(1.0f, 1.0f, node);
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));

        // inverse operation to MapCoordsFromNodeBox
        vec = entity.MapCoordsToNodeBox(20.0f, 20.0f, node);
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
    TEST_CASE(test::Type::Feature)

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
    {
        game::ScriptVar foo("foo", 123, game::ScriptVar::ReadWrite);
        game::ScriptVar bar("bar", 1.0f, game::ScriptVar::ReadOnly);
        klass.AddScriptVar(foo);
        klass.AddScriptVar(std::move(bar));
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

    TEST_REQUIRE(instance.FindScriptVarByName("foo"));
    TEST_REQUIRE(instance.FindScriptVarByName("bar"));
    TEST_REQUIRE(instance.FindScriptVarByName("foo")->IsReadOnly() == false);
    TEST_REQUIRE(instance.FindScriptVarByName("bar")->IsReadOnly() == true);
    instance.FindScriptVarByName("foo")->SetValue(444);
    TEST_REQUIRE(instance.FindScriptVarByName("foo")->GetValue<int>() == 444);

    // todo: test more of the instance api
}

void unit_test_entity_clone_track_bug()
{
    TEST_CASE(test::Type::Feature)

    // cloning an entity class with animation track requires
    // remapping the node ids.
    game::EntityNodeClass node;
    node.SetName("root");

    game::TransformActuatorClass actuator;
    actuator.SetNodeId(node.GetId());

    game::AnimationClass track;
    track.SetName("test1");
    track.AddActuator(actuator);

    game::EntityClass klass;
    klass.AddNode(std::move(node));
    klass.AddAnimation(track);

    {
        auto clone = klass.Clone();
        const auto& cloned_node = clone.GetNode(0);
        const auto& cloned_track = clone.GetAnimation(0);
        TEST_REQUIRE(cloned_track.GetActuatorClass(0).GetNodeId() == cloned_node.GetId());
    }
}

void unit_test_entity_class_coords()
{
    TEST_CASE(test::Type::Feature)

    game::EntityClass entity;
    entity.SetName("test");

    {
        game::EntityNodeClass node;
        node.SetName("node0");
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);
        entity.LinkChild(nullptr,entity.AddNode(std::move(node)));
    }
    {
        game::EntityNodeClass node;
        node.SetName("node1");
        node.SetTranslation(glm::vec2(100.0f, 100.0f));
        node.SetSize(glm::vec2(50.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(math::Pi*0.5);
        entity.LinkChild(entity.FindNodeByName("node0"), entity.AddNode(std::move(node)));
    }

    // the hit coordinate is in the *model* space with the
    // top left corner of the model itself being at 0,0
    // and then extending to the width and height of the node's
    // width and height.
    // so anything that falls outside the range of x >= 0 && x <= width
    // and y >= 0 && y <= height is not within the model itself.
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-5.0f, -5.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].y));
    }
    {
        // expected: outside the box.
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-6.0f, -5.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(-5.0f, -6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(6.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }
    {
        // expected: outside the box
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(0.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 0);
    }

    // node1's transform is relative to node0
    // since they're linked together, remember it's rotated by 90deg
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(100.0f, 100.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(25.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(100.0f, 75.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(5.0f, hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(105.0f , 75.0f , &hits , &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].y));
    }
    {
        std::vector<game::EntityNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        entity.CoarseHitTest(105.0f , 124.0f , &hits , &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(math::equals(49.0f , hitpos[0].x));
        TEST_REQUIRE(math::equals(0.0f , hitpos[0].y));
    }

    // map coords to/from entity node's model
    // the coordinates that go in are in the entity coordinate space.
    // the coordinates that come out are relative to the nodes model
    // coordinate space. The model itself has width/size extent in this
    // space thus any results that fall outside x < 0 && x width or
    // y < 0 && y > height are not within the model's extents.
    {
        auto vec = entity.MapCoordsToNodeBox(0.0f, 0.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(5.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));
        vec = entity.MapCoordsFromNodeBox(5.0f, 5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));

        vec = entity.MapCoordsToNodeBox(-5.0f, -5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
        vec = entity.MapCoordsFromNodeBox(0.0f, 0.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(-5.0f, vec.x));
        TEST_REQUIRE(math::equals(-5.0f, vec.y));

        vec = entity.MapCoordsToNodeBox(5.0f, 5.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(10.0f, vec.x));
        TEST_REQUIRE(math::equals(10.0f, vec.y));
        vec = entity.MapCoordsFromNodeBox(10.0f, 10.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(5.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));

        vec = entity.MapCoordsToNodeBox(15.0f, 15.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));
        vec = entity.MapCoordsFromNodeBox(20.0f, 20.0f, entity.FindNodeByName("node0"));
        TEST_REQUIRE(math::equals(15.0f, vec.x));
        TEST_REQUIRE(math::equals(15.0f, vec.y));
    }
    {
        auto vec = entity.MapCoordsToNodeBox(100.0f, 100.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(25.0f, vec.x));
        TEST_REQUIRE(math::equals(5.0f, vec.y));
        vec = entity.MapCoordsFromNodeBox(25.0f, 5.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(100.0f, vec.x));
        TEST_REQUIRE(math::equals(100.0f, vec.y));

        vec = entity.MapCoordsToNodeBox(105.0f, 75.0f, entity.FindNodeByName("node1"));
        TEST_REQUIRE(math::equals(0.0f, vec.x));
        TEST_REQUIRE(math::equals(0.0f, vec.y));
    }

}

struct PerfTestDrawableTag{};

namespace mem {
    template<>
    struct AllocatorInstance<PerfTestDrawableTag> {
        static mem::Allocator& Get() noexcept {
            using Allocator = mem::MemoryPool<game::DrawableItem>;
            static Allocator allocator(512);
            return allocator;
        }
    };

    template<>
    struct AllocatorInstance<mem::DefaultAllocatorTag<game::RigidBodyItem>> {
        static mem::Allocator& Get() noexcept {
            using AllocatorType = mem::MemoryPool<game::RigidBodyItem>;
            static AllocatorType allocator(512);
            return allocator;
        }
    };
    template<>
    struct AllocatorInstance<mem::DefaultAllocatorTag<game::EntityNode>> {
        static mem::Allocator& Get() noexcept {
            using AllocatorType = mem::MemoryPool<game::EntityNode>;
            static AllocatorType allocator(512);
            return allocator;
        }
    };

} // mem

void measure_item_allocation_time()
{
    TEST_CASE(test::Type::Other)

    auto item = std::make_shared<game::DrawableItemClass>();
    item->SetDrawableId("rectangle");
    item->SetMaterialId("test");
    item->SetRenderPass(game::DrawableItemClass::RenderPass::MaskCover);

    auto std_ret = test::TimedTest(1000, [&item]() {
        for (size_t i=0; i<1000; ++i) {
            auto instance = std::make_unique<game::DrawableItem>(item);
        }
    });
    test::PrintTestTimes("standard alloc", std_ret);

    auto mem_ret = test::TimedTest(1000, [&item]() {
        for (size_t i=0; i<1000; ++i) {
            auto instance = mem::make_unique<game::DrawableItem, PerfTestDrawableTag>(item);
        }
    });
    test::PrintTestTimes("pooled alloc", mem_ret);
}

void measure_entity_allocation_time()
{
    TEST_CASE(test::Type::Other)

    auto entity = std::make_shared<game::EntityClass>();
    entity->SetName("TestEntityClass");
    entity->SetLifetime(5.0f);
    entity->SetFlag(game::EntityClass::Flags::UpdateEntity, false);
    entity->SetFlag(game::EntityClass::Flags::WantsMouseEvents, true);
    entity->SetSriptFileId("script_123.lua");
    entity->SetTag("foo bar");
    {
        game::EntityNodeClass node;
        node.SetName("body");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);

        game::DrawableItemClass draw;
        draw.SetDrawableId("rectangle");
        draw.SetMaterialId("test");
        draw.SetRenderPass(game::DrawableItemClass::RenderPass::MaskCover);
        node.SetDrawable(draw);

        game::RigidBodyItemClass body;
        body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
        body.SetSimulation(game::RigidBodyItemClass::Simulation::Dynamic);
        body.SetFlag(game::RigidBodyItemClass::Flags::Bullet, true);
        body.SetFriction(2.0f);
        node.SetRigidBody(body);

        entity->LinkChild(nullptr, entity->AddNode(std::move(node)));
    }

    auto std_ret = test::TimedTest(1000, [&entity]() {
        for (size_t i = 0; i < 1000; ++i)
        {
            auto instance = std::make_unique<game::Entity>(entity);
        }
    });
    test::PrintTestTimes("standard alloc", std_ret);
}

// simulate entity update in a tight loop
void measure_entity_update_time()
{
    TEST_CASE(test::Type::Other)

    auto entity = std::make_shared<game::EntityClass>();
    entity->SetName("TestEntityClass");
    entity->SetLifetime(5.0f);
    entity->SetFlag(game::EntityClass::Flags::UpdateEntity, false);
    entity->SetFlag(game::EntityClass::Flags::WantsMouseEvents, true);
    entity->SetSriptFileId("script_123.lua");
    entity->SetTag("foo bar");
    {
        game::EntityNodeClass node;
        node.SetName("body");
        node.SetTranslation(glm::vec2(10.0f, 10.0f));
        node.SetSize(glm::vec2(10.0f, 10.0f));
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetRotation(0.0f);

        game::DrawableItemClass draw;
        draw.SetDrawableId("rectangle");
        draw.SetMaterialId("test");
        draw.SetRenderPass(game::DrawableItemClass::RenderPass::MaskCover);
        node.SetDrawable(draw);

        entity->LinkChild(nullptr, entity->AddNode(std::move(node)));
    }

    // create the entities
    std::vector<std::unique_ptr<game::Entity>> entities;
    for (size_t i=0; i<10000; ++i)
    {
        entities.emplace_back(new game::Entity(entity));
    }

    auto ret = test::TimedTest(1000, [&entities]() {
        for (auto& entity : entities) {
            auto* body = entity->FindNodeByClassName("body");
            body->Translate(0.01f, 0.01f);
        }
    });
    test::PrintTestTimes("node update", ret);
}

int test_main(int argc, char* argv[])
{
    unit_test_entity_node();
    unit_test_entity_class();
    unit_test_entity_instance();
    unit_test_entity_clone_track_bug();
    unit_test_entity_class_coords();

    measure_item_allocation_time();
    measure_entity_allocation_time();
    measure_entity_update_time();
    return 0;
}
