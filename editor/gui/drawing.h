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

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include "engine/renderer.h"
#include "game/treeop.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/drawable.h"

class QWidget;

namespace gui
{

void DrawLine(const gfx::Transform& view,
              const glm::vec2& src, const glm::vec2& dst,
              gfx::Painter& painter);

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& view);
void DrawBasisVectors(gfx::Transform& view, std::vector<engine::DrawPacket>& packets, int layer);

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

// Draw an overlay of viewport illustration. The viewport is the logical
// game viewport that the game can adjust in order to define the view
// into the game world.
void DrawViewport(gfx::Painter& painter, gfx::Transform& view,
    float game_view_width,
    float game_view_height,
    unsigned widget_width,
    unsigned widget_height);

void ShowMessage(const std::string& msg, gfx::Painter& painter,
                 unsigned widget_width, unsigned widget_height);

void PrintMousePos(const gfx::Transform& view,
                   gfx::Painter& painter, QWidget* widget);

// generic draw hook implementation for embellishing some nodes
// with things such as selection rectangle in order to visually
// indicate the selected node when editing a scene/animation.
class DrawHook : public engine::EntityClassDrawHook,
                 public engine::EntityInstanceDrawHook,
                 public engine::SceneClassDrawHook
{
public:
    DrawHook()
    {}

    template<typename Node>
    DrawHook(const Node* selected)
        : mSelectedItem(selected)
    {}

    template<typename Node>
    DrawHook(const Node* selected, const game::FRect& view)
      : mSelectedItem(selected)
      , mViewRect(view)
    {}

    void SetDrawVectors(bool on_off)
    { mDrawVectors = on_off; }
    void SetIsPlaying(bool on_off)
    { mPlaying = on_off; }

    // EntityNode
    virtual bool InspectPacket(const game::EntityNode* node, engine::DrawPacket& packet) override
    {
        return FilterPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNode* node, gfx::Transform& trans,
                               std::vector<engine::DrawPacket>& packets) override
    {
        GenericAppendPackets(node, trans, packets);
    }


    // EntityNodeClass
    virtual bool InspectPacket(const game::EntityNodeClass* node, engine::DrawPacket& packet) override
    {
        return FilterPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNodeClass* node, gfx::Transform& trans,
                               std::vector<engine::DrawPacket>& packets) override
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
    bool FilterPacket(const Node* node, engine::DrawPacket& packet)
    {
        if (!node->TestFlag(Node::Flags::VisibleInEditor))
            return false;
        else if(!mViewRect.IsEmpty())
        {
            const auto& rect = game::ComputeBoundingRect(packet.transform);
            if (!DoesIntersect(mViewRect, rect))
                return false;
        }
        return true;
    }

    template<typename Node>
    void GenericAppendPackets(const Node* node, gfx::Transform& trans, std::vector<engine::DrawPacket>& packets)
    {
        const auto* drawable   = node->GetDrawable();
        const auto is_selected = mDrawSelection || node == mSelectedItem;
        const auto is_mask     = drawable && drawable->GetRenderPass() == game::RenderPass::Mask;
        const auto is_playing  = mPlaying;

        // add a visualization for a mask item when not playing and when
        // the mask object is not selected or when there's no drawable item.
        if ((is_mask || !drawable) && !is_selected && !is_playing)
        {
            static const auto yellow = std::make_shared<gfx::MaterialClassInst>(
                    gfx::CreateMaterialClassFromColor(gfx::Color::DarkYellow));
            static const auto rect  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
            // visualize it.
            trans.Push(node->GetModelTransform());
                engine::DrawPacket box;
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

        static const auto green  = std::make_shared<gfx::MaterialClassInst>(
                gfx::CreateMaterialClassFromColor(gfx::Color::Green));
        static const auto rect   = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
        static const auto circle = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline, 2.0f);
        const auto& size = node->GetSize();
        const auto layer = 250;

        // draw the selection rectangle.
        trans.Push(node->GetModelTransform());
            engine::DrawPacket selection;
            selection.transform = trans.GetAsMatrix();
            selection.material  = green;
            selection.drawable  = rect;
            selection.layer     = layer;
            packets.push_back(selection);
        trans.Pop();

       //if (!mDrawIndicators)
       //     return;

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

        // draw the basis vectors
        if (mDrawVectors)
        {
            trans.Push();
                DrawBasisVectors(trans, packets, layer);
            trans.Pop();
        }

        // draw the resize indicator. (lower right corner box)
        trans.Push();
            trans.Scale(10.0f/scale.x, 10.0f/scale.y);
            trans.Translate(size.x*0.5f-10.0f/scale.x, size.y*0.5f-10.0f/scale.y);
            engine::DrawPacket sizing_box;
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
            engine::DrawPacket rotation_circle;
            rotation_circle.transform = trans.GetAsMatrix();
            rotation_circle.material  = green;
            rotation_circle.drawable  = circle;
            rotation_circle.layer     = layer;
            packets.push_back(rotation_circle);
        trans.Pop();
    }
private:
    const void* mSelectedItem = nullptr;
    const game::FRect mViewRect;
    bool mPlaying        = false;
    bool mDrawSelection  = false;
    bool mDrawIndicators = true;
    bool mDrawVectors    = false;
};

} // namespace
