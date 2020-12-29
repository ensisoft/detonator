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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include "gamelib/renderer.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/drawable.h"

namespace gui
{

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& view);

enum class GridDensity {
    Grid10x10   = 10,
    Grid20x20   = 20,
    Grid50x50   = 50,
    Grid100x100 = 100
};

void DrawCoordinateGrid(gfx::Painter& painter, gfx::Transform& view,
    GridDensity grid,   // grid density setting.
    float zoom,         // overall zoom level
    float xs, float ys, // scaling factors for the axis
    unsigned width, unsigned height); // viewport (widget) size


// generic draw hook implementation for embellishing some nodes
// with things such as selection rectangle in order to visually
// indicate the selected node when editing a scene/animation.
class DrawHook : public game::EntityClassDrawHook,
                 public game::EntityInstanceDrawHook,
                 public game::SceneClassDrawHook
{
public:
    template<typename Node>
    DrawHook(const Node* selected, bool playing)
      : mSelectedItem(selected)
      , mPlaying(playing)
    {}

    // EntityNode
    virtual bool InspectPacket(const game::EntityNode* node, game::DrawPacket& packet) override
    {
        return FilterPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNode* node, gfx::Transform& trans,
    std::vector<game::DrawPacket>& packets) override
    {
        GenericAppendPackets(node, trans, packets);
    }


    // EntityNodeClass
    virtual bool InspectPacket(const game::EntityNodeClass* node, game::DrawPacket& packet) override
    {
        return FilterPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNodeClass* node, gfx::Transform& trans,
                               std::vector<game::DrawPacket>& packets) override
    {
        GenericAppendPackets(node, trans, packets);
    }

    // SceneClassDrawHook
    virtual bool FilterEntity(const game::SceneNodeClass& entity) override
    {
        if (!entity.TestFlag(game::SceneNodeClass::Flags::VisibleInEditor))
            return false;
        return true;
    }
    virtual void BeginDrawEntity(const game::SceneNodeClass& entity, gfx::Painter& painter, gfx::Transform& trans) override
    {
        mDrawSelection  = (&entity == mSelectedItem);
        mDrawIndicators = false;
    }
    virtual void EndDrawEntity(const game::SceneNodeClass& entity, gfx::Painter& painter, gfx::Transform& trans) override
    {
        if (mDrawSelection)
        {
            DrawBasisVectors(painter, trans);
        }
        mDrawSelection = false;
    }
private:
    template<typename Node>
    bool FilterPacket(const Node* node, game::DrawPacket& packet)
    {
        if (!node->TestFlag(Node::Flags::VisibleInEditor))
            return false;
        return true;
    }

    template<typename Node>
    void GenericAppendPackets(const Node* node, gfx::Transform& trans, std::vector<game::DrawPacket>& packets)
    {
        const auto* drawable   = node->GetDrawable();
        const auto is_selected = mDrawSelection || node == mSelectedItem;
        const auto is_mask     = drawable && drawable->GetRenderPass() == game::RenderPass::Mask;
        const auto is_playing  = mPlaying;

        // add a visualization for a mask item when not playing and when
        // the mask object is not selected or when there's no drawable item.
        if ((is_mask || !drawable) && !is_selected && !is_playing)
        {
            static const auto yellow = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::DarkYellow));
            static const auto rect  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
            // visualize it.
            trans.Push(node->GetModelTransform());
                game::DrawPacket box;
                box.transform = trans.GetAsMatrix();
                box.material  = yellow;
                box.drawable  = rect;
                box.layer     = 250;
                box.pass      = game::RenderPass::Draw;
                packets.push_back(box);
            trans.Pop();
        }

        if (!is_selected)
            return;

        static const auto green  = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::Green));
        static const auto rect   = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
        static const auto circle = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline, 2.0f);
        const auto& size = node->GetSize();
        const auto layer = 250;

        // draw the selection rectangle.
        trans.Push(node->GetModelTransform());
            game::DrawPacket selection;
            selection.transform = trans.GetAsMatrix();
            selection.material  = green;
            selection.drawable  = rect;
            selection.layer     = layer;
            packets.push_back(selection);
        trans.Pop();

        if (!mDrawIndicators)
            return;

        // decompose the matrix in order to get the combined scaling component
        // so that we can use the inverse scale to keep the resize and rotation
        // indicators always with same size.
        const auto& mat = trans.GetAsMatrix();
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew,  perspective);

        // draw the resize indicator. (lower right corner box)
        trans.Push();
            trans.Scale(10.0f/scale.x, 10.0f/scale.y);
            trans.Translate(size.x*0.5f-10.0f/scale.x, size.y*0.5f-10.0f/scale.y);
            game::DrawPacket sizing_box;
            sizing_box.transform = trans.GetAsMatrix();
            sizing_box.material  = green;
            sizing_box.drawable  = rect;
            sizing_box.layer     = layer;
            packets.push_back(sizing_box);
        trans.Pop();

        // draw the rotation indicator. (upper left corner circle)
        trans.Push();
            trans.Scale(10.0f/scale.x, 10.0f/scale.y);
            trans.Translate(-size.x*0.5f, -size.y*0.5f);
            game::DrawPacket rotation_circle;
            rotation_circle.transform = trans.GetAsMatrix();
            rotation_circle.material  = green;
            rotation_circle.drawable  = circle;
            rotation_circle.layer     = layer;
            packets.push_back(rotation_circle);
        trans.Pop();
    }
private:
    const void* mSelectedItem = nullptr;
    const bool mPlaying   = false;
    bool mDrawSelection  = false;
    bool mDrawIndicators = true;
};


} // namespace
