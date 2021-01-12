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

DlgSettings::DlgSettings(QWidget* parent, AppSettings& settings, TextEditor::Settings& editor)
    : QDialog(parent)
    , mSettings(settings)
    , mEditorSettings(editor)
{
    mUI.setupUi(this);
    SetUIValue(mUI.edtImageEditorExecutable, settings.image_editor_executable);
    SetUIValue(mUI.edtImageEditorArguments, settings.image_editor_arguments);
    SetUIValue(mUI.edtShaderEditorExecutable, settings.shader_editor_executable);
    SetUIValue(mUI.edtShaderEditorArguments, settings.shader_editor_arguments);
    SetUIValue(mUI.edtScriptEditorExecutable, settings.script_editor_executable);
    SetUIValue(mUI.edtScriptEditorArguments, settings.script_editor_arguments);
    SetUIValue(mUI.cmbWinOrTab, settings.default_open_win_or_tab);

    // add our own style.
    mUI.cmbStyle->addItem(GAMESTUDIO_DEFAULT_STYLE_NAME);

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
    SetValue(mUI.editorFontSize, QString::number(mEditorSettings.font_size));
    SetValue(mUI.editorTheme, mEditorSettings.theme);
    SetValue(mUI.editorShowLineNumbers, mEditorSettings.show_line_numbers);
    SetValue(mUI.editorHightlightCurrentLine, mEditorSettings.highlight_current_line);
    SetValue(mUI.editorHightlightSyntax, mEditorSettings.highlight_syntax);
    SetValue(mUI.editorInsertSpaces, mEditorSettings.insert_spaces);
    SetValue(mUI.editorShowLineNumbers, mEditorSettings.show_line_numbers);
}

void DlgSettings::on_btnAccept_clicked()
{
    GetUIValue(mUI.edtImageEditorExecutable,  &mSettings.image_editor_executable);
    GetUIValue(mUI.edtImageEditorArguments,   &mSettings.image_editor_arguments);
    GetUIValue(mUI.edtShaderEditorExecutable, &mSettings.shader_editor_executable);
    GetUIValue(mUI.edtShaderEditorArguments,  &mSettings.shader_editor_arguments);
    GetUIValue(mUI.edtScriptEditorExecutable, &mSettings.script_editor_executable);
    GetUIValue(mUI.edtScriptEditorArguments,  &mSettings.script_editor_arguments);
    GetUIValue(mUI.cmbWinOrTab, &mSettings.default_open_win_or_tab);
    GetUIValue(mUI.cmbStyle, &mSettings.style_name);
    // text editor settings.
    GetUIValue(mUI.editorTheme,                 &mEditorSettings.theme);
    GetUIValue(mUI.editorShowLineNumbers,       &mEditorSettings.show_line_numbers);
    GetUIValue(mUI.editorHightlightCurrentLine, &mEditorSettings.highlight_current_line);
    GetUIValue(mUI.editorHightlightSyntax,      &mEditorSettings.highlight_syntax);
    GetUIValue(mUI.editorInsertSpaces,          &mEditorSettings.insert_spaces);
    GetUIValue(mUI.editorShowLineNumbers,       &mEditorSettings.show_line_numbers);
    GetUIValue(mUI.editorFontSize,              &mEditorSettings.font_size);
    mEditorSettings.font_description = mUI.editorFont->currentFont().toString();
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

} // namespace

