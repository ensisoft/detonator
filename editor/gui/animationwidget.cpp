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
#  include <QAbstractTableModel>
#  include <QAbstractListModel>
#include "warnpop.h"

#include <algorithm>

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "animationwidget.h"
#include "utility.h"

namespace gui
{

class AnimationWidget::ComponentModel : public QAbstractListModel
{
public:
    ComponentModel(scene::Animation& anim) : mAnimation(anim)
    {}

    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mAnimation.GetNumComponents());
    }
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto  i = static_cast<size_t>(index.row());
        const auto& c = mAnimation.GetComponent(i);

        if (role == Qt::SizeHintRole)
            return QSize(0, 16);
        if (role == Qt::DisplayRole)
            return app::FromUtf8(c.GetName());
        return QVariant();
    }
    void AddComponent(scene::Animation::Component&& component)
    {
        const auto row = static_cast<int>(mAnimation.GetNumComponents());
        beginInsertRows(QModelIndex(), row, row);
        mAnimation.AddComponent(std::move(component));
        endInsertRows();
    }
    void DelComponent(size_t index)
    {
        const auto row = static_cast<int>(index);
        beginRemoveRows(QModelIndex(), row, row);
        mAnimation.DelComponent(index);
        endRemoveRows();
    }

private:
    scene::Animation& mAnimation;
};

class AnimationWidget::Tool
{
public:
    virtual ~Tool() = default;
    virtual void Render(gfx::Painter& painter) const = 0;
    virtual void MouseMove(QMouseEvent* mickey) = 0;
    virtual void MousePress(QMouseEvent* mickey) = 0;
    virtual bool MouseRelease(QMouseEvent* mickey) = 0;
private:
};

class AnimationWidget::PlaceRectTool : public AnimationWidget::Tool
{
public:
    PlaceRectTool(AnimationWidget::State& state) : mState(state)
    {}
    virtual void Render(gfx::Painter& painter) const
    {
        const auto& diff = mCurrent - mStart;
        if (diff.x() <= 0.0f || diff.y() <= 0.0f)
            return;

        const float x = mStart.x();
        const float y = mStart.y();
        const float w = diff.x();
        const float h = diff.y();
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, w, h), gfx::Color::Green, 2, 0.0f);
    }
    virtual void MouseMove(QMouseEvent* mickey)
    {
        if (mEngaged)
        {
            mCurrent = mickey->pos();
        }
    }
    virtual void MousePress(QMouseEvent* mickey) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mStart = mickey->pos();
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey)
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mEngaged = false;
            const auto& diff = mCurrent - mStart;
            if (diff.x() <= 0.0f || diff.y() <= 0.0f)
                return false;
        }

        std::string name;
        for (size_t i=0; i<666666; ++i)
        {
            name = "Component_" + std::to_string(i);
            if (CheckNameAvailability(name))
                break;
        }
        // todo: the width and height need to account the zoom level.
        const auto& diff = mCurrent - mStart;
        const float xpos = mStart.x() + mState.camera_offset_x;
        const float ypos = mStart.y() + mState.camera_offset_y;
        const float width  = diff.x();
        const float height = diff.y();
        auto drawable = std::make_shared<gfx::Rectangle>(name + "/Rect", width, height);
        auto material = mState.workspace->MakeMaterial("Checkerboard");

        scene::Animation::Component component;
        component.SetMaterial("Rectangle", material);
        component.SetDrawable("Checkerboard", drawable);
        component.SetName(name);
        component.SetTranslation(glm::vec2(xpos, ypos));
        component.SetScale(glm::vec2(1.0f, 1.0f));
        mState.model->AddComponent(std::move(component));
        DEBUG("Added new rectangle '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.animation.GetNumComponents(); ++i)
        {
            const auto& component = mState.animation.GetComponent(i);
            if (component.GetName() == name)
                return false;
        }
        return true;
    }


private:
    AnimationWidget::State& mState;
    QPoint mStart;
    QPoint mCurrent;
    bool mEngaged = false;
};

class AnimationWidget::CameraTool : public AnimationWidget::Tool
{
public:
    CameraTool(State& state) : mState(state)
    {}
    virtual void Render(gfx::Painter& painter) const override
    {}
    virtual void MouseMove(QMouseEvent* mickey) override
    {
        if (mEngaged)
        {
            const auto& pos = mickey->pos();
            const auto& delta = pos - mMousePos;
            const float x = delta.x();
            const float y = delta.y();
            mState.camera_offset_x -= x;
            mState.camera_offset_y -= y;
            //DEBUG("Camera offset %1, %2", mState.camera_offset_x, mState.camera_offset_y);
            mMousePos = pos;
        }
    }
    virtual void MousePress(QMouseEvent* mickey) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mMousePos = mickey->pos();
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey) override
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

