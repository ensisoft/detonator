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

#pragma once

#include "config.h"
#include "base/assert.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/color4f.h"
#include "game/entity.h"
#include "game/scene.h"
#include "engine/classlib.h"

class TestClassLib : public engine::ClassLibrary
{
public:
    virtual engine::ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const override
    {
        return nullptr;
    }
    virtual engine::ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override
    {
        return nullptr;
    }
    virtual engine::ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override
    {
        return nullptr;
    }
    virtual engine::ClassHandle<const uik::Window> FindUIById(const std::string& id) const override
    {
        return nullptr;
    }
    virtual std::shared_ptr<const gfx::MaterialClass> FindMaterialClassById(const std::string& name) const override
    {
        if (name == "uv_test")
            return std::make_shared<gfx::TextureMap2DClass>(
                    gfx::CreateMaterialClassFromTexture("assets/textures/uv_test_512.png"));
        else if (name == "checkerboard")
            return std::make_shared<gfx::TextureMap2DClass>(
                    gfx::CreateMaterialClassFromTexture("assets/textures/Checkerboard.png"));
        else if (name == "color")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::HotPink));
        else if (name == "object")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::Gold));
        else if (name == "ground")
            return std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color::DarkGreen));
        BUG("No such material class.");
        return nullptr;
    }
    virtual std::shared_ptr<const gfx::DrawableClass> FindDrawableClassById(const std::string& name) const override
    {
        if (name == "circle")
            return std::make_shared<gfx::CircleClass>();
        else if (name == "rectangle")
            return std::make_shared<gfx::RectangleClass>();
        else if (name == "trapezoid")
            return std::make_shared<gfx::TrapezoidClass>();
        BUG("No such drawable class.");
        return nullptr;
    }
    virtual std::shared_ptr<const game::EntityClass> FindEntityClassByName(const std::string& name) const override
    {
        if (name == "unit_box")
        {
            auto klass = std::make_shared<game::EntityClass>();

            game::EntityNodeClass box;
            box.SetSize(glm::vec2(1.0f, 1.0f));
            box.SetName("box");
            game::DrawableItemClass draw;
            draw.SetDrawableId("rectangle");
            draw.SetMaterialId("uv_test");
            box.SetDrawable(draw);
            klass->LinkChild(nullptr, klass->AddNode(std::move(box)));
            return klass;
        }

        if (name == "box")
        {
            auto klass = std::make_shared<game::EntityClass>();

            game::EntityNodeClass box;
            box.SetSize(glm::vec2(40.0f, 40.0f));
            box.SetName("box");
            game::DrawableItemClass draw;
            draw.SetDrawableId("rectangle");
            draw.SetMaterialId("uv_test");
            box.SetDrawable(draw);
            game::RigidBodyItemClass body;
            body.SetSimulation(game::RigidBodyItemClass::Simulation::Dynamic);
            body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Box);
            box.SetRigidBody(body);

            klass->LinkChild(nullptr, klass->AddNode(std::move(box)));
            return klass;
        }
        if (name == "circle")
        {
            auto klass = std::make_shared<game::EntityClass>();
            game::EntityNodeClass circle;
            circle.SetSize(glm::vec2(50.0f, 50.0f));
            circle.SetName("circle");
            game::DrawableItemClass draw;
            draw.SetDrawableId("circle");
            draw.SetMaterialId("uv_test");
            circle.SetDrawable(draw);
            game::RigidBodyItemClass body;
            body.SetSimulation(game::RigidBodyItemClass::Simulation::Dynamic);
            body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
            circle.SetRigidBody(body);

            klass->LinkChild(nullptr, klass->AddNode(std::move(circle)));
            return klass;

        }
        else if (name == "ground")
        {
            auto klass = std::make_shared<game::EntityClass>();

            game::EntityNodeClass node;
            node.SetName("ground");
            node.SetSize(glm::vec2(400.0f, 20.0f));
            node.SetRotation(0.2);
            game::DrawableItemClass draw;
            draw.SetDrawableId("rectangle");
            draw.SetMaterialId("ground");
            node.SetDrawable(draw);
            game::RigidBodyItemClass body;
            body.SetSimulation(game::RigidBodyItemClass::Simulation::Static);
            body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Box);
            node.SetRigidBody(body);

            klass->LinkChild(nullptr, klass->AddNode(std::move(node)));
            return klass;
        }
        else if (name == "robot")
        {
            auto klass = std::make_shared<game::EntityClass>();
            {
                game::EntityNodeClass torso;
                torso.SetName("torso");
                torso.SetSize(glm::vec2(120.0f, 250.0f));
                game::DrawableItemClass draw;
                draw.SetDrawableId("trapezoid");
                draw.SetMaterialId("checkerboard");
                torso.SetDrawable(draw);
                klass->LinkChild(nullptr, klass->AddNode(torso));
            }
            {
                game::EntityNodeClass head;
                head.SetName("head");
                head.SetSize(glm::vec2(90.0f, 90.0f));
                head.SetTranslation(glm::vec2(0.0f, -185.0f));
                game::DrawableItemClass draw;
                draw.SetDrawableId("circle");
                draw.SetMaterialId("checkerboard");
                head.SetDrawable(draw);
                klass->LinkChild(klass->FindNodeByName("torso"), klass->AddNode(head));
            }

            {
                game::EntityNodeClass joint;
                joint.SetName("shoulder joint R");
                joint.SetSize(glm::vec2(40.0f, 40.0f));
                joint.SetTranslation(glm::vec2(80.0f, -104.0f));
                game::DrawableItemClass draw;
                draw.SetDrawableId("circle");
                draw.SetMaterialId("color");
                joint.SetDrawable(draw);
                klass->LinkChild(klass->FindNodeByName("torso"), klass->AddNode(joint));

                game::EntityNodeClass arm;
                arm.SetName("arm R");
                arm.SetTranslation(glm::vec2(0.0f, 50.0f));
                arm.SetSize(glm::vec2(25.0f, 130.0f));
                draw.SetDrawableId("rectangle");
                draw.SetMaterialId("checkerboard");
                arm.SetDrawable(draw);
                klass->LinkChild(klass->FindNodeByName("shoulder joint R"), klass->AddNode(arm));
            }

            {
                game::EntityNodeClass joint;
                joint.SetName("shoulder joint L");
                joint.SetSize(glm::vec2(40.0f, 40.0f));
                joint.SetTranslation(glm::vec2(-80.0f, -104.0f));
                game::DrawableItemClass draw;
                draw.SetDrawableId("circle");
                draw.SetMaterialId("color");
                joint.SetDrawable(draw);
                klass->LinkChild(klass->FindNodeByName("torso"), klass->AddNode(joint));

                game::EntityNodeClass arm;
                arm.SetName("arm L");
                arm.SetTranslation(glm::vec2(0.0f, 50.0f));
                arm.SetSize(glm::vec2(25.0f, 130.0f));
                draw.SetDrawableId("rectangle");
                draw.SetMaterialId("checkerboard");
                arm.SetDrawable(draw);
                klass->LinkChild(klass->FindNodeByName("shoulder joint L"), klass->AddNode(arm));
            }

            game::TransformActuatorClass right_arm_up;
            right_arm_up.SetDuration(0.5f);
            right_arm_up.SetStartTime(0.0f);
            right_arm_up.SetEndRotation(-math::Pi);
            right_arm_up.SetEndPosition(klass->FindNodeByName("shoulder joint R")->GetTranslation());
            right_arm_up.SetEndSize(klass->FindNodeByName("shoulder joint R")->GetSize());
            right_arm_up.SetNodeId(klass->FindNodeByName("shoulder joint R")->GetId());
            game::TransformActuatorClass right_arm_down;
            right_arm_down.SetDuration(0.5f);
            right_arm_down.SetStartTime(0.5f);
            right_arm_down.SetEndRotation(0);
            right_arm_down.SetEndPosition(klass->FindNodeByName("shoulder joint R")->GetTranslation());
            right_arm_down.SetEndSize(klass->FindNodeByName("shoulder joint R")->GetSize());
            right_arm_down.SetNodeId(klass->FindNodeByName("shoulder joint R")->GetId());

            auto track = std::make_unique<game::AnimationTrackClass>();
            track->SetDuration(2.0f);
            track->SetName("idle");
            track->SetLooping(true);
            track->AddActuator(std::move(right_arm_up));
            track->AddActuator(std::move(right_arm_down));
            klass->AddAnimationTrack(std::move(track));
            return klass;
        }

        return nullptr;
    }

    virtual std::shared_ptr<const game::EntityClass> FindEntityClassById(const std::string& id) const override
    { return nullptr; }
    virtual std::shared_ptr<const game::SceneClass> FindSceneClassByName(const std::string&) const override
    { return nullptr; }
    virtual std::shared_ptr<const game::SceneClass> FindSceneClassById(const std::string&) const override
    { return nullptr; }
private:
};