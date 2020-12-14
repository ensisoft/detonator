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

#define LOGTAG "animation"

#include "config.h"

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/treewidget.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/animationwidget.h"
#include "editor/gui/utility.h"
#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"

namespace gui
{

class AnimationWidget::Tool
{
public:
    virtual ~Tool() = default;
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const = 0;
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) = 0;
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) = 0;
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) = 0;
private:
};

class AnimationWidget::PlaceTool : public AnimationWidget::Tool
{
public:
    PlaceTool(AnimationWidget::State& state, const QString& material, const QString& drawable)
        : mState(state)
        , mMaterialName(material)
        , mDrawableName(drawable)
    {
        mDrawableClass = mState.workspace->GetDrawableClassByName(mDrawableName);
        mMaterialClass = mState.workspace->GetMaterialClassByName(mMaterialName);
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
        mDrawable = gfx::CreateDrawableInstance(mDrawableClass);
    }
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const
    {
        if (!mEngaged)
            return;

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width  = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        view.Push();
            view.Scale(width, height);
            view.Translate(xpos, ypos);
            painter.Draw(*mDrawable, view, *mMaterial);

            // draw a selection rect around it.
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline),
                view, gfx::SolidColor(gfx::Color::Green));
        view.Pop();
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view)
    {
        if (!mEngaged)
            return;

        const auto& view_to_model = glm::inverse(view.GetAsMatrix());
        const auto& p = mickey->pos();
        mCurrent = view_to_model * glm::vec4(p.x(), p.y(), 1.0f, 1.0f);
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            const auto& view_to_model = glm::inverse(view.GetAsMatrix());
            const auto& p = mickey->pos();
            mStart = view_to_model * glm::vec4(p.x(), p.y(), 1.0f, 1.0f);
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view)
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return false;

        std::string name;
        for (size_t i=0; i<666666; ++i)
        {
            name = "Node " + std::to_string(i);
            if (CheckNameAvailability(name))
                break;
        }

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        game::AnimationNodeClass node;
        node.SetMaterial(mMaterialClass->GetId());
        node.SetDrawable(mDrawableClass->GetId());
        node.SetName(name);
        // the given object position is to be aligned with the center of the shape
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto& root  = mState.animation->GetRenderTree();
        auto* child = mState.animation->AddNode(std::move(node));
        root.AppendChild(child);

        mState.scenegraph_tree_view->Rebuild();
        mState.scenegraph_tree_view->SelectItemById(app::FromUtf8(child->GetClassId()));

        DEBUG("Added new shape '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.animation->GetNumNodes(); ++i)
        {
            const auto& node = mState.animation->GetNode(i);
            if (node.GetName() == name)
                return false;
        }
        return true;
    }


private:
    AnimationWidget::State& mState;
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec4 mStart;
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec4 mCurrent;
    bool mEngaged = false;
    bool mAlwaysSquare = false;
private:
    QString mMaterialName;
    QString mDrawableName;
    std::shared_ptr<const gfx::DrawableClass> mDrawableClass;
    std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
    std::unique_ptr<gfx::Material> mMaterial;
    std::unique_ptr<gfx::Drawable> mDrawable;
};

class AnimationWidget::CameraTool : public AnimationWidget::Tool
{
public:
    CameraTool(State& state) : mState(state)
    {}
    virtual void Render(gfx::Painter& painter, gfx::Transform&) const override
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform&) override
    {
        if (mEngaged)
        {
            const auto& pos = mickey->pos();
            const auto& delta = pos - mMousePos;
            const float x = delta.x();
            const float y = delta.y();
            mState.camera_offset_x += x;
            mState.camera_offset_y += y;
            //DEBUG("Camera offset %1, %2", mState.camera_offset_x, mState.camera_offset_y);
            mMousePos = pos;
        }
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform&) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mMousePos = mickey->pos();
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform&) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mEngaged = false;
            return false;
        }
        return true;
    }
private:
    AnimationWidget::State& mState;
private:
    QPoint mMousePos;
    bool mEngaged = false;

};

class AnimationWidget::MoveTool : public AnimationWidget::Tool
{
public:
    MoveTool(State& state, game::AnimationNodeClass* node)
      : mState(state)
      , mNode(node)
    {}
    virtual void Render(gfx::Painter&, gfx::Transform&) const override
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);

        const auto& tree = mState.animation->GetRenderTree();
        const auto& tree_node = tree.FindNodeByValue(mNode);
        const auto* parent = tree.FindParent(tree_node);
        // if the object we're moving has a parent we need to map the mouse movement
        // correctly taking into account that the hierarchy might include several rotations.
        // simplest thing to do is to map the mouse to the object's parent's coordinate space
        // and thus express/measure the object's translation delta relative to it's parent
        // (as it is in the hierarchy).
        // todo: this could be simplified if we expressed the view transformation in the render tree's
        // root node. then the below else branch should go away(?)
        if (parent && parent->GetValue())
        {
            const auto& mouse_pos_in_node = mState.animation->MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
            const auto& mouse_delta = mouse_pos_in_node - mPreviousMousePos;

            glm::vec2 position = mNode->GetTranslation();
            position.x += mouse_delta.x;
            position.y += mouse_delta.y;
            mNode->SetTranslation(position);
            mPreviousMousePos = mouse_pos_in_node;
        }
        else
        {
            // object doesn't have a parent, movement can be expressed using the animations
            // coordinate space.
            const auto& mouse_delta = mouse_pos_in_view - glm::vec4(mPreviousMousePos, 0.0f, 0.0f);
            glm::vec2 position = mNode->GetTranslation();
            position.x += mouse_delta.x;
            position.y += mouse_delta.y;
            mNode->SetTranslation(position);
            mPreviousMousePos = glm::vec2(mouse_pos_in_view.x, mouse_pos_in_view.y);
        }
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);

        // see the comments in MouseMove about the branched logic.
        const auto& tree = mState.animation->GetRenderTree();
        const auto& tree_node = tree.FindNodeByValue(mNode);
        const auto* parent = tree.FindParent(tree_node);
        if (parent && parent->GetValue())
        {
            mPreviousMousePos = mState.animation->MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
        }
        else
        {
            mPreviousMousePos = glm::vec2(mouse_pos_in_view.x, mouse_pos_in_view.y);
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        // nothing to be done here.
        return false;
    }
