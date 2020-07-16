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
#include "warnpop.h"

#include <algorithm>
#include <cmath>

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "base/assert.h"
#include "base/math.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "animationwidget.h"
#include "utility.h"
#include "treewidget.h"

namespace gui
{

class AnimationWidget::TreeModel : public TreeWidget::TreeModel
{
public:
    TreeModel(game::Animation& anim)  : mAnimation(anim)
    {}

    virtual void Flatten(std::vector<TreeWidget::TreeItem>& list)
    {
        auto& root = mAnimation.GetRenderTree();

        class Visitor : public game::Animation::RenderTree::Visitor
        {
        public:
            Visitor(std::vector<TreeWidget::TreeItem>& list)
                : mList(list)
            {}
            virtual void EnterNode(game::AnimationNode* node)
            {
                TreeWidget::TreeItem item;
                item.SetId(node ? app::FromUtf8(node->GetId()) : "root");
                item.SetText(node ? app::FromUtf8(node->GetName()) : "Root");
                item.SetUserData(node);
                item.SetLevel(mLevel);
                if (node)
                {
                    item.SetIcon(QIcon("icons:eye.png"));
                    if (!node->TestFlag(game::AnimationNode::Flags::VisibleInEditor))
                        item.SetIconMode(QIcon::Disabled);
                    else item.SetIconMode(QIcon::Normal);
                }
                mList.push_back(item);
                mLevel++;
            }
            virtual void LeaveNode(game::AnimationNode* node)
            {
                mLevel--;
            }

        private:
            unsigned mLevel = 0;
            std::vector<TreeWidget::TreeItem>& mList;
        };
        Visitor visitor(list);
        root.PreOrderTraverse(visitor);
    }
private:
    game::Animation& mAnimation;
};


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
        mDrawable = mState.workspace->MakeDrawable(mDrawableName);
        mMaterial = mState.workspace->MakeMaterial(mMaterialName);
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

        game::AnimationNode node;
        node.SetMaterial(app::ToUtf8(mMaterialName), mMaterial);
        node.SetDrawable(app::ToUtf8(mDrawableName), mDrawable);
        node.SetName(name);
        // the given object position is to be aligned with the center of the shape
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto& root  = mState.animation.GetRenderTree();
        auto* child = mState.animation.AddNode(std::move(node));
        root.AppendChild(child);

        mState.scenegraph_tree_view->Rebuild();
        mState.scenegraph_tree_view->SelectItemById(app::FromUtf8(child->GetId()));

