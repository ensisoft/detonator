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
#  include "ui_materialwidget.h"
#  include <QMap>
#  include <QString>
#  include <QFileSystemWatcher>
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <vector>

#include "editor/gui/mainwidget.h"
#include "graphics/fwd.h"
#include "graphics/types.h"
#include "graphics/material.h"

namespace gfx {
    class MaterialInstance;
    class Material;
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class Uniform;
    class Sampler;
    class DlgTextEdit;

    class MaterialWidget : public MainWidget
    {
        Q_OBJECT
    public:
        explicit MaterialWidget(app::Workspace* workspace);
        MaterialWidget(app::Workspace* workspace, const app::Resource& resource);
       ~MaterialWidget() override;;

        QString GetId() const override;
        QImage TakeScreenshot() const override;
        void InitializeSettings(const UISettings& settings) override;
        void SetViewerMode() override;
        void AddActions(QToolBar& bar) override;
        void AddActions(QMenu& menu) override;
        bool SaveState(Settings& settings) const override;
        bool LoadState(const Settings& settings) override;
        bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        void ZoomIn() override;
        void ZoomOut() override;
        void ReloadShaders() override;
        void ReloadTextures() override;
        void Shutdown() override;
        void Update(double dt) override;
        void Render() override;
        void Save() override;
        bool HasUnsavedChanges() const override;
        bool GetStats(Stats* stats) const override;
    private slots:
        void on_widgetColor_colorChanged(QColor color);
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionReloadShaders_triggered();
        void on_actionReloadTextures_triggered();
        void on_actionNewMap_triggered();
        void on_actionDelMap_triggered();
        void on_actionAddFile_triggered();
        void on_actionAddText_triggered();
        void on_actionAddBitmap_triggered();
        void on_actionEditTexture_triggered();
        void on_actionRemoveTexture_triggered();
        void on_actionCreateShader_triggered();
        void on_actionSelectShader_triggered();
        void on_actionEditShader_triggered();
        void on_actionShowShader_triggered();
        void on_actionCustomizeShader_triggered();
        void on_btnResetShader_clicked();
        void on_btnAddTextureMap_clicked();
        void on_btnResetTextureMap_clicked();
        void on_btnEditTexture_clicked();
        void on_btnSelectTextureRect_clicked();
        void on_textureMapWidget_SelectionChanged();
        void on_textureMapWidget_CustomContextMenuRequested(const QPoint&);
        void on_materialName_textChanged(const QString& text);
        void on_materialType_currentIndexChanged(int);
        void on_surfaceType_currentIndexChanged(int);
        void on_particleStartColor_colorChanged(QColor color);
        void on_particleMidColor_colorChanged(QColor color);
        void on_particleEndColor_colorChanged(QColor color);
        void on_diffuseColor_colorChanged(QColor color);
        void on_ambientColor_colorChanged(QColor color);
        void on_specularColor_colorChanged(QColor color);
        void on_specularExponent_valueChanged(double);
        void on_particleBaseRotation_valueChanged(double);
        void on_particleRotationMode_currentIndexChanged(int);
        void on_tileWidth_valueChanged(int);
        void on_tileHeight_valueChanged(int);
        void on_tileLeftOffset_valueChanged(int);
        void on_tileTopOffset_valueChanged(int);
        void on_tileLeftPadding_valueChanged(int);
        void on_tileTopPadding_valueChanged(int);
        void on_activeMap_currentIndexChanged(int);
        void on_alphaCutoff_valueChanged(bool, double);
        void on_baseColor_colorChanged(QColor color);
        void on_particleAction_currentIndexChanged(int);
        void on_cmbGradientType_currentIndexChanged(int);
        void on_spriteFps_valueChanged(double);
        void on_chkBlendPreMulAlpha_stateChanged(int);
        void on_chkStaticInstance_stateChanged(int);
        void on_chkEnableBloom_stateChanged(int);
        void on_chkBlendFrames_stateChanged(int);
        void on_chkLooping_stateChanged(int);
        void on_spriteCols_valueChanged(int);
        void on_spriteRows_valueChanged(int);
        void on_spriteSheet_toggled(bool);
        void on_spriteDuration_valueChanged(double);
        void on_colorMap0_colorChanged(QColor);
        void on_colorMap1_colorChanged(QColor);
        void on_colorMap2_colorChanged(QColor);
        void on_colorMap3_colorChanged(QColor);
        void on_gradientOffsetX_valueChanged(int value);
        void on_gradientOffsetY_valueChanged(int value);
        void on_gradientGamma_valueChanged(double);
        void on_textureScaleX_valueChanged(double);
        void on_textureScaleY_valueChanged(double);
        void on_textureRotation_valueChanged(double);
        void on_textureVelocityX_valueChanged(double);
        void on_textureVelocityY_valueChanged(double);
        void on_textureVelocityZ_valueChanged(double);
        void on_textureMinFilter_currentIndexChanged(int);
        void on_textureMagFilter_currentIndexChanged(int);
        void on_textureWrapX_currentIndexChanged(int);
        void on_textureWrapY_currentIndexChanged(int);
        void on_chkAllowPacking_stateChanged(int);
        void on_chkAllowResizing_stateChanged(int);
        void on_chkPreMulAlpha_stateChanged(int);
        void on_chkBlurTexture_stateChanged(int);
        void on_chkDetectEdges_stateChanged(int);
        void on_cmbColorSpace_currentIndexChanged(int);
        void on_textureMapName_textChanged(const QString& text);
        void on_textureMapName_editingFinished();
        void on_textureSourceName_textChanged(const QString& text);
        void on_findMap_textChanged(const QString& text);
        void on_cmbModel_currentIndexChanged(int);
        void AddNewTextureSrcFromFile();
        void AddNewTextureSrcFromText();
        void AddNewTextureSrcFromBitmap();
        void UniformValueChanged(const Uniform* uniform);
        void ShaderFileChanged();
        void ResourceUpdated(const app::Resource* resource);

