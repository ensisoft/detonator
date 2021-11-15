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
#  include <QStringList>
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QDir>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/gui/dlgproject.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "graphics/device.h"

namespace gui
{

DlgProject::DlgProject(QWidget* parent, app::Workspace& workspace, app::Workspace::ProjectSettings& settings)
    : QDialog(parent)
    , mWorkspace(workspace)
    , mSettings(settings)
{
    const QString& library = mSettings.GetApplicationLibrary();
    mUI.setupUi(this);
    PopulateFromEnum<gfx::Device::MinFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::Device::MagFilter>(mUI.cmbMagFilter);
    PopulateFromEnum<app::Workspace::ProjectSettings::WindowMode>(mUI.cmbWindowMode);
    PopulateFromEnum<app::Workspace::ProjectSettings::MousePointerUnits>(mUI.mouseUnits);
    PopulateFromEnum<audio::SampleType>(mUI.audioFormat);
    PopulateFromEnum<audio::Channels>(mUI.audioChannels);
    SetList(mUI.mouseDrawable, workspace.ListCursors());
    SetList(mUI.mouseMaterial, workspace.ListAllMaterials());
    SetUIValue(mUI.edtAppIdentifier, mSettings.application_identifier);
    SetUIValue(mUI.cmbMSAA, mSettings.multisample_sample_count);
    SetUIValue(mUI.cmbMinFilter, mSettings.default_min_filter);
    SetUIValue(mUI.cmbMagFilter, mSettings.default_mag_filter);
    SetUIValue(mUI.cmbWindowMode, mSettings.window_mode);
    SetUIValue(mUI.wndWidth, mSettings.window_width);
    SetUIValue(mUI.wndHeight, mSettings.window_height);
    SetUIValue(mUI.chkWndCanResize, mSettings.window_can_resize);
    SetUIValue(mUI.chkWndHasBorder, mSettings.window_has_border);
    SetUIValue(mUI.chkSrgb, mSettings.config_srgb);
    SetUIValue(mUI.chkVsync, mSettings.window_vsync);
    SetUIValue(mUI.chkCursor, mSettings.window_cursor);
    SetUIValue(mUI.chkGrabMouse, mSettings.grab_mouse);
    SetUIValue(mUI.chkSaveGeom, mSettings.save_window_geometry);
    SetUIValue(mUI.edtAppName, mSettings.application_name);
    SetUIValue(mUI.edtGameScript, mSettings.game_script);
    SetUIValue(mUI.edtAppVersion, mSettings.application_version);
    SetUIValue(mUI.edtAppLibrary, library);
    SetUIValue(mUI.ticksPerSecond, mSettings.ticks_per_second);
    SetUIValue(mUI.updatesPerSecond, mSettings.updates_per_second);
    SetUIValue(mUI.edtWorkingFolder, mSettings.working_folder);
    SetUIValue(mUI.edtArguments, mSettings.command_line_arguments);
    SetUIValue(mUI.chkGameProcess, mSettings.use_gamehost_process);
    SetUIValue(mUI.numVeloIterations, mSettings.num_velocity_iterations);
    SetUIValue(mUI.numPosIterations, mSettings.num_position_iterations);
    SetUIValue(mUI.gravityX, mSettings.gravity.x);
    SetUIValue(mUI.gravityY, mSettings.gravity.y);
    SetUIValue(mUI.scaleX, mSettings.physics_scale.x);
    SetUIValue(mUI.scaleY, mSettings.physics_scale.y);
    SetUIValue(mUI.viewportWidth, mSettings.viewport_width);
    SetUIValue(mUI.viewportHeight, mSettings.viewport_height);
    SetUIValue(mUI.clearColor, mSettings.clear_color);
    SetUIValue(mUI.mouseDrawable, ListItemId(mSettings.mouse_pointer_drawable));
    SetUIValue(mUI.mouseMaterial, ListItemId(mSettings.mouse_pointer_material));
    SetUIValue(mUI.mouse, mSettings.mouse_pointer_visible);
    SetUIValue(mUI.hotspotX, mSettings.mouse_pointer_hotspot.x);
    SetUIValue(mUI.hotspotY, mSettings.mouse_pointer_hotspot.y);
    SetUIValue(mUI.mouseUnits, mSettings.mouse_pointer_units);
    SetUIValue(mUI.cursorWidth, mSettings.mouse_pointer_size.x);
    SetUIValue(mUI.cursorHeight, mSettings.mouse_pointer_size.y);
    SetUIValue(mUI.audioFormat, mSettings.audio_sample_type);
    SetUIValue(mUI.audioChannels, mSettings.audio_channels);
    SetUIValue(mUI.audioSampleRate, mSettings.audio_sample_rate);
    SetUIValue(mUI.audioBufferSize, mSettings.audio_buffer_size);
}

void DlgProject::on_btnAccept_clicked()
{
    GetUIValue(mUI.cmbMSAA, &mSettings.multisample_sample_count);
    GetUIValue(mUI.cmbMinFilter, &mSettings.default_min_filter);
    GetUIValue(mUI.cmbMagFilter, &mSettings.default_mag_filter);
    GetUIValue(mUI.wndWidth, &mSettings.window_width);
    GetUIValue(mUI.wndHeight, &mSettings.window_height);
    GetUIValue(mUI.cmbWindowMode, &mSettings.window_mode);
    GetUIValue(mUI.chkWndCanResize, &mSettings.window_can_resize);
    GetUIValue(mUI.chkWndHasBorder, &mSettings.window_has_border);
    GetUIValue(mUI.chkVsync, &mSettings.window_vsync);
    GetUIValue(mUI.chkSrgb, &mSettings.config_srgb);
    GetUIValue(mUI.chkCursor, &mSettings.window_cursor);
    GetUIValue(mUI.chkGrabMouse, &mSettings.grab_mouse);
    GetUIValue(mUI.chkSaveGeom, &mSettings.save_window_geometry);
    GetUIValue(mUI.edtAppName, &mSettings.application_name);
    GetUIValue(mUI.edtAppVersion, &mSettings.application_version);
    GetUIValue(mUI.edtGameScript, &mSettings.game_script);
    GetUIValue(mUI.ticksPerSecond, &mSettings.ticks_per_second);
    GetUIValue(mUI.updatesPerSecond, &mSettings.updates_per_second);
    GetUIValue(mUI.edtWorkingFolder, &mSettings.working_folder);
    GetUIValue(mUI.edtArguments, &mSettings.command_line_arguments);
    GetUIValue(mUI.chkGameProcess, &mSettings.use_gamehost_process);
    GetUIValue(mUI.numVeloIterations, &mSettings.num_velocity_iterations);
    GetUIValue(mUI.numPosIterations, &mSettings.num_position_iterations);
    GetUIValue(mUI.gravityX, &mSettings.gravity.x);
    GetUIValue(mUI.gravityY, &mSettings.gravity.y);
    GetUIValue(mUI.scaleX, &mSettings.physics_scale.x);
    GetUIValue(mUI.scaleY, &mSettings.physics_scale.y);
    GetUIValue(mUI.viewportWidth, &mSettings.viewport_width);
    GetUIValue(mUI.viewportHeight, &mSettings.viewport_height);
    GetUIValue(mUI.clearColor, &mSettings.clear_color);
    GetUIValue(mUI.mouse, &mSettings.mouse_pointer_visible);
    GetUIValue(mUI.hotspotX, &mSettings.mouse_pointer_hotspot.x);
    GetUIValue(mUI.hotspotY, &mSettings.mouse_pointer_hotspot.y);
    GetUIValue(mUI.mouseUnits, &mSettings.mouse_pointer_units);
    GetUIValue(mUI.cursorWidth,  &mSettings.mouse_pointer_size.x);
    GetUIValue(mUI.cursorHeight, &mSettings.mouse_pointer_size.y);
    GetUIValue(mUI.audioFormat, &mSettings.audio_sample_type);
    GetUIValue(mUI.audioChannels, &mSettings.audio_channels);
    GetUIValue(mUI.audioSampleRate, &mSettings.audio_sample_rate);
    GetUIValue(mUI.audioBufferSize, &mSettings.audio_buffer_size);
    mSettings.mouse_pointer_material = GetItemId(mUI.mouseMaterial);
    mSettings.mouse_pointer_drawable = GetItemId(mUI.mouseDrawable);

    QString library;
    GetUIValue(mUI.edtAppLibrary, &library);
    mSettings.SetApplicationLibrary(library);
    accept();
}
void DlgProject::on_btnCancel_clicked()
{
    reject();
}
void DlgProject::on_btnSelectEngine_clicked()
{
#if defined(POSIX_OS)
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Engine Library"), "", tr("Library files (*.so)"));
#elif defined(WINDOWS_OS)
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Engine Library"), "", tr("Library files (*.dll)"));
#endif
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace.MapFileToWorkspace(list[0]);
    SetValue(mUI.edtAppLibrary, file);
}

void DlgProject::on_btnResetClearColor_clicked()
{
    SetUIValue(mUI.clearColor, QColor(50, 77, 100, 255));
}

void DlgProject::on_btnSelectMaterial_clicked()
{
    DlgMaterial dlg(this, &mWorkspace, GetItemId(mUI.mouseMaterial));
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.mouseMaterial, ListItemId(dlg.GetSelectedMaterialId()));
}

} // namespace

