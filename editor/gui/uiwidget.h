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

#include "warnpush.h"
#  include "ui_uiwidget.h"
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <deque>

#include "engine/ui.h"
#include "graphics/types.h"
#include "graphics/material.h"
#include "uikit/window.h"
#include "uikit/fwd.h"
#include "editor/gui/mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class MouseTool;
    class PlayWindow;

    class UIWidget : public MainWidget
    {
        Q_OBJECT

    public:
        explicit UIWidget(app::Workspace* workspace);
        UIWidget(app::Workspace* workspace, const app::Resource& resource);
       ~UIWidget() override;

        virtual QString GetId() const override;
        virtual QImage TakeScreenshot() const override;
        virtual void InitializeSettings(const UISettings& settings) override;
        virtual void InitializeContent() override;
        virtual void SetViewerMode() override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipbboad)  const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void RunGameLoopOnce() override;
        virtual void Shutdown() override;
        virtual void Update(double dt) override;
        virtual void Render() override;
        virtual void Save() override;
        virtual void Undo() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool OnEscape() override;
        virtual bool GetStats(Stats* stats) const override;
        virtual void Refresh() override;
        virtual bool LaunchScript(const app::AnyString& id) override;
        virtual void OnAddResource(const app::Resource* resource) override;
        virtual void OnRemoveResource(const app::Resource* resource) override;
        virtual void OnUpdateResource(const app::Resource* resource) override;
    private slots:
        void on_widgetColor_colorChanged(QColor color);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionClose_triggered();
        void on_actionSave_triggered();
        void on_actionPreview_triggered();
        void on_actionWidgetDelete_triggered();
        void on_actionWidgetDuplicate_triggered();
        void on_actionWidgetOrder_triggered();
        void on_windowName_textChanged(const QString& text);
        void on_windowKeyMap_currentIndexChanged(int);
        void on_windowStyleFile_currentIndexChanged(int);
        void on_windowScriptFile_currentIndexChanged(int);
        void on_chkEnableKeyMap_stateChanged(int);
        void on_chkRecvMouseEvents_stateChanged(int);
        void on_chkRecvKeyEvents_stateChanged(int);
        void on_widgetName_textChanged(const QString& text);
        void on_widgetWidth_valueChanged(double value);
        void on_widgetHeight_valueChanged(double value);
        void on_widgetXPos_valueChanged(double value);
        void on_widgetYPos_valueChanged(double value);
        void on_toggleChecked_stateChanged(int);
        void on_rbText_textChanged();
        void on_rbPlacement_currentIndexChanged(int);
        void on_rbSelected_stateChanged(int);
        void on_btnText_textChanged();
        void on_lblText_textChanged();
        void on_lblLineHeight_valueChanged(double);
        void on_chkText_textChanged();
        void on_chkPlacement_currentIndexChanged(int);
        void on_chkCheck_stateChanged(int);
        void on_spinMin_valueChanged(int);
        void on_spinMax_valueChanged(int);
        void on_spinVal_valueChanged(int);
        void on_sliderVal_valueChanged(double);
        void on_progText_textChanged();
        void on_progVal_valueChanged(int);
        void on_btnResetProgVal_clicked();
        void on_btnReloadKeyMap_clicked();
        void on_btnSelectKeyMap_clicked();
        void on_btnReloadStyle_clicked();
        void on_btnSelectStyle_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnResetTransform_clicked();
        void on_btnMoreViewportSettings_clicked();
        void on_btnEditWidgetStyle_clicked();
        void on_btnEditWidgetStyleString_clicked();
        void on_btnResetWidgetStyle_clicked();
        void on_btnEditWidgetAnimationString_clicked();
        void on_btnResetWidgetAnimationString_clicked();
        void on_cmbScrollAreaVerticalScrollbarMode_currentIndexChanged(int);
        void on_cmbScrollAreaHorizontalScrollbarMode_currentIndexChanged(int);
        void on_shapeDrawable_currentIndexChanged(int);
        void on_shapeMaterial_currentIndexChanged(int);
        void on_shapeRotation_valueChanged(double);
        void on_shapeCenterX_valueChanged(double);
        void on_shapeCenterY_valueChanged(double);
        void on_btnShapeContentEditDrawable_clicked();
        void on_btnShapeContentEditMaterial_clicked();
        void on_btnShapeContentSelectMaterial_clicked();
        void on_btnShapeContentPlus90_clicked();
        void on_btnShapeContentMinus90_clicked();

        void on_btnEditWindowStyle_clicked();
        void on_btnEditWindowStyleString_clicked();
        void on_btnResetWindowStyle_clicked();
        void on_btnEditScript_clicked();
        void on_btnAddScript_clicked();
        void on_btnResetScript_clicked();
        void on_chkWidgetEnabled_stateChanged(int);
        void on_chkWidgetVisible_stateChanged(int);
        void on_chkWidgetClipChildren_stateChanged(int);
        void on_tree_customContextMenuRequested(QPoint);

        void TreeCurrentWidgetChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void WidgetStyleEdited();
    private:
        void PaintScene(gfx::Painter& painter, double sec);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        bool KeyRelease(QKeyEvent* key);
        void UpdateCurrentWidgetProperties();
        void TranslateCamera(float dx, float dy);
        void TranslateCurrentWidget(float dx, float dy);
        void DisplayCurrentWidgetProperties();
        void DisplayCurrentCameraLocation();
        void RebuildCombos();
        void PlaceNewWidget(unsigned index);
        uik::Widget* GetCurrentWidget();
        const uik::Widget* GetCurrentWidget() const;
        bool LoadStyleVerbose(const QString& name);
        bool LoadStyleQuiet(const std::string& uri);
        bool LoadKeysVerbose(const std::string& uri);
        bool LoadKeysQuiet(const std::string& uri);
        void UpdateDeletedResourceReferences();
        void UncheckPlacementActions();
        uik::FSize GetFormSize() const;
    private:
        Ui::UIWidget mUI;
        QMenu* mWidgets = nullptr;
    private:
        class TreeModel;
        class PlaceWidgetTool;
        class MoveWidgetTool;
        class ResizeWidgetTool;
        std::unique_ptr<TreeModel> mWidgetTree;
        std::unique_ptr<MouseTool> mCurrentTool;
        enum class PlayState {
            Playing, Paused, Stopped
        };
        PlayState mPlayState = PlayState::Stopped;
        struct State {
            // UI tree widget.
            gui::TreeWidget* tree = nullptr;
            // current workspace.
            app::Workspace* workspace = nullptr;
            // design time window that is used to create the widget hierarchy
            std::shared_ptr<uik::Window> window;
            // transient window state that only exists when we're playing
            // the window, i.e. passing events such as mouse events to it.
            std::unique_ptr<uik::TransientState> active_state;
            std::unique_ptr<uik::AnimationStateArray> animation_state;
            // the active window that only exists when we're playing
            // the window. originally it's a bit-wise copy of the
            // design time window but then obviously changes state
            // while the playback happens and events are dispatched.
            std::unique_ptr<uik::Window> active_window;
            std::unique_ptr<engine::UIPainter> painter;
            std::unique_ptr<engine::UIStyle> style;
            std::unique_ptr<engine::UIKeyMap> keymap;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
        } mState;

        double mCurrentTime = 0.0;
        double mPlayTime    = 0.0;
        double mViewTransformStartTime = 0.0;
        float  mViewTransformRotation  = 0.0f;
        bool mCameraWasLoaded = false;

        unsigned mRefreshTick = 0;
        std::deque<std::string> mMessageQueue;
        std::size_t mOriginalHash = 0;
        boost::circular_buffer<uik::Window> mUndoStack;

        // the preview window if any.
        std::unique_ptr<PlayWindow> mPreview;
    };

    QString GenerateUIScriptSource(QString window);

} // namespace