private:
    game::AnimationNodeClass* mNode = nullptr;
    AnimationWidget::State& mState;
    // previous mouse position, for each mouse move we update the objects'
    // position by the delta between previous and current mouse pos.
    glm::vec2 mPreviousMousePos;

};

class AnimationWidget::ResizeTool : public AnimationWidget::Tool
{
public:
    ResizeTool(State& state, game::AnimationNodeClass* node)
      : mState(state)
      , mNode(node)
    {}
    virtual void Render(gfx::Painter&, gfx::Transform&) const override
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
        const auto& mouse_pos_in_node = mState.animation->MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);

        // double the mouse movement so that the object's size follows the actual mouse movement.
        // Since the object's position is with respect to the center of the shape
        // adding some delta d to any extent (width or height i.e dx or dy) will
        // only grow that dimension by half d on either side of the axis, thus
        // falling behind the actual mouse movement.
        const auto& mouse_delta = (mouse_pos_in_node - mPreviousMousePos); // * 2.0f;

        const bool maintain_aspect_ratio = mickey->modifiers() & Qt::ControlModifier;
        if (maintain_aspect_ratio)
        {
            const glm::vec2& size = mNode->GetSize();

            const auto aspect_ratio = size.x / size.y;
            const auto new_height = std::clamp(size.y + mouse_delta.y, 0.0f, size.y + mouse_delta.y);
            const auto new_width  = new_height * aspect_ratio;
            mNode->SetSize(glm::vec2(new_width, new_height));
        }
        else
        {
            glm::vec2 size = mNode->GetSize();
            // don't allow negative sizes.
            size.x = std::clamp(size.x + mouse_delta.x, 0.0f, size.x + mouse_delta.x);
            size.y = std::clamp(size.y + mouse_delta.y, 0.0f, size.y + mouse_delta.y);
            mNode->SetSize(size);
        }
        mPreviousMousePos = mouse_pos_in_node;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
        mPreviousMousePos = mState.animation->MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        // nothing to be done here.
        return false;
    }
private:
    game::AnimationNodeClass* mNode = nullptr;
    AnimationWidget::State& mState;
    // previous mouse position, for each mouse move we update the objects'
    // position by the delta between previous and current mouse pos.
    glm::vec2 mPreviousMousePos;

};

class AnimationWidget::RotateTool : public AnimationWidget::Tool
{
public:
    RotateTool(State& state, game::AnimationNodeClass* node)
      : mState(state)
      , mNode(node)
    {}
    virtual void Render(gfx::Painter&, gfx::Transform&) const override
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);

        const auto& node_size = mNode->GetSize();
        const auto& node_center_in_view = glm::vec4(mState.animation->MapCoordsFromNode(node_size.x*0.5f, node_size.y*0.5, mNode),
            1.0f, 1.0f);

        // compute the delta between the current mouse position angle and the previous mouse position angle
        // wrt the node's center point. Then and add the angle delta increment to the node's rotation angle.
        const auto previous_angle = GetAngleRadians(mPreviousMousePos - node_center_in_view);
        const auto current_angle  = GetAngleRadians(mouse_pos_in_view - node_center_in_view);
        const auto angle_delta = current_angle - previous_angle;

        double angle = mNode->GetRotation();
        angle += angle_delta;
        // keep it in the -180 - 180 degrees [-Pi, Pi] range.
        angle = math::wrap(-math::Pi, math::Pi, angle);
        mNode->SetRotation(angle);

        //DEBUG("Node angle %1", qRadiansToDegrees(angle));

        mPreviousMousePos = mouse_pos_in_view;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        const auto& mouse_pos = mickey->pos();
        const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
        mPreviousMousePos = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        return false;
    }
private:
    float GetAngleRadians(const glm::vec4& p) const
    {
        const auto hypotenuse = std::sqrt(p.x*p.x + p.y*p.y);
        // acos returns principal angle range which is [0, Pi] radians
        // but we want to map to a range of [0, 2*Pi] i.e. full circle.
        // therefore we check the Y position.
        const auto principal_angle = std::acos(p.x / hypotenuse);
        if (p.y < 0.0f)
            return math::Pi * 2.0 - principal_angle;
        return principal_angle;
    }
private:
    AnimationWidget::State& mState;
    game::AnimationNodeClass* mNode = nullptr;
    // previous mouse position, for each mouse move we update the object's
    // position by the delta between previous and current mouse pos.
    glm::vec4 mPreviousMousePos;
};

