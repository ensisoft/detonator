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
#  include <QWidget>
#  include "ui_palettematerial.h"
#include "warnpop.h"

#include <string>

#include "editor/app/types.h"
#include "editor/gui/utility.h"
#include "editor/gui/types.h"

namespace app {
    class Workspace;
} //

namespace gui
{
    class PaletteMaterial : public QWidget
    {
        Q_OBJECT

    public:
        PaletteMaterial(const app::Workspace* workspace, QWidget* parent);

        void SetMaterialPreviewScale(const Size2Df& scale)
        { mPreviewScale = scale; }

        void SetLabel(const QString& str)
        {
            SetValue(mUI.label, str);
        }
        void SetIndex(std::size_t index)
        { mIndex = index; }
        void ResetMaterial()
        {
            SetValue(mUI.cmbMaterial, -1);
            SetEnabled(mUI.btnResetMaterial, false);
        }
        void SetMaterial(const app::AnyString& id)
        {
            SetValue(mUI.cmbMaterial, ListItemId(id))
                ? SetEnabled(mUI.btnResetMaterial, true)
                : SetEnabled(mUI.btnResetMaterial, false);
        }
        app::AnyString GetMaterialId() const
        {
            return GetItemId(mUI.cmbMaterial);
        }
        std::size_t GetIndex() const
        { return mIndex; }

        void UpdateMaterialList(const ResourceList& list);
    signals:
        void ValueChanged(const PaletteMaterial* material);

    private slots:
        void on_btnSelectMaterial_clicked();
        void on_btnSetMaterialParams_clicked();
        void on_btnResetMaterial_clicked();
        void on_cmbMaterial_currentIndexChanged(int);
    private:
        Ui::PaletteMaterial mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        std::size_t mIndex = 0;
        Size2Df mPreviewScale = {1.0f, 1.0f};
    };
} // namespace