        DEBUG("Added new shape '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.animation.GetNumNodes(); ++i)
        {
            const auto& node = mState.animation.GetNode(i);
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
    std::shared_ptr<gfx::Drawable> mDrawable;
    std::shared_ptr<gfx::Material> mMaterial;
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
    MoveTool(State& state, game::AnimationNode* node)
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

        const auto& tree = mState.animation.GetRenderTree();
        const auto& tree_node = tree.FindNodeByValue(mNode);
        const auto* parent = tree.FindParent(tree_node);
        // if the object we're moving has a parent we need to map the mouse movement
        // correctly taking into account that the hierarchy might include several rotations.
        // simplest thing to do is to map the mouse to the object's parent's coordinate space
        // and thus express/measure the object's translation delta relative to it's parent
        // (as it is in the hiearchy).
        // todo: this could be simplified if we expressed the view transformation in the render tree's
        // root node. then the below else branch should go away(?)
        if (parent && parent->GetValue())
        {
            const auto& mouse_pos_in_node = mState.animation.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
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
        const auto& tree = mState.animation.GetRenderTree();
        const auto& tree_node = tree.FindNodeByValue(mNode);
        const auto* parent = tree.FindParent(tree_node);
        if (parent && parent->GetValue())
        {
            mPreviousMousePos = mState.animation.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
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
    game::AnimationNode* mNode = nullptr;
    AnimationWidget::State& mState;
    // previous mouse position, for each mouse move we update the objects'
    // position by the delta between previous and current mouse pos.
    glm::vec2 mPreviousMousePos;

};

class AnimationWidget::ResizeTool : public AnimationWidget::Tool
{
public:
    ResizeTool(State& state, game::AnimationNode* node)
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
        const auto& mouse_pos_in_node = mState.animation.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);

        // double the mouse movement so that the object's size follows the actual mouse movement.
        // Since the object's position is with respect to the center of the shape
        // adding some delta d to any extent (width or height i.e dx or dy) will
        // only grow that dimension by half d on either side of the axis, thus
        // falling behind the actual mouse movement.
        const auto& mouse_delta = (mouse_pos_in_node - mPreviousMousePos) * 2.0f;

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
        mPreviousMousePos = mState.animation.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
    {
        // nothing to be done here.
        return false;
    }
private:
    game::AnimationNode* mNode = nullptr;
    AnimationWidget::State& mState;
    // previous mouse position, for each mouse move we update the objects'
    // position by the delta between previous and current mouse pos.
    glm::vec2 mPreviousMousePos;

};

class AnimationWidget::RotateTool : public AnimationWidget::Tool
{
public:
    RotateTool(State& state, game::AnimationNode* node)
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
        const auto& node_center_in_view = glm::vec4(mState.animation.MapCoordsFromNode(node_size.x*0.5f, node_size.y*0.5, mNode),
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
    game::AnimationNode* mNode = nullptr;
    // previous mouse position, for each mouse move we update the object's
    // position by the delta between previous and current mouse pos.
    glm::vec4 mPreviousMousePos;
};

AnimationWidget::AnimationWidget(app::Workspace* workspace)
{
    DEBUG("Create AnimationWidget");

    mUI.setupUi(this);

    mTreeModel.reset(new TreeModel(mState.animation));

    mState.scenegraph_tree_model = mTreeModel.get();
    mState.scenegraph_tree_view  = mUI.tree;
    mState.workspace = workspace;

    // this fucking cunt whore will already emit selection changed signal
    mUI.materials->blockSignals(true);
    mUI.materials->addItems(workspace->ListMaterials());
    mUI.materials->blockSignals(false);

    mUI.drawables->blockSignals(true);
    mUI.drawables->addItems(workspace->ListDrawables());
    mUI.drawables->blockSignals(false);

    mUI.name->setText("My Animation");

    mUI.tree->SetModel(mTreeModel.get());
    mUI.tree->Rebuild();

    mUI.widget->setFramerate(60);
    mUI.widget->onZoomIn  = std::bind(&AnimationWidget::zoomIn, this);
    mUI.widget->onZoomOut = std::bind(&AnimationWidget::zoomOut, this);
    mUI.widget->onInitScene  = [&](unsigned width, unsigned height) {
        if (!mCameraWasLoaded) {
            mState.camera_offset_x = width * 0.5;
            mState.camera_offset_y = height * 0.5;
        }

        // offset the viewport so that the origin of the 2d space is in the middle of the viewport
        const auto dist_x = mState.camera_offset_x  - (width / 2.0f);
        const auto dist_y = mState.camera_offset_y  - (height / 2.0f);
        mUI.translateX->setValue(dist_x);
        mUI.translateY->setValue(dist_y);
    };
    mUI.widget->onPaintScene = std::bind(&AnimationWidget::paintScene,
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

        updateCurrentNodeProperties();
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

            std::vector<game::AnimationNode*> nodes_hit;
            std::vector<glm::vec2> hitbox_coords;
            mState.animation.CoarseHitTest(mouse_view_position.x, mouse_view_position.y,
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
                const game::AnimationNode* previous  = selected
                    ? static_cast<const game::AnimationNode*>(selected->GetUserData())
                    : nullptr;

                // if the currently selected node is among the ones being hit
                // then retain that selection.
                // otherwise select the last one of the list. (the rightmost child)
                game::AnimationNode* hit = nodes_hit.back();
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

                mUI.tree->SelectItemById(app::FromUtf8(hit->GetId()));
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
        mUI.actionNewRect->setChecked(false);
        mUI.actionNewCircle->setChecked(false);
        mUI.actionNewTriangle->setChecked(false);
        mUI.actionNewArrow->setChecked(false);
    };

    mUI.widget->onKeyPress = [&](QKeyEvent* key) {
        switch (key->key()) {
            case Qt::Key_Delete:
                on_actionDeleteComponent_triggered();
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
                updateCurrentNodePosition(-20.0f, 0.0f);
                break;
            case Qt::Key_Right:
                updateCurrentNodePosition(20.0f, 0.0f);
                break;
            case Qt::Key_Up:
                updateCurrentNodePosition(0.0f, -20.0f);
                break;
            case Qt::Key_Down:
                updateCurrentNodePosition(0.0f, 20.0f);
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
    mDrawableMenu = new QMenu(this);
    mDrawableMenu->menuAction()->setIcon(QIcon("icons:particle.png"));
    mDrawableMenu->menuAction()->setText("New...");

    // update the drawable menu with drawable items.
    for (size_t i=0; i<workspace->GetNumResources(); ++i)
    {
        const auto& resource = workspace->GetResource(i);
        const auto& name = resource.GetName();
        if (resource.GetType() == app::Resource::Type::ParticleSystem)
        {
            QAction* action = mDrawableMenu->addAction(name);
            connect(action, &QAction::triggered,
                    this,   &AnimationWidget::placeNewParticleSystem);
        }
    }


    setWindowTitle("My Animation");

    PopulateFromEnum<game::AnimationNode::RenderPass>(mUI.renderPass);
    PopulateFromEnum<game::AnimationNode::RenderStyle>(mUI.renderStyle);

    connect(mUI.tree, &TreeWidget::currentRowChanged,
            this, &AnimationWidget::currentComponentRowChanged);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &AnimationWidget::treeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &AnimationWidget::treeClickEvent);

    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable,
            this,      &AnimationWidget::newResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,
            this,      &AnimationWidget::resourceToBeDeleted);
}

AnimationWidget::AnimationWidget(app::Workspace* workspace, const app::Resource& resource)
  : AnimationWidget(workspace)
{
    DEBUG("Editing animation '%1'", resource.GetName());
    mUI.name->setText(resource.GetName());
    setWindowTitle(resource.GetName());

    mState.animation = *resource.GetContent<game::Animation>();
    mState.animation.Prepare(*workspace);

    mUI.tree->Rebuild();
}

AnimationWidget::~AnimationWidget()
{
    DEBUG("Destroy AnimationWidget");
}

void AnimationWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewRect);
    bar.addAction(mUI.actionNewCircle);
    bar.addAction(mUI.actionNewTriangle);
    bar.addAction(mUI.actionNewArrow);
    bar.addSeparator();
    bar.addAction(mDrawableMenu->menuAction());
}
void AnimationWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewRect);
    menu.addAction(mUI.actionNewCircle);
    menu.addAction(mUI.actionNewTriangle);
    menu.addAction(mUI.actionNewArrow);
    menu.addSeparator();
    menu.addAction(mDrawableMenu->menuAction());
}

bool AnimationWidget::saveState(Settings& settings) const
{
    settings.saveWidget("Animation", mUI.name);
    settings.saveWidget("Animation", mUI.scaleX);
    settings.saveWidget("Animation", mUI.scaleY);
    settings.saveWidget("Animation", mUI.rotation);
    settings.saveWidget("Animation", mUI.showGrid);
    settings.saveWidget("Animation", mUI.zoom);

    settings.setValue("Animation", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("Animation", "camera_offset_y", mState.camera_offset_y);

    // the animation can already serialize into JSON.
    // so let's use the JSON serialization in the animation
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mState.animation.ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Animation", "content", base64);
    return true;
}
bool AnimationWidget::loadState(const Settings& settings)
{
    ASSERT(mState.workspace);

    settings.loadWidget("Animation", mUI.name);
    settings.loadWidget("Animation", mUI.scaleX);
    settings.loadWidget("Animation", mUI.scaleY);
    settings.loadWidget("Animation", mUI.rotation);
    settings.loadWidget("Animation", mUI.showGrid);
    settings.loadWidget("Animation", mUI.zoom);

    mState.camera_offset_x = settings.getValue("Animation", "camera_offset_x", mState.camera_offset_x);
    mState.camera_offset_y = settings.getValue("Animation", "camera_offset_y", mState.camera_offset_y);
    // set a flag to *not* adjust the camere on gfx widget init to the middle the of widget.
    mCameraWasLoaded = true;

    const std::string& base64 = settings.getValue("Animation", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = game::Animation::FromJson(json);
    if (!ret.has_value())
    {
        WARN("Failed to load animation widget state.");
        return false;
    }
    mState.animation = std::move(ret.value());
    mState.animation.Prepare(*mState.workspace);

    mUI.tree->Rebuild();
    return true;
}

void AnimationWidget::zoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void AnimationWidget::zoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void AnimationWidget::reloadShaders()
{
    mUI.widget->reloadShaders();
}

void AnimationWidget::reloadTextures()
{
    mUI.widget->reloadTextures();
}

void AnimationWidget::shutdown()
{
    mUI.widget->dispose();
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
    mPlayState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mTime = 0.0f;

    mState.animation.Reset();
}

void AnimationWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;
    const QString& name = GetValue(mUI.name);

    if (mState.workspace->HasResource(name, app::Resource::Type::Animation))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText("Workspace already contains animation by this name. Overwrite?");
        if (msg.exec() == QMessageBox::No)
            return;
    }

    app::AnimationResource resource(mState.animation, name);
    mState.workspace->SaveResource(resource);
    INFO("Saved animation '%1'", name);
    NOTE("Saved animation '%1'", name);

    setWindowTitle(name);
}

void AnimationWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "__Primitive_Rectangle"));

    mUI.actionNewRect->setChecked(true);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewTriangle->setChecked(false);
    mUI.actionNewArrow->setChecked(false);
}