AnimationWidget::AnimationWidget(app::Workspace* workspace)
{
    DEBUG("Create AnimationWidget");

    mUI.setupUi(this);
    mState.animation = std::make_shared<game::AnimationClass>();
    mTreeModel.reset(new TreeModel(*mState.animation));

    mState.scenegraph_tree_model = mTreeModel.get();
    mState.scenegraph_tree_view  = mUI.tree;
    mState.workspace = workspace;
    mState.renderer.SetLoader(workspace);

    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);

    mUI.widget->onZoomIn  = std::bind(&AnimationWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&AnimationWidget::ZoomOut, this);
    mUI.widget->onInitScene  = [&](unsigned width, unsigned height) {
        if (!mCameraWasLoaded) {
            // if the camera hasn't been loaded then compute now the
            // initial position for the camera.
            mState.camera_offset_x = width * 0.5;
            mState.camera_offset_y = height * 0.5;
        }

        // offset the viewport so that the origin of the 2d space is in the middle of the viewport
        const auto dist_x = mState.camera_offset_x  - (width / 2.0f);
        const auto dist_y = mState.camera_offset_y  - (height / 2.0f);
        SetValue(mUI.translateX, dist_x);
        SetValue(mUI.translateY, dist_y);
    };
    mUI.widget->onPaintScene = std::bind(&AnimationWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);

    mUI.widget->onMouseMove = [&](QMouseEvent* mickey) {
        if (mCurrentTool) {
            gfx::Transform view;
            view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
            view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
            view.Rotate(qDegreesToRadians(mUI.rotation->value()));
            view.Translate(mState.camera_offset_x, mState.camera_offset_y);
            mCurrentTool->MouseMove(mickey, view);
        }

        const auto width  = mUI.widget->width();
        const auto height = mUI.widget->height();

        // update the properties that might have changed as the result of application
        // of the current tool.

        // update the distance to center.
        const auto dist_x = mState.camera_offset_x - (width / 2.0f);
        const auto dist_y = mState.camera_offset_y - (height / 2.0f);
        mUI.translateX->setValue(dist_x);
        mUI.translateY->setValue(dist_y);

        DisplayCurrentNodeProperties();
    };
    mUI.widget->onMousePress = [&](QMouseEvent* mickey) {

        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);

        if (!mCurrentTool)
        {
            // On a mouse press start we want to select the tool based on where the pointer
            // is and which object it interects with in the game when the press starts.
            //
            // If the mouse pointer doesn't intersect with an object we create a new
            // camera tool for moving the viewport and object selection gets cleared.
            //
            // If the mouse pointer intersecs with an object that is the same object
            // that was already selected:
            // Check if the pointer intersects with one of the resizing boxes inside
            // the object's selection box. If it does then we create a new resizing tool
            // otherwise we create a new move tool instance for moving the object.
            //
            // If the mouse pointer intersects with an object that is not the same object
            // that was previously selected:
            // Select the object.
            //

            // take the widget space mouse coordinate and transform into view/camera space.
            const auto mouse_widget_position_x = mickey->pos().x();
            const auto mouse_widget_position_y = mickey->pos().y();

            const auto& widget_to_view = glm::inverse(view.GetAsMatrix());
            const auto& mouse_view_position = widget_to_view * glm::vec4(mouse_widget_position_x,
                mouse_widget_position_y, 1.0f, 1.0f);

            std::vector<game::AnimationNodeClass*> nodes_hit;
            std::vector<glm::vec2> hitbox_coords;
            mState.animation->CoarseHitTest(mouse_view_position.x, mouse_view_position.y,
                &nodes_hit, &hitbox_coords);

            // if nothing was hit clear the selection.
            if (nodes_hit.empty())
            {
                mUI.tree->ClearSelection();
                mCurrentTool.reset(new CameraTool(mState));
            }
            else
            {
                const TreeWidget::TreeItem* selected = mUI.tree->GetSelectedItem();
                const game::AnimationNodeClass* previous  = selected
                    ? static_cast<const game::AnimationNodeClass*>(selected->GetUserData())
                    : nullptr;

                // if the currently selected node is among the ones being hit
                // then retain that selection.
                // otherwise select the last one of the list. (the rightmost child)
                game::AnimationNodeClass* hit = nodes_hit.back();
                glm::vec2 hitpos = hitbox_coords.back();
                for (size_t i=0; i<nodes_hit.size(); ++i)
                {
                    if (nodes_hit[i] == previous)
                    {
                        hit = nodes_hit[i];
                        hitpos = hitbox_coords[i];
                        break;
                    }
                }

                const auto& size = hit->GetSize();
                // check if any particular special area of interest is being hit
                const bool bottom_right_hitbox_hit = hitpos.x >= size.x - 10.0f &&
                                                     hitpos.y >= size.y - 10.0f;
                const bool top_left_hitbox_hit     = hitpos.x >= 0 && hitpos.x <= 10.0f &&
                                                     hitpos.y >= 0 && hitpos.y <= 10.0f;
                if (bottom_right_hitbox_hit)
                    mCurrentTool.reset(new ResizeTool(mState, hit));
                else if (top_left_hitbox_hit)
                    mCurrentTool.reset(new RotateTool(mState, hit));
                else mCurrentTool.reset(new MoveTool(mState, hit));

                mUI.tree->SelectItemById(app::FromUtf8(hit->GetClassId()));
            }
        }
        if (mCurrentTool)
            mCurrentTool->MousePress(mickey, view);
    };
    mUI.widget->onMouseRelease = [&](QMouseEvent* mickey) {
        if (!mCurrentTool)
            return;

        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);

        mCurrentTool->MouseRelease(mickey, view);

        mCurrentTool.release();
        CheckPlacementActions(nullptr);
    };

    mUI.widget->onKeyPress = [&](QKeyEvent* key) {
        switch (key->key()) {
            case Qt::Key_Delete:
                on_actionNodeDelete_triggered();
                break;
            case Qt::Key_W:
                mState.camera_offset_y += 20.0f;
                break;
            case Qt::Key_S:
                mState.camera_offset_y -= 20.0f;
                break;
            case Qt::Key_A:
                mState.camera_offset_x += 20.0f;
                break;
            case Qt::Key_D:
                mState.camera_offset_x -= 20.0f;
                break;
            case Qt::Key_Left:
                UpdateCurrentNodePosition(-20.0f, 0.0f);
                break;
            case Qt::Key_Right:
                UpdateCurrentNodePosition(20.0f, 0.0f);
                break;
            case Qt::Key_Up:
                UpdateCurrentNodePosition(0.0f, -20.0f);
                break;
            case Qt::Key_Down:
                UpdateCurrentNodePosition(0.0f, 20.0f);
                break;
            case Qt::Key_Escape:
                mUI.tree->ClearSelection();
                break;

            default:
                return false;
        }
        return true;
    };

    // create the memu for creating instances of user defined drawables
    // since there doesn't seem to be a way to do this in the designer.
    mParticleSystems = new QMenu(this);
    mParticleSystems->menuAction()->setIcon(QIcon("icons:particle.png"));
    mParticleSystems->menuAction()->setText("Particle");
    mCustomShapes = new QMenu(this);
    mCustomShapes->menuAction()->setIcon(QIcon("icons:polygon.png"));
    mCustomShapes->menuAction()->setText("Polygon");

    PopulateFromEnum<game::AnimationNodeClass::RenderPass>(mUI.renderPass);
    PopulateFromEnum<game::AnimationNodeClass::RenderStyle>(mUI.renderStyle);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.name, QString("My Animation"));
    SetValue(mUI.ID, mState.animation->GetId());
    setWindowTitle("My Animation");

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged,
            this, &AnimationWidget::CurrentNodeChanged);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &AnimationWidget::treeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &AnimationWidget::treeClickEvent);

    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable,
            this,      &AnimationWidget::newResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,
            this,      &AnimationWidget::resourceToBeDeleted);
    connect(workspace, &app::Workspace::ResourceUpdated,
            this, &AnimationWidget::resourceUpdated);

    RebuildDrawableMenus();
    RebuildComboLists();
}

