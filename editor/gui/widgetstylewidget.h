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
#  include <QWidget>
#  include "ui_widgetstylewidget.h"
#include "warnpop.h"

#include <string>

namespace uik {
    class Widget;
} // namespace

namespace game {
    class UIStyle;
    class UIPainter;
}// namespace

namespace gui
{
    struct ListItem;

    class WidgetStyleWidget : public QWidget
    {
        Q_OBJECT

    public:
        WidgetStyleWidget(QWidget* parent);

        void SetWidget(uik::Widget* widget);

        void SetWorkspace(app::Workspace* workspace)
        { mWorkspace = workspace;}
        void SetStyle(game::UIStyle* style)
        { mStyle = style; }
        void SetPainter(game::UIPainter* painter)
        { mPainter = painter;}
        void SetPropertySelector(const std::string& prop)
        { mSelector = prop; }
        void RebuildMaterialCombos(const std::vector<ListItem>& list);

        bool IsUnderEdit() const;

    private slots:
        void on_widgetFontName_currentIndexChanged(int);
        void on_widgetFontSize_currentIndexChanged(int);
        void on_widgetTextVAlign_currentIndexChanged(int);
        void on_widgetTextHAlign_currentIndexChanged(int);
        void on_widgetTextColorEnable_stateChanged(int);
        void on_widgetTextBlink_stateChanged(int);
        void on_widgetTextUnderline_stateChanged(int);
        void on_widgetTextColor_colorChanged(QColor color);
        void on_widgetBackground_currentIndexChanged(int);
        void on_widgetBorder_currentIndexChanged(int);
        void on_btnResetWidgetFontName_clicked();
        void on_btnResetWidgetFontSize_clicked();
        void on_btnResetWidgetTextVAlign_clicked();
        void on_btnResetWidgetTextHAlign_clicked();
        void on_btnResetWidgetTextProp_clicked();
        void on_btnResetWidgetBackground_clicked();
        void on_btnResetWidgetBorder_clicked();
        void on_btnSelectWidgetBackground_clicked();
        void on_btnSelectWidgetBorder_clicked();
    private:
        void UpdateCurrentWidgetProperties();
    private:
        Ui::Style mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        uik::Widget* mWidget       = nullptr;
        game::UIStyle* mStyle      = nullptr;
        game::UIPainter* mPainter  = nullptr;
        std::string mSelector;
    };
} // namespace