void AnimationWidget::on_actionNewCircle_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "__Primitive_Circle"));

    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(true);
    mUI.actionNewTriangle->setChecked(false);
    mUI.actionNewArrow->setChecked(false);
}

void AnimationWidget::on_actionNewTriangle_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "__Primitive_Triangle"));

    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewTriangle->setChecked(true);
    mUI.actionNewArrow->setChecked(false);
}

void AnimationWidget::on_actionNewArrow_triggered()
{
    mCurrentTool.reset(new PlaceTool(mState, "Checkerboard", "__Primitive_Arrow"));

    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewTriangle->setChecked(false);
    mUI.actionNewArrow->setChecked(true);
}

void AnimationWidget::on_actionDeleteComponent_triggered()
{
    const game::AnimationNode* item = GetCurrentNode();
    if (item == nullptr)
        return;

    auto& tree = mState.animation.GetRenderTree();

    // find the game graph node that contains this AnimationNode.
    auto* node = tree.FindNodeByValue(item);

    // traverse the tree starting from the node to be deleted
    // and capture the ids of the animation nodes that are part
    // of this hiearchy.
    struct Carcass {
        std::string id;
        std::string name;
    };
    std::vector<Carcass> graveyard;
    node->PreOrderTraverseForEach([&](game::AnimationNode* value) {
        Carcass carcass;
        carcass.id   = value->GetId();
        carcass.name = value->GetName();
        graveyard.push_back(carcass);
    });

    for (auto& carcass : graveyard)
    {
        DEBUG("Deleting child '%1', %2", carcass.name, carcass.id);
        mState.animation.DeleteNodeById(carcass.id);
    }

    // find the parent node
    auto* parent = tree.FindParent(node);

    parent->DeleteChild(node);

    mUI.tree->Rebuild();
}

