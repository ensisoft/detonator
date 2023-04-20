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
#include "game/animation.h"
#include "game/entity.h"

void apply_flag(game::SetFlagActuatorClass::FlagName flag,
                game::SetFlagActuatorClass::FlagAction action,
                game::EntityNode& node)
{
    game::SetFlagActuatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetFlagName(flag);
    klass.SetFlagAction(action);

    game::SetFlagActuator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void apply_value(game::SetValueActuatorClass::ParamName name,
                 game::SetValueActuatorClass::ParamValue value,
                 game::EntityNode& node)
{
    game::SetValueActuatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetParamName(name);
    klass.SetEndValue(value);

    game::SetValueActuator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void apply_material_value(const char* name,
                          game::MaterialActuatorClass::MaterialParam  value,
                          game::EntityNode& node)
{
    game::MaterialActuatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetMaterialParam(name, value);

    game::MaterialActuator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void unit_test_setflag_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::SetFlagActuatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetFlagName(game::SetFlagActuatorClass::FlagName::Drawable_VisibleInGame);
    klass.SetFlagAction(game::SetFlagActuatorClass::FlagAction::Off);

    TEST_REQUIRE(klass.GetNodeId() == "1234");
    TEST_REQUIRE(klass.GetStartTime() == real::float32(0.1f));
    TEST_REQUIRE(klass.GetDuration()  == real::float32(0.5f));
    TEST_REQUIRE(klass.GetFlagName() == game::SetFlagActuatorClass::FlagName::Drawable_VisibleInGame);
    TEST_REQUIRE(klass.GetFlagAction() == game::SetFlagActuatorClass::FlagAction::Off);

    // serialize
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::SetFlagActuatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetName() == "test");
        TEST_REQUIRE(copy.GetNodeId() == "1234");
        TEST_REQUIRE(copy.GetStartTime() == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()  == real::float32(0.5f));
        TEST_REQUIRE(copy.GetFlagName() == game::SetFlagActuatorClass::FlagName::Drawable_VisibleInGame);
        TEST_REQUIRE(copy.GetFlagAction() == game::SetFlagActuatorClass::FlagAction::Off);
    }

    // copy assignment
    {
        game::SetFlagActuatorClass copy(klass);
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        copy = klass;
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy and clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetId() == klass.GetId());
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetHash()  != klass.GetHash());
        TEST_REQUIRE(clone->GetId()    != klass.GetId());
    }

    // instance
    {
        game::EntityNodeClass node_klass;

        game::DrawableItemClass draw_class;
        draw_class.SetFlag(game::DrawableItemClass::Flags::VisibleInGame, true);
        draw_class.SetFlag(game::DrawableItemClass::Flags::UpdateMaterial, true);
        draw_class.SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, true);
        draw_class.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, true);
        draw_class.SetFlag(game::DrawableItemClass::Flags::FlipHorizontally, true);
        node_klass.SetDrawable(draw_class);

        game::RigidBodyItemClass rigid_body_class;
        rigid_body_class.SetFlag(game::RigidBodyItemClass::Flags::Bullet, true);
        rigid_body_class.SetFlag(game::RigidBodyItemClass::Flags::Sensor, true);
        rigid_body_class.SetFlag(game::RigidBodyItemClass::Flags::Enabled, true);
        rigid_body_class.SetFlag(game::RigidBodyItemClass::Flags::CanSleep, true);
        rigid_body_class.SetFlag(game::RigidBodyItemClass::Flags::DiscardRotation, true);
        node_klass.SetRigidBody(rigid_body_class);

        game::TextItemClass text_class;
        text_class.SetFlag(game::TextItemClass::Flags::VisibleInGame, true);
        text_class.SetFlag(game::TextItemClass::Flags::UnderlineText, true);
        text_class.SetFlag(game::TextItemClass::Flags::BlinkText, true);
        node_klass.SetTextItem(text_class);

        // create node instance
        game::EntityNode node(node_klass);
        const auto* draw = node.GetDrawable();
        const auto* body = node.GetRigidBody();
        const auto* text = node.GetTextItem();

        // drawable flags.
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_VisibleInGame,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_UpdateDrawable,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_UpdateMaterial,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_FlipHorizontally,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_Restart,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::VisibleInGame) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateDrawable) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateMaterial) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::FlipHorizontally) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::RestartDrawable) == false);

        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_VisibleInGame,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_UpdateDrawable,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_UpdateMaterial,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_FlipHorizontally,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::Drawable_Restart,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::VisibleInGame)   == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateDrawable)  == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateMaterial)  == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::FlipHorizontally) == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::RestartDrawable) == true);

        // ridig body flags.
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Bullet,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Sensor,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Enabled,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_CanSleep,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_DiscardRotation,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Bullet)          == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Sensor)          == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::CanSleep)        == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Enabled)         == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::DiscardRotation) == false);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Bullet,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Sensor,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_Enabled,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_CanSleep,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::RigidBody_DiscardRotation,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Bullet)          == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Sensor)          == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::CanSleep)        == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::Enabled)         == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBodyItem::Flags::DiscardRotation) == true);

        // text item flags.
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_Blink,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_Underline,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_VisibleInGame,
                   game::SetFlagActuatorClass::FlagAction::Off, node);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::BlinkText) == false);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::UnderlineText) == false);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::VisibleInGame) == false);
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_Blink,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_Underline,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        apply_flag(game::SetFlagActuatorClass::FlagName::TextItem_VisibleInGame,
                   game::SetFlagActuatorClass::FlagAction::Toggle, node);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::BlinkText) == true);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::UnderlineText) == true);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::VisibleInGame) == true);
    }
}