AnimationWidget::AnimationWidget(app::Workspace* workspace, const app::Resource& resource)
  : AnimationWidget(workspace)
{
    DEBUG("Editing animation '%1'", resource.GetName());
    const game::AnimationClass* content = nullptr;
    resource.GetContent(&content);
    mState.animation = std::make_shared<game::AnimationClass>(*content);
    mOriginalHash    = mState.animation->GetHash();
    mCameraWasLoaded = true;

    SetValue(mUI.name, resource.GetName());
    SetValue(mUI.ID, content->GetId());
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    setWindowTitle(resource.GetName());

    // if some resource has been deleted we need to replace it.
    for (size_t i=0; i<mState.animation->GetNumNodes(); ++i)
    {
        auto& node = mState.animation->GetNode(i);
        const auto& material = node.GetMaterialId();
        const auto& drawable = node.GetDrawableId();
        if (!material.empty() && !workspace->IsValidMaterial(material))
        {
            WARN("Animation node '%1' uses material '%2' that is deleted.", node.GetName(), material);
            node.SetMaterial("_checkerboard");
        }
        if (!drawable.empty() && !workspace->IsValidDrawable(drawable))
        {
            WARN("Animation node '%1' uses drawable '%2' that is deleted.", node.GetName(), drawable);
            node.SetDrawable("_rect");
        }
    }
    mTreeModel.reset(new TreeModel(*mState.animation));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
}

AnimationWidget::~AnimationWidget()
{
    DEBUG("Destroy AnimationWidget");
}

void AnimationWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewRect);
    bar.addAction(mUI.actionNewRoundRect);
    bar.addAction(mUI.actionNewCircle);
    bar.addAction(mUI.actionNewIsocelesTriangle);
    bar.addAction(mUI.actionNewRightTriangle);
    bar.addAction(mUI.actionNewTrapezoid);
    bar.addAction(mUI.actionNewParallelogram);
    bar.addAction(mUI.actionNewCapsule);
    bar.addSeparator();
    bar.addAction(mCustomShapes->menuAction());
    bar.addSeparator();
    bar.addAction(mParticleSystems->menuAction());

}
void AnimationWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewRect);
    menu.addAction(mUI.actionNewRoundRect);
    menu.addAction(mUI.actionNewCircle);
    menu.addAction(mUI.actionNewIsocelesTriangle);
    menu.addAction(mUI.actionNewRightTriangle);
    menu.addAction(mUI.actionNewTrapezoid);
    menu.addAction(mUI.actionNewParallelogram);
    menu.addAction(mUI.actionNewCapsule);
    menu.addSeparator();
    menu.addAction(mCustomShapes->menuAction());
    menu.addSeparator();
    menu.addAction(mParticleSystems->menuAction());
}

bool AnimationWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("Animation", mUI.name);
    settings.saveWidget("Animation", mUI.ID);
    settings.saveWidget("Animation", mUI.scaleX);
    settings.saveWidget("Animation", mUI.scaleY);
    settings.saveWidget("Animation", mUI.rotation);
    settings.saveWidget("Animation", mUI.chkShowOrigin);
    settings.saveWidget("Animation", mUI.chkShowGrid);
    settings.saveWidget("Animation", mUI.cmbGrid);
    settings.saveWidget("Animation", mUI.zoom);
    settings.saveWidget("Animation", mUI.widget);
    settings.setValue("Animation", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("Animation", "camera_offset_y", mState.camera_offset_y);
    // the animation can already serialize into JSON.
    // so let's use the JSON serialization in the animation
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mState.animation->ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Animation", "content", base64);
    return true;
}
bool AnimationWidget::LoadState(const Settings& settings)
{
    ASSERT(mState.workspace);

    settings.loadWidget("Animation", mUI.name);
    settings.loadWidget("Animation", mUI.ID);
    settings.loadWidget("Animation", mUI.scaleX);
    settings.loadWidget("Animation", mUI.scaleY);
    settings.loadWidget("Animation", mUI.rotation);
    settings.loadWidget("Animation", mUI.chkShowOrigin);
    settings.loadWidget("Animation", mUI.chkShowGrid);
    settings.loadWidget("Animation", mUI.cmbGrid);
    settings.loadWidget("Animation", mUI.zoom);
    settings.loadWidget("Animation", mUI.widget);
    setWindowTitle(mUI.name->text());

    mState.camera_offset_x = settings.getValue("Animation", "camera_offset_x", mState.camera_offset_x);
    mState.camera_offset_y = settings.getValue("Animation", "camera_offset_y", mState.camera_offset_y);
    // set a flag to *not* adjust the camere on gfx widget init to the middle the of widget.
    mCameraWasLoaded = true;

    const std::string& base64 = settings.getValue("Animation", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = game::AnimationClass::FromJson(json);
    if (!ret.has_value())
    {
        ERROR("Failed to load animation widget state.");
        return false;
    }
    auto klass = std::move(ret.value());
    auto hash  = klass.GetHash();
    mState.animation = FindSharedAnimation(hash);
    if (!mState.animation)
    {
        mState.animation = std::make_shared<game::AnimationClass>(std::move(klass));
        ShareAnimation(mState.animation);
    }

    // if some resource has been deleted we need to replace it.
    for (size_t i=0; i<mState.animation->GetNumNodes(); ++i)
    {
        auto& node = mState.animation->GetNode(i);
        const auto& material = node.GetMaterialId();
        const auto& drawable = node.GetDrawableId();
        if (!material.empty() && !mState.workspace->IsValidMaterial(material))
        {
            WARN("Animation node '%1' uses material '%2' that is deleted.", node.GetName(), material);
            node.SetMaterial("_checkerboard");
        }
        if (!drawable.empty() && !mState.workspace->IsValidDrawable(drawable))
        {
            WARN("Animation node '%1' uses drawable '%2' that is deleted.",
                node.GetName(), drawable);
            node.SetDrawable("_rect");
        }
    }

    mOriginalHash = mState.animation->GetHash();
    mTreeModel.reset(new TreeModel(*mState.animation));
    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();
    return true;
}

bool AnimationWidget::CanZoomIn() const
{
    const auto max = mUI.zoom->maximum();
    const auto val = mUI.zoom->value();
    return val < max;
}
bool AnimationWidget::CanZoomOut() const
{
    const auto min = mUI.zoom->minimum();
    const auto val = mUI.zoom->value();
    return val > min;
}

void AnimationWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void AnimationWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void AnimationWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}

void AnimationWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}

void AnimationWidget::Shutdown()
{
    mUI.widget->dispose();
}

void AnimationWidget::Update(double secs)
{
    // update the animation if we're currently playing
    if (mPlayState == PlayState::Playing)
    {
        mState.animation->Update(mAnimationTime, secs);
        mState.renderer.Update(*mState.animation, mAnimationTime, secs);

        mAnimationTime += secs;
        mUI.time->setText(QString::number(mAnimationTime));
    }
    mCurrentTime += secs;
}

void AnimationWidget::Render()
{
    mUI.widget->triggerPaint();
}

bool AnimationWidget::ConfirmClose()
{
    const auto hash = mState.animation->GetHash();
    if (hash == mOriginalHash)
        return true;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
    const auto ret = msg.exec();
    if (ret == QMessageBox::Cancel)
        return false;
    else if (ret == QMessageBox::No)
        return true;

    on_actionSave_triggered();
    return true;
}

void AnimationWidget::Refresh()
{
    auto selected = mUI.trackList->selectedItems();
    std::unordered_set<std::string> selected_item_ids;
    for (const auto* item : selected)
        selected_item_ids.insert(app::ToUtf8(item->data(Qt::UserRole).toString()));

    mUI.trackList->clear();
    mUI.btnDeleteTrack->setEnabled(false);
    mUI.btnEditTrack->setEnabled(false);
    for (size_t i=0; i<mState.animation->GetNumTracks(); ++i)
    {
        const auto& track = mState.animation->GetAnimationTrack(i);
        const auto& name  = app::FromUtf8(track.GetName());
        const auto& id    = app::FromUtf8(track.GetId());
        QListWidgetItem* item = new QListWidgetItem;
        item->setText(name);
        item->setData(Qt::UserRole, id);
        item->setIcon(QIcon("icons:animation_track.png"));
        mUI.trackList->addItem(item);
        if (selected_item_ids.find(track.GetId()) != selected_item_ids.end())
            item->setSelected(true);
    }
}

void AnimationWidget::on_actionPlay_triggered()
{
    mPlayState = PlayState::Playing;
    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}

void AnimationWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}

void AnimationWidget::on_actionStop_triggered()
{
    mAnimationTime = 0.0f;
    mPlayState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.time->setText("0");
}

void AnimationWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;
    const QString& name = GetValue(mUI.name);
    app::AnimationResource resource(*mState.animation, name);
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "widget", mUI.widget);

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.animation->GetHash();

    INFO("Saved animation '%1'", name);
    NOTE("Saved animation '%1'", name);
    setWindowTitle(name);
}

void AnimationWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "Rectangle"));

    CheckPlacementActions(mUI.actionNewRect);
}

void AnimationWidget::on_actionNewCircle_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "Circle"));

    CheckPlacementActions(mUI.actionNewCircle);
}

void AnimationWidget::on_actionNewIsocelesTriangle_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "IsoscelesTriangle"));

    CheckPlacementActions(mUI.actionNewIsocelesTriangle);
}

void AnimationWidget::on_actionNewRightTriangle_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "RightTriangle"));

    CheckPlacementActions(mUI.actionNewRightTriangle);
}

void AnimationWidget::on_actionNewRoundRect_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "RoundRect"));

    CheckPlacementActions(mUI.actionNewRoundRect);
}

void AnimationWidget::on_actionNewTrapezoid_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "Trapezoid"));

    CheckPlacementActions(mUI.actionNewTrapezoid);
}
void AnimationWidget::on_actionNewParallelogram_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "Parallelogram"));

    CheckPlacementActions(mUI.actionNewParallelogram);
}

void AnimationWidget::on_actionNewCapsule_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "Capsule"));

    CheckPlacementActions(mUI.actionNewCapsule);
}

void AnimationWidget::on_actionNodeDelete_triggered()
{
    const game::AnimationNodeClass* item = GetCurrentNode();
    if (item == nullptr)
        return;

    auto& tree = mState.animation->GetRenderTree();

    // find the game graph node that contains this AnimationNode.
    auto* node = tree.FindNodeByValue(item);

    // traverse the tree starting from the node to be deleted
    // and capture the ids of the animation nodes that are part
    // of this hierarchy.
    struct Carcass {
        std::string id;
        std::string name;
    };
    std::vector<Carcass> graveyard;
    node->PreOrderTraverseForEach([&](game::AnimationNodeClass* value) {
        Carcass carcass;
        carcass.id   = value->GetClassId();
        carcass.name = value->GetName();
        graveyard.push_back(carcass);
    });

    for (auto& carcass : graveyard)
    {
        DEBUG("Deleting child '%1', %2", carcass.name, carcass.id);
        mState.animation->DeleteNodeById(carcass.id);
    }

    // find the parent node
    auto* parent = tree.FindParent(node);

    parent->DeleteChild(node);

    mUI.tree->Rebuild();
}

void AnimationWidget::on_actionNodeMoveUpLayer_triggered()
{
    game::AnimationNodeClass* node = GetCurrentNode();
    if (node == nullptr)
        return;
    const int layer = node->GetLayer();
    node->SetLayer(layer + 1);

    DisplayCurrentNodeProperties();
}
void AnimationWidget::on_actionNodeMoveDownLayer_triggered()
{
    game::AnimationNodeClass* node = GetCurrentNode();
    if (node == nullptr)
        return;
    const int layer = node->GetLayer();
    node->SetLayer(layer - 1);

    DisplayCurrentNodeProperties();
}