void AnimationWidget::on_tree_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionDeleteComponent);
    menu.exec(QCursor::pos());
}

void AnimationWidget::on_plus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(math::clamp(-180.0, 180.0, value + 90.0f));
}

void AnimationWidget::on_minus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(math::clamp(-180.0, 180.0, value - 90.0f));
}

void AnimationWidget::on_cPlus90_clicked()
{
    const auto value = mUI.cRotation->value();
    mUI.cRotation->setValue(math::clamp(-180.0, 180.0, value + 90.0f));
}
void AnimationWidget::on_cMinus90_clicked()
{
    const auto value = mUI.cRotation->value();
    mUI.cRotation->setValue(math::clamp(-180.0, 180.0, value - 90.0f));
}

void AnimationWidget::on_resetTransform_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    mState.camera_offset_x = width * 0.5f;
    mState.camera_offset_y = height * 0.5f;

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
        const auto& material_name = app::ToUtf8(name);
        node->SetMaterial(material_name, mState.workspace->MakeMaterial(material_name));
    }
}

void AnimationWidget::on_drawables_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const auto& drawable_name = app::ToUtf8(name);
        node->SetDrawable(drawable_name, mState.workspace->MakeDrawable(drawable_name));
    }
}

void AnimationWidget::on_renderPass_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const game::AnimationNode::RenderPass pass = GetValue(mUI.renderPass);
        node->SetRenderPass(pass);
    }
}