void unit_test_setval_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::SetValueActuatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::SetValueActuatorClass::Interpolation::Cosine);
    klass.SetParamName(game::SetValueActuatorClass::ParamName::LinearVelocity);
    klass.SetEndValue(glm::vec2(2.0f, -3.0f));

    // serialize.
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::SetValueActuatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "1234");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetParamName() == game::SetValueActuatorClass::ParamName::LinearVelocity);
        TEST_REQUIRE(*copy.GetEndValue<glm::vec2>() == glm::vec2(2.0f, -3.0f));
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy assignment and ctor.
    {
        game::SetValueActuatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "1234");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetParamName() == game::SetValueActuatorClass::ParamName::LinearVelocity);
        TEST_REQUIRE(*copy.GetEndValue<glm::vec2>() == glm::vec2(2.0f, -3.0f));
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        copy = klass;
        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy and clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());
        TEST_REQUIRE(copy->GetId()   == klass.GetId());
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetHash()  != klass.GetHash());
        TEST_REQUIRE(clone->GetId()    != klass.GetId());
    }

    // instance.
    {
        game::EntityNodeClass node_klass;

        game::DrawableItemClass draw_class;
        draw_class.SetTimeScale(1.0f);
        node_klass.SetDrawable(draw_class);

        game::RigidBodyItemClass rigid_body_class;
        rigid_body_class.SetAngularDamping(-6.0f);
        node_klass.SetRigidBody(rigid_body_class);

        game::TextItemClass text_class;
        text_class.SetText("text");
        text_class.SetTextColor(game::Color::HotPink);
        node_klass.SetTextItem(text_class);

        game::EntityNode node(node_klass);

        apply_value(game::SetValueActuatorClass::ParamName::DrawableTimeScale, 2.0f, node);
        apply_value(game::SetValueActuatorClass::ParamName::LinearVelocity, glm::vec2(-1.0f, -1.0f), node);
        apply_value(game::SetValueActuatorClass::ParamName::AngularVelocity, 4.0f, node);
        apply_value(game::SetValueActuatorClass::ParamName::TextItemText, std::string("hello"), node);
        apply_value(game::SetValueActuatorClass::ParamName::TextItemColor, game::Color::Blue, node);

        const auto* draw = node.GetDrawable();
        const auto* body = node.GetRigidBody();
        const auto* text = node.GetTextItem();
        TEST_REQUIRE(draw->GetTimeScale() == real::float32(2.0f));
        TEST_REQUIRE(body->GetLinearVelocityAdjustment() == glm::vec2(-1.0f, -1.0f));
        TEST_REQUIRE(body->GetAngularVelocityAdjustment() == real::float32(4.0f));
        TEST_REQUIRE(text->GetTextColor() == game::Color4f(game::Color::Blue));
        TEST_REQUIRE(text->GetText() == "hello");
    }

}

