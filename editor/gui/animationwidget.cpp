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
#  include <QMessageBox>
#  include <base64/base64.h>
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
    virtual void Render(gfx::Painter& painter) const
    {
        const auto& diff = mCurrent - mStart;
        if (diff.x() <= 0.0f || diff.y() <= 0.0f)
            return;

        const float x = mStart.x();
        const float y = mStart.y();
        const float w = diff.x();
        const float h = diff.y();
        gfx::Transform t;
        t.Resize(w, h);
        t.Translate(x, y);
        painter.Draw(*mDrawable, t, *mMaterial);

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

        scene::Animation::Component component;
        component.SetMaterial(app::ToUtf8(mMaterialName), mMaterial);
        component.SetDrawable(app::ToUtf8(mDrawableName), mDrawable);
        component.SetName(name);
        component.SetTranslation(glm::vec2(xpos, ypos));
        component.SetSize(glm::vec2(width, height));
        component.SetScale(glm::vec2(1.0f, 1.0f));
        mState.model->AddComponent(std::move(component));
        DEBUG("Added new shape '%1'", name);
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

AnimationWidget::AnimationWidget(app::Workspace* workspace)
{
    DEBUG("Create AnimationWidget");

    mState.model.reset(new ComponentModel(mState.animation));
    mState.workspace = workspace;

    mUI.setupUi(this);
    // this fucking cunt whore will already emit selection changed signal
    mUI.materials->blockSignals(true);
    mUI.materials->addItems(workspace->ListMaterials());
    mUI.materials->blockSignals(false);

    mUI.name->setText("My Animation");
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
        mUI.actionNewCircle->setChecked(false);
        mUI.actionNewTriangle->setChecked(false);
        mUI.actionNewArrow->setChecked(false);
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

    PopulateFromEnum<scene::Animation::RenderPass>(mUI.renderPass);

    auto* model = mUI.components->selectionModel();
    connect(model, &QItemSelectionModel::currentRowChanged,
            this,  &AnimationWidget::currentComponentRowChanged);

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
    GetProperty(resource, "xpos", mUI.translateX);
    GetProperty(resource, "ypos", mUI.translateY);
    GetProperty(resource, "xscale", mUI.scaleX);
    GetProperty(resource, "yscale", mUI.scaleY);
    GetProperty(resource, "rotation", mUI.rotation);
    setWindowTitle(resource.GetName());

    mState.animation = *resource.GetContent<scene::Animation>();
    mState.animation.Prepare(*workspace);
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
    settings.saveWidget("Animation", mUI.translateX);
    settings.saveWidget("Animation", mUI.translateY);
    settings.saveWidget("Animation", mUI.scaleX);
    settings.saveWidget("Animation", mUI.scaleY);
    settings.saveWidget("Animation", mUI.rotation);

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
    settings.loadWidget("Animation", mUI.translateX);
    settings.loadWidget("Animation", mUI.translateY);
    settings.loadWidget("Animation", mUI.scaleX);
    settings.loadWidget("Animation", mUI.scaleY);
    settings.loadWidget("Animation", mUI.rotation);

    const std::string& base64 = settings.getValue("Animation", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = scene::Animation::FromJson(json);
    if (!ret.has_value())
    {
        WARN("Failed to load animation widget state.");
        return false;
    }
    mState.animation = std::move(ret.value());
    mState.animation.Prepare(*mState.workspace);
    return true;
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
    const auto& name = mUI.name->text();
    if (name.isEmpty())
    {
        mUI.name->setFocus();
        return;
    }
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

    SetProperty(resource, "xpos", mUI.translateX);
    SetProperty(resource, "ypos", mUI.translateY);
    SetProperty(resource, "xscale", mUI.scaleX);
    SetProperty(resource, "yscale", mUI.scaleY);
    SetProperty(resource, "rotation", mUI.rotation);
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

void AnimationWidget::on_cPlus90_clicked()
{
    const auto value = mUI.cRotation->value();
    mUI.cRotation->setValue(value + 90.0f);
}
void AnimationWidget::on_cMinus90_clicked()
{
    const auto value = mUI.cRotation->value();
    mUI.cRotation->setValue(value - 90.0f);
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
        mUI.cProperties->setEnabled(false);
        mUI.cTransform->setEnabled(false);
    }
    else
    {
        const auto& component = mState.animation.GetComponent(row);
        const auto& translate = component.GetTranslation();
        const auto& size = component.GetSize();
        SetValue(mUI.cName, component.GetName());
        SetValue(mUI.renderPass, component.GetRenderPass());
        SetValue(mUI.layer, component.GetLayer());
        SetValue(mUI.materials, component.GetMaterialName());
        SetValue(mUI.cTranslateX, translate.x);
        SetValue(mUI.cTranslateY, translate.y);
        SetValue(mUI.cSizeX, size.x);
        SetValue(mUI.cSizeY, size.y);
        SetValue(mUI.cRotation, qRadiansToDegrees(component.GetRotation()));
        mUI.cProperties->setEnabled(true);
        mUI.cTransform->setEnabled(true);
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
    }
}

void AnimationWidget::resourceToBeDeleted(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        const auto index = mUI.materials->findText(resource->GetName());
        mUI.materials->blockSignals(true);
        mUI.materials->removeItem(index);
        for (size_t i=0; i<mState.animation.GetNumComponents(); ++i)
        {
            auto& component = mState.animation.GetComponent(i);
            const auto& material = app::FromUtf8(component.GetMaterialName());
            if (material == resource->GetName())
            {
                WARN("Component '%1' uses a material '%2' that is deleted.",
                    component.GetName(), resource->GetName());
                component.SetMaterial("Checkerboard", mState.workspace->MakeMaterial("Checkerboard"));
            }
        }
        if (auto* comp = GetCurrentComponent())
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

        mDrawableMenu->clear();
        // update the drawable menu with drawable items.
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

void AnimationWidget::on_layer_valueChanged(int layer)
{
    if (auto* component = GetCurrentComponent())
    {
        component->SetLayer(layer);
    }
}

void AnimationWidget::on_cSizeX_valueChanged(double value)
{
    if (auto* component = GetCurrentComponent())
    {
        auto size = component->GetSize();
        size.x = value;
        component->SetSize(size);
    }
}
void AnimationWidget::on_cSizeY_valueChanged(double value)
{
    if (auto* component = GetCurrentComponent())
    {
        auto size = component->GetSize();
        size.y = value;
        component->SetSize(size);
    }
}
void AnimationWidget::on_cTranslateX_valueChanged(double value)
{
    if (auto* component = GetCurrentComponent())
    {
        auto translate = component->GetTranslation();
        translate.x = value;
        component->SetTranslation(translate);
    }
}
void AnimationWidget::on_cTranslateY_valueChanged(double value)
{
    if (auto* component = GetCurrentComponent())
    {
        auto translate = component->GetTranslation();
        translate.y = value;
        component->SetTranslation(translate);
    }
}
void AnimationWidget::on_cRotation_valueChanged(double value)
{
    if (auto* component = GetCurrentComponent())
    {
        component->SetRotation(qDegreesToRadians(value));
    }
}

void AnimationWidget::on_cName_textChanged(const QString& text)
{
    if (auto* component = GetCurrentComponent())
    {
        component->SetName(app::ToUtf8(text));
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

    // pop view transformation
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