void AnimationWidget::on_renderStyle_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const game::AnimationNode::RenderStyle style = GetValue(mUI.renderStyle);
        node->SetRenderStyle(style);
    }
}

void AnimationWidget::currentComponentRowChanged()
{
    const game::AnimationNode* node = GetCurrentNode();
    if (node == nullptr)
    {
        mUI.cProperties->setEnabled(false);
        mUI.cTransform->setEnabled(false);
    }
    else
    {
        updateCurrentNodeProperties();
    }
}

void AnimationWidget::placeNewParticleSystem()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the text in the action as the name of the drawable.
    const auto drawable = action->text();

    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResource(drawable, app::Resource::Type::ParticleSystem);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceTool(mState, material, drawable));
}

void AnimationWidget::newResourceAvailable(const app::Resource* resource)
{
    // this is simple, just add new resources in the appropriate UI objects.
    if (resource->GetType() == app::Resource::Type::Material)
    {
        mUI.materials->addItem(resource->GetName());
    }
    else if (resource->GetType() == app::Resource::Type::ParticleSystem)
    {
        QAction* action = mDrawableMenu->addAction(resource->GetName());
        connect(action, &QAction::triggered, this, &AnimationWidget::placeNewParticleSystem);

        mUI.drawables->addItem(resource->GetName());
    }
}

void AnimationWidget::resourceToBeDeleted(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        const auto index = mUI.materials->findText(resource->GetName());
        mUI.materials->blockSignals(true);
        mUI.materials->removeItem(index);
        for (size_t i=0; i<mState.animation.GetNumNodes(); ++i)
        {
            auto& component = mState.animation.GetNode(i);
            const auto& material = app::FromUtf8(component.GetMaterialName());
            if (material == resource->GetName())
            {
                WARN("Animation node '%1' uses a material '%2' that is deleted.",
                    component.GetName(), resource->GetName());
                component.SetMaterial("Checkerboard", mState.workspace->MakeMaterial("Checkerboard"));
            }
        }
        if (auto* comp = GetCurrentNode())
        {
            // either this material still exists or the component's material
            // was changed in the loop above.
            // in either case the material name should be found in the current
            // list of material names in the UI combobox.
            const auto& material = app::FromUtf8(comp->GetMaterialName());
            const auto index = mUI.materials->findText(material);
            ASSERT(index != -1);
            mUI.materials->setCurrentIndex(index);
        }

        mUI.materials->blockSignals(false);
    }
    else if (resource->GetType() == app::Resource::Type::ParticleSystem)
    {
        // todo: what do do with drawables that are no longer available ?
        const auto index = mUI.drawables->findText(resource->GetName());
        mUI.drawables->blockSignals(true);
        mUI.drawables->removeItem(index);
        for (size_t i=0; i<mState.animation.GetNumNodes(); ++i)
        {
            auto& node = mState.animation.GetNode(i);
            const auto& drawable = app::FromUtf8(node.GetDrawableName());
            if (drawable == resource->GetName())
            {
                WARN("Animation node '%1' uses a drawable '%2' that is deleted.",
                    node.GetName(), resource->GetName());
                node.SetDrawable("__Primitive_Rectangle", mState.workspace->MakeDrawable("__Primitive_Rectangle"));
            }
        }
        if (auto* node = GetCurrentNode())
        {
            // either this material still exists or the component's material
            // was changed in the loop above.
            // in either case the material name should be found in the current
            // list of material names in the UI combobox.
            const auto& material = app::FromUtf8(node->GetDrawableName());
            const auto index = mUI.drawables->findText(material);
            ASSERT(index != -1);
            mUI.drawables->setCurrentIndex(index);
        }
        mUI.drawables->blockSignals(false);

        // rebuild the drawable menu
        mDrawableMenu->clear();
        for (size_t i=0; i<mState.workspace->GetNumResources(); ++i)
        {
            const auto& resource = mState.workspace->GetResource(i);
            const auto& name = resource.GetName();
            if (resource.GetType() == app::Resource::Type::ParticleSystem)
            {
                QAction* action = mDrawableMenu->addAction(name);
                connect(action, &QAction::triggered,
                        this,   &AnimationWidget::placeNewParticleSystem);
            }
        }
    }
}

