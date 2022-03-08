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
#  include "ui_polygonwidget.h"
#include "warnpop.h"

#include <vector>
#include <memory>

#include "editor/gui/mainwidget.h"
#include "editor/app/workspace.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"

namespace gui
{
    class ShapeWidget : public MainWidget
    {
        Q_OBJECT

    public:
        ShapeWidget(app::Workspace* workspace);
        ShapeWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ShapeWidget();

        virtual void Initialize(const UISettings& settings) override;
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Render() override;
        virtual void Update(double secs) override;
        virtual void Save() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewTriangleFan_toggled(bool checked);
        void on_actionClear_triggered();
        void on_blueprints_currentIndexChanged(int);
        void on_btnResetBlueprint_clicked();
        void on_staticInstance_stateChanged(int);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);

    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseDoubleClick(QMouseEvent* mickey);
        bool OnKeyPressEvent(QKeyEvent* key);

    private:
        Ui::ShapeWidget mUI;
    private:
        enum class GridDensity {
            Grid10x10 = 10,
            Grid20x20 = 20,
            Grid50x50 = 50,
            Grid100x100 = 100
        };
        GridDensity mGrid = GridDensity::Grid20x20;
        // the current workspace.
        app::Workspace* mWorkspace = nullptr;
        // the current polygon we're editing.
        gfx::PolygonClass mPolygon;
        // the current draw being added.
        gfx::PolygonClass mCurrentDraw;
        // the current material for the blueprint (the background image)
        std::unique_ptr<gfx::Material> mBlueprint;
        // the list of points for adding the next draw command.
        std::vector<QPoint> mPoints;
        // the most current (latest) point is where the mouse
        // currently is.
        QPoint mCurrentPoint;
        // true when adding a new draw command.
        bool mActive = false;
        // true when dragging a vertex around.
        bool mDragging = false;
        // true when playback is paused.
        bool mPaused = false;
        // true when playing.
        bool mPlaying = false;
        // current animation time.
        double mTime = 0.0f;
        // Index of the currently selected vertex.
        std::size_t mVertexIndex = 0;
    private:
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    private:
    };
} // namespace
