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
#include "game/animator.h"
#include "game/transform_animator.h"
#include "game/kinematic_animator.h"
#include "game/material_animator.h"
#include "game/property_animator.h"
#include "game/entity.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_light.h"

void apply_flag(game::BooleanPropertyAnimatorClass::PropertyName flag,
                game::BooleanPropertyAnimatorClass::PropertyAction action,
                game::EntityNode& node)
{
    game::BooleanPropertyAnimatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetFlagName(flag);
    klass.SetFlagAction(action);

    game::BooleanPropertyAnimator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void apply_value(game::PropertyAnimatorClass::PropertyName name,
                 game::PropertyAnimatorClass::PropertyValue value,
                 game::EntityNode& node)
{
    game::PropertyAnimatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetPropertyName(name);
    klass.SetEndValue(value);

    game::PropertyAnimator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void apply_material_value(const char* name,
                          game::MaterialAnimatorClass::MaterialParam  value,
                          game::EntityNode& node)
{
    game::MaterialAnimatorClass klass;
    klass.SetNodeId(node.GetClassId());
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetMaterialParam(name, value);

    game::MaterialAnimator actuator(klass);
    actuator.Start(node);
    actuator.Finish(node);
}

void unit_test_setflag_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::BooleanPropertyAnimatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetFlagName(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame);
    klass.SetFlagAction(game::BooleanPropertyAnimatorClass::PropertyAction::Off);
    klass.SetJointId("joint123");

    TEST_REQUIRE(klass.GetNodeId() == "1234");
    TEST_REQUIRE(klass.GetStartTime() == real::float32(0.1f));
    TEST_REQUIRE(klass.GetDuration()  == real::float32(0.5f));
    TEST_REQUIRE(klass.GetFlagName() == game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame);
    TEST_REQUIRE(klass.GetFlagAction() == game::BooleanPropertyAnimatorClass::PropertyAction::Off);

    // serialize
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::BooleanPropertyAnimatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
        TEST_REQUIRE(copy.GetName() == "test");
        TEST_REQUIRE(copy.GetNodeId() == "1234");
        TEST_REQUIRE(copy.GetStartTime() == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()  == real::float32(0.5f));
        TEST_REQUIRE(copy.GetFlagName() == game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame);
        TEST_REQUIRE(copy.GetFlagAction() == game::BooleanPropertyAnimatorClass::PropertyAction::Off);
        TEST_REQUIRE(copy.GetJointId() == "joint123");
    }

    // copy assignment
    {
        game::BooleanPropertyAnimatorClass copy(klass);
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

        game::RigidBodyClass rigid_body_class;
        rigid_body_class.SetFlag(game::RigidBodyClass::Flags::Bullet, true);
        rigid_body_class.SetFlag(game::RigidBodyClass::Flags::Sensor, true);
        rigid_body_class.SetFlag(game::RigidBodyClass::Flags::Enabled, true);
        rigid_body_class.SetFlag(game::RigidBodyClass::Flags::CanSleep, true);
        rigid_body_class.SetFlag(game::RigidBodyClass::Flags::DiscardRotation, true);
        node_klass.SetRigidBody(rigid_body_class);

        game::TextItemClass text_class;
        text_class.SetFlag(game::TextItemClass::Flags::VisibleInGame, true);
        text_class.SetFlag(game::TextItemClass::Flags::UnderlineText, true);
        text_class.SetFlag(game::TextItemClass::Flags::BlinkText, true);
        node_klass.SetTextItem(text_class);

        game::BasicLightClass light_class;
        light_class.SetFlag(game::BasicLightClass::Flags::Enabled, false);
        node_klass.SetBasicLight(light_class);

        // create node instance
        game::EntityNode node(node_klass);
        const auto* draw = node.GetDrawable();
        const auto* body = node.GetRigidBody();
        const auto* text = node.GetTextItem();
        const auto* light = node.GetBasicLight();

        // drawable flags.
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_UpdateDrawable,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_UpdateMaterial,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_FlipHorizontally,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_Restart,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::VisibleInGame) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateDrawable) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateMaterial) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::FlipHorizontally) == false);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::RestartDrawable) == false);

        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_VisibleInGame,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_UpdateDrawable,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_UpdateMaterial,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_FlipHorizontally,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::Drawable_Restart,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::VisibleInGame)   == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateDrawable)  == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::UpdateMaterial)  == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::FlipHorizontally) == true);
        TEST_REQUIRE(draw->TestFlag(game::DrawableItem::Flags::RestartDrawable) == true);

        // ridig body flags.
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Bullet,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Sensor,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Enabled,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_CanSleep,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_DiscardRotation,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Bullet) == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Sensor) == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::CanSleep) == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Enabled) == false);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::DiscardRotation) == false);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Bullet,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Sensor,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_Enabled,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_CanSleep,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::RigidBody_DiscardRotation,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Bullet) == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Sensor) == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::CanSleep) == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::Enabled) == true);
        TEST_REQUIRE(body->TestFlag(game::RigidBody::Flags::DiscardRotation) == true);

        // text item flags.
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_Blink,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_Underline,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_VisibleInGame,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::BlinkText) == false);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::UnderlineText) == false);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::VisibleInGame) == false);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_Blink,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_Underline,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::TextItem_VisibleInGame,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::BlinkText) == true);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::UnderlineText) == true);
        TEST_REQUIRE(text->TestFlag(game::TextItemClass::Flags::VisibleInGame) == true);

        // Basic light flags.
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::BasicLight_Enabled,
                   game::BooleanPropertyAnimatorClass::PropertyAction::On, node);
        TEST_REQUIRE(light->TestFlag(game::BasicLightClass::Flags::Enabled) == true);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::BasicLight_Enabled,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Off, node);
        TEST_REQUIRE(light->TestFlag(game::BasicLightClass::Flags::Enabled) == false);
        apply_flag(game::BooleanPropertyAnimatorClass::PropertyName::BasicLight_Enabled,
                   game::BooleanPropertyAnimatorClass::PropertyAction::Toggle, node);
        TEST_REQUIRE(light->TestFlag(game::BasicLightClass::Flags::Enabled) == true);
    }
}

