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
#  include "ui_widgetstylewidget.h"
#include "warnpop.h"

#include <string>

namespace app {
    class Workspace;
    struct ResourceListItem;
}

namespace uik {
    class Widget;
} // namespace

namespace engine {
    class UIStyle;
    class UIPainter;
}// namespace

namespace gui
{
    class WidgetStyleWidget : public QWidget
    {
        Q_OBJECT

    public:
        WidgetStyleWidget(QWidget* parent);

        void SetWidget(uik::Widget* widget);

        void SetWorkspace(app::Workspace* workspace)
        { mWorkspace = workspace;}
        void SetStyle(engine::UIStyle* style)
        { mStyle = style; }
        void SetPainter(engine::UIPainter* painter)
        { mPainter = painter;}
        void SetPropertySelector(const std::string& prop)
        { mSelector = prop; }
        void RebuildMaterialCombos(const std::vector<app::ResourceListItem>& list);

        bool IsUnderEdit() const;

    signals:
        void StyleEdited();

    private slots:
        void on_widgetFontName_currentIndexChanged(int);
        void on_widgetFontSize_currentIndexChanged(int);
        void on_widgetTextVAlign_currentIndexChanged(int);
        void on_widgetTextHAlign_currentIndexChanged(int);
        void on_widgetTextBlink_stateChanged(int);
        void on_widgetTextUnderline_stateChanged(int);
        void on_widgetTextColor_colorChanged(QColor color);
        void on_widgetBackground_currentIndexChanged(int);
        void on_widgetBorder_currentIndexChanged(int);
        void on_btnSelectAppFont_clicked();
        void on_btnSelectCustomFont_clicked();
        void on_btnResetWidgetFontName_clicked();
        void on_btnResetWidgetFontSize_clicked();
        void on_btnResetWidgetTextVAlign_clicked();
        void on_btnResetWidgetTextHAlign_clicked();
        void on_btnResetWidgetTextProp_clicked();
        void on_btnResetWidgetBackground_clicked();
        void on_btnResetWidgetBorder_clicked();
        void on_btnResetWidgetTextColor_clicked();
        void on_btnSelectWidgetBackground_clicked();
        void on_btnSelectWidgetBorder_clicked();

        void SetBackgroundMaterial();
        void SetBackgroundColor();
        void SetBackgroundGradient();

        void SetBorderMaterial();
        void SetBorderColor();
        void SetBorderGradient();
    private:
        void UpdateCurrentWidgetProperties();
        void UpdateWidgetStyleString();
        void SetMaterialColor(const char* key);
        void SetMaterialGradient(const char* key);
    private:
        Ui::Style mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        uik::Widget* mWidget       = nullptr;
        engine::UIStyle* mStyle      = nullptr;
        engine::UIPainter* mPainter  = nullptr;
        std::string mSelector;
    };
} // namespace