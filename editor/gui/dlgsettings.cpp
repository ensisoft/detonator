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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QApplication>
#  include <QStringList>
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QDir>
#  include <QStyle>
#  include <QStyleFactory>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/gui/dlgsettings.h"
#include "editor/gui/utility.h"

namespace gui
{

DlgSettings::DlgSettings(QWidget* parent, AppSettings& settings,
    TextEditor::Settings& editor,
    ScriptWidget::Settings& script,
    MainWidget::UISettings& widget)
    : QDialog(parent)
    , mSettings(settings)
    , mEditorSettings(editor)
    , mScriptSettings(script)
    , mWidgetSettings(widget)
{
    mUI.setupUi(this);
    PopulateFromEnum<MainWidget::GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<GfxWindow::MouseCursor>(mUI.cmbMouseCursor);

    SetUIValue(mUI.edtImageEditorExecutable,  settings.image_editor_executable);
    SetUIValue(mUI.edtImageEditorArguments,   settings.image_editor_arguments);
    SetUIValue(mUI.edtShaderEditorExecutable, settings.shader_editor_executable);
    SetUIValue(mUI.edtShaderEditorArguments,  settings.shader_editor_arguments);
    SetUIValue(mUI.edtScriptEditorExecutable, settings.script_editor_executable);
    SetUIValue(mUI.edtScriptEditorArguments,  settings.script_editor_arguments);
    SetUIValue(mUI.edtAudioEditorExecutable,  settings.audio_editor_executable);
    SetUIValue(mUI.edtAudioEditorArguments,   settings.audio_editor_arguments);
    SetUIValue(mUI.cmbWinOrTab,               settings.default_open_win_or_tab);
    SetUIValue(mUI.spinFrameDelay,            settings.frame_delay);
    SetUIValue(mUI.cmbMouseCursor,            settings.mouse_cursor);
    SetUIValue(mUI.chkSaveAutomatically,      settings.save_automatically_on_play);
    SetUIValue(mUI.chkVSYNC,                  settings.vsync);
    SetUIValue(mUI.edtPythonExecutable,       settings.python_executable);
    SetUIValue(mUI.edtEmscriptenPath,         settings.emsdk);
    SetUIValue(mUI.clearColor,                settings.clear_color);
    SetUIValue(mUI.gridColor,                 settings.grid_color);
    SetUIValue(mUI.cmbGrid,                   widget.grid);
    SetUIValue(mUI.zoom,                      widget.zoom);
    SetUIValue(mUI.chkShowGrid,               widget.show_grid);
    SetUIValue(mUI.chkShowOrigin,             widget.show_origin);
    SetUIValue(mUI.chkShowViewport,           widget.show_viewport);
    SetUIValue(mUI.chkSnapToGrid,             widget.snap_to_grid);
    SetUIValue(mUI.edtLuaFormatterExec,       mScriptSettings.lua_formatter_exec);
    SetUIValue(mUI.edtLuaFormatterArgs,       mScriptSettings.lua_formatter_args);
    SetUIValue(mUI.editorFormatOnSave,        mScriptSettings.lua_format_on_save);

    // add Qt's built-in / plugin styles.
    const auto& styles = QStyleFactory::keys();
    for (const auto& style : styles)
    {
        mUI.cmbStyle->addItem(style);
    }
    SetValue(mUI.cmbStyle, settings.style_name);

    mUI.editorTheme->addItem("Monokai");
    QFont font;
    if (font.fromString(mEditorSettings.font_description)) {
        QSignalBlocker b(mUI.editorFont);
        mUI.editorFont->setCurrentFont(font);
    }

    const auto font_sizes = QFontDatabase::standardSizes();
    for (int size : font_sizes)
        mUI.editorFontSize->addItem(QString::number(size));
    SetValue(mUI.editorFontSize,               QString::number(mEditorSettings.font_size));
    SetValue(mUI.editorTheme,                 mEditorSettings.theme);
    SetValue(mUI.editorShowLineNumbers,       mEditorSettings.show_line_numbers);
    SetValue(mUI.editorHightlightCurrentLine, mEditorSettings.highlight_current_line);
    SetValue(mUI.editorHightlightSyntax,      mEditorSettings.highlight_syntax);
    SetValue(mUI.editorInsertSpaces,          mEditorSettings.insert_spaces);
    SetValue(mUI.editorShowLineNumbers,       mEditorSettings.show_line_numbers);
}

void DlgSettings::on_btnAccept_clicked()
{
    GetUIValue(mUI.edtImageEditorExecutable,  &mSettings.image_editor_executable);
    GetUIValue(mUI.edtImageEditorArguments,   &mSettings.image_editor_arguments);
    GetUIValue(mUI.edtShaderEditorExecutable, &mSettings.shader_editor_executable);
    GetUIValue(mUI.edtShaderEditorArguments,  &mSettings.shader_editor_arguments);
    GetUIValue(mUI.edtScriptEditorExecutable, &mSettings.script_editor_executable);
    GetUIValue(mUI.edtScriptEditorArguments,  &mSettings.script_editor_arguments);
    GetUIValue(mUI.edtAudioEditorExecutable,  &mSettings.audio_editor_executable);
    GetUIValue(mUI.edtAudioEditorArguments,   &mSettings.audio_editor_arguments);
    GetUIValue(mUI.cmbWinOrTab,               &mSettings.default_open_win_or_tab);
    GetUIValue(mUI.cmbStyle,                  &mSettings.style_name);
    GetUIValue(mUI.chkSaveAutomatically,      &mSettings.save_automatically_on_play);
    GetUIValue(mUI.spinFrameDelay,            &mSettings.frame_delay);
    GetUIValue(mUI.cmbMouseCursor,            &mSettings.mouse_cursor);
    GetUIValue(mUI.chkVSYNC,                  &mSettings.vsync);
    GetUIValue(mUI.edtPythonExecutable,       &mSettings.python_executable);
    GetUIValue(mUI.edtEmscriptenPath,         &mSettings.emsdk);
    GetUIValue(mUI.clearColor,                &mSettings.clear_color);
    GetUIValue(mUI.gridColor,                 &mSettings.grid_color);
    // text editor settings.
    GetUIValue(mUI.editorTheme,                 &mEditorSettings.theme);
    GetUIValue(mUI.editorShowLineNumbers,       &mEditorSettings.show_line_numbers);
    GetUIValue(mUI.editorHightlightCurrentLine, &mEditorSettings.highlight_current_line);
    GetUIValue(mUI.editorHightlightSyntax,      &mEditorSettings.highlight_syntax);
    GetUIValue(mUI.editorInsertSpaces,          &mEditorSettings.insert_spaces);
    GetUIValue(mUI.editorShowLineNumbers,       &mEditorSettings.show_line_numbers);
    GetUIValue(mUI.editorFontSize,              &mEditorSettings.font_size);
    mEditorSettings.font_description = mUI.editorFont->currentFont().toString();

    GetUIValue(mUI.cmbGrid,         &mWidgetSettings.grid);
    GetUIValue(mUI.zoom,            &mWidgetSettings.zoom);
    GetUIValue(mUI.chkShowGrid,     &mWidgetSettings.show_grid);
    GetUIValue(mUI.chkShowOrigin,   &mWidgetSettings.show_origin);
    GetUIValue(mUI.chkShowViewport, &mWidgetSettings.show_viewport);
    GetUIValue(mUI.chkSnapToGrid,   &mWidgetSettings.snap_to_grid);

    GetUIValue(mUI.edtLuaFormatterExec, &mScriptSettings.lua_formatter_exec);
    GetUIValue(mUI.edtLuaFormatterArgs, &mScriptSettings.lua_formatter_args);
    GetUIValue(mUI.editorFormatOnSave,  &mScriptSettings.lua_format_on_save);

    accept();
}
void DlgSettings::on_btnCancel_clicked()
{
    reject();
}

void DlgSettings::on_btnSelectImageEditor_clicked()
{
    QString filter;

    #if defined(WINDOWS_OS)
        filter = "Executables (*.exe)";
    #endif

    const QString& executable = QFileDialog::getOpenFileName(this,
        tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtImageEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtImageEditorExecutable->setCursorPosition(0);
}

void DlgSettings::on_btnSelectShaderEditor_clicked()
{
    QString filter;

    #if defined(WINDOWS_OS)
        filter = "Executables (*.exe)";
    #endif

    const QString& executable = QFileDialog::getOpenFileName(this,
        tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtShaderEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtShaderEditorExecutable->setCursorPosition(0);
}

void DlgSettings::on_btnSelectScriptEditor_clicked()
{
    QString filter;

#if defined(WINDOWS_OS)
    filter = "Executables (*.exe)";
#endif

    const QString& executable = QFileDialog::getOpenFileName(this,
      tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtScriptEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtScriptEditorExecutable->setCursorPosition(0);
}

void DlgSettings::on_btnSelectAudioEditor_clicked()
{
    QString filter;

#if defined(WINDOWS_OS)
    filter = "Executables (*.exe)";
#endif

    const QString& executable = QFileDialog::getOpenFileName(this,
      tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtAudioEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtAudioEditorExecutable->setCursorPosition(0);
}

void DlgSettings::on_btnSelectPython_clicked()
{
    QString filter;

#if defined(WINDOWS_OS)
    filter = "Python (python.exe)";
#else
    filter = "Python (python)";
#endif

    const QString& executable = QFileDialog::getOpenFileName(this,
        tr("Select Python Executable"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtPythonExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtPythonExecutable->setCursorPosition(0);
}
void DlgSettings::on_btnSelectEmsdk_clicked()
{
    const QString& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Emsdk folder"), QString());
    if (dir.isEmpty())
        return;

    mUI.edtEmscriptenPath->setText(QDir::toNativeSeparators(dir));
    mUI.edtEmscriptenPath->setCursorPosition(0);
}

void DlgSettings::on_btnResetClearColor_clicked()
{
    constexpr QColor color = {0x23, 0x23, 0x23, 255};
    SetUIValue(mUI.clearColor, color);
}

void DlgSettings::on_btnResetGridColor_clicked()
{
    constexpr QColor color = {0xe3, 0xe3, 0xe3, 50};
    SetUIValue(mUI.gridColor, color);
}

} // namespace