void unit_test_setval_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::PropertyAnimatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::PropertyAnimatorClass::Interpolation::Cosine);
    klass.SetPropertyName(game::PropertyAnimatorClass::PropertyName::RigidBody_LinearVelocity);
    klass.SetEndValue(glm::vec2(2.0f, -3.0f));
    klass.SetJointId("joint123");

    // serialize.
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::PropertyAnimatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformAnimatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetJointId() == "joint123");
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "1234");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetPropertyName() == game::PropertyAnimatorClass::PropertyName::RigidBody_LinearVelocity);
        TEST_REQUIRE(*copy.GetEndValue<glm::vec2>() == glm::vec2(2.0f, -3.0f));
        TEST_REQUIRE(copy.GetId() == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy assignment and ctor.
    {
        game::PropertyAnimatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformAnimatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetJointId()       == "joint123");
        TEST_REQUIRE(copy.GetName()          == "test");
        TEST_REQUIRE(copy.GetNodeId()        == "1234");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetPropertyName() == game::PropertyAnimatorClass::PropertyName::RigidBody_LinearVelocity);
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

        game::RigidBodyClass rigid_body_class;
        rigid_body_class.SetAngularDamping(-6.0f);
        node_klass.SetRigidBody(rigid_body_class);

        game::TextItemClass text_class;
        text_class.SetText("text");
        text_class.SetTextColor(game::Color::HotPink);
        node_klass.SetTextItem(text_class);

        game::BasicLightClass light_class;
        light_class.SetDirection(glm::vec3{1.0f, 0.0f, 0.0f});
        light_class.SetTranslation(glm::vec3(0.0f, 0.0f, 0.0f));
        light_class.SetAmbientColor(game::Color::White);
        light_class.SetDiffuseColor(game::Color::White);
        light_class.SetSpecularColor(game::Color::White);
        light_class.SetConstantAttenuation(1.0f);
        light_class.SetLinearAttenuation(0.0f);
        light_class.SetQuadraticAttenuation(0.0f);
        light_class.SetSpotHalfAngle(game::FDegrees(0.0f));
        node_klass.SetBasicLight(light_class);

        game::EntityNode node(node_klass);

        const auto* draw = node.GetDrawable();
        const auto* body = node.GetRigidBody();
        const auto* text = node.GetTextItem();
        const auto* light = node.GetBasicLight();

        apply_value(game::PropertyAnimatorClass::PropertyName::Drawable_TimeScale, 2.0f, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::RigidBody_LinearVelocity, glm::vec2(-1.0f, -1.0f), node);
        apply_value(game::PropertyAnimatorClass::PropertyName::RigidBody_AngularVelocity, 4.0f, node);
        TEST_REQUIRE(draw->GetTimeScale() == real::float32(2.0f));
        TEST_REQUIRE(body->GetLinearVelocityAdjustment() == glm::vec2(-1.0f, -1.0f));
        TEST_REQUIRE(body->GetAngularVelocityAdjustment() == real::float32(4.0f));


        apply_value(game::PropertyAnimatorClass::PropertyName::TextItem_Text, std::string("hello"), node);
        apply_value(game::PropertyAnimatorClass::PropertyName::TextItem_Color, game::Color::Blue, node);
        TEST_REQUIRE(text->GetTextColor() == game::Color4f(game::Color::Blue));
        TEST_REQUIRE(text->GetText() == "hello");

        // basic light
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_Direction, glm::vec3(0.0f, 1.0f, 0.0f), node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_Translation, glm::vec3(0.0f, 0.0f, 100.0f), node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_AmbientColor, game::Color::Red, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_DiffuseColor, game::Color::Green, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_SpecularColor, game::Color::Blue, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_ConstantAttenuation, 2.0f, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_LinearAttenuation, 3.0f, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_QuadraticAttenuation, 4.0f, node);
        apply_value(game::PropertyAnimatorClass::PropertyName::BasicLight_SpotHalfAngle, 180.0f, node);

        TEST_REQUIRE(light->GetDirection() == glm::vec3(0.0f, 1.0f, 0.0f));
        TEST_REQUIRE(light->GetTranslation() == glm::vec3(0.0f, 0.0f, 100.0f));
        TEST_REQUIRE(light->GetAmbientColor() == game::Color::Red);
        TEST_REQUIRE(light->GetDiffuseColor() == game::Color::Green);
        TEST_REQUIRE(light->GetSpecularColor() == game::Color::Blue);
        TEST_REQUIRE(light->GetConstantAttenuation() == 2.0f);
        TEST_REQUIRE(light->GetLinearAttenuation() == 3.0f);
        TEST_REQUIRE(light->GetQuadraticAttenuation() == 4.0f);
        TEST_REQUIRE(light->GetSpotHalfAngle().ToDegrees() == 180.0f);
    }

}