AnimationWidget::AnimationWidget()
{
    DEBUG("Create AnimationWidget");

    mState.model.reset(new ComponentModel(mState.animation));

    mUI.setupUi(this);
    mUI.components->setModel(mState.model.get());
    mUI.widget->setFramerate(60);
    mUI.widget->onInitScene  = [&](unsigned width, unsigned height) {
        // offset the viewport so that the origin of the 2d space is in the middle of the viewport
        mState.camera_offset_x = width / -2.0f;
        mState.camera_offset_y = height / -2.0f;
    };
    mUI.widget->onPaintScene = std::bind(&AnimationWidget::paintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove = [&](QMouseEvent* mickey) {
        if (mCurrentTool)
            mCurrentTool->MouseMove(mickey);
    };
    mUI.widget->onMousePress = [&](QMouseEvent* mickey) {

        if (!mCurrentTool)
            mCurrentTool.reset(new CameraTool(mState));

        mCurrentTool->MousePress(mickey);
    };
    mUI.widget->onMouseRelease = [&](QMouseEvent* mickey) {
        if (!mCurrentTool)
            return;

        mCurrentTool->MouseRelease(mickey);

        mCurrentTool.release();
        mUI.actionNewRect->setChecked(false);
    };

    setWindowTitle("My Animation");

    PopulateFromEnum<scene::Animation::RenderPass>(mUI.renderPass);

    auto* model = mUI.components->selectionModel();
    connect(model, &QItemSelectionModel::currentRowChanged,
            this,  &AnimationWidget::currentComponentRowChanged);
}

AnimationWidget::AnimationWidget(const app::Resource& resource, app::Workspace* workspace)
  : AnimationWidget()
{

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
    bar.addAction(mUI.actionNewRect);
}
void AnimationWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionNewRect);
}

void AnimationWidget::setWorkspace(app::Workspace* workspace)
{
    mState.workspace = workspace;
    mUI.materials->addItem("Checkerboard"); // this is the default, todo: maybe ask from workspace
    mUI.materials->addItems(workspace->ListMaterials());
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


void AnimationWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceRectTool(mState));
    mUI.actionNewRect->setChecked(true);
}

void AnimationWidget::on_actionDeleteComponent_triggered()
{
    QModelIndexList items = mUI.components->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    std::sort(items.begin(), items.end());

    int removed = 0;

    for (int i=0; i<items.size(); ++i)
    {
        const auto& index = items[i];
        const auto row = index.row() - removed;
        const auto component_index = static_cast<size_t>(row);
        mState.model->DelComponent(component_index);
        ++removed;
    }
}

void AnimationWidget::on_components_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionDeleteComponent);
    menu.exec(QCursor::pos());
}

void AnimationWidget::on_plus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value + 90.0f);
}

void AnimationWidget::on_minus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value - 90.0f);
}

void AnimationWidget::on_resetTransform_clicked()
{
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
}

void AnimationWidget::on_materials_currentIndexChanged(const QString& name)
{
    if (auto* component = GetCurrentComponent())
    {
        const auto& material_name = app::ToUtf8(name);
        component->SetMaterial(material_name, mState.workspace->MakeMaterial(material_name));
    }
}

void AnimationWidget::on_renderPass_currentIndexChanged(const QString& name)
{
    if (auto* component = GetCurrentComponent())
    {
        const scene::Animation::RenderPass pass = GetValue(mUI.renderPass);
        component->SetRenderPass(pass);
    }
}

void AnimationWidget::currentComponentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    const auto row = current.row();
    if (row == -1)
    {
        mUI.componentProperties->setEnabled(false);
    }
    else
    {
        const auto& component = mState.animation.GetComponent(row);
        SetValue(mUI.renderPass, component.GetRenderPass());
        SetValue(mUI.layer, component.GetLayer());
        SetValue(mUI.materials, component.GetMaterialName());
        mUI.componentProperties->setEnabled(true);
    }
}
void AnimationWidget::on_layer_valueChanged(int layer)
{
    if (auto* component = GetCurrentComponent())
    {
        component->SetLayer(layer);
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

    const float angle = qDegreesToRadians(mUI.rotation->value());
    const bool has_view_transform = mUI.transform->isChecked();

    gfx::Transform view;
    view.Translate(-mState.camera_offset_x, -mState.camera_offset_y);

    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Rotate(angle);
    view.Translate(GetValue(mUI.translateX), GetValue(mUI.translateY));

    // begin the animation transformation space
    view.Push();
    mState.animation.Draw(painter, view);
    view.Pop();

    // pop view transformation
    view.Pop();

    if (mCurrentTool)
        mCurrentTool->Render(painter);

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

    mUI.time->setText(QString::number(mTime));
}

scene::Animation::Component* AnimationWidget::GetCurrentComponent()
{
    const auto& indices = mUI.components->selectionModel()->selectedRows();
    if (indices.isEmpty())
        return nullptr;

    const auto component_index = indices[0].row();
    auto& component = mState.animation.GetComponent(component_index);
    return &component;
}

} // namespace