void AnimationWidget::on_actionNodeDuplicate_triggered()
{
    game::AnimationNodeClass* node = GetCurrentNode();
    if (node == nullptr)
        return;

    // do a deep copy of a hierarchy of nodes starting from
    // the selected node and add the new hierarchy as a new
    // child of the selected node's parent

    auto& tree = mState.animation->GetRenderTree();
    auto* tree_node = tree.FindNodeByValue(node);
    auto* tree_node_parent = tree.FindParent(tree_node);

    // deep copy of the node.
    auto copy_root = tree_node->Clone();
    // replace all node references with copies of the nodes.
    copy_root.PreOrderTraverseForEachTreeNode([&](game::AnimationClass::RenderTreeNode* node) {
        game::AnimationNodeClass* child = mState.animation->AddNode((*node)->Clone());
        child->SetName(base::FormatString("Copy of %1", (*node)->GetName()));
        node->SetValue(child);
    });
    // update the the translation for the parent of the new hierarchy
    // so that it's possible to tell it apart from the source of the copy.
    copy_root->SetTranslation(node->GetTranslation() * 1.2f);
    tree_node_parent->AppendChild(std::move(copy_root));

    mState.scenegraph_tree_view->Rebuild();
    mState.scenegraph_tree_view->SelectItemById(app::FromUtf8(copy_root->GetClassId()));
}

void AnimationWidget::on_tree_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
}

void AnimationWidget::on_plus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(math::clamp(-180.0, 180.0, value + 90.0f));
    mViewTransformRotation = value;
    mViewTransformStartTime = mCurrentTime;
}

void AnimationWidget::on_minus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(math::clamp(-180.0, 180.0, value - 90.0f));
    mViewTransformRotation = value;
    mViewTransformStartTime = mCurrentTime;
}

void AnimationWidget::on_cPlus90_clicked()
{
    const auto value = mUI.nodeRotation->value();
    mUI.nodeRotation->setValue(math::clamp(-180.0, 180.0, value + 90.0f));
}
void AnimationWidget::on_cMinus90_clicked()
{
    const auto value = mUI.nodeRotation->value();
    mUI.nodeRotation->setValue(math::clamp(-180.0, 180.0, value - 90.0f));
}

void AnimationWidget::on_resetTransform_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mState.camera_offset_x = width * 0.5f;
    mState.camera_offset_y = height * 0.5f;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // this is camera offset to the center of the widget.
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
}

void AnimationWidget::on_materials_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        node->ResetMaterial();
        if (!name.isEmpty())
            node->SetMaterial(mState.workspace->GetMaterialClassByName(name)->GetId());
    }
}

void AnimationWidget::on_drawables_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        node->ResetDrawable();
        if (!name.isEmpty())
            node->SetDrawable(mState.workspace->GetDrawableClassByName(name)->GetId());
    }
}

void AnimationWidget::on_renderPass_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const game::AnimationNodeClass::RenderPass pass = GetValue(mUI.renderPass);
        node->SetRenderPass(pass);
    }
}

void AnimationWidget::on_renderStyle_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const game::AnimationNodeClass::RenderStyle style = GetValue(mUI.renderStyle);
        node->SetRenderStyle(style);
    }
}

void AnimationWidget::CurrentNodeChanged()
{
    const game::AnimationNodeClass* node = GetCurrentNode();
    if (node == nullptr)
    {
        mUI.cProperties->setEnabled(false);
        mUI.cTransform->setEnabled(false);
    }
    else
    {
        mUI.cProperties->setEnabled(true);
        mUI.cTransform->setEnabled(true);
        DisplayCurrentNodeProperties();
    }
}

void AnimationWidget::placeNewParticleSystem()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the text in the action as the name of the drawable.
    const auto drawable = action->text();

    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceByName(drawable, app::Resource::Type::ParticleSystem);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceTool(mState, material, drawable));
}

void AnimationWidget::placeNewCustomShape()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the text in the action as the name of the drawable.
    const auto drawable = action->text();
    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceByName(drawable, app::Resource::Type::CustomShape);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceTool(mState, material, drawable));
}

void AnimationWidget::newResourceAvailable(const app::Resource* resource)
{
    RebuildComboLists();
    RebuildDrawableMenus();
}

void AnimationWidget::resourceUpdated(const app::Resource* resource)
{
    RebuildComboLists();
    RebuildDrawableMenus();
    DisplayCurrentNodeProperties();
}

void AnimationWidget::resourceToBeDeleted(const app::Resource* resource)
{
    for (size_t i=0; i<mState.animation->GetNumNodes(); ++i)
    {
        auto& node = mState.animation->GetNode(i);
        if (node.GetMaterialId() == resource->GetIdUtf8())
        {
            WARN("Animation node '%1' uses a material '%2' that is deleted.", node.GetName(), resource->GetName());
            node.SetMaterial("_checkerboard");
        }
        else if (node.GetDrawableId() == resource->GetIdUtf8())
        {
            WARN("Animation node '%1' uses a drawable '%2' that is deleted.", node.GetName(), resource->GetName());
            node.SetDrawable("_rect");
        }
    }

    RebuildComboLists();
    RebuildDrawableMenus();
    DisplayCurrentNodeProperties();
}

void AnimationWidget::treeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.animation->GetRenderTree();
    auto* src_value = static_cast<game::AnimationNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::AnimationNodeClass*>(target->GetUserData());

    // find the game graph node that contains this AnimationNode.
    auto* src_node   = tree.FindNodeByValue(src_value);
    auto* src_parent = tree.FindParent(src_node);

    // check if we're trying to drag a parent onto its own child
    if (src_node->FindNodeByValue(dst_value))
        return;

    game::AnimationClass::RenderTreeNode branch = *src_node;
    src_parent->DeleteChild(src_node);

    auto* dst_node  = tree.FindNodeByValue(dst_value);
    dst_node->AppendChild(std::move(branch));

}

void AnimationWidget::treeClickEvent(TreeWidget::TreeItem* item)
{
    //DEBUG("Tree click event: %1", item->GetId());
    auto* node = static_cast<game::AnimationNodeClass*>(item->GetUserData());
    if (node == nullptr)
        return;

    const bool visibility = node->TestFlag(game::AnimationNodeClass::Flags::VisibleInEditor);
    node->SetFlag(game::AnimationNodeClass::Flags::VisibleInEditor, !visibility);
    item->SetIconMode(visibility ? QIcon::Disabled : QIcon::Normal);
}

void AnimationWidget::on_layer_valueChanged(int layer)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetLayer(layer);
    }
}

void AnimationWidget::on_lineWidth_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetLineWidth(value);
    }
}

void AnimationWidget::on_alpha_valueChanged()
{
    UpdateCurrentNodeAlpha();
}