void unit_test_kinematic_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::KinematicAnimatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetTarget(game::KinematicAnimatorClass::Target::RigidBody);
    klass.SetInterpolation(game::TransformAnimatorClass::Interpolation::Cosine);
    klass.SetEndAngularVelocity(3.0f);
    klass.SetEndAngularAcceleration(5.0f);
    klass.SetEndLinearVelocity(glm::vec2(1.0f, 2.0f));
    klass.SetEndLinearAcceleration(glm::vec2(-1.0f, -2.0f));

    TEST_REQUIRE(klass.GetInterpolation()          == game::TransformAnimatorClass::Interpolation::Cosine);
    TEST_REQUIRE(klass.GetName()                   == "test");
    TEST_REQUIRE(klass.GetNodeId()                 == "1234");
    TEST_REQUIRE(klass.GetStartTime()              == real::float32(0.1f));
    TEST_REQUIRE(klass.GetDuration()               == real::float32(0.5f));
    TEST_REQUIRE(klass.GetEndAngularVelocity()     == real::float32(3.0f));
    TEST_REQUIRE(klass.GetEndAngularAcceleration() == real::float32(5.0f));
    TEST_REQUIRE(klass.GetEndLinearVelocity()      == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(klass.GetEndLinearAcceleration()  == glm::vec2(-1.0f, -2.0f));

    // serialize
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::KinematicAnimatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation()          == game::TransformAnimatorClass::Interpolation::Cosine);
        TEST_REQUIRE(copy.GetName()                   == "test");
        TEST_REQUIRE(copy.GetNodeId()                 == "1234");
        TEST_REQUIRE(copy.GetStartTime()              == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()               == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndAngularVelocity()     == real::float32(3.0f));
        TEST_REQUIRE(copy.GetEndAngularAcceleration() == real::float32(5.0f));
        TEST_REQUIRE(copy.GetEndLinearVelocity()      == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(copy.GetEndLinearAcceleration()  == glm::vec2(-1.0f, -2.0f));

        TEST_REQUIRE(copy.GetId()   == klass.GetId());
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    // copy assignment and copy ctor
    {
        game::KinematicAnimatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation()       == game::TransformAnimatorClass::Interpolation::Cosine);
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
        game::KinematicAnimator instance(klass);

        game::EntityNodeClass klass;
        game::RigidBodyClass body;
        klass.SetRigidBody(body);

        // create node instance
        game::EntityNode node(klass);

        // start based on the node.
        instance.Start(node);

        instance.Apply(node, 1.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(3.0f));

        node.GetRigidBody()->ClearPhysicsAdjustments();

        instance.Apply(node, 0.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(0.0f, 0.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(0.0f));

    }
}

