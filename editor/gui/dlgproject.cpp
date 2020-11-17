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
#  include <QStringList>
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QDir>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/gui/dlgproject.h"
#include "editor/gui/utility.h"
#include "graphics/device.h"

namespace gui
{

DlgProject::DlgProject(QWidget* parent, app::Workspace::ProjectSettings& settings)
    : QDialog(parent)
    , mSettings(settings)
{
    mUI.setupUi(this);
    PopulateFromEnum<gfx::Device::MinFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::Device::MagFilter>(mUI.cmbMagFilter);
    SetUIValue(mUI.cmbMSAA, mSettings.multisample_sample_count);
    SetUIValue(mUI.cmbMinFilter, mSettings.default_min_filter);
    SetUIValue(mUI.cmbMagFilter, mSettings.default_mag_filter);
    SetUIValue(mUI.wndWidth, mSettings.window_width);
    SetUIValue(mUI.wndHeight, mSettings.window_height);
    SetUIValue(mUI.chkWndIsFullscreen, mSettings.window_set_fullscreen);
    SetUIValue(mUI.chkWndCanResize, mSettings.window_can_resize);
    SetUIValue(mUI.chkWndHasBorder, mSettings.window_has_border);
    SetUIValue(mUI.edtAppName, mSettings.application_name);
    SetUIValue(mUI.edtAppVersion, mSettings.application_version);
    SetUIValue(mUI.edtAppLibrary, mSettings.application_library);
    SetUIValue(mUI.ticksPerSecond, mSettings.ticks_per_second);
    SetUIValue(mUI.updatesPerSecond, mSettings.updates_per_second);
    SetUIValue(mUI.edtWorkingFolder, mSettings.working_folder);
    SetUIValue(mUI.edtArguments, mSettings.command_line_arguments);
    SetUIValue(mUI.chkGameProcess, mSettings.use_gamehost_process);
}

void DlgProject::on_btnAccept_clicked()
{
    GetUIValue(mUI.cmbMSAA, &mSettings.multisample_sample_count);
    GetUIValue(mUI.cmbMinFilter, &mSettings.default_min_filter);
    GetUIValue(mUI.cmbMagFilter, &mSettings.default_mag_filter);
    GetUIValue(mUI.wndWidth, &mSettings.window_width);
    GetUIValue(mUI.wndHeight, &mSettings.window_height);
    GetUIValue(mUI.chkWndIsFullscreen, &mSettings.window_set_fullscreen);
    GetUIValue(mUI.chkWndCanResize, &mSettings.window_can_resize);
    GetUIValue(mUI.chkWndHasBorder, &mSettings.window_has_border);
    GetUIValue(mUI.edtAppName, &mSettings.application_name);
    GetUIValue(mUI.edtAppVersion, &mSettings.application_version);
    GetUIValue(mUI.edtAppLibrary, &mSettings.application_library);
    GetUIValue(mUI.ticksPerSecond, &mSettings.ticks_per_second);
    GetUIValue(mUI.updatesPerSecond, &mSettings.updates_per_second);
    GetUIValue(mUI.edtWorkingFolder, &mSettings.working_folder);
    GetUIValue(mUI.edtArguments, &mSettings.command_line_arguments);
    GetUIValue(mUI.chkGameProcess, &mSettings.use_gamehost_process);
    accept();
}
void DlgProject::on_btnCancel_clicked()
{
    reject();
}

} // namespace