void AnimationWidget::treeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.animation.GetRenderTree();
    auto* src_value = static_cast<game::AnimationNode*>(item->GetUserData());
    auto* dst_value = static_cast<game::AnimationNode*>(target->GetUserData());

    // find the game graph node that contains this AnimationNode.
    auto* src_node   = tree.FindNodeByValue(src_value);
    auto* src_parent = tree.FindParent(src_node);

    // check if we're trying to drag a parent onto its own child
    if (src_node->FindNodeByValue(dst_value))
        return;

    game::Animation::RenderTreeNode branch = *src_node;
    src_parent->DeleteChild(src_node);

    auto* dst_node  = tree.FindNodeByValue(dst_value);
    dst_node->AppendChild(std::move(branch));

}

void AnimationWidget::treeClickEvent(TreeWidget::TreeItem* item)
{
    //DEBUG("Tree click event: %1", item->GetId());
    auto* node = static_cast<game::AnimationNode*>(item->GetUserData());
    if (node == nullptr)
        return;

    const bool visibility = node->TestFlag(game::AnimationNode::Flags::VisibleInEditor);
    node->SetFlag(game::AnimationNode::Flags::VisibleInEditor, !visibility);
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

void AnimationWidget::on_cSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
    }
}
void AnimationWidget::on_cSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
    }
}
void AnimationWidget::on_cTranslateX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.x = value;
        node->SetTranslation(translate);
    }
}
void AnimationWidget::on_cTranslateY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.y = value;
        node->SetTranslation(translate);
    }
}
void AnimationWidget::on_cRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetRotation(qDegreesToRadians(value));
    }
}

void AnimationWidget::on_cName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    if (!item->GetUserData())
        return;
    auto* node = static_cast<game::AnimationNode*>(item->GetUserData());

    node->SetName(app::ToUtf8(text));
    item->SetText(text);

    mUI.tree->Update();
}

void AnimationWidget::on_chkUpdateMaterial_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNode::Flags::UpdateMaterial, state);
    }
}
void AnimationWidget::on_chkUpdateDrawable_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNode::Flags::UpdateDrawable, state);
    }
}
void AnimationWidget::on_chkDoesRender_stateChanged(int state)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::AnimationNode::Flags::DoesRender, state);
    }
}