void unit_test_transform_actuator()
{
    TEST_CASE(test::Type::Feature)

    game::TransformAnimatorClass act;
    act.SetName("test");
    act.SetNodeId("123");
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::TransformAnimatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    TEST_REQUIRE(act.GetInterpolation() == game::TransformAnimatorClass::Interpolation::Cosine);
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
        game::TransformAnimatorClass copy;
        copy.FromJson(json);
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformAnimatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformAnimatorClass::Interpolation::Cosine);
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
        game::TransformAnimator instance(act);
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

    game::MaterialAnimatorClass klass;
    klass.SetName("test");
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::MaterialAnimatorClass::Interpolation::Cosine);
    klass.SetMaterialParam("kColor", game::Color4f(game::Color::Green));

    // serialize.
    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::MaterialAnimatorClass copy;
        TEST_REQUIRE(copy.FromJson(json));
        TEST_REQUIRE(copy.GetInterpolation()              == game::TransformAnimatorClass::Interpolation::Cosine);
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
        game::MaterialAnimatorClass copy(klass);
        TEST_REQUIRE(copy.GetInterpolation()              == game::TransformAnimatorClass::Interpolation::Cosine);
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
    TEST_REQUIRE(track.GetNumAnimators() == 0);

    game::TransformAnimatorClass act;
    act.SetNodeId(node.GetClassId());
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::TransformAnimatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    track.AddAnimator(act);
    TEST_REQUIRE(track.GetNumAnimators() == 1);

    // serialize
    {
        data::JsonObject json;
        track.IntoJson(json);

        game::AnimationClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetNumAnimators() == 1);
        TEST_REQUIRE(ret.IsLooping()       == true);
        TEST_REQUIRE(ret.GetName()         == "test");
        TEST_REQUIRE(ret.GetDuration()     == real::float32(10.0f));
        TEST_REQUIRE(ret.GetId()           == track.GetId());
        TEST_REQUIRE(ret.GetHash()         == track.GetHash());
    }

    // copy assignment and copy ctor
    {
        auto copy(track);
        TEST_REQUIRE(copy.GetNumAnimators() == 1);
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
    game::TransformAnimatorClass transform;
    transform.SetNodeId(node0.GetId());
    anim.AddAnimator(transform);

    game::EntityClass klass;
    klass.SetName("entity");
    klass.AddAnimation(anim);
    klass.LinkChild(nullptr, klass.AddNode(node0));

    auto entity = game::CreateEntityInstance(klass);
    TEST_REQUIRE(entity->PlayAnimationByName("test"));
    TEST_REQUIRE(entity->IsAnimating());
    TEST_REQUIRE(entity->GetCurrentAnimation(0));
    TEST_REQUIRE(entity->GetCurrentAnimation(0)->GetClassName() == "test");

    entity->Update(0.5f);
    TEST_REQUIRE(entity->IsAnimating());
    TEST_REQUIRE(entity->GetCurrentAnimation(0));
    TEST_REQUIRE(entity->GetCurrentAnimation(0)->GetClassName() == "test");
    entity->Update(0.6f);
    TEST_REQUIRE(!entity->IsAnimating());
    TEST_REQUIRE(entity->GetNumCurrentAnimations() == 0);
    TEST_REQUIRE(entity->GetFinishedAnimations()[0]->GetClassName() == "test");

    entity->Update(0.1f);
    TEST_REQUIRE(!entity->IsAnimating());
    TEST_REQUIRE(entity->GetNumCurrentAnimations() == 0);
    TEST_REQUIRE(entity->GetFinishedAnimations().empty());
}

