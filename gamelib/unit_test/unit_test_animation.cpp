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
#include "gamelib/gfxfactory.h"

bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

class TestFactory : public game::GfxFactory
{
public:
    // GfxFactory
    virtual std::shared_ptr<const gfx::MaterialClass> GetMaterialClass(const std::string& name) const override
    {
        return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color::Yellow));
    }
    virtual std::shared_ptr<const gfx::DrawableClass> GetDrawableClass(const std::string& name) const override
    {
        return std::make_shared<gfx::CircleClass>();
    }

    virtual std::shared_ptr<gfx::Material> MakeMaterial(const std::string& name) const override
    {
        ASSERT(!"No such material.");
        return nullptr;
    }
    virtual std::shared_ptr<gfx::Drawable> MakeDrawable(const std::string& name) const override
    {
        ASSERT(!"No such drawable.");
        return nullptr;
    }
};

TestFactory* g_factory = nullptr;

void unit_test_animation_node()
{
    game::AnimationNodeClass node;
    node.SetName("root");
    node.SetDrawable("rectangle");
    node.SetMaterial("test");
    node.SetSize(glm::vec2(100.0f, 100.0f));
    node.SetTranslation(glm::vec2(150.0f, -150.0f));
    node.SetScale(glm::vec2(4.0f, 5.0f));
    node.SetRotation(1.5f);
    node.SetRenderPass(game::AnimationNodeClass::RenderPass::Mask);
    node.SetFlag(game::AnimationNodeClass::Flags::UpdateDrawable, true);
    node.SetFlag(game::AnimationNodeClass::Flags::RestartDrawable, false);
    node.SetLayer(10);
    node.SetLineWidth(5.0f);

    TEST_REQUIRE(node.GetName()         == "root");
    TEST_REQUIRE(node.GetDrawableName() == "rectangle");
    TEST_REQUIRE(node.GetMaterialName() == "test");
    TEST_REQUIRE(node.GetSize()         == glm::vec2(100.0f, 100.0f));
    TEST_REQUIRE(node.GetTranslation()  == glm::vec2(150.0f, -150.0f));
    TEST_REQUIRE(node.GetScale()        == glm::vec2(4.0f, 5.0f));
    TEST_REQUIRE(node.GetRotation()     == real::float32(1.5f));
    TEST_REQUIRE(node.GetLineWidth()    == real::float32(5.0f));
    TEST_REQUIRE(node.GetRenderPass()   == game::AnimationNodeClass::RenderPass::Mask);
    TEST_REQUIRE(node.TestFlag(game::AnimationNodeClass::Flags::UpdateDrawable) == true);
    TEST_REQUIRE(node.TestFlag(game::AnimationNodeClass::Flags::RestartDrawable) == false);
    TEST_REQUIRE(node.GetLayer() == 10);

    // to/from json
    {
        auto ret = game::AnimationNodeClass::FromJson(node.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName()         == "root");
        TEST_REQUIRE(ret->GetDrawableName() == "rectangle");
        TEST_REQUIRE(ret->GetMaterialName() == "test");
        TEST_REQUIRE(ret->GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(ret->GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(ret->GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(ret->GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(ret->GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(ret->GetRenderPass()   == game::AnimationNodeClass::RenderPass::Mask);
        TEST_REQUIRE(ret->TestFlag(game::AnimationNodeClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(ret->TestFlag(game::AnimationNodeClass::Flags::RestartDrawable) == false);
        TEST_REQUIRE(ret->GetHash() == node.GetHash());
    }

    // test copy and copy ctor
    {
        auto copy(node);
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId()   == node.GetId());
        copy = node;
        TEST_REQUIRE(copy.GetHash() == node.GetHash());
        TEST_REQUIRE(copy.GetId()   == node.GetId());
    }

    // test clone
    {
        auto clone = node.Clone();
        TEST_REQUIRE(clone.GetHash() != node.GetHash());
        TEST_REQUIRE(clone.GetId()   != node.GetId());
        TEST_REQUIRE(clone.GetName()         == "root");
        TEST_REQUIRE(clone.GetDrawableName() == "rectangle");
        TEST_REQUIRE(clone.GetMaterialName() == "test");
        TEST_REQUIRE(clone.GetSize()         == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(clone.GetTranslation()  == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(clone.GetScale()        == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(clone.GetRotation()     == real::float32(1.5f));
        TEST_REQUIRE(clone.GetLineWidth()    == real::float32(5.0f));
        TEST_REQUIRE(clone.GetRenderPass()   == game::AnimationNodeClass::RenderPass::Mask);
        TEST_REQUIRE(clone.TestFlag(game::AnimationNodeClass::Flags::UpdateDrawable) == true);
        TEST_REQUIRE(clone.TestFlag(game::AnimationNodeClass::Flags::RestartDrawable) == false);
    }

    // test instance state.
    {
        node.Prepare(*g_factory);

        // check initial state.
        game::AnimationNode instance(node);
        TEST_REQUIRE(instance.GetName()        == "root");
        TEST_REQUIRE(instance.GetSize()        == glm::vec2(100.0f, 100.0f));
        TEST_REQUIRE(instance.GetTranslation() == glm::vec2(150.0f, -150.0f));
        TEST_REQUIRE(instance.GetScale()       == glm::vec2(4.0f, 5.0f));
        TEST_REQUIRE(instance.GetRotation()    == real::float32(1.5f));
        TEST_REQUIRE(instance.GetLineWidth()   == real::float32(5.0f));
        TEST_REQUIRE(instance.GetRenderPass()  == game::AnimationNodeClass::RenderPass::Mask);

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

void unit_test_animation_transform_actuator()
{
    game::AnimationTransformActuatorClass act;
    act.SetNodeId("123");
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::AnimationTransformActuatorClass::Interpolation::Cosine);
    act.SetEndPosition(glm::vec2(100.0f, 50.0f));
    act.SetEndSize(glm::vec2(5.0f, 6.0f));
    act.SetEndScale(glm::vec2(3.0f, 8.0f));
    act.SetEndRotation(1.5f);

    TEST_REQUIRE(act.GetInterpolation() == game::AnimationTransformActuatorClass::Interpolation::Cosine);
    TEST_REQUIRE(act.GetNodeId()        == "123");
    TEST_REQUIRE(act.GetStartTime()     == real::float32(0.1f));
    TEST_REQUIRE(act.GetDuration()      == real::float32(0.5f));
    TEST_REQUIRE(act.GetEndPosition()   == glm::vec2(100.0f, 50.0f));
    TEST_REQUIRE(act.GetEndSize()       == glm::vec2(5.0f, 6.0f));
    TEST_REQUIRE(act.GetEndScale()      == glm::vec2(3.0f, 8.0f));
    TEST_REQUIRE(act.GetEndRotation()   == real::float32(1.5f));

    // serialize
    {
        game::AnimationTransformActuatorClass copy;
        copy.FromJson(act.ToJson());
        TEST_REQUIRE(copy.GetInterpolation() == game::AnimationTransformActuatorClass::Interpolation::Cosine);
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
        TEST_REQUIRE(copy.GetInterpolation() == game::AnimationTransformActuatorClass::Interpolation::Cosine);
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
        game::AnimationTransformActuator instance(act);
        game::AnimationNodeClass klass;
        klass.SetDrawable("drawable");
        klass.SetMaterial("material");
        klass.SetTranslation(glm::vec2(5.0f, 5.0f));
        klass.SetSize(glm::vec2(1.0f, 1.0f));
        klass.SetRotation(0.0f);
        klass.SetScale(glm::vec2(1.0f, 1.0f));
        klass.Prepare(*g_factory);

        // create node instance
        game::AnimationNode node(klass);

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
    game::AnimationNodeClass klass;
    klass.SetDrawable("drawable");
    klass.SetMaterial("material");
    klass.SetTranslation(glm::vec2(5.0f, 5.0f));
    klass.SetSize(glm::vec2(1.0f, 1.0f));
    klass.SetRotation(0.0f);
    klass.SetScale(glm::vec2(1.0f, 1.0f));
    klass.Prepare(*g_factory);

    // create node instance
    game::AnimationNode node(klass);


    game::AnimationTrackClass track;
    track.SetName("test");
    track.SetLooping(true);
    track.SetDuration(10.0f);
    TEST_REQUIRE(track.GetName() == "test");
    TEST_REQUIRE(track.IsLooping());
    TEST_REQUIRE(track.GetDuration() == real::float32(10.0f));
    TEST_REQUIRE(track.GetNumActuators() == 0);

    game::AnimationTransformActuatorClass act;
    act.SetNodeId(node.GetId());
    act.SetStartTime(0.1f);
    act.SetDuration(0.5f);
    act.SetInterpolation(game::AnimationTransformActuatorClass::Interpolation::Cosine);
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

void unit_test_animation()
{
    game::AnimationClass animation;
    {
        game::AnimationNodeClass node;
        node.SetName("root");
        animation.AddNode(std::move(node));
    }
    {
        game::AnimationNodeClass node;
        node.SetName("child_1");
        animation.AddNode(std::move(node));
    }
    {
        game::AnimationNodeClass node;
        node.SetName("child_2");
        animation.AddNode(std::move(node));
    }

    {
        game::AnimationTrackClass track;
        track.SetName("test1");
        animation.AddAnimationTrack(track);
    }

    {
        game::AnimationTrackClass track;
        track.SetName("test2");
        animation.AddAnimationTrack(track);
    }

    TEST_REQUIRE(animation.GetNumNodes() == 3);
    TEST_REQUIRE(animation.GetNode(0).GetName() == "root");
    TEST_REQUIRE(animation.GetNode(1).GetName() == "child_1");
    TEST_REQUIRE(animation.GetNode(2).GetName() == "child_2");
    TEST_REQUIRE(animation.FindNodeByName("root"));
    TEST_REQUIRE(animation.FindNodeByName("child_1"));
    TEST_REQUIRE(animation.FindNodeByName("child_2"));
    TEST_REQUIRE(animation.FindNodeByName("foobar") == nullptr);
    TEST_REQUIRE(animation.FindNodeById(animation.GetNode(0).GetId()));
    TEST_REQUIRE(animation.FindNodeById(animation.GetNode(1).GetId()));
    TEST_REQUIRE(animation.FindNodeById("asg") == nullptr);
    TEST_REQUIRE(animation.GetNumTracks() == 2);
    TEST_REQUIRE(animation.FindAnimationTrackByName("test1"));
    TEST_REQUIRE(animation.FindAnimationTrackByName("sdgasg") == nullptr);

    // serialization
    {
        auto ret = game::AnimationClass::FromJson(animation.ToJson());
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetNumNodes() == 3);
        TEST_REQUIRE(ret->GetNode(0).GetName() == "root");
        TEST_REQUIRE(ret->GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(ret->GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(ret->GetNumTracks() == 2);
        TEST_REQUIRE(ret->FindAnimationTrackByName("test1"));
        TEST_REQUIRE(ret->GetId()   == animation.GetId());
        TEST_REQUIRE(ret->GetHash() == animation.GetHash());
    }

    // copy construction and assignment
    {
        auto copy(animation);
        TEST_REQUIRE(copy.GetNumNodes() == 3);
        TEST_REQUIRE(copy.GetNode(0).GetName() == "root");
        TEST_REQUIRE(copy.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(copy.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(copy.GetNumTracks() == 2);
        TEST_REQUIRE(copy.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(copy.GetId()   == animation.GetId());
        TEST_REQUIRE(copy.GetHash() == animation.GetHash());

        copy = animation;
        TEST_REQUIRE(copy.GetNumNodes() == 3);
        TEST_REQUIRE(copy.GetNode(0).GetName() == "root");
        TEST_REQUIRE(copy.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(copy.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(copy.GetNumTracks() == 2);
        TEST_REQUIRE(copy.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(copy.GetId()   == animation.GetId());
        TEST_REQUIRE(copy.GetHash() == animation.GetHash());
    }

    // clone
    {
        auto clone(animation.Clone());
        TEST_REQUIRE(clone.GetNumNodes() == 3);
        TEST_REQUIRE(clone.GetNode(0).GetName() == "root");
        TEST_REQUIRE(clone.GetNode(1).GetName() == "child_1");
        TEST_REQUIRE(clone.GetNode(2).GetName() == "child_2");
        TEST_REQUIRE(clone.GetNumTracks() == 2);
        TEST_REQUIRE(clone.FindAnimationTrackByName("test1"));
        TEST_REQUIRE(clone.GetId()   != animation.GetId());
        TEST_REQUIRE(clone.GetHash() != animation.GetHash());
    }

    animation.Prepare(*g_factory);

    // instance
    {
        game::Animation instance(animation);
        TEST_REQUIRE(instance.GetCurrentTime() == real::float32(0.0f));
        TEST_REQUIRE(instance.GetNumNodes() == 3);
        TEST_REQUIRE(instance.FindNodeByName("root"));
        TEST_REQUIRE(instance.FindNodeById(animation.GetNode(0).GetId()));

        instance.Update(1.0f);
        TEST_REQUIRE(instance.GetCurrentTime() == real::float32(1.0f));
        instance.Reset();
        TEST_REQUIRE(instance.GetCurrentTime() == real::float32(0.0f));
    }

    animation.DeleteNodeByIndex(0);
    TEST_REQUIRE(animation.GetNumNodes() == 2);
    TEST_REQUIRE(animation.DeleteNodeByName("child_1"));
    TEST_REQUIRE(animation.GetNumNodes() == 1);
    TEST_REQUIRE(animation.DeleteNodeById(animation.GetNode(0).GetId()));
    TEST_REQUIRE(animation.GetNumNodes() == 0);

    animation.DeleteAnimationTrack(0);
    TEST_REQUIRE(animation.GetNumTracks() == 1);
    animation.DeleteAnimationTrackByName("test2");
    TEST_REQUIRE(animation.GetNumTracks() == 0);
}

void unit_test_animation_render_tree()
{
    game::AnimationClass anim_class;

    // todo: rotational component in transform
    // todo: scaling component in transform

    {
        game::AnimationNodeClass node_class;
        node_class.SetName("parent");
        node_class.SetDrawable("drawable");
        node_class.SetMaterial("material");
        node_class.SetTranslation(glm::vec2(10.0f, 10.0f));
        node_class.SetSize(glm::vec2(10.0f, 10.0f));
        node_class.SetRotation(0.0f);
        node_class.SetScale(glm::vec2(1.0f, 1.0f));
        auto* ret = anim_class.AddNode(node_class);
        auto& tree = anim_class.GetRenderTree();
        tree.AppendChild(ret);
    }

    // transforms relative to the parent
    {
        game::AnimationNodeClass node_class;
        node_class.SetName("child");
        node_class.SetDrawable("drawable");
        node_class.SetMaterial("material");
        node_class.SetTranslation(glm::vec2(10.0f, 10.0f));
        node_class.SetSize(glm::vec2(2.0f, 2.0f));
        node_class.SetRotation(0.0f);
        node_class.SetScale(glm::vec2(1.0f, 1.0f));
        auto* ret = anim_class.AddNode(node_class);
        auto& tree = anim_class.GetRenderTree();
        tree.GetChildNode(0).AppendChild(ret);
    }

    // remember, the shape is aligned around the position

    // hit testing
    {
        std::vector<game::AnimationNodeClass*> hits;
        std::vector<glm::vec2> hitpos;
        anim_class.CoarseHitTest(0.0f, 0.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.empty());
        TEST_REQUIRE(hitpos.empty());

        anim_class.CoarseHitTest(6.0f, 6.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "parent");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));

        hits.clear();
        hitpos.clear();
        anim_class.CoarseHitTest(20.0f, 20.0f, &hits, &hitpos);
        TEST_REQUIRE(hits.size() == 1);
        TEST_REQUIRE(hitpos.size() == 1);
        TEST_REQUIRE(hits[0]->GetName() == "child");
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].x));
        TEST_REQUIRE(math::equals(1.0f, hitpos[0].y));
    }

    // whole bounding box.
    {
        const auto& box = anim_class.GetBoundingBox();
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(16.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(16.0f, box.GetHeight()));
    }

    // node bounding box
    {
        const auto* node = anim_class.FindNodeByName("parent");
        const auto& box = anim_class.GetBoundingBox(node);
        TEST_REQUIRE(math::equals(5.0f, box.GetX()));
        TEST_REQUIRE(math::equals(5.0f, box.GetY()));
        TEST_REQUIRE(math::equals(10.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(10.0f, box.GetHeight()));
    }
    // node bounding box
    {
        const auto* node = anim_class.FindNodeByName("child");
        const auto box = anim_class.GetBoundingBox(node);
        TEST_REQUIRE(math::equals(19.0f, box.GetX()));
        TEST_REQUIRE(math::equals(19.0f, box.GetY()));
        TEST_REQUIRE(math::equals(2.0f, box.GetWidth()));
        TEST_REQUIRE(math::equals(2.0f, box.GetHeight()));
    }

    // coordinate mapping
    {
        const auto* node = anim_class.FindNodeByName("child");
        auto vec = anim_class.MapCoordsFromNode(1.0f, 1.0f, node);
        TEST_REQUIRE(math::equals(20.0f, vec.x));
        TEST_REQUIRE(math::equals(20.0f, vec.y));

        // inverse operation to MapCoordsFromNode
        vec = anim_class.MapCoordsToNode(20.0f, 20.0f, node);
        TEST_REQUIRE(math::equals(1.0f, vec.x));
        TEST_REQUIRE(math::equals(1.0f, vec.y));
    }

    // copying preserves the render tree.
    {
        game::AnimationClass klass;
        game::AnimationNodeClass parent;
        game::AnimationNodeClass child0;
        game::AnimationNodeClass child1;
        game::AnimationNodeClass child2;
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

}

int test_main(int argc, char* argv[])
{
    TestFactory factory;
    g_factory = &factory;

    unit_test_animation_node();
    unit_test_animation_transform_actuator();
    unit_test_animation_track();
    unit_test_animation();
    unit_test_animation_render_tree();
    return 0;
}