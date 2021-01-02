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
    class PolygonWidget : public MainWidget
    {
        Q_OBJECT

    public:
        PolygonWidget(app::Workspace* workspace);
        PolygonWidget(app::Workspace* workspace, const app::Resource& resource);
       ~PolygonWidget();

        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Render() override;
        virtual void Update(double secs) override;
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewTriangleFan_toggled(bool checked);
        void on_actionClear_triggered();
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
        Ui::PolygonWidget mUI;
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
        // the current material for the blueprint (the background image)
        std::shared_ptr<gfx::Material> mBlueprint;
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
        // cache the name of the material used as the blueprint
        // (background image) when designing the shape
        QString mBlueprintName;
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    private:
    };
} // namespace