void AnimationWidget::on_nodeSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
    }
}
void AnimationWidget::on_nodeSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
    }
}
void AnimationWidget::on_nodeTranslateX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.x = value;
        node->SetTranslation(translate);
    }
}
void AnimationWidget::on_nodeTranslateY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.y = value;
        node->SetTranslation(translate);
    }
}

void AnimationWidget::on_nodeScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
    }
}

void AnimationWidget::on_nodeScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
    }
}

void AnimationWidget::on_nodeRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetRotation(qDegreesToRadians(value));
    }
}

void AnimationWidget::on_nodeName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    if (!item->GetUserData())
        return;
    auto* node = static_cast<game::AnimationNodeClass*>(item->GetUserData());

    node->SetName(app::ToUtf8(text));
    item->SetText(text);

    mUI.tree->Update();
}

void AnimationWidget::on_chkUpdateMaterial_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNodeClass::Flags::UpdateMaterial, state);
    }
}
void AnimationWidget::on_chkUpdateDrawable_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNodeClass::Flags::UpdateDrawable, state);
    }
}
void AnimationWidget::on_chkDoesRender_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNodeClass::Flags::DoesRender, state);
    }
}

void AnimationWidget::on_chkRestart_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNodeClass::Flags::RestartDrawable, state);
    }
}

void AnimationWidget::on_chkOverrideAlpha_stateChanged(int state)
{
    UpdateCurrentNodeAlpha();
}

void AnimationWidget::on_btnNewTrack_clicked()
{
    // sharing the animation class object with the new animation
    // track widget.
    AnimationTrackWidget* widget = new AnimationTrackWidget(mState.workspace, mState.animation);
    emit OpenNewWidget(widget);
}
void AnimationWidget::on_btnEditTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    QString id = item->data(Qt::UserRole).toString();

    for (size_t i=0; i<mState.animation->GetNumTracks(); ++i)
    {
        const auto& klass = mState.animation->GetAnimationTrack(i);
        if (klass.GetId() != app::ToUtf8(id))
            continue;

        AnimationTrackWidget* widget = new AnimationTrackWidget(mState.workspace,
            mState.animation, klass);
        emit OpenNewWidget(widget);
    }
}
void AnimationWidget::on_btnDeleteTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    QString id = item->data(Qt::UserRole).toString();

    mState.animation->DeleteAnimationTrackById(app::ToUtf8(id));

    // this will remove it from the widget.
    delete item;
}

void AnimationWidget::on_trackList_itemSelectionChanged()
{
    auto list = mUI.trackList->selectedItems();
    mUI.btnEditTrack->setEnabled(!list.isEmpty());
    mUI.btnDeleteTrack->setEnabled(!list.isEmpty());
}

void AnimationWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto view_rotation_time = math::clamp(0.0f, 1.0f,
        mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    gfx::Transform view;
    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    // camera offset should be reflected in the translateX/Y UI components as well.
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    class DrawHook : public game::AnimationClassDrawHook
    {
    public:
        DrawHook(game::AnimationNodeClass* selected, PlayState playstate)
          : mSelected(selected)
          , mPlayState(playstate)
        {}
        virtual bool InspectPacket(const game::AnimationNodeClass* node, game::AnimationDrawPacket&) override
        {
            if (!node->TestFlag(game::AnimationNodeClass::Flags::VisibleInEditor))
                return false;
            return true;
        }
        virtual void AppendPackets(const game::AnimationNodeClass* node, gfx::Transform& trans, std::vector<game::AnimationDrawPacket>& packets) override
        {
            const auto is_mask     = node->GetRenderPass() == game::AnimationNodeClass::RenderPass::Mask;
            const auto is_selected = node == mSelected;
            const auto is_playing  = mPlayState == PlayState::Playing;

            if (is_mask && !is_selected && !is_playing)
            {
                static const auto yellow = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::DarkYellow));
                static const auto rect  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
                // visualize it.
                trans.Push(node->GetModelTransform());
                    game::AnimationDrawPacket box;
                    box.transform = trans.GetAsMatrix();
                    box.material  = yellow;
                    box.drawable  = rect; //node->GetDrawable();
                    box.layer     = node->GetLayer() + 1;
                    box.pass      = game::AnimationNodeClass::RenderPass::Draw;
                    packets.push_back(box);
                trans.Pop();
            }

            if (!is_selected)
                return;

            static const auto green = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::Green));
            static const auto rect  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
            static const auto circle = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline, 2.0f);
            const auto& size = node->GetSize();
            const auto layer = is_mask ? node->GetLayer() + 1 : node->GetLayer();

            // draw the selection rectangle.
            trans.Push(node->GetModelTransform());
                game::AnimationDrawPacket selection;
                selection.transform = trans.GetAsMatrix();
                selection.material  = green;
                selection.drawable  = rect;
                selection.layer     = layer;
                packets.push_back(selection);
            trans.Pop();

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
                game::AnimationDrawPacket sizing_box;
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
                game::AnimationDrawPacket rotation_circle;
                rotation_circle.transform = trans.GetAsMatrix();
                rotation_circle.material  = green;
                rotation_circle.drawable  = circle;
                rotation_circle.layer     = layer;
                packets.push_back(rotation_circle);
            trans.Pop();
        }
    private:
        const game::AnimationNodeClass* mSelected = nullptr;
        const PlayState mPlayState = PlayState::Playing;
    };

    DrawHook hook(GetCurrentNode(), mPlayState);

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        view.Push();

        const float zoom = GetValue(mUI.zoom);
        const float xs = GetValue(mUI.scaleX);
        const float ys = GetValue(mUI.scaleY);
        const int grid_size = std::max(width / xs, height / ys) / zoom;
        // work out the scale factor for the grid. we want some convenient scale so that
        // each grid cell maps to some convenient number of units (a multiple of 10)
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const auto cell_size_units  = static_cast<int>(grid); //50;
        const auto num_grid_lines = (grid_size / cell_size_units) - 1;
        const auto num_cells = num_grid_lines + 1;
        const auto cell_size_normalized = 1.0f / (num_grid_lines + 1);
        const auto cell_scale_factor = cell_size_units / cell_size_normalized;

        // figure out what is the current coordinate of the center of the window/viewport in
        // as expressed in the view transformation's coordinate space. (In other words
        // figure out which combination of view basis axis puts me in the middle of the window
        // in window space.)
        auto world_to_model = glm::inverse(view.GetAsMatrix());
        auto world_origin_in_model = world_to_model * glm::vec4(width / 2.0f, height / 2.0, 1.0f, 1.0f);

        view.Scale(cell_scale_factor, cell_scale_factor);

        // to make the grid cover the whole viewport we can easily do it by rendering
        // the grid in each quadrant of the coordinate space aligned around the center
        // point of the viewport. Then it doesn't matter if the view transformation
        // includes rotation or not.
        const auto grid_origin_x = (int)world_origin_in_model.x / cell_size_units * cell_size_units;
        const auto grid_origin_y = (int)world_origin_in_model.y / cell_size_units * cell_size_units;
        const auto grid_width = cell_size_units * num_cells;
        const auto grid_height = cell_size_units * num_cells;

        view.Translate(grid_origin_x, grid_origin_y);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));
        view.Translate(-grid_width, 0);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));
        view.Translate(0, -grid_height);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));
        view.Translate(grid_width, 0);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));

        view.Pop();
    }

    // begin the animation transformation space
    view.Push();
        mState.renderer.BeginFrame();
        mState.renderer.Draw(*mState.animation, painter, view, &hook);
        mState.renderer.EndFrame();
    view.Pop();

    if (mCurrentTool)
        mCurrentTool->Render(painter, view);

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        view.Push();
            view.Scale(100.0f, 5.0f);
            view.Translate(0.0f, -2.5f);
            painter.Draw(gfx::Arrow(), view, gfx::SolidColor(gfx::Color::Green));
        view.Pop();

        view.Push();
            view.Scale(100.0f, 5.0f);
            view.Translate(-50.0f, -2.5f);
            view.Rotate(math::Pi * 0.5f);
            view.Translate(0.0f, 50.0f);
            painter.Draw(gfx::Arrow(), view, gfx::SolidColor(gfx::Color::Red));
        view.Pop();
    }

    // pop view transformation
    view.Pop();
}

