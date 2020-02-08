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

class AnimationWidget::PlaceTool : public AnimationWidget::Tool
{
public:
    PlaceTool(const std::string& type, scene::Animation& animation,  ComponentModel& model)
      : mType(type)
      , mModel(model)
      , mAnimation(animation)
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
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, w, h),
            gfx::Color::Green, 2, 0.0f);
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

        scene::Animation::Component component;
        component.SetDrawable(mType);
        component.SetMaterial("Checkerboard");
        component.SetName(name);
        mModel.AddComponent(std::move(component));
        DEBUG("Added new component (%1) '%2'", mType, name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mAnimation.GetNumComponents(); ++i)
        {
            const auto& component = mAnimation.GetComponent(i);
            if (component.GetName() == name)
                return false;
        }
        return true;
    }


private:
    const std::string mType;
    scene::Animation& mAnimation;
    ComponentModel& mModel;
    QPoint mStart;
    QPoint mCurrent;
    bool mEngaged = false;
};

AnimationWidget::AnimationWidget()
{
    DEBUG("Create AnimationWidget");

    mModel.reset(new ComponentModel(mAnimation));

    mUI.setupUi(this);
    mUI.components->setModel(mModel.get());
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&AnimationWidget::paintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove = [&](QMouseEvent* mickey) {
        if (mCurrentTool)
            mCurrentTool->MouseMove(mickey);
    };
    mUI.widget->onMousePress = [&](QMouseEvent* mickey) {
        if (mCurrentTool)
            mCurrentTool->MousePress(mickey);
    };
    mUI.widget->onMouseRelease = [&](QMouseEvent* mickey) {
        if (!mCurrentTool)
            return;

        if (mCurrentTool->MouseRelease(mickey))
        {
            mAnimation.Prepare(*mWorkspace);
        }

        mCurrentTool.release();
        mUI.actionNewRect->setChecked(false);
    };

    setWindowTitle("My Animation");
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
    bar.addAction(mUI.actionNewRect);
}
void AnimationWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionNewRect);
}

void AnimationWidget::setWorkspace(app::Workspace* workspace)
{
    mWorkspace = workspace;
}

void AnimationWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceTool("Rectangle", mAnimation, *mModel));
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
        mModel->DelComponent(component_index);
        ++removed;
    }
}

void AnimationWidget::on_components_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionDeleteComponent);
    menu.exec(QCursor::pos());
}

void AnimationWidget::paintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    gfx::Transform view;
    view.Translate(-mCameraOffsetX, -mCameraOffsetY);
    view.Push();

    mAnimation.Update(secs);
    mAnimation.Draw(painter);

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
}

} // namespace
