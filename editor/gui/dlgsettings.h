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
#  include "ui_dlgsettings.h"
#  include <QDialog>
#  include <QTextDocument>
#include "warnpop.h"

#include "editor/app/code-tools.h"
#include "editor/gui/appsettings.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/mainwidget.h"

namespace gui
{
    class DlgSettings :  public QDialog
    {
        Q_OBJECT
    public:
        DlgSettings(QWidget* parent, AppSettings& settings,
                    ScriptWidget::Settings& script,
                    MainWidget::UISettings& widget);

    private:
        void UpdateSampleCode();
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnSelectImageEditor_clicked();
        void on_btnSelectShaderEditor_clicked();
        void on_btnSelectScriptEditor_clicked();
        void on_btnSelectAudioEditor_clicked();
        void on_btnSelectPython_clicked();
        void on_btnSelectEmsdk_clicked();
        void on_btnResetClearColor_clicked();
        void on_btnResetGridColor_clicked();

        void on_editorFontName_currentIndexChanged(int);
        void on_editorFontSize_currentIndexChanged(int);
        void on_editorHightlightSyntax_stateChanged(int);
        void on_editorHightlightCurrentLine_stateChanged(int);
        void on_editorShowLineNumbers_stateChanged(int);
        void on_editorInsertSpaces_stateChanged(int);

    private:
        Ui::DlgSettings mUI;
    private:
        AppSettings& mSettings;
        ScriptWidget::Settings& mScriptSettings;
        MainWidget::UISettings& mWidgetSettings;

    private:
        QTextDocument mSampleCode;
        app::CodeAssistant mAssistant;
    };
} // namespace


