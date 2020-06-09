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
    TreeModel(scene::Animation& anim)  : mAnimation(anim)
    {}

    virtual void Flatten(std::vector<TreeWidget::TreeItem>& list)
    {
        auto& root = mAnimation.GetRenderTree();

        class Visitor : public scene::Animation::RenderTree::Visitor
        {
        public:
            Visitor(std::vector<TreeWidget::TreeItem>& list)
                : mList(list)
            {}
            virtual void EnterNode(scene::AnimationNode* node)
            {
                TreeWidget::TreeItem item;
                item.SetId(node ? app::FromUtf8(node->GetId()) : "root");
                item.SetText(node ? app::FromUtf8(node->GetName()) : "Root");
                item.SetUserData(node);
                item.SetLevel(mLevel);
                mList.push_back(item);
                mLevel++;
            }
            virtual void LeaveNode(scene::AnimationNode* node)
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
        const float d = std::sqrt(diff.x()*diff.x() + diff.y()*diff.y());
        const float w = mAlwaysSquare ? d : diff.x();
        const float h = mAlwaysSquare ? d : diff.y();
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
            mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
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
            name = "Node " + std::to_string(i);
            if (CheckNameAvailability(name))
                break;
        }
        // todo: the width and height need to account the zoom level.
        const auto& diff = mCurrent - mStart;
        const float xpos = mStart.x() - mState.camera_offset_x;
        const float ypos = mStart.y() - mState.camera_offset_y;
        const float hypo = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
        const float width  = mAlwaysSquare ? hypo : diff.x();
        const float height = mAlwaysSquare ? hypo : diff.y();

        scene::AnimationNode node;
        node.SetMaterial(app::ToUtf8(mMaterialName), mMaterial);
        node.SetDrawable(app::ToUtf8(mDrawableName), mDrawable);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos, ypos));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto& root  = mState.animation.GetRenderTree();
        auto* child = mState.animation.AddNode(std::move(node));
        root.AppendChild(child);

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
    QPoint mStart;
    QPoint mCurrent;
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
            mState.camera_offset_x += x;
            mState.camera_offset_y += y;
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

    mState.scenegraph.reset(new TreeModel(mState.animation));
    mState.workspace = workspace;

    mUI.setupUi(this);
    // this fucking cunt whore will already emit selection changed signal
    mUI.materials->blockSignals(true);
    mUI.materials->addItems(workspace->ListMaterials());
    mUI.materials->blockSignals(false);

    mUI.name->setText("My Animation");

    mUI.tree->SetModel(mState.scenegraph.get());
    mUI.tree->Rebuild();

    mUI.widget->setFramerate(60);
    mUI.widget->onInitScene  = [&](unsigned width, unsigned height) {
        // offset the viewport so that the origin of the 2d space is in the middle of the viewport
        mState.camera_offset_x = width / 2.0f;
        mState.camera_offset_y = height / 2.0f;
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

        mUI.tree->Rebuild();
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

    PopulateFromEnum<scene::AnimationNode::RenderPass>(mUI.renderPass);
    PopulateFromEnum<scene::AnimationNode::RenderStyle>(mUI.renderStyle);

    connect(mUI.tree, &TreeWidget::currentRowChanged,
            this, &AnimationWidget::currentComponentRowChanged);
    connect(mUI.tree, &TreeWidget::dragEvent,
            this, &AnimationWidget::treeDragEvent);

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
    settings.saveWidget("Animation", mUI.translateX);
    settings.saveWidget("Animation", mUI.translateY);
    settings.saveWidget("Animation", mUI.scaleX);
    settings.saveWidget("Animation", mUI.scaleY);
    settings.saveWidget("Animation", mUI.rotation);
    settings.saveWidget("Animation", mUI.showGrid);

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
    settings.loadWidget("Animation", mUI.showGrid);

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

    mUI.tree->Rebuild();
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
    const scene::AnimationNode* item = GetCurrentNode();
    if (item == nullptr)
        return;

    auto& tree = mState.animation.GetRenderTree();

    // find the scene graph node that contains this AnimationNode.
    auto* node = tree.FindNodeByValue(item);

    // traverse the tree starting from the node to be deleted
    // and capture the ids of the animation nodes that are part
    // of this hiearchy.
    struct Carcass {
        std::string id;
        std::string name;
    };
    std::vector<Carcass> graveyard;
    node->PreOrderTraverseForEach([&](scene::AnimationNode* value) {
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
    if (auto* node = GetCurrentNode())
    {
        const auto& material_name = app::ToUtf8(name);
        node->SetMaterial(material_name, mState.workspace->MakeMaterial(material_name));
    }
}

void AnimationWidget::on_renderPass_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const scene::AnimationNode::RenderPass pass = GetValue(mUI.renderPass);
        node->SetRenderPass(pass);
    }
}

