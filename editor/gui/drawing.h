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

class QWidget;

namespace gui
{

void DrawLine(const gfx::Transform& view,
              const glm::vec2& src, const glm::vec2& dst,
              gfx::Painter& painter);

enum class ToolHotspot {
    None,
    Rotate,
    Resize,
    Remove
};
ToolHotspot TestToolHotspot(gfx::Transform& trans, const gfx::FRect& rect, const glm::vec4& view_pos);

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
    unsigned height, // viewport (widget) size
    float camera_offset_x,
    float camera_offset_y,
    game::Perspective perspective
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
void ShowMessage(const std::string& msg, const gfx::FRect& rect, gfx::Painter& painter);
void ShowMessage(const std::string& msg, const gfx::FPoint& pos, gfx::Painter& painter);
void ShowError(const std::string& msg, const gfx::FPoint& pos, gfx::Painter& painter);
void ShowInstruction(const std::string& msg, const gfx::FRect& rect, gfx::Painter& painter);

void PrintMousePos(const gfx::Transform& view, gfx::Painter& painter, QWidget* widget);

// Print current mouse position inside the widget's viewport mapped
// into the game plane world coordinate.
void PrintWorldPlanePos(const glm::mat4& projection,
                        const glm::mat4& world_to_view,
                        gfx::Painter& painter,
                        QWidget* widget);


template<typename UI, typename State>
void MakeViewTransform(const UI& ui, const State& state, gfx::Transform& view)
{
    view.Scale(GetValue(ui.scaleX), GetValue(ui.scaleY));
    view.Scale(GetValue(ui.zoom), GetValue(ui.zoom));
    view.RotateAroundZ(qDegreesToRadians(ui.rotation->value()));
    view.Translate(state.camera_offset_x, state.camera_offset_y);
}

template<typename UI, typename State>
void MakeViewTransform(const UI& ui, const State& state, gfx::Transform& view, float rotation)
{
    view.Scale(GetValue(ui.scaleX), GetValue(ui.scaleY));
    view.Scale(GetValue(ui.zoom), GetValue(ui.zoom));
    view.RotateAroundZ(qDegreesToRadians(rotation));
    view.Translate(state.camera_offset_x, state.camera_offset_y);
}


template<typename UI, typename State>
glm::mat4 CreatePerspectiveCorrectViewMatrix(const UI& ui, const State& state, game::Perspective perspective)
{
    const float zoom = GetValue(ui.zoom);
    const float xs = GetValue(ui.scaleX);
    const float ys = GetValue(ui.scaleY);
    const float rotation = GetValue(ui.rotation);

    return engine::CreateViewMatrix(state.camera_offset_x,
                                    state.camera_offset_y,
                                    zoom*xs, zoom*ys,
                                    perspective,
                                    rotation);
}

template<typename UI>
glm::mat4 CreatePerspectiveCorrectProjMatrix(const UI& ui, game::Perspective perspective)
{
    const auto width  = ui.widget->width();
    const auto height = ui.widget->height();
    return engine::CreateProjectionMatrix(perspective, width, height);
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
    DrawHook(const game::SceneNodeClass* selected, const game::FRect& view)
      : mSelectedSceneNode(selected)
      , mViewRect(view)
      , mDrawScene(true)
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
        if (mDrawScene)
            return;

        GenericAppendEntityPackets(node, trans, packets);
    }
    // EntityNodeClass
    virtual bool InspectPacket(const game::EntityNodeClass* node, engine::DrawPacket& packet) override
    {
        return GenericFilterEntityPacket(node, packet);
    }
    virtual void AppendPackets(const game::EntityNodeClass* node, gfx::Transform& trans,
                               std::vector<engine::DrawPacket>& packets) override
    {
        if (mDrawScene)
            return;

        GenericAppendEntityPackets(node, trans, packets);
    }

    // SceneClassDrawHook
    virtual bool FilterEntity(const game::SceneNodeClass& node, gfx::Painter& painter, gfx::Transform& trans) override
    {
        if (node.TestFlag(game::SceneNodeClass::Flags::VisibleInEditor))
            return true;

        // broken entity reference.
        const auto& klass = node.GetEntityClass();
        if (!klass)
            return false;

        if (&node == mSelectedSceneNode)
        {
            const auto& entity = node.GetEntityClass();
            const auto& box    = entity->GetBoundingRect();
            DrawSelectionBox(painter, trans, box);
            if (mDrawVectors)
                DrawBasisVectors(painter, trans);
        }
        return false;
    }
    virtual void BeginDrawEntity(const game::SceneNodeClass& node, gfx::Painter& painter, gfx::Transform& trans) override
    {
    }
    virtual void EndDrawEntity(const game::SceneNodeClass& node, gfx::Painter& painter, gfx::Transform& trans) override
    {
        // broken entity reference.
        const auto& entity_klass = node.GetEntityClass();
        if (!entity_klass)
            return;

        if (&node == mSelectedSceneNode)
        {
            const auto& box  = entity_klass->GetBoundingRect();
            DrawSelectionBox(painter, trans, box);
            if (mDrawVectors)
                DrawBasisVectors(painter, trans);
        }
        else if (!node.TestFlag(game::SceneNodeClass::Flags::VisibleInGame))
        {
            const auto& box  = entity_klass->GetBoundingRect();
            DrawInvisibleItemBox(painter, trans, box);
        }
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
    const game::SceneNodeClass* mSelectedSceneNode        = nullptr;
    const game::FRect mViewRect;
    bool mPlaying        = false;
    bool mDrawVectors    = false;
    bool mDrawScene      = false;
    glm::mat4 mView;
};

} // namespace