    private:
        void CreateCustomShaderStub();
        void CreateShaderStubFromSource(const char* source);

        void ClearCustomUniforms();
        void ApplyShaderDescription();
        void SetTextureFlags();
        void SetMaterialProperties();
        void ShowMaterialProperties();
        void ShowTextureSrcProperties();
        void ShowTextureMapProperties();
        void PaintScene(gfx::Painter& painter, double sec);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        bool KeyPress(QKeyEvent* key);
        gfx::TextureMap* GetSelectedTextureMap();
        gfx::TextureSource* GetSelectedTextureSrc();
        float GetMaterialRenderTime() const;
    private:
        Ui::MaterialWidget mUI;
    private:
        // current workspace object
        app::Workspace* mWorkspace = nullptr;
        // the material class we're editing
        std::shared_ptr<gfx::MaterialClass> mMaterial;
        std::unique_ptr<gfx::MaterialInstance> mMaterialInst;
        std::shared_ptr<gfx::Drawable> mDrawable;
        // play state
        enum PlayState {
            Playing, Paused, Stopped
        };
        PlayState mState = PlayState::Stopped;
        // current playback time (if playing)
        float mTime = 0.0f;
        // the original material hash that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
        std::vector<Uniform*> mUniforms;
        std::vector<Sampler*> mSamplers;
        glm::vec3 mModelRotationTotal = {0.0f, 0.0f, 0.0f};
        glm::vec3 mModelRotationDelta = {0.0f, 0.0f, 0.0f};
        glm::vec3 mLightPositionTotal = {0.0f, 0.0f, 0.0f};
        glm::vec3 mLightPositionDelta = {0.0f, 0.0f, 0.0f};
        QPoint mMouseDownPoint;

        enum class MouseState {
            Nada, RotateModel, MoveLight
        };
        MouseState mMouseState = MouseState::Nada;

        QFileSystemWatcher mFileWatcher;

        DlgTextEdit* mShaderEditor = nullptr;
        std::string mCustomizedSource;
    };
} // namespace
