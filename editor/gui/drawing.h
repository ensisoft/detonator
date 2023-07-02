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
#  include <QtMath>
#include "warnpop.h"

#include "engine/camera.h"
#include "engine/renderer.h"
#include "game/treeop.h"
#include "game/enum.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "graphics/drawable.h"
#include "editor/gui/nerd.h"

class QWidget;

namespace gui
{

void DrawLine(gfx::Painter& painter, const glm::vec2& src, const glm::vec2& dst);

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& trans);
void DrawSelectionBox(gfx::Painter& painter, gfx::Transform& trans, const gfx::FRect& rect);
void DrawInvisibleItemBox(gfx::Painter& painter, gfx::Transform& trans, const gfx::FRect& rect);

void DrawBasisVectors(gfx::Transform& trans, std::vector<engine::DrawPacket>& packets, int layer);
void DrawSelectionBox(gfx::Transform& trans, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect, int layer);
void DrawInvisibleItemBox(gfx::Transform& trans, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect, int layer);

enum class GridDensity {
    Grid10x10   = 10,
    Grid20x20   = 20,
    Grid50x50   = 50,
    Grid100x100 = 100
};

void SetGridColor(const gfx::Color4f& color);

void DrawCoordinateGrid(gfx::Painter& painter, gfx::Transform& view,
    GridDensity grid,   // grid density setting.
    float zoom,         // overall zoom level
    float xs,
    float ys, // scaling factors for the axis
    unsigned width, // viewport (widget) size
    unsigned height // viewport (widget) size
    );

void DrawCoordinateGrid(gfx::Painter& painter,
    GridDensity grid,   // grid density setting.
    float zoom,         // overall zoom level
    float xs,
    float ys, // scaling factors for the axis
    unsigned width, // viewport (widget) size
    unsigned height // viewport (widget) size
);

// Draw an overlay of viewport illustration. The viewport is the logical
// game viewport that the game can adjust in order to define the view
// into the game world.
void DrawViewport(gfx::Painter& painter, gfx::Transform& view,
    float game_view_width,
    float game_view_height,
    unsigned widget_width,
    unsigned widget_height);

void ShowMessage(const std::string& msg, gfx::Painter& painter);
void ShowMessage(const std::string& msg, const Rect2Df& rect, gfx::Painter& painter);
void ShowMessage(const std::string& msg, const Point2Df& pos, gfx::Painter& painter);
void ShowError(const std::string& msg, const Point2Df& pos, gfx::Painter& painter);
void ShowInstruction(const std::string& msg, const Rect2Df& rect, gfx::Painter& painter);

void PrintMousePos(const gfx::Transform& view, gfx::Painter& painter, QWidget* widget);

// Print current mouse position inside the widget's viewport mapped
// into the game plane world coordinate.
void PrintMousePos(const glm::mat4& view_to_clip,
                   const glm::mat4& world_to_view,
                   gfx::Painter& painter,
                   QWidget* widget);