game::AnimationNodeClass* AnimationWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    if (!item->GetUserData())
        return nullptr;
    return static_cast<game::AnimationNodeClass*>(item->GetUserData());
}

void AnimationWidget::UpdateCurrentNodeAlpha()
{
    if (auto* node = GetCurrentNode())
    {
        const float value  = mUI.alpha->value();
        const float max    = mUI.alpha->maximum();
        const float alpha  = value / max;
        const bool checked = GetValue(mUI.chkOverrideAlpha);
        node->SetFlag(game::AnimationNodeClass::Flags::OverrideAlpha, checked);
        node->SetAlpha(alpha);
        const auto& material = mState.workspace->FindMaterialClass(node->GetMaterialId());

        if (!checked || !material)
            return;

        const bool has_alpha_blending = material->GetSurfaceType() ==
                                        gfx::MaterialClass::SurfaceType::Transparent;
        if (has_alpha_blending)
            return;

        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("The current material doesn't enable transparency. Setting alpha will have no effect."));
        msg.exec();
    }
}

void AnimationWidget::UpdateCurrentNodePosition(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
    }
}

void AnimationWidget::DisplayCurrentNodeProperties()
{
    if (const auto* node = GetCurrentNode())
    {
        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        const auto& scale = node->GetScale();
        const auto& material = mState.workspace->MapMaterialIdToName(node->GetMaterialId());
        const auto& drawable = mState.workspace->MapDrawableIdToName(node->GetDrawableId());
        SetValue(mUI.nodeID, node->GetClassId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.renderPass, node->GetRenderPass());
        SetValue(mUI.renderStyle, node->GetRenderStyle());
        SetValue(mUI.layer, node->GetLayer());
        SetValue(mUI.materials, material);
        SetValue(mUI.drawables, drawable);
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeSizeX, size.x);
        SetValue(mUI.nodeSizeY, size.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));
        SetValue(mUI.lineWidth, node->GetLineWidth());
        SetValue(mUI.alpha, NormalizedFloat(node->GetAlpha()));
        SetValue(mUI.chkUpdateMaterial, node->TestFlag(game::AnimationNodeClass::Flags::UpdateMaterial));
        SetValue(mUI.chkUpdateDrawable, node->TestFlag(game::AnimationNodeClass::Flags::UpdateDrawable));
        SetValue(mUI.chkDoesRender, node->TestFlag(game::AnimationNodeClass::Flags::DoesRender));
    }
}

void AnimationWidget::RebuildComboLists()
{
    // Prepend an empty string so that the node's drawable/material
    // can actually be set to "nothing" which is convenient when
    // it's just node that doesn't need to render anything (transformation node)
    SetList(mUI.materials, QStringList("") + mState.workspace->ListAllMaterials());
    SetList(mUI.drawables, QStringList("") + mState.workspace->ListAllDrawables());
}

void AnimationWidget::RebuildDrawableMenus()
{
    // rebuild the drawable menus for custom shapes
    // and particle systems.
    mParticleSystems->clear();
    mCustomShapes->clear();
    for (size_t i = 0; i < mState.workspace->GetNumResources(); ++i)
    {
        const auto &resource = mState.workspace->GetResource(i);
        const auto &name = resource.GetName();
        if (resource.GetType() == app::Resource::Type::ParticleSystem)
        {
            QAction *action = mParticleSystems->addAction(name);
            connect(action, &QAction::triggered,
                    this, &AnimationWidget::placeNewParticleSystem);
        }
        else if (resource.GetType() == app::Resource::Type::CustomShape)
        {
            QAction *action = mCustomShapes->addAction(name);
            connect(action, &QAction::triggered,
                    this, &AnimationWidget::placeNewCustomShape);
        }
    }
}

void AnimationWidget::CheckPlacementActions(QAction* selected)
{
    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewIsocelesTriangle->setChecked(false);
    mUI.actionNewRightTriangle->setChecked(false);
    mUI.actionNewRoundRect->setChecked(false);
    mUI.actionNewTrapezoid->setChecked(false);
    mUI.actionNewParallelogram->setChecked(false);
    mUI.actionNewCapsule->setChecked(false);

    if (selected)
        selected->setChecked(true);
}

} // namespace
