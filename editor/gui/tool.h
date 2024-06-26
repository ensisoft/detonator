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
#  include <QPoint>
#  include <QMouseEvent>
#  include <QString>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <memory>
#include <tuple>
#include <cmath>

#include "base/math.h"
#include "base/utility.h"
#include "game/enum.h"
#include "engine/camera.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"

#include "editor/gui/utility.h"
#include "editor/gui/nerd.h"
#include "editor/gui/types.h"

namespace gui
{
    class UIAnimator
    {
    public:
        template<typename UI, typename State>
        void Jump(const UI& ui, const State& state, const glm::vec2& target)
        {
            mViewTransformRotationStart  = GetValue(ui.rotation);
            mViewTransformRotationStop   = 0.0f; //GetValue(ui.rotation);
            mViewTransformTranslateStart = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformTranslateStop  = target;
            mViewTransformStartTime      = base::GetTime();
            mTransformView = true;
        }


        template<typename UI, typename State>
        void Reset(const UI& ui, const State& state)
        {
            mViewTransformRotationStart  = GetValue(ui.rotation);
            mViewTransformRotationStop   = 0.0f;
            mViewTransformTranslateStart = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformTranslateStop  = glm::vec2{0.0f, 0.0f};
            mViewTransformStartTime      = base::GetTime();
            mTransformView = true;
        }
        template<typename UI, typename State>
        void Minus90(const UI& ui, const State& state)
        {
            const float value = GetValue(ui.rotation);
            mViewTransformRotationStart  = value;
            mViewTransformRotationStop   = math::clamp(-180.0f, 180.0f, value - 90.0f);
            mViewTransformTranslateStop  = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformTranslateStart = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformStartTime      = base::GetTime();
            mTransformView = true;
        }
        template<typename UI, typename State>
        void Plus90(const UI& ui, const State& state)
        {
            const float value = GetValue(ui.rotation);
            mViewTransformRotationStart  = value;
            mViewTransformRotationStop   = math::clamp(-180.0f, 180.0f, value + 90.0f);
            mViewTransformTranslateStop  = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformTranslateStart = glm::vec2{state.camera_offset_x, state.camera_offset_y};
            mViewTransformStartTime      = base::GetTime();
            mTransformView = true;
        }

        template<typename UI, typename State>
        void Update(UI& ui, State& state)
        {
            if (!mTransformView)
                return;

            const auto time = base::GetTime();
            const auto time_diff = time - mViewTransformStartTime;
            const auto view_transform_time = math::clamp(0.0, 1.0, time_diff);
            const auto view_rotation_angle = math::interpolate(mViewTransformRotationStart, mViewTransformRotationStop,
                                                               view_transform_time, math::Interpolation::Cosine);
            const auto view_translation = math::interpolate(mViewTransformTranslateStart, mViewTransformTranslateStop,
                                                            view_transform_time, math::Interpolation::Cosine);

            state.camera_offset_x = view_translation.x;
            state.camera_offset_y = view_translation.y;
            SetValue(ui.translateX, state.camera_offset_x);
            SetValue(ui.translateY, state.camera_offset_y);
            SetValue(ui.rotation, view_rotation_angle);
            mTransformView = view_transform_time != 1.0f;
        }

    private:
        float mViewTransformRotationStart = 0.0f;
        float mViewTransformRotationStop  = 0.0f;
        glm::vec2 mViewTransformTranslateStart = {0.0f, 0.0f};
        glm::vec2 mViewTransformTranslateStop  = {0.0f, 0.0f};
        double mViewTransformStartTime = 0.0;
        bool mTransformView = false;
    };

    class MouseEvent
    {
    public:
        MouseEvent(const QMouseEvent* mickey)
          : mMickey(mickey)
        {}
        template<typename UI, typename State>
        MouseEvent(const QMouseEvent* mickey, const UI& ui, const State& state, engine::GameView view = engine::GameView::AxisAligned)
          : mMickey(mickey)
          , mWindowSize(ToVec2(ui.widget->size()))
          , mCameraPos(state.camera_offset_x, state.camera_offset_y)
          , mCameraScale(GetValue(ui.scaleX), GetValue(ui.scaleY))
          , mZoom(GetValue(ui.zoom))
          , mCameraRotation(GetValue(ui.rotation))
          , mCanTransform(true)
          , mGameView(view)
        {}
        inline Point2Df WindowPos() const noexcept
        { return mMickey->pos(); }
        inline bool CanTransform() const
        { return mCanTransform; }

