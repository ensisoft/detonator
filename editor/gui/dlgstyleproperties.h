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
#  include <QColor>
#  include <QDialog>
#  include "ui_dlgstyleproperties.h"
#include "warnpop.h"

#include <string>
#include <memory>
#include <vector>

namespace app {
    class Workspace;
    struct ListItem;
} // namespace

namespace uik {
    class Widget;
} // namespace

namespace engine {
    class UIStyle;
    class UIPainter;
}// namespace

namespace gui
{
    class DlgWidgetStyleProperties : public QDialog
    {
        Q_OBJECT

    public:
        DlgWidgetStyleProperties(QWidget* parent,
            engine::UIStyle* style,
            app::Workspace* workspace,
            std::string identifier);
       ~DlgWidgetStyleProperties();

        void SetMaterials(const std::vector<app::ListItem>& list);
        void SetPainter(engine::UIPainter* painter)
        { mPainter = painter; }
    private:
        void ShowPropertyValue();
        void SetPropertyValue();
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnOpenFontFile_clicked();
        void on_btnSelectFont_clicked();
        void on_btnResetProperty_clicked();
        void on_cmbWidgetClass_currentIndexChanged(int);
        void on_cmbSelector_currentIndexChanged(int);
        void on_widgetFontName_currentIndexChanged(int);
        void on_widgetFontSize_currentIndexChanged(int);
        void on_widgetTextVAlign_currentIndexChanged(int);
        void on_widgetTextHAlign_currentIndexChanged(int);
        void on_widgetColor_colorChanged(QColor color);
        void on_widgetFlag_stateChanged(int);
        void on_widgetFloat_valueChanged(double);
        void on_widgetShape_currentIndexChanged(int);
        void on_widgetMaterial_currentIndexChanged(int);
        void TableSelectionChanged(const QItemSelection, const QItemSelection);
        void SetWidgetMaterial();
        void SetWidgetColor();
        void SetWidgetGradient();
    private:
        Ui::DlgStyleProperties mUI;
    private:
        class PropertyModel;
        std::unique_ptr<PropertyModel> mModel;
    private:
        app::Workspace* mWorkspace  = nullptr;
        engine::UIStyle* mStyle     = nullptr;
        engine::UIPainter* mPainter = nullptr;
        std::string mIdentifier;
    };
} // namespace