void unit_test_animation_state()
{
    TEST_CASE(test::Type::Feature)

    game::EntityStateControllerClass klass;

    game::EntityStateClass idle;
    idle.SetName("idle");
    klass.AddState(idle);

    game::EntityStateClass run;
    run.SetName("run");
    klass.AddState(run);

    game::EntityStateClass jump;
    jump.SetName("jump");
    klass.AddState(jump);

    game::EntityStateTransitionClass idle_to_run;
    idle_to_run.SetName("idle to run");
    idle_to_run.SetSrcStateId(idle.GetId());
    idle_to_run.SetDstStateId(run.GetId());
    klass.AddTransition(idle_to_run);

    game::EntityStateTransitionClass run_to_idle;
    run_to_idle.SetName("run to idle");
    run_to_idle.SetSrcStateId(run.GetId());
    run_to_idle.SetDstStateId(idle.GetId());
    run_to_idle.SetDuration(1.0f);
    klass.AddTransition(run_to_idle);

    game::EntityStateTransitionClass idle_to_jump;
    idle_to_jump.SetName("idle to jump");
    idle_to_jump.SetSrcStateId(idle.GetId());
    idle_to_jump.SetDstStateId(jump.GetId());
    klass.AddTransition(idle_to_jump);

    game::EntityStateTransitionClass jump_to_idle;
    jump_to_idle.SetName("jump to idle");
    jump_to_idle.SetSrcStateId(jump.GetId());
    jump_to_idle.SetDstStateId(idle.GetId());
    klass.AddTransition(jump_to_idle);

    klass.SetInitialStateId(idle.GetId());

    auto anim = game::CreateStateControllerInstance(klass);
    std::vector<game::Entity::EntityStateUpdate> actions;

    anim->Update(0.0f, &actions);
    TEST_REQUIRE(anim->GetControllerState() == game::EntityStateController::State::InState);
    TEST_REQUIRE(anim->GetCurrentState()->GetId() == idle.GetId());
    TEST_REQUIRE(actions.size() == 4);
    TEST_REQUIRE(std::get_if<game::EntityStateController::EnterState>(&actions[0]));
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateState>(&actions[1]));
    TEST_REQUIRE(std::get_if<game::EntityStateController::EvalTransition>(&actions[2])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EvalTransition>(&actions[2])->to->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EvalTransition>(&actions[3])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EvalTransition>(&actions[3])->to->GetName() == "jump");

    // immediate transition from idle to run
    {
        auto* eval = std::get_if<game::EntityStateController::EvalTransition>(&actions[2]);
        anim->BeginStateTransition(eval->transition, eval->to);
    }

    TEST_REQUIRE(anim->GetControllerState() == game::EntityStateController::State::InTransition);
    TEST_REQUIRE(anim->GetCurrentState() == nullptr);
    TEST_REQUIRE(anim->GetPrevState()->GetName() == "idle");
    TEST_REQUIRE(anim->GetNextState()->GetName() == "run");
    TEST_REQUIRE(anim->GetTransition()->GetName() == "idle to run");

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 5);
    TEST_REQUIRE(std::get_if<game::EntityStateController::LeaveState>(&actions[0])->state->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::StartTransition>(&actions[1])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::StartTransition>(&actions[1])->from->GetName() == "idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::StartTransition>(&actions[1])->to->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateTransition>(&actions[2])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::FinishTransition>(&actions[3])->transition->GetName() == "idle to run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EnterState>(&actions[4])->state->GetName() == "run");

    TEST_REQUIRE(anim->GetCurrentState()->GetName() == "run");
    TEST_REQUIRE(anim->GetPrevState() == nullptr);
    TEST_REQUIRE(anim->GetNextState() == nullptr);
    TEST_REQUIRE(anim->GetTransition() == nullptr);
    TEST_REQUIRE(anim->GetControllerState() == game::EntityStateController::State::InState);

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 2);
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateState>(&actions[0])->state->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EvalTransition>(&actions[1])->transition->GetName() == "run to idle");

    // begin transition from run to idle.
    {
        auto* eval = std::get_if<game::EntityStateController::EvalTransition>(&actions[1]);
        anim->BeginStateTransition(eval->transition, eval->to);
    }

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 3);
    TEST_REQUIRE(std::get_if<game::EntityStateController::LeaveState>(&actions[0])->state->GetName() == "run");
    TEST_REQUIRE(std::get_if<game::EntityStateController::StartTransition>(&actions[1])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateTransition>(&actions[2])->transition->GetName() == "run to idle");

    actions.clear();
    anim->Update(1.0f/60.0f, &actions);
    TEST_REQUIRE(actions.size() == 1);
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateTransition>(&actions[0])->transition->GetName() == "run to idle");

    actions.clear();
    anim->Update(1.0f - (2 * 1.0f/60.0f), &actions);
    TEST_REQUIRE(actions.size() == 3);
    TEST_REQUIRE(std::get_if<game::EntityStateController::UpdateTransition>(&actions[0])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::FinishTransition>(&actions[1])->transition->GetName() == "run to idle");
    TEST_REQUIRE(std::get_if<game::EntityStateController::EnterState>(&actions[2])->state->GetName() == "idle");

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