        inline Point2Df MapToPlane() const noexcept
        {
            const auto& view_to_clip = engine::CreateProjectionMatrix(engine::Projection::Orthographic, mWindowSize);
            const auto& world_to_view = engine::CreateModelViewMatrix(mGameView, mCameraPos, mCameraScale * mZoom,
                                                                      mCameraRotation);
            const auto ret = engine::MapFromWindowToWorldPlane(view_to_clip, world_to_view, WindowPos(), mWindowSize);
            return {ret.x, ret.y};
        }
        inline Point2Df MapMouse(const gfx::Transform& old_view_transform) const
        {
            if (mCanTransform)
                return MapToPlane();

            const auto& mouse_pos = mMickey->pos();
            const auto& widget_to_view = glm::inverse(old_view_transform.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
            return {mouse_pos_in_view.x, mouse_pos_in_view.y};
        }
        inline Point2Df MapBetweenPerspectives(const Point2Df& point,
                                               engine::GameView src,
                                               engine::GameView dst) const
        {
            if (src == dst)
                return point;

            const auto src_view_to_clip = engine::CreateProjectionMatrix(engine::Projection::Orthographic, mWindowSize.x, mWindowSize.y);
            const auto src_world_to_view = engine::CreateModelViewMatrix(engine::GameView::AxisAligned, mCameraPos,
                                                                         mCameraScale * mZoom, mCameraRotation);
            const auto dst_view_to_clip = engine::CreateProjectionMatrix(engine::Projection::Orthographic, mWindowSize.x, mWindowSize.y);
            const auto dst_world_to_view = engine::CreateModelViewMatrix(engine::GameView::Dimetric, mCameraPos,
                                                                         mCameraScale * mZoom, mCameraRotation);

            glm::vec2 ret;
            if (src == engine::GameView::AxisAligned && dst == engine::GameView::Dimetric)
            {
                ret = engine::MapFromScenePlaneToTilePlane(src_view_to_clip, src_world_to_view,
                                                           dst_view_to_clip, dst_world_to_view,
                                                           glm::vec4{point.x(), point.y(), 0.0f, 1.0});
            }
            else if (src == engine::GameView::Dimetric && dst == engine::GameView::AxisAligned)
            {
                ret = engine::MapFromTilePlaneToScenePlane(src_view_to_clip, src_world_to_view,
                                                           dst_view_to_clip, dst_world_to_view,
                                                           glm::vec4{point.x(), point.y(), 0.0f, 1.0});
            }
            return ret;
        }

        const QMouseEvent* operator->() const
        { return mMickey; }
    private:
        const QMouseEvent* mMickey = nullptr;
        const glm::vec2 mWindowSize  = {0.0f, 0.0f};
        const glm::vec2 mCameraPos   = {0.0f, 0.0f};
        const glm::vec2 mCameraScale = {0.0f, 0.0f};
        const float mZoom            = 1.0f;
        const float mCameraRotation  = 0.0f;
        const bool mCanTransform     = false;
        const engine::GameView mGameView = engine::GameView::AxisAligned;
    };