void unit_test_kinematic_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::KinematicActuatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::TransformActuatorClass::Interpolation::Cosine);
    klass.SetEndAngularVelocity(3.0f);
    klass.SetEndLinearVelocity(glm::vec2(1.0f, 2.0f));

    TEST_REQUIRE(klass.GetInterpolation()       == game::TransformActuatorClass::Interpolation::Cosine);
    TEST_REQUIRE(klass.GetName()                == "test");
    TEST_REQUIRE(klass.GetNodeId()              == "1234");
    TEST_REQUIRE(klass.GetStartTime()           == real::float32(0.1f));
    TEST_REQUIRE(klass.GetDuration()            == real::float32(0.5f));
    TEST_REQUIRE(klass.GetEndLinearVelocity()   == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(klass.GetEndAngularVelocity()  == real::float32(3.0f));

    // serialize
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::KinematicActuatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation()       == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()                == "test");
        TEST_REQUIRE(copy.GetNodeId()              == "1234");
        TEST_REQUIRE(copy.GetStartTime()           == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()            == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndLinearVelocity()   == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(copy.GetEndAngularVelocity()  == real::float32(3.0f));
        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy assignment and copy ctor
    {
        game::KinematicActuatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation()       == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()                == "test");
        TEST_REQUIRE(copy.GetNodeId()              == "1234");
        TEST_REQUIRE(copy.GetStartTime()           == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()            == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndLinearVelocity()   == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(copy.GetEndAngularVelocity()  == real::float32(3.0f));
        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        copy = klass;
        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy and clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());
        TEST_REQUIRE(copy->GetId()   == klass.GetId());
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetHash()  != klass.GetHash());
        TEST_REQUIRE(clone->GetId()    != klass.GetId());
    }

    // instance
    {
        game::KinematicActuator instance(klass);

        game::EntityNodeClass klass;
        game::RigidBodyItemClass body;
        klass.SetRigidBody(body);

        // create node instance
        game::EntityNode node(klass);

        // start based on the node.
        instance.Start(node);

        instance.Apply(node, 1.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(3.0f));

        instance.Apply(node, 0.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(0.0f, 0.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(0.0f));

    }
}