void AnimationWidget::on_renderStyle_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        const scene::AnimationNode::RenderStyle style = GetValue(mUI.renderStyle);
        node->SetRenderStyle(style);
    }
}

void AnimationWidget::currentComponentRowChanged()
{
    const scene::AnimationNode* node = GetCurrentNode();
    if (node == nullptr)
    {
        mUI.cProperties->setEnabled(false);
        mUI.cTransform->setEnabled(false);
    }
    else
    {
        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        SetValue(mUI.cName, node->GetName());
        SetValue(mUI.renderPass, node->GetRenderPass());
        SetValue(mUI.renderStyle, node->GetRenderStyle());
        SetValue(mUI.layer, node->GetLayer());
        SetValue(mUI.materials, node->GetMaterialName());
        SetValue(mUI.cTranslateX, translate.x);
        SetValue(mUI.cTranslateY, translate.y);
        SetValue(mUI.cSizeX, size.x);
        SetValue(mUI.cSizeY, size.y);
        SetValue(mUI.cRotation, qRadiansToDegrees(node->GetRotation()));
        SetValue(mUI.lineWidth, node->GetLineWidth());
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
        for (size_t i=0; i<mState.animation.GetNumNodes(); ++i)
        {
            auto& component = mState.animation.GetNode(i);
            const auto& material = app::FromUtf8(component.GetMaterialName());
            if (material == resource->GetName())
            {
                WARN("Component '%1' uses a material '%2' that is deleted.",
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

void AnimationWidget::treeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.animation.GetRenderTree();
    auto* src_value = static_cast<scene::AnimationNode*>(item->GetUserData());
    auto* dst_value = static_cast<scene::AnimationNode*>(target->GetUserData());

    // find the scene graph node that contains this AnimationNode.
    auto* src_node   = tree.FindNodeByValue(src_value);
    auto* src_parent = tree.FindParent(src_node);

    // check if we're trying to drag a parent onto its own child
    if (src_node->FindNodeByValue(dst_value))
        return;

    scene::Animation::RenderTreeNode branch = *src_node;
    src_parent->DeleteChild(src_node);

    auto* dst_node  = tree.FindNodeByValue(dst_value);
    dst_node->AppendChild(std::move(branch));

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
    auto* node = static_cast<scene::AnimationNode*>(item->GetUserData());

    node->SetName(app::ToUtf8(text));
    item->SetText(text);

    mUI.tree->Update();
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
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Rotate(angle);
    view.Translate(GetValue(mUI.translateX), GetValue(mUI.translateY));

    class DrawHook : public scene::Animation::DrawHook
    {
    public:
        DrawHook(scene::AnimationNode* selected)
          : mSelected(selected)
        {}
        virtual void AppendPackets(const scene::AnimationNode* node, const scene::Animation::DrawPacket& packet,
            std::vector<scene::Animation::DrawPacket>& packets) override
        {
            if (node != mSelected)
                return;

            scene::Animation::DrawPacket selection;
            selection.transform = packet.transform;
            selection.material  = std::make_shared<gfx::Material>(gfx::SolidColor(gfx::Color::Green));
            selection.drawable  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Wireframe);
            selection.layer     = packet.layer;
            packets.push_back(selection);
        }
    private:
        scene::AnimationNode* mSelected = nullptr;
    };

    DrawHook hook(GetCurrentNode());

    // render endless background grid.
    if (mUI.showGrid->isChecked())
    {
        view.Push();

        const auto grid_size = std::max(width, height);
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

scene::AnimationNode* AnimationWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    if (!item->GetUserData())
        return nullptr;
    return static_cast<scene::AnimationNode*>(item->GetUserData());
}

} // namespace
