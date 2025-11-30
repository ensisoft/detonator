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
#  include <QMenu>
#include "warnpop.h"

#include <vector>
#include <memory>

#include "editor/gui/mainwidget.h"
#include "editor/app/workspace.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/tool/polygon.h"

namespace gui
{
    class DlgTextEdit;

    class ShapeWidget : public MainWidget
    {
        Q_OBJECT

    public:
        enum class MeshType {
            Simple2DRenderMesh,
            Simple2DShardEffectMesh
        };

        explicit ShapeWidget(app::Workspace* workspace);
        ShapeWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ShapeWidget() override;

        QString GetId() const override;
        void InitializeSettings(const UISettings& settings) override;
        void InitializeContent() override;
        void SetViewerMode() override;
        void AddActions(QToolBar& bar) override;
        void AddActions(QMenu& menu) override;
        bool SaveState(Settings& settings) const override;
        bool LoadState(const Settings& settings) override;
        bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        void ReloadShaders() override;
        void ReloadTextures() override;
        void Shutdown() override;
        void Render() override;
        void Update(double secs) override;
        void Save() override;
        bool HasUnsavedChanges() const override;
        bool GetStats(Stats* stats) const override;
        void OnAddResource(const app::Resource* resource) override;
        void OnRemoveResource(const app::Resource* resource) override;

    private slots:
        void on_widgetColor_colorChanged(const QColor& color);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewTriangleFan_triggered();
        void on_actionClear_triggered();
        void on_actionShowShader_triggered();
        void on_actionCustomizeShader_triggered();
        void on_blueprints_currentIndexChanged(int);
        void on_btnResetShader_clicked();
        void on_btnResetBlueprint_clicked();
        void on_staticInstance_stateChanged(int);
        void on_cmbMeshType_currentIndexChanged(int);

    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseDoubleClick(QMouseEvent* mickey);
        bool OnKeyPressEvent(QKeyEvent* key);

        template<typename Vertex>
        void PaintVertices2D(gfx::Painter& painter) const;

        template<typename Vertex>
        void PickVertex2D(const QPoint& pick_point, float width, float height);

        template<typename Vertex>
        void InsertVertex2D(const QPoint& click_point, float width, float height);

        MeshType GetMeshType() const;
        void SetMeshType(MeshType mesh);
        void CreateMeshBuilder();

    private:
        Ui::ShapeWidget mUI;
        QMenu* mHamburger = nullptr;
    private:
        class VertexDataTable;
        class MouseTool;
        template<typename T> class AddVertex2DTriangleFanTool;
        template<typename T> class MoveVertex2DTool;

        enum class GridDensity {
            Grid10x10 = 10,
            Grid20x20 = 20,
            Grid50x50 = 50,
            Grid100x100 = 100
        };
        GridDensity mGrid = GridDensity::Grid20x20;

        struct State {
            // the current workspace.
            app::Workspace* workspace = nullptr;
            // the current polygon we're editing.
            gfx::PolygonMeshClass polygon;
            // the builder tool for editing the shape
            std::unique_ptr<gfx::tool::IPolygonBuilder> builder;
            // the data table.
            std::unique_ptr<VertexDataTable> table;
        } mState;
        std::unique_ptr<MouseTool> mMouseTool;

        // the current material for the blueprint (the background image)
        std::unique_ptr<gfx::Material> mBlueprint;

        // true when playback is paused.
        bool mPaused = false;
        // true when playing.
        bool mPlaying = false;
        // current animation time.
        double mTime = 0.0f;
        // Index of the currently selected vertex.
        std::size_t mVertexIndex = 0xffffff;
    private:
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    private:
        DlgTextEdit* mShaderEditor = nullptr;
        std::string mCustomizedSource;
    };
} // namespace