    // Interface for transforming simple mouse actions
    // into actions that manipulate some state such as
    // animation/scene render tree.
    class MouseTool
    {
    public:
        virtual ~MouseTool() = default;
        // Render the visualization of the current tool and/or the action
        // being performed.
        virtual void Render(gfx::Painter& window, gfx::Painter& world) const {}
        // Act on mouse move event.
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& view) = 0;
        // Act on mouse press event.
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) = 0;
        // Act on mouse release event. Typically this completes the tool i.e.
        // the use and application of this tool.
        // If the tool is now "done" returns true, otherwise returns false to
        // indicate that the tool can continue operation.
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) = 0;
        // Act on a key press. Returns true if the key was consumed otherwise false
        // an the key is passed on to the next handler.
        virtual bool KeyPress(QKeyEvent* key) { return false; }
        // Return true if the tool was cancelled as a result of some input action.
        virtual bool IsCancelled() const { return false; }
        // Check the conditions of the tool after some changes such as a resource
        // update has occurred. Returns true if the tool can still apply
        // or false if the tool is no longer valid.
        virtual bool Validate() const { return true; }

        // Dummy migration shims. Plan is to get rid of the transform param
        // but it can't be done until every caller has been refactored!
        inline void MouseMove(const MouseEvent& mickey)
        {
            gfx::Transform  dummy;
            MouseMove(mickey, dummy);
        }
        inline void MousePress(const MouseEvent& mickey)
        {
            gfx::Transform dummy;
            MousePress(mickey, dummy);
        }
        inline bool MouseRelease(const MouseEvent& mickey)
        {
            gfx::Transform dummy;
            return MouseRelease(mickey, dummy);
        }
    private:
    };

    // Move/translate the camera.
    // TODO: remove this class and use the new version
    template<typename CameraState>
    class MoveCameraTool : public MouseTool
    {
    public:
        explicit MoveCameraTool(CameraState& state)
          : mState(state)
        {}
        virtual void Render(gfx::Painter& window, gfx::Painter&) const override
        {}
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
        {
            const auto& pos = mickey->pos();
            const auto& delta = pos - mMousePos;
            const float x = delta.x();
            const float y = delta.y();
            // this is actually semi-incorrect here, we're essentially expressing
            // the world-camera offset using "world offset from the origin"
            // instead of "camera offset from the world origin". Which produces
            // the same result but uses incorrect names here. In fact we're later
            // using the camera_offset directly as the view transformation and
            // those instances should use the *inverse* of the camera translation
            mState.camera_offset_x += x;
            mState.camera_offset_y += y;
            mMousePos = pos;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform&) override
        {
            mMousePos = mickey->pos();
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
        {
            // done on mouse release
            return true;
        }
    private:
        CameraState& mState;
        QPoint mMousePos;
    };

    template<typename CameraState>
    class PerspectiveCorrectCameraTool : public MouseTool
    {
    public:
        template<typename UI>
        PerspectiveCorrectCameraTool(const UI& ui, CameraState& state)
          : mState(state)
        {
            const auto width  = ui.widget->width();
            const auto height = ui.widget->height();
            const float zoom   = GetValue(ui.zoom);
            const float xs     = GetValue(ui.scaleX);
            const float ys     = GetValue(ui.scaleY);
            const float rotation = 0.0f; // ignored so that the camera movement stays
                                         // irrespective of the camera rotation

            mViewToClip  = engine::CreateProjectionMatrix(engine::Projection::Orthographic, width, height);
            mWorldToView = engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                         mState.camera_offset_x,
                                                         mState.camera_offset_y,
                                                         zoom * xs, zoom * ys,
                                                         rotation);
            mWindowSize = glm::vec2{width, height};
        }
        virtual void Render(gfx::Painter& window, gfx::Painter&) const override
        {}
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& ) override
        {
            const auto world_pos = engine::MapFromWindowToWorld(mViewToClip,
                                                                mWorldToView,
                                                                ToVec2(mickey->pos()),
                                                                mWindowSize);
            const auto world_delta = world_pos - mWorldPos;
            mState.camera_offset_x -= world_delta.x;
            mState.camera_offset_y -= world_delta.y;
            mWorldPos = world_pos;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform& ) override
        {
            mWorldPos = engine::MapFromWindowToWorld(mViewToClip,
                                                     mWorldToView,
                                                     ToVec2(mickey->pos()),
                                                     mWindowSize);
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
        {
            // done on mouse release
            return true;
        }
    private:
        glm::mat4 mViewToClip;
        glm::mat4 mWorldToView;
        glm::vec2 mWindowSize;
        CameraState& mState;
        glm::vec4 mWorldPos;
    };


    template<typename TreeModel, typename TreeNode>
    class MoveRenderTreeNodeTool : public MouseTool
    {
    public:
        MoveRenderTreeNodeTool(TreeModel& model, TreeNode* selected, bool snap = false, unsigned grid = 0,
                               engine::GameView::EnumValue mapping = engine::GameView::AxisAligned)
            : mModel(model)
            , mNode(selected)
            , mSnapToGrid(snap)
            , mGridSize(grid)
            , mMapping(mapping)
        {}
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            const glm::vec2 mouse_pos = mickey.MapMouse(trans);

            const auto& tree = mModel.GetRenderTree();
            // if the object we're moving has a parent we need to map the mouse movement
            // correctly taking into account that the hierarchy might include several rotations.
            // simplest thing to do is to map the mouse to the object's parent's coordinate space
            // and thus express/measure the object's translation delta relative to it's parent
            // (as it is in the hierarchy).
            // todo: this could be simplified if we expressed the view transformation in the render tree's
            // root node. then the below else branch should go away(?)
            if (tree.HasParent(mNode) && tree.GetParent(mNode))
            {
                const auto* parent = tree.GetParent(mNode);
                const auto& mouse_pos_in_node = mModel.MapCoordsToNodeBox(mouse_pos, parent);
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
                const auto& mouse_delta = mouse_pos - mPreviousMousePos;
                glm::vec2 position = mNode->GetTranslation();
                position.x += mouse_delta.x;
                position.y += mouse_delta.y;
                mNode->SetTranslation(position);
                mPreviousMousePos = mouse_pos;
            }
            // set a flag that it was moved only if it actually was.
            // otherwise simply selecting a node will snap it to the
            // new place if snap to grid was on.
            mWasMoved = true;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey.MapMouse(trans);

            // see the comments in MouseMove about the branched logic.
            const auto& tree = mModel.GetRenderTree();
            // parent could be nullptr which is the implicit root.
            if (tree.HasParent(mNode) && tree.GetParent(mNode))
            {
                const auto* parent = tree.GetParent(mNode);
                mPreviousMousePos = mModel.MapCoordsToNodeBox(mouse_pos, parent);
            }
            else
            {
                mPreviousMousePos = mouse_pos;
            }
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
        {
            if (!mWasMoved)
                return true;

            if (mickey->modifiers() & Qt::ControlModifier)
                mSnapToGrid = !mSnapToGrid;

            if (mSnapToGrid)
            {
                glm::vec2 position = mNode->GetTranslation();
                position = mickey.MapBetweenPerspectives(position, engine::GameView::AxisAligned, mMapping);
                position.x = std::round(position.x / mGridSize) * mGridSize;
                position.y = std::round(position.y / mGridSize) * mGridSize;
                position = mickey.MapBetweenPerspectives(position, mMapping, engine::GameView::AxisAligned);
                mNode->SetTranslation(position);
            }
            // we're done.
            return true;
        }
    private:
        TreeModel& mModel;
        TreeNode*  mNode = nullptr;
        // previous mouse position, for each mouse move we update the objects'
        // position by the delta between previous and current mouse pos.
        glm::vec2 mPreviousMousePos;
        // true if we want the x,y coords to be aligned on grid size units.
        bool mSnapToGrid = false;
        bool mWasMoved = false;
        unsigned mGridSize = 0;
        engine::GameView mMapping = engine::GameView::AxisAligned;
    };

    template<typename TreeModel, typename TreeNode>
    class ScaleRenderTreeNodeTool : public MouseTool
    {
    public:
        ScaleRenderTreeNodeTool(TreeModel& model, TreeNode* selected)
          : mModel(model)
          , mNode(selected)
        {
            mScale = mNode->GetScale();
        }
        virtual void Render(gfx::Painter& window, gfx::Painter& world) const override
        {

        }

        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
        {
            const glm::vec2 mouse_pos = mickey.MapToPlane();
            const glm::vec2 mouse_diff = mouse_pos - mMouseDown;
            mViewSize += mouse_diff;

            // compute new scaling factor needed to grow the original (real size)
            // to the new view size.
            const auto scale = mViewSize / mRealSize;

            mNode->SetScale(scale);
            mMouseDown = mouse_pos;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform&) override
        {
            mMouseDown = mickey.MapToPlane();

            const auto rect = mModel.FindEntityBoundingRect(mNode);
            const auto width = rect.GetWidth();
            const auto height = rect.GetHeight();
            const auto size = glm::vec2{width*0.5f, height*0.5f};

            // unscaled size of the bounding rect, i.e. when the node's scaling
            // factor is 1.0f
            mRealSize = size / mScale;

            mViewSize = size;
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
        {
            // we're done
            return true;
        }
    private:
        TreeModel& mModel;
        TreeNode*  mNode = nullptr;
        glm::vec2 mMouseDown;
        glm::vec2 mRealSize;
        glm::vec2 mViewSize;
        glm::vec2 mScale;
    };

    template<typename TreeModel, typename TreeNode>
    class ResizeRenderTreeNodeTool : public MouseTool
    {
    public:
        ResizeRenderTreeNodeTool(TreeModel& model, TreeNode* selected, bool snap = false, unsigned grid = 0)
          : mModel(model)
          , mNode(selected)
          , mSnapToGrid(snap)
          , mGridSize(grid)
        {}
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey.MapMouse(trans);
            const auto& mouse_pos_in_node = mModel.MapCoordsToNodeBox(mouse_pos, mNode);
            const auto& mouse_delta = (mouse_pos_in_node - mPreviousMousePos);
            const bool maintain_aspect_ratio = mickey->modifiers() & Qt::ShiftModifier;

            if (maintain_aspect_ratio)
            {
                const glm::vec2& size = mNode->GetSize();

                const auto aspect_ratio = size.x / size.y;
                const auto new_height = math::clamp(0.0f, size.y + mouse_delta.y, size.y + mouse_delta.y);
                const auto new_width  = new_height * aspect_ratio;
                mNode->SetSize(glm::vec2(new_width, new_height));
            }
            else
            {
                glm::vec2 size = mNode->GetSize();
                // don't allow negative sizes.
                size.x = math::clamp(0.0f, size.x + mouse_delta.x, size.x + mouse_delta.x);
                size.y = math::clamp(0.0f, size.y + mouse_delta.y, size.y + mouse_delta.y);
                mNode->SetSize(size);
            }
            mPreviousMousePos = mouse_pos_in_node;
            mWasMoved = true;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey.MapMouse(trans);
            mPreviousMousePos = mModel.MapCoordsToNodeBox(mouse_pos, mNode);
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
        {
            if (!mWasMoved)
                return true;
            if (mickey->modifiers() & Qt::ControlModifier)
                mSnapToGrid = !mSnapToGrid;

            if (mSnapToGrid)
            {
                const auto& position = mNode->GetTranslation();
                const auto& size     = mNode->GetSize();
                const auto& bottom_right = position + size * 0.5f;
                const auto& aligned_bottom_right = glm::vec2(
                        std::round(bottom_right.x / mGridSize) * mGridSize,
                        std::round(bottom_right.y / mGridSize) * mGridSize);
                const auto size_delta = aligned_bottom_right - bottom_right;
                auto next_size = size + size_delta * 2.0f;
                // don't let snap to 0 size
                mNode->SetSize(glm::max(next_size, glm::vec2(mGridSize, mGridSize)));
            }
            return true;
        }
    private:
        TreeModel& mModel;
        TreeNode* mNode = nullptr;
        // previous mouse position, for each mouse move we update the objects'
        // position by the delta between previous and current mouse pos.
        glm::vec2 mPreviousMousePos;
        bool mSnapToGrid = false;
        bool mWasMoved = false;
        unsigned mGridSize = 0;
    };

    template<typename TreeModel, typename TreeNode>
    class RotateRenderTreeNodeTool : public MouseTool
    {
    public:
        RotateRenderTreeNodeTool(TreeModel& model, TreeNode* selected)
            : mModel(model)
            , mNode(selected)
        {
            if constexpr (std::is_same_v<TreeNode, game::EntityPlacement>)
            {
                mNodeCenterInWorld = mModel.MapCoordsFromNodeBox(0.0f, 0.0f, mNode);
            }
            else
            {
                const auto &node_size = mNode->GetSize();
                mNodeCenterInWorld = mModel.MapCoordsFromNodeBox(node_size.x * 0.5f, node_size.y * 0.5, mNode);
            }
        }
        virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            const glm::vec2 world_mouse_pos = mickey.MapMouse(trans);
            // compute the delta between the current mouse position angle and the previous mouse position angle
            // wrt the node's center point. Then and add the angle delta increment to the node's rotation angle.
            const auto previous_angle = GetAngleRadians(mPreviousMousePos - mNodeCenterInWorld);
            const auto current_angle  = GetAngleRadians(world_mouse_pos -mNodeCenterInWorld);
            const auto angle_delta = current_angle - previous_angle;

            double angle = mNode->GetRotation();
            angle += angle_delta;
            // keep it in the -180 - 180 degrees [-Pi, Pi] range.
            angle = math::wrap(-math::Pi, math::Pi, angle);
            mNode->SetRotation(angle);
            mPreviousMousePos = world_mouse_pos;
        }
        virtual void MousePress(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            mPreviousMousePos = mickey.MapMouse(trans);
        }
        virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& trans) override
        {
            return true;
        }
    private:
        float GetAngleRadians(const glm::vec2& p) const
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
        TreeModel& mModel;
        TreeNode* mNode = nullptr;
        // previous mouse position, for each mouse move we update the object's
        // position by the delta between previous and current mouse pos.
        glm::vec2 mPreviousMousePos;
        glm::vec2 mNodeCenterInWorld;
    };

    template<typename EntityType, typename NodeType>
    std::tuple<NodeType*, glm::vec2> SelectNode(const glm::mat4& view_to_clip,
                                                const glm::mat4& world_to_view,
                                                const Point2Df& window_point,
                                                const Size2Df& window_size,
                                                EntityType& entity,
                                                NodeType* currently_selected)
    {
        const auto& world_pos = engine::MapFromWindowToWorldPlane(view_to_clip,
                                                                  world_to_view,
                                                                  window_point,
                                                                  window_size);
        std::vector<NodeType*> hit_nodes;
        std::vector<glm::vec2> hit_boxes;
        entity.CoarseHitTest(world_pos, &hit_nodes, &hit_boxes);

        // if nothing was hit then return early.
        if (hit_nodes.empty())
            return std::make_tuple(nullptr, glm::vec2{});

        // if the currently selected node is among those that were hit
        // then retain that, otherwise select the node that is at the
        // topmost layer. (biggest layer value)
        NodeType* hit = hit_nodes[0];
        glm::vec2 box;
        int layer = hit->GetLayer();
        for (size_t i=0; i<hit_nodes.size(); ++i)
        {
            if (currently_selected == hit_nodes[i])
            {
                hit = hit_nodes[i];
                box = hit_boxes[i];
                break;
            }
            else if (hit_nodes[i]->GetLayer() >= layer)
            {
                hit = hit_nodes[i];
                box = hit_boxes[i];
                layer = hit->GetLayer();
            }
        }
        return std::make_tuple(hit, box);
    }

    template<typename EntityType, typename NodeType, typename UI, typename State>
    std::tuple<NodeType*, glm::vec2> SelectNode(const UI& ui, const State& state, const Point2Df& window_point,
                                                EntityType& entity,
                                                NodeType* currently_selected)
    {
        const auto& window_size = ui.widget->size();
        return SelectNode<EntityType, NodeType>(CreateProjectionMatrix(ui),
                                                CreateViewMatrix(ui, state),
                                                window_point,
                                                window_size,
                                                entity, currently_selected);
    }

    template<typename EntityType, typename NodeType>
    std::tuple<NodeType*, glm::vec2> SelectNode(const QPoint& mouse_click_point,
                                                const gfx::Transform& view,
                                                EntityType& entity,
                                                NodeType* currently_selected = nullptr)

    {
        const auto& view_to_entity = glm::inverse(view.GetAsMatrix());
        const auto click_pos_in_view = glm::vec4(mouse_click_point.x(),
                                                 mouse_click_point.y(), 1.0f, 1.0f);
        const auto click_pos_in_entity = view_to_entity * click_pos_in_view;

        std::vector<NodeType*> hit_nodes;
        std::vector<glm::vec2> hit_boxes;
        entity.CoarseHitTest(click_pos_in_entity.x, click_pos_in_entity.y, &hit_nodes, &hit_boxes);

        // if nothing was hit then return early.
        if (hit_nodes.empty())
            return std::make_tuple(nullptr, glm::vec2{});

        // if the currently selected node is among those that were hit
        // then retain that, otherwise select the node that is at the
        // topmost layer. (biggest layer value)
        NodeType* hit = hit_nodes[0];
        glm::vec2 box;
        int layer = hit->GetLayer();
        for (size_t i=0; i<hit_nodes.size(); ++i)
        {
            if (currently_selected == hit_nodes[i])
            {
                hit = hit_nodes[i];
                box = hit_boxes[i];
                break;
            }
            else if (hit_nodes[i]->GetLayer() >= layer)
            {
                hit = hit_nodes[i];
                box = hit_boxes[i];
                layer = hit->GetLayer();
            }
        }
        return std::make_tuple(hit, box);
    }

    enum class ToolHotspot {
        None,
        Rotate,
        Resize,
        Remove
    };

    static ToolHotspot TestToolHotspot(const glm::mat4& view_to_clip,
                                       const glm::mat4& world_to_view,
                                       const glm::mat4& model_to_world,
                                       const gfx::FRect& world_box,
                                       const Point2Df& window_point,
                                       const Size2Df& window_size)
    {
        const auto& world_pos = engine::MapFromWindowToWorldPlane(view_to_clip,
                                                                  world_to_view,
                                                                  window_point,
                                                                  window_size);

        // decompose the incoming view transformation matrix
        // in order to figure out the scaling factor. we'll use the inverse
        // scale for the indicators in order to keep them a constant size
        // regardless of the view's current scaling factors.
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(world_to_view, scale, orientation, translation, skew, perspective);

        glm::vec4 rotate_hit_pos;
        glm::vec4 resize_hit_pos;
        glm::vec4 remove_hit_pos;

        // the hotspots are relative to the base model transform.
        // we transform the world position to the possible hotspot
        // coordinate spaces and check if we're inside a hotspot box.
        gfx::Transform hotspot(model_to_world);

        // rotation circle
        hotspot.Push();
          hotspot.Scale(10.0f/scale.x, 10.0f/scale.y);
          hotspot.Translate(world_box.GetPosition());
          rotate_hit_pos = glm::inverse(hotspot.GetAsMatrix()) * world_pos;
        hotspot.Pop();

        if (rotate_hit_pos.x >= 0.0f && rotate_hit_pos.x <= 1.0f &&
            rotate_hit_pos.y >= 0.0f && rotate_hit_pos.y <= 1.0f)
            return ToolHotspot::Rotate;

        const auto [_0, _1, _2, bottom_right] = world_box.GetCorners();

        // resize box
        hotspot.Push();
          hotspot.Scale(10.0f/scale.x, 10.0f/scale.y);
          hotspot.Translate(bottom_right);
          hotspot.Translate(-10.0f/scale.x, -10.0f/scale.y);
          resize_hit_pos = glm::inverse(hotspot.GetAsMatrix()) * world_pos;
        hotspot.Pop();

        if (resize_hit_pos.x >= 0.0f && resize_hit_pos.x <= 1.0f &&
            resize_hit_pos.y >= 0.0f && resize_hit_pos.y <= 1.0f)
            return ToolHotspot::Resize;

        remove_hit_pos = glm::inverse(hotspot.GetAsMatrix()) * world_pos;
        if (world_box.TestPoint(remove_hit_pos.x, remove_hit_pos.y))
            return ToolHotspot::Remove;

        return ToolHotspot::None;
    }

    template<typename UI, typename State>
    ToolHotspot TestToolHotspot(const UI& ui, const State& state,
                                const glm::mat4& model_to_world,
                                const gfx::FRect& world_box,
                                const Point2Df& window_point)
    {
        return TestToolHotspot(CreateProjectionMatrix(ui),
                               CreateViewMatrix(ui, state),
                               model_to_world, world_box, window_point, ui.widget->size());
    }


} // namespace
