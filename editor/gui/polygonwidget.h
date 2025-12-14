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
#  include <glm/vec3.hpp>
#  include <QMenu>
#include "warnpop.h"

#include <vector>
#include <memory>
#include <unordered_set>
#include <optional>

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
            Simple2DShardEffectMesh,
            Dimetric2DRenderMesh,
            Isometric2DRenderMesh
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
        bool OnEscape() override;

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
        void on_tableView_customContextMenuRequested(const QPoint& point);

    private:
        using PolygonClassHandle = std::shared_ptr<const gfx::PolygonMeshClass>;

        void PaintScene(gfx::Painter& painter, double secs);
        void PaintEditScene(const QRect& rect, const PolygonClassHandle& polygon, gfx::Device* device);
        void PaintLitAxonometricScene(const QRect& rect, const PolygonClassHandle& polygon, gfx::Device* device) const;
        void Paint3DAxonometricScene(const QRect& rect, const PolygonClassHandle& polygon, gfx::Device* device) const;
        void PaintViewRect(const QRect& rect, const app::AnyString& name, gfx::Device* device) const;
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseDoubleClick(QMouseEvent* mickey);
        void OnMouseWheel(QWheelEvent* wheel);
        bool OnKeyPressEvent(QKeyEvent* key);
        bool OnKeyReleaseEvent(QKeyEvent* key);

        template<typename Vertex>
        void PaintVertices2D(gfx::Painter& painter) const;

        template<typename Vertex>
        void PaintVertices25D(gfx::Painter& painter) const;

        enum class PickMode {
            Normal, Cycling, Sticky
        };

        template<typename Vertex>
        void PickVertex2D(const QPoint& pick_point, float width, float height, PickMode mode);
        template<typename Vertex>
        void HoverVertex2D(const QPoint& pick_point, float width, float height);

        template<typename Vertex>
        void InsertVertex2D(const QPoint& click_point, float width, float height);

        template<typename Vertex>
        void ScrollAxonometricVertex(const QWheelEvent* wheel);
        void ScrollAxonometricLight(const QWheelEvent* wheel);

        MeshType GetMeshType() const;
        void SetMeshType(MeshType mesh);
        void CreateMeshBuilder();
        void SetSelectedVertexNormal(const glm::vec3& normal);
        void ConfigureTilePainter(gfx::Painter& painter) const;

        enum class ViewType {
            EditView,
            LitView,
            Axo3DView
        };

        QRect GetMainRenderRect() const;
        QRect GetLitRenderRect() const;
        QRect GetAxo3DRenderRect() const;
        QRect GetViewRect(ViewType view) const;

        bool IsHovered(size_t index) const;
        bool IsSelected(size_t index) const;

    private:
        Ui::ShapeWidget mUI;
        QMenu* mHamburger = nullptr;
    private:
        class VertexDataTable;
        class MouseTool;
        template<typename T> class AddVertex2DTriangleFanTool;
        template<typename T> class MoveVertex2DTool;

        enum class Hotkey {
            None, KeyX, KeyY, KeyZ
        };
        std::unordered_set<Hotkey> mHotkeysPressed;

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
            std::shared_ptr<gfx::PolygonMeshClass> polygon;
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
        static constexpr auto InvalidIndex = 0xffffffff;
        // Index of the currently selected vertex.
        std::size_t mSelectedVertex = InvalidIndex;

        struct PickCandidate {
            float tangent = 0.0f;
            float distance = 0.0f;
            int index = 0;

        };
        std::vector<PickCandidate> mPickingCandidates;
        std::size_t mPickingIndex = 0;

        std::optional<float> mPixelDistance2Dand3D;

        unsigned mAxonometricTextureWidth  = 0;
        unsigned mAxonometricTextureHeight = 0;
        glm::vec3 mAxonometricTileBaseSize = {0.0f, 0.0f, 0.0f};
        glm::vec3 mAxonometricLightPosition = {0.0f, 0.0f, 0.0f};
        ViewType mMainView = ViewType::EditView;

        enum class LightType {
            None, Spot, Point, Directional
        };
        LightType mLightType = LightType::Point;

        mutable std::vector<std::string> mMessages;
    private:
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    private:
        DlgTextEdit* mShaderEditor = nullptr;
        std::string mCustomizedSource;
    };
} // namespace