void unit_test_transform_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::TransformActuatorClass act;
    act.SetName("test");
    act.SetNodeId("123");
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::TransformActuatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    TEST_REQUIRE(act.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
    TEST_REQUIRE(act.GetName()          == "test");
    TEST_REQUIRE(act.GetNodeId()        == "123");
    TEST_REQUIRE(act.GetStartTime()     == real::float32(0.1f));
    TEST_REQUIRE(act.GetDuration()      == real::float32(0.5f));
    TEST_REQUIRE(act.GetEndPosition()   == glm::vec2(100.0f, 50.0f));
    TEST_REQUIRE(act.GetEndSize()       == glm::vec2(5.0f, 6.0f));
    TEST_REQUIRE(act.GetEndScale()      == glm::vec2(3.0f, 8.0f));
    TEST_REQUIRE(act.GetEndRotation()   == real::float32(1.5f));

    // serialize
    {
        data::JsonObject json;
        act.IntoJson(json);
        game::TransformActuatorClass copy;
        copy.FromJson(json);
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "123");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndPosition()   == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(copy.GetEndSize()       == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(copy.GetEndScale()      == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(copy.GetEndRotation()   == real::float32(1.5f));
        TEST_REQUIRE(copy.GetId()            == act.GetId());
        TEST_REQUIRE(copy.GetHash()          == act.GetHash());
    }

    // copy assignment and copy ctor
    {
        auto copy(act);
        TEST_REQUIRE(copy.GetHash()          == act.GetHash());
        TEST_REQUIRE(copy.GetId()            == act.GetId());
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "123");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndPosition()   == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(copy.GetEndSize()       == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(copy.GetEndScale()      == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(copy.GetEndRotation()   == real::float32(1.5f));

        copy = act;
        TEST_REQUIRE(copy.GetHash() == act.GetHash());
        TEST_REQUIRE(copy.GetId()   == act.GetId());
    }

    // copy and clone
    {
        auto copy = act.Copy();
        TEST_REQUIRE(copy->GetHash()          == act.GetHash());
        TEST_REQUIRE(copy->GetId()            == act.GetId());
        TEST_REQUIRE(copy->GetNodeId()        == "123");
        TEST_REQUIRE(copy->GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy->GetDuration()      == real::float32(0.5f));

        auto clone = act.Clone();
        TEST_REQUIRE(clone->GetHash()          != act.GetHash());
        TEST_REQUIRE(clone->GetId()            != act.GetId());
        TEST_REQUIRE(clone->GetNodeId()        == "123");
        TEST_REQUIRE(clone->GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(clone->GetDuration()      == real::float32(0.5f));
    }


    // instance
    {
        game::TransformActuator instance(act);
        game::EntityNodeClass klass;
        klass.SetTranslation(glm::vec2(5.0f, 5.0f));
        klass.SetSize(glm::vec2(1.0f, 1.0f));
        klass.SetRotation(0.0f);
        klass.SetScale(glm::vec2(1.0f, 1.0f));

        // create node instance
        game::EntityNode node(klass);

        // start based on the node.
        instance.Start(node);

        instance.Apply(node, 1.0f);
        TEST_REQUIRE(node.GetTranslation() == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(node.GetSize()        == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(node.GetScale()       == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(node.GetRotation()    == real::float32(1.5));

        instance.Apply(node, 0.0f);
        TEST_REQUIRE(node.GetTranslation() == glm::vec2(5.0f, 5.0f));
        TEST_REQUIRE(node.GetSize()        == glm::vec2(1.0f, 1.0f));
        TEST_REQUIRE(node.GetScale()       == glm::vec2(1.0f, 1.0f));
        TEST_REQUIRE(node.GetRotation()    == real::float32(0.0));

        instance.Finish(node);
        TEST_REQUIRE(node.GetTranslation() == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(node.GetSize()        == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(node.GetScale()       == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(node.GetRotation()    == real::float32(1.5));
    }
}

void unit_test_material_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::MaterialActuatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::MaterialActuatorClass::Interpolation::Cosine);
    klass.SetMaterialParam("kColor", game::Color4f(game::Color::Green));

    // serialize.
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::MaterialActuatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation()              == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()                       == "test");
        TEST_REQUIRE(copy.GetNodeId()                     == "1234");
        TEST_REQUIRE(copy.GetStartTime()                  == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()                   == real::float32(0.5f));
        TEST_REQUIRE(copy.GetId()                         == klass.GetId());
        TEST_REQUIRE(copy.GetHash()                       == klass.GetHash());
        TEST_REQUIRE(*copy.GetMaterialParamValue<game::Color4f>("kColor") == game::Color::Green);

    }
    // copy assignment and copy ctor
    {
        game::MaterialActuatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation()              == game::TransformActuatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()                       == "test");
        TEST_REQUIRE(copy.GetNodeId()                     == "1234");
        TEST_REQUIRE(copy.GetStartTime()                  == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()                   == real::float32(0.5f));
        TEST_REQUIRE(copy.GetId()                         == klass.GetId());
        TEST_REQUIRE(copy.GetHash()                       == klass.GetHash());
        TEST_REQUIRE(*copy.GetMaterialParamValue<game::Color4f>("kColor") == game::Color::Green);
        copy = klass;
        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy and clone
    {
        auto copy = klass.Copy();
        TEST_REQUIRE(copy->GetHash() == klass.GetHash());
        TEST_REQUIRE(copy->GetId()   == klass.GetId());
        auto clone = klass.Clone();
        TEST_REQUIRE(clone->GetHash()  != klass.GetHash());
        TEST_REQUIRE(clone->GetId()    != klass.GetId());
    }

    // instance
    {
        game::EntityNodeClass node_klass;

        game::DrawableItemClass draw_class;
        draw_class.SetMaterialParam("kFloat", 1.0f);
        draw_class.SetMaterialParam("kColor", base::Color::Red);
        draw_class.SetMaterialParam("kVec2", glm::vec2(1.0f, 2.0f));
        draw_class.SetMaterialParam("kVec3", glm::vec3(1.0f, 2.0f, 3.0f));
        draw_class.SetMaterialParam("kVec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        draw_class.SetMaterialParam("kInt", 123);
        node_klass.SetDrawable(draw_class);
        game::EntityNode node(node_klass);

        apply_material_value("kFloat", -1.0f, node);
        apply_material_value("kColor", game::Color::Green, node);
        apply_material_value("kVec2", glm::vec2(2.0f, 1.0f), node);
        apply_material_value("kVec3", glm::vec3(3.0f, 2.0f, 1.0f), node);
        apply_material_value("kVec4", glm::vec4(4.0f, 3.0f, 2.0f, 1.0f), node);
        apply_material_value("kInt", 321, node);

        const auto* draw = node.GetDrawable();
        TEST_REQUIRE(*draw->GetMaterialParamValue<float>("kFloat") == real::float32(-1.0f));
        TEST_REQUIRE(*draw->GetMaterialParamValue<game::Color4f>("kColor") == base::Color::Green);
        TEST_REQUIRE(*draw->GetMaterialParamValue<glm::vec2>("kVec2") == glm::vec2(2.0f, 1.0f));
        TEST_REQUIRE(*draw->GetMaterialParamValue<glm::vec3>("kVec3") == glm::vec3(3.0f, 2.0f, 1.0f));
        TEST_REQUIRE(*draw->GetMaterialParamValue<glm::vec4>("kVec4") == glm::vec4(4.0f, 3.0f, 2.0f, 1.0f));
        TEST_REQUIRE(*draw->GetMaterialParamValue<int>("kInt") == 321);
    }
}


void unit_test_animation_track()
{
    TEST_CASE(test::Type::Feature)

    game::EntityNodeClass klass;
    klass.SetTranslation(glm::vec2(5.0f, 5.0f));
    klass.SetSize(glm::vec2(1.0f, 1.0f));
    klass.SetRotation(0.0f);
    klass.SetScale(glm::vec2(1.0f, 1.0f));

    // create node instance
    game::EntityNode node(klass);

    game::AnimationClass track;
    track.SetName("test");
    track.SetLooping(true);
    track.SetDuration(10.0f);
    TEST_REQUIRE(track.GetName() == "test");
    TEST_REQUIRE(track.IsLooping());
    TEST_REQUIRE(track.GetDuration() == real::float32(10.0f));
    TEST_REQUIRE(track.GetNumActuators() == 0);

    game::TransformActuatorClass act;
    act.SetNodeId(node.GetClassId());
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::TransformActuatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    track.AddActuator(act);
    TEST_REQUIRE(track.GetNumActuators() == 1);

    // serialize
    {
        data::JsonObject json;
        track.IntoJson(json);

        game::AnimationClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetNumActuators() == 1);
        TEST_REQUIRE(ret.IsLooping()       == true);
        TEST_REQUIRE(ret.GetName()         == "test");
        TEST_REQUIRE(ret.GetDuration()     == real::float32(10.0f));
        TEST_REQUIRE(ret.GetId()           == track.GetId());
        TEST_REQUIRE(ret.GetHash()         == track.GetHash());
    }

    // copy assignment and copy ctor
    {
        auto copy(track);
        TEST_REQUIRE(copy.GetNumActuators() == 1);
        TEST_REQUIRE(copy.IsLooping()       == true);
        TEST_REQUIRE(copy.GetName()         == "test");
        TEST_REQUIRE(copy.GetDuration()     == real::float32(10.0f));
        TEST_REQUIRE(copy.GetId()           == track.GetId());
        TEST_REQUIRE(copy.GetHash()         == track.GetHash());
        copy = track;
        TEST_REQUIRE(copy.GetId()           == track.GetId());
        TEST_REQUIRE(copy.GetHash()         == track.GetHash());
    }

    // clone
    {
        auto clone = track.Clone();
        TEST_REQUIRE(clone.GetId() != track.GetId());
        TEST_REQUIRE(clone.GetHash() != track.GetHash());
    }

    // instance
    {
        game::Animation instance(track);
        TEST_REQUIRE(!instance.IsComplete());

        instance.Update(5.0f);
        instance.Apply(node);

        instance.Update(5.0f);
        instance.Apply(node);

        TEST_REQUIRE(instance.IsComplete());
        TEST_REQUIRE(node.GetTranslation() == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(node.GetSize() == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(node.GetScale() == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(node.GetRotation() == real::float32(1.5f));
    }
}
void unit_test_animation_complete()
{
    TEST_CASE(test::Type::Feature)

    game::EntityNodeClass node0;
    node0.SetName("node0");
    node0.SetTranslation(glm::vec2(5.0f, 5.0f));
    node0.SetSize(glm::vec2(1.0f, 1.0f));
    node0.SetRotation(0.0f);
    node0.SetScale(glm::vec2(1.0f, 1.0f));

    game::AnimationClass anim;
    anim.SetName("test");
    anim.SetLooping(false);
    anim.SetDuration(1.0f);
    anim.SetDelay(0.0f);
    game::TransformActuatorClass transform;
    transform.SetNodeId(node0.GetId());
    anim.AddActuator(transform);

    game::EntityClass klass;
    klass.SetName("entity");
    klass.AddAnimation(anim);
    klass.LinkChild(nullptr, klass.AddNode(node0));

    auto entity = game::CreateEntityInstance(klass);
    TEST_REQUIRE(entity->PlayAnimationByName("test"));
    TEST_REQUIRE(entity->IsAnimating());
    TEST_REQUIRE(entity->GetCurrentAnimation());
    TEST_REQUIRE(entity->GetCurrentAnimation()->GetClassName() == "test");

    entity->Update(0.5f);
    TEST_REQUIRE(entity->IsAnimating());
    TEST_REQUIRE(entity->GetCurrentAnimation());
    TEST_REQUIRE(entity->GetCurrentAnimation()->GetClassName() == "test");
    entity->Update(0.6f);
    TEST_REQUIRE(!entity->IsAnimating());
    TEST_REQUIRE(!entity->GetCurrentAnimation());
    TEST_REQUIRE(entity->GetFinishedAnimation());
    TEST_REQUIRE(entity->GetFinishedAnimation()->GetClassName() == "test");

    entity->Update(0.1f);
    TEST_REQUIRE(!entity->IsAnimating());
    TEST_REQUIRE(!entity->GetCurrentAnimation());
    TEST_REQUIRE(!entity->GetFinishedAnimation());
}

void unit_test_animation_state()
{
    TEST_CASE(test::Type::Feature)

    game::AnimatorClass klass;

    game::AnimationStateClass idle;
    idle.SetName("idle");
    klass.AddState(idle);

    game::AnimationStateClass run;
    run.SetName("run");
    klass.AddState(run);

    game::AnimationStateClass jump;
    jump.SetName("jump");
    klass.AddState(jump);

    game::AnimationStateTransitionClass idle_to_run;
    idle_to_run.SetName("idle to run");
    idle_to_run.SetSrcStateId(idle.GetId());
    idle_to_run.SetDstStateId(run.GetId());
    klass.AddTransition(idle_to_run);

    game::AnimationStateTransitionClass run_to_idle;
    run_to_idle.SetName("run to idle");
    run_to_idle.SetSrcStateId(run.GetId());
    run_to_idle.SetDstStateId(idle.GetId());
    run_to_idle.SetDuration(1.0f);
    klass.AddTransition(run_to_idle);

    game::AnimationStateTransitionClass idle_to_jump;
    idle_to_jump.SetName("idle to jump");
    idle_to_jump.SetSrcStateId(idle.GetId());
    idle_to_jump.SetDstStateId(jump.GetId());
    klass.AddTransition(idle_to_jump);

    game::AnimationStateTransitionClass jump_to_idle;
    jump_to_idle.SetName("jump to idle");
    jump_to_idle.SetSrcStateId(jump.GetId());
    jump_to_idle.SetDstStateId(idle.GetId());
    klass.AddTransition(jump_to_idle);

    klass.SetInitialStateId(idle.GetId());

    auto anim = game::CreateAnimatorInstance(klass);
    std::vector<game::Animator::Action> actions;

    anim->Update(0.0f, &actions);
    TEST_REQUIRE(anim->GetAnimatorState() == game::Animator::State::InState);
    TEST_REQUIRE(anim->GetCurrentState()->GetId() == idle.GetId());
    TEST_REQUIRE(actions.size() == 4);
    TEST_REQUIRE(std::get_if<game::Animator::EnterState>(&actions[0]));
    TEST_REQUIRE(std::get_if<game::Animator::UpdateState>(&actions[1]));
    TEST_REQUIRE(std::get_if<game::Animator::EvalTransition>(&actions[2])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::Animator::EvalTransition>(&actions[2])->to->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::Animator::EvalTransition>(&actions[3])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::Animator::EvalTransition>(&actions[3])->to->GetName() == "jump");

    // immediate transition from idle to run
    {
        auto* eval = std::get_if<game::Animator::EvalTransition>(&actions[2]);
        anim->Update(eval->transition, eval->to);
    }

    TEST_REQUIRE(anim->GetAnimatorState() == game::Animator::State::InTransition);
    TEST_REQUIRE(anim->GetCurrentState() == nullptr);
    TEST_REQUIRE(anim->GetPrevState()->GetName() == "idle");
    TEST_REQUIRE(anim->GetNextState()->GetName() == "run");
    TEST_REQUIRE(anim->GetTransition()->GetName() == "idle to run");

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 5);
    TEST_REQUIRE(std::get_if<game::Animator::LeaveState>(&actions[0])->state->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::Animator::StartTransition>(&actions[1])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::Animator::StartTransition>(&actions[1])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::Animator::StartTransition>(&actions[1])->to->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::Animator::UpdateTransition>(&actions[2])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::Animator::FinishTransition>(&actions[3])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::Animator::EnterState>(&actions[4])->state->GetName() == "run");

    TEST_REQUIRE(anim->GetCurrentState()->GetName() == "run");
    TEST_REQUIRE(anim->GetPrevState() == nullptr);
    TEST_REQUIRE(anim->GetNextState() == nullptr);
    TEST_REQUIRE(anim->GetTransition() == nullptr);
    TEST_REQUIRE(anim->GetAnimatorState() == game::Animator::State::InState);

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 2);
    TEST_REQUIRE(std::get_if<game::Animator::UpdateState>(&actions[0])->state->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::Animator::EvalTransition>(&actions[1])->transition->GetName() == "run to idle");

    // begin transition from run to idle.
    {
        auto* eval = std::get_if<game::Animator::EvalTransition>(&actions[1]);
        anim->Update(eval->transition, eval->to);
    }

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 3);
    TEST_REQUIRE(std::get_if<game::Animator::LeaveState>(&actions[0])->state->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::Animator::StartTransition>(&actions[1])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::Animator::UpdateTransition>(&actions[2])->transition->GetName() == "run to idle");

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 1);
    TEST_REQUIRE(std::get_if<game::Animator::UpdateTransition>(&actions[0])->transition->GetName() == "run to idle");

    actions.clear();
    anim->Update(1.0f - (2 * 1.0f/60.0f), &actions);
    TEST_REQUIRE(actions.size() == 3);
    TEST_REQUIRE(std::get_if<game::Animator::UpdateTransition>(&actions[0])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::Animator::FinishTransition>(&actions[1])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::Animator::EnterState>(&actions[2])->state->GetName() == "idle");

}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_setflag_actuator();
    unit_test_setval_actuator();
    unit_test_transform_actuator();
    unit_test_kinematic_actuator();
    unit_test_material_actuator();
    unit_test_animation_track();
    unit_test_animation_complete();
    unit_test_animation_state();
    return 0;
}
) // TEST_MAIN