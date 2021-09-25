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
#include "base/assert.h"
#include "base/math.h"
#include "data/json.h"
#include "game/animation.h"
#include "game/entity.h"


void unit_test_setflag_actuator()
{
    game::SetFlagActuatorClass klass;
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
        game::SetFlagActuator instance(klass);

        game::EntityNodeClass node_klass;
        game::DrawableItemClass draw_class;
        draw_class.SetFlag(game::DrawableItemClass::Flags::VisibleInGame, true);
        node_klass.SetDrawable(draw_class);

        // create node instance
        game::EntityNode node(node_klass);
        // start based on the node.
        instance.Start(node);
        instance.Finish(node);
        TEST_REQUIRE(node.GetDrawable()->TestFlag(game::DrawableItemClass::Flags::VisibleInGame) == false);
    }

}

void unit_test_kinematic_actuator()
{
    game::KinematicActuatorClass klass;
    klass.SetNodeId("1234");
    klass.SetStartTime(0.1f);
    klass.SetDuration(0.5f);
    klass.SetInterpolation(game::TransformActuatorClass::Interpolation::Cosine);
    klass.SetEndAngularVelocity(3.0f);
    klass.SetEndLinearVelocity(glm::vec2(1.0f, 2.0f));

    TEST_REQUIRE(klass.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
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
        body.SetLinearVelocity(glm::vec2(0.0f, 1.0f));
        body.SetAngularVelocity(5.0f);
        klass.SetRigidBody(body);

        // create node instance
        game::EntityNode node(klass);

        // start based on the node.
        instance.Start(node);

        instance.Apply(node, 1.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(3.0f));

        instance.Apply(node, 0.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocityAdjustment() == glm::vec2(0.0f, 1.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocityAdjustment() == real::float32(5.0f));

    }
}

void unit_test_transform_actuator()
{
    game::TransformActuatorClass act;
    act.SetNodeId("123");
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::TransformActuatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    TEST_REQUIRE(act.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(copy.GetNodeId()        == "123");
        TEST_REQUIRE(copy.GetStartTime()     == real::float32(0.1f));
        TEST_REQUIRE(copy.GetDuration()      == real::float32(0.5f));
        TEST_REQUIRE(copy.GetEndPosition()   == glm::vec2(100.0f, 50.0f));
        TEST_REQUIRE(copy.GetEndSize()       == glm::vec2(5.0f, 6.0f));
        TEST_REQUIRE(copy.GetEndScale()      == glm::vec2(3.0f, 8.0f));
        TEST_REQUIRE(copy.GetEndRotation()   == real::float32(1.5f));
        TEST_REQUIRE(copy.GetId()   == act.GetId());
        TEST_REQUIRE(copy.GetHash() == act.GetHash());
    }

    // copy assignment and copy ctor
    {
        auto copy(act);
        TEST_REQUIRE(copy.GetHash() == act.GetHash());
        TEST_REQUIRE(copy.GetId()   == act.GetId());
        TEST_REQUIRE(copy.GetInterpolation() == game::TransformActuatorClass::Interpolation::Cosine);
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


void unit_test_animation_track()
{
    game::EntityNodeClass klass;
    klass.SetTranslation(glm::vec2(5.0f, 5.0f));
    klass.SetSize(glm::vec2(1.0f, 1.0f));
    klass.SetRotation(0.0f);
    klass.SetScale(glm::vec2(1.0f, 1.0f));

    // create node instance
    game::EntityNode node(klass);

    game::AnimationTrackClass track;
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

        auto ret = game::AnimationTrackClass::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNumActuators() == 1);
        TEST_REQUIRE(ret->IsLooping()       == true);
        TEST_REQUIRE(ret->GetName()         == "test");
        TEST_REQUIRE(ret->GetDuration()     == real::float32(10.0f));
        TEST_REQUIRE(ret->GetId()           == track.GetId());
        TEST_REQUIRE(ret->GetHash()         == track.GetHash());
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
        game::AnimationTrack instance(track);
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


int test_main(int argc, char* argv[])
{
    unit_test_setflag_actuator();
    unit_test_transform_actuator();
    unit_test_kinematic_actuator();
    unit_test_animation_track();
    return 0;
}
