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
#include "gamelib/animation.h"
#include "gamelib/entity.h"

bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
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

    TEST_REQUIRE(klass.GetInterpolation()       == game::TransformActuatorClass::Interpolation::Cosine);
    TEST_REQUIRE(klass.GetNodeId()              == "1234");
    TEST_REQUIRE(klass.GetStartTime()           == real::float32(0.1f));
    TEST_REQUIRE(klass.GetDuration()            == real::float32(0.5f));
    TEST_REQUIRE(klass.GetEndLinearVelocity()   == glm::vec2(1.0f, 2.0f));
    TEST_REQUIRE(klass.GetEndAngularVelocity()  == real::float32(3.0f));

    // serialize
    {
        game::KinematicActuatorClass copy;
        TEST_REQUIRE(copy.FromJson(klass.ToJson()));
        TEST_REQUIRE(copy.GetInterpolation()       == game::TransformActuatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocity() == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocity() == real::float32(3.0f));

        instance.Apply(node, 0.0f);
        TEST_REQUIRE(node.GetRigidBody()->GetLinearVelocity() == glm::vec2(0.0f, 1.0f));
        TEST_REQUIRE(node.GetRigidBody()->GetAngularVelocity() == real::float32(5.0f));

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
        game::TransformActuatorClass copy;
        copy.FromJson(act.ToJson());
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
        auto ret = game::AnimationTrackClass::FromJson(track.ToJson());
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
    unit_test_transform_actuator();
    unit_test_kinematic_actuator();
    unit_test_animation_track();
    return 0;
}
