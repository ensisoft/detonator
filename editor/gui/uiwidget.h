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

    class UIWidget : public MainWidget
    {
        Q_OBJECT

    public:
        UIWidget(app::Workspace* workspace);
        UIWidget(app::Workspace* workspace, const app::Resource& resource);
       ~UIWidget();

        virtual void Initialize(const UISettings& settings) override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const;
        virtual bool LoadState(const Settings& settings);
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipbboad)  const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double dt) override;
        virtual void Render() override;
        virtual void Save() override;
        virtual void Undo() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual bool OnEscape() override;
        virtual bool GetStats(Stats* stats) const override;
        virtual void Refresh() override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewForm_triggered();
        void on_actionNewLabel_triggered();
        void on_actionNewPushButton_triggered();
        void on_actionNewGroupBox_triggered();
        void on_actionNewCheckBox_triggered();
        void on_actionNewSpinBox_triggered();
        void on_actionNewSlider_triggered();
        void on_actionNewProgressBar_triggered();
        void on_actionNewRadioButton_triggered();
        void on_actionWidgetDelete_triggered();
        void on_actionWidgetDuplicate_triggered();
        void on_windowName_textChanged(const QString& text);
        void on_windowStyleFile_currentIndexChanged(int);
        void on_windowScriptFile_currentIndexChanged(int);
        void on_widgetName_textChanged(const QString& text);
        void on_widgetWidth_valueChanged(double value);
        void on_widgetHeight_valueChanged(double value);
        void on_widgetXPos_valueChanged(double value);
        void on_widgetYPos_valueChanged(double value);
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
        void on_btnReloadStyle_clicked();
        void on_btnSelectStyle_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnResetTransform_clicked();
        void on_btnEditWidgetStyle_clicked();
        void on_btnResetWidgetStyle_clicked();
        void on_btnEditWindowStyle_clicked();
        void on_btnResetWindowStyle_clicked();
        void on_btnEditScript_clicked();
        void on_btnAddScript_clicked();
        void on_btnResetScript_clicked();
        void on_chkWidgetEnabled_stateChanged(int);
        void on_chkWidgetVisible_stateChanged(int);
        void on_tree_customContextMenuRequested(QPoint);

        void TreeCurrentWidgetChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
        void WidgetStyleEdited();
    private:
        void PaintScene(gfx::Painter& painter, double sec);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        void UpdateCurrentWidgetProperties();
        void TranslateCamera(float dx, float dy);
        void TranslateCurrentWidget(float dx, float dy);
        void DisplayCurrentWidgetProperties();
        void DisplayCurrentCameraLocation();
        void RebuildCombos();
        uik::Widget* GetCurrentWidget();
        const uik::Widget* GetCurrentWidget() const;
        bool LoadStyleVerbose(const QString& name);
        bool LoadStyleQuiet(const std::string& uri);
        void UpdateDeletedResourceReferences();
        uik::FSize GetFormSize() const;
    private:
        Ui::UIWidget mUI;
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
            uik::Window window;
            // transient window state that only exists when we're playing
            // the window, i.e. passing events such as mouse events to it.
            std::unique_ptr<uik::State> active_state;
            // the active window that only exists when we're playing
            // the window. originally it's a bit-wise copy of the
            // design time window but then obviously changes state
            // while the playback happens and events are dispatched.
            std::unique_ptr<uik::Window> active_window;
            std::unique_ptr<engine::UIPainter> painter;
            std::unique_ptr<engine::UIStyle> style;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
        } mState;

        double mCurrentTime = 0.0;
        double mPlayTime    = 0.0;
        double mViewTransformStartTime = 0.0;
        float  mViewTransformRotation  = 0.0f;
        bool mCameraWasLoaded = false;
        QString mCurrentStyle;

        unsigned mRefreshTick = 0;
        std::deque<std::string> mMessageQueue;
        std::size_t mOriginalHash = 0;
        boost::circular_buffer<uik::Window> mUndoStack;
    };
} // namespace
