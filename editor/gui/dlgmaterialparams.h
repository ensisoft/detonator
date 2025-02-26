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
#  include "ui_dlgmaterialparams.h"
#  include <QDialog>
#include "warnpop.h"

#include <vector>
#include <unordered_set>

#include "editor/app/workspace.h"
#include "game/fwd.h"
#include "game/entity_node_drawable_item.h"
#include "graphics/fwd.h"

namespace gui
{
    class Uniform;

    class DlgMaterialParams : public QDialog
    {
        Q_OBJECT

    public:
        DlgMaterialParams(QWidget* parent, game::DrawableItemClass* item);
        DlgMaterialParams(QWidget* parent, game::DrawableItemClass* item, game::MaterialAnimatorClass* actuator);

        void AdaptInterface(const app::Workspace* workspace, const gfx::MaterialClass* material);
    private slots:
        void on_textureMaps_currentIndexChanged(int);
        void on_btnResetActiveMap_clicked();
        void on_tileIndex_valueChanged(int);
        void on_btnResetTileIndex_clicked();
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void UniformValueChanged(const gui::Uniform* uniform);
        void ToggleUniform(bool checked);
    private:
        Ui::DlgMaterialParams mUI;
    private:
        game::DrawableItemClass* mItem = nullptr;
        game::MaterialAnimatorClass* mActuator = nullptr;
        game::DrawableItemClass::MaterialParamMap mOldParams;
        std::vector<gui::Uniform*> mUniforms;

        struct ColorUniform {
            std::string desc;
            std::string name;
            gfx::Color4f material_default;
        };
        // know built-in color uniforms
        std::vector<ColorUniform> mColorUniforms;

        std::unordered_set<std::string> mKnownChanges;
    };

} // namespace