void AnimationWidget::paintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    // update the animation if we're currently playing
    if (mPlayState == PlayState::Playing)
    {
        mState.animation.Update(secs);
        mTime += secs;
    }

    gfx::Transform view;
    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    // camera offset should be reflected in the translateX/Y UI components as well.
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    class DrawHook : public game::Animation::DrawHook
    {
    public:
        DrawHook(game::AnimationNode* selected)
          : mSelected(selected)
        {}
        virtual bool InspectPacket(const game::AnimationNode* node, game::Animation::DrawPacket&) override
        {
            if (!node->TestFlag(game::AnimationNode::Flags::VisibleInEditor))
                return false;
            return true;
        }
        virtual void AppendPackets(const game::AnimationNode* node, gfx::Transform& trans, std::vector<game::Animation::DrawPacket>& packets) override
        {
            if (node != mSelected)
                return;

            static const auto green = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::Green));
            static const auto rect  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
            static const auto circle = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline, 2.0f);

            const auto& size = node->GetSize();

            // draw the selection rectangle.
            trans.Push(node->GetModelTransform());
                game::Animation::DrawPacket selection;
                selection.transform = trans.GetAsMatrix();
                selection.material  = green;
                selection.drawable  = rect;
                selection.layer     = node->GetLayer();
                packets.push_back(selection);
            trans.Pop();

            // draw the resize indicator. (lower right corner box)
            trans.Push();
                trans.Scale(10.0f, 10.0f);
                trans.Translate(size.x*0.5f-10.0f, size.y*0.5f-10.0f);
                game::Animation::DrawPacket sizing_box;
                sizing_box.transform = trans.GetAsMatrix();
                sizing_box.material  = green;
                sizing_box.drawable  = rect;
                sizing_box.layer     = node->GetLayer();
                packets.push_back(sizing_box);
            trans.Pop();

            // draw the rotation indicator. (upper left corner circle)
            trans.Push();
                trans.Scale(10.0f, 10.0f);
                trans.Translate(-size.x*0.5f, -size.y*0.5f);
                game::Animation::DrawPacket rotation_circle;
                rotation_circle.transform = trans.GetAsMatrix();
                rotation_circle.material  = green;
                rotation_circle.drawable  = circle;
                rotation_circle.layer     = node->GetLayer();
                packets.push_back(rotation_circle);
            trans.Pop();
        }
    private:
        game::AnimationNode* mSelected = nullptr;
    };

    DrawHook hook(GetCurrentNode());

    // render endless background grid.
    if (mUI.showGrid->isChecked())
    {
        view.Push();

        const float zoom = GetValue(mUI.zoom);
        const float xs = GetValue(mUI.scaleX);
        const float ys = GetValue(mUI.scaleY);
        const int grid_size = std::max(width / xs, height / ys) / zoom;
        // work out the scale factor for the grid. we want some convenient scale so that
        // each grid cell maps to some convenient number of units (a multiple of 10)
        const auto cell_size_units  = 50;
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
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));
        view.Translate(-grid_width, 0);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));
        view.Translate(0, -grid_height);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));
        view.Translate(grid_width, 0);
        painter.Draw(gfx::Grid(num_grid_lines, num_grid_lines), view,
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::Material::SurfaceType::Transparent));

        view.Pop();
    }


    // begin the animation transformation space
    view.Push();
        mState.animation.Draw(painter, view, &hook);
    view.Pop();

    if (mCurrentTool)
        mCurrentTool->Render(painter, view);

    // right arrow
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

    // pop view transformation
    view.Pop();

    mUI.time->setText(QString::number(mTime));
}

game::AnimationNode* AnimationWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    if (!item->GetUserData())
        return nullptr;
    return static_cast<game::AnimationNode*>(item->GetUserData());
}

void AnimationWidget::updateCurrentNodeProperties()
{
    if (const auto* node = GetCurrentNode())
    {
        QSignalBlocker s(mUI.cTransform);

        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        SetValue(mUI.cName, node->GetName());
        SetValue(mUI.renderPass, node->GetRenderPass());
        SetValue(mUI.renderStyle, node->GetRenderStyle());
        SetValue(mUI.layer, node->GetLayer());
        SetValue(mUI.materials, node->GetMaterialName());
        SetValue(mUI.drawables, node->GetDrawableName());
        SetValue(mUI.cTranslateX, translate.x);
        SetValue(mUI.cTranslateY, translate.y);
        SetValue(mUI.cSizeX, size.x);
        SetValue(mUI.cSizeY, size.y);
        SetValue(mUI.cRotation, qRadiansToDegrees(node->GetRotation()));
        SetValue(mUI.lineWidth, node->GetLineWidth());
        SetValue(mUI.chkUpdateMaterial, node->TestFlag(game::AnimationNode::Flags::UpdateMaterial));
        SetValue(mUI.chkUpdateDrawable, node->TestFlag(game::AnimationNode::Flags::UpdateDrawable));
        SetValue(mUI.chkDoesRender, node->TestFlag(game::AnimationNode::Flags::DoesRender));
        mUI.cProperties->setEnabled(true);
        mUI.cTransform->setEnabled(true);
    }
}

void AnimationWidget::updateCurrentNodePosition(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
    }
}

} // namespace