template<typename UI, typename State>
void PrintMousePos(const UI& ui, const State& state, gfx::Painter& painter, game::Perspective perspective = game::Perspective::AxisAligned)
{
    PrintMousePos(CreateProjectionMatrix(ui, perspective),
                  CreateViewMatrix(ui, state, perspective),
                  painter, ui.widget);
}

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
    DrawHook(const game::EntityNode* selected)
      : mSelectedEntityNode(selected)
    {}
    DrawHook(const game::EntityNodeClass* selected)
      : mSelectedEntityClassNode(selected)
    {}
    DrawHook(const game::EntityPlacement* selected, const game::FRect& view)
      : mSelectedSceneNode(selected)
      , mViewRect(view)
    {}

    void SetDrawVectors(bool on_off)
    { mDrawVectors = on_off; }
    void SetIsPlaying(bool on_off)
    { mPlaying = on_off; }
    void SetViewMatrix(const glm::mat4& view)
    { mView = view; }

    // EntityNode
    virtual bool InspectPacket(const game::EntityNode* node, engine::DrawPacket& packet) override
    {
        return GenericFilterEntityPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNode* node, gfx::Transform& trans,
                               std::vector<engine::DrawPacket>& packets) override
    {
        GenericAppendEntityPackets(node, trans, packets);
    }
    // EntityNodeClass
    virtual bool InspectPacket(const game::EntityNodeClass* node, engine::DrawPacket& packet) override
    {
        return GenericFilterEntityPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNodeClass* node, gfx::Transform& model,
                               std::vector<engine::DrawPacket>& packets) override
    {
        GenericAppendEntityPackets(node, model, packets);
    }

    // SceneClassDrawHook
    virtual bool FilterEntity(const game::EntityPlacement& placement) override
    {
        if (!placement.TestFlag(game::EntityPlacement::Flags::VisibleInEditor))
            return false;
        else if (placement.IsBroken())
            return false;
        return true;
    }
    virtual void BeginDrawEntity(const game::EntityPlacement& placement) override
    {
    }
    virtual void EndDrawEntity(const game::EntityPlacement& placement) override
    {
    }

    virtual void AppendPackets(const game::EntityPlacement& placement, gfx::Transform& model,
                               std::vector<engine::DrawPacket>& packets)
    {
        if (placement.IsBroken())
            return;

        const auto& entity = placement.GetEntityClass();
        const auto& bounds =  entity->GetBoundingRect();

        model.Push();
        model.Translate(bounds.GetPosition());
        model.Translate(bounds.GetWidth()*0.5f, bounds.GetHeight()*0.5f);

        if (&placement == mSelectedSceneNode)
        {
            DrawSelectionBox(model, packets, bounds, placement.GetLayer() + 1);
            if (mDrawVectors)
                DrawBasisVectors(model, packets, placement.GetLayer() + 1);
        }
        else if (!placement.TestFlag(game::EntityPlacement::Flags::VisibleInGame))
        {
            DrawInvisibleItemBox(model, packets, bounds, placement.GetLayer() + 1);
        }

        model.Pop();
    }

private:
    template<typename Node>
    bool GenericFilterEntityPacket(const Node* node, engine::DrawPacket& packet)
    {
        if (!node->TestFlag(Node::Flags::VisibleInEditor))
            return false;
        else if(!mViewRect.IsEmpty())
        {
            const auto& rect = game::ComputeBoundingRect(mView * packet.transform);
            if (!DoesIntersect(mViewRect, rect))
                return false;
        }
        return true;
    }

    template<typename Node>
    void GenericAppendEntityPackets(const Node* node, gfx::Transform& trans, std::vector<engine::DrawPacket>& packets)
    {
        const auto is_playing  = mPlaying;
        const auto is_selected = IsSelected(node);
        const auto is_visible_game   = IsVisibleInGame(*node);
        const auto is_visible_editor = IsVisibleInEditor(*node);

        const auto& size = node->GetSize();
        const gfx::FRect rect(0.0f, 0.0f, size.x, size.y);

        // if a node is visible in the editor but doesn't draw any game
        // time content, i.e. not visible in game or won't draw anything otherwise
        // then add a visualization for it.
        if ((!is_visible_game && !is_selected && !is_playing) && is_visible_editor)
        {
            DrawInvisibleItemBox(trans, packets, rect, node->GetLayer() + 1);
        }

        if (!is_selected)
            return;

        DrawSelectionBox(trans, packets, rect, node->GetLayer() + 1);
        if (mDrawVectors)
            DrawBasisVectors(trans, packets, node->GetLayer() + 1);
    }

    inline bool IsSelected(const game::EntityNodeClass* node) const
    {
        return node == mSelectedEntityClassNode;
    }
    inline bool IsSelected(const game::EntityNode* node) const
    {
        return node == mSelectedEntityNode;
    }

    template<typename EntityNode>
    bool IsVisibleInGame(const EntityNode& node) const
    {
        if (const auto* draw = node.GetDrawable())
        {
            if (draw->GetRenderPass() == game::RenderPass::DrawColor &&
                draw->TestFlag(game::DrawableItemClass::Flags::VisibleInGame))
                return true;
        }
        if (const auto* item= node.GetTextItem())
        {
            if (item->TestFlag(game::TextItem::Flags::VisibleInGame))
                return true;
        }
        return false;
    }

    template<typename EntityNode>
    bool IsVisibleInEditor(const EntityNode& node) const
    {
        return node.TestFlag(EntityNode::Flags::VisibleInEditor);
    }

private:
    const game::EntityNode* mSelectedEntityNode           = nullptr;
    const game::EntityNodeClass* mSelectedEntityClassNode = nullptr;
    const game::EntityPlacement* mSelectedSceneNode       = nullptr;
    const game::FRect mViewRect;
    bool mPlaying        = false;
    bool mDrawVectors    = false;
    glm::mat4 mView;
};

} // namespace
