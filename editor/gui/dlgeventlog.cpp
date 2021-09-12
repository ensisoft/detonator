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

#include "config.h"

#include "warnpush.h"
#  include <QAbstractTableModel>
#  include <QFileDialog>
#include "warnpop.h"

#include "data/json.h"
#include "editor/app/window-eventlog.h"
#include "editor/gui/dlgeventlog.h"
#include "editor/gui/utility.h"

namespace gui
{

class DlgEventLog::TableModel : public QAbstractTableModel
{
public:
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::SizeHintRole)
            return QSize(0, 16);
        else if (role == Qt::DisplayRole && mLog)
        {
            if (index.column() == 0)
                return app::toString(mLog->GetEventTime(index.row()));
            else if (index.column() == 1)
                return app::toString(mLog->GetEventType(index.row()));
            else if (index.column() == 2)
                return app::toString(mLog->GetEventDesc(index.row()));
        }
        else if (role == Qt::DecorationRole && index.column() == 0)
        {
            // todo:
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0)
                return "Time";
            else if (section == 1)
                return "Type";
            else if (section == 2)
                return "Description";
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    { return mLog ? mLog->GetNumEvents() : 0; }
    virtual int columnCount(const QModelIndex&) const override
    { return 3; }

    template<typename Event>
    void RecordEvent(const Event& event, app::EventLogRecorder& recorder,  unsigned millis)
    {
        QAbstractTableModel::beginInsertRows(QModelIndex(),
           static_cast<int>(mLog->GetNumEvents()),
           static_cast<int>(mLog->GetNumEvents()));
        recorder.RecordEvent(event, millis);
        QAbstractTableModel::endInsertRows();
    }
    void SetLog(app::WindowEventLog* log)
    {
        QAbstractTableModel::beginResetModel();
        mLog = log;
        QAbstractTableModel::endResetModel();
    }
private:
    app::WindowEventLog* mLog = nullptr;
    app::WindowEventLog::event_time_t mFirstTime = 0;
};

DlgEventLog::DlgEventLog(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    mTableModel.reset(new TableModel);
    mUI.tableView->setModel(mTableModel.get());

    PopulateFromEnum<app::WindowEventLog::TimeMode>(mUI.cmbTimeMode);
    SetValue(mUI.cmbTimeMode, app::WindowEventLog::TimeMode::Relative);
    SetEnabled(mUI.btnClose, true);
    SetEnabled(mUI.btnPlay, false);
    SetEnabled(mUI.btnStop, false);
    SetEnabled(mUI.btnOpen, true);
    SetEnabled(mUI.btnSave, false);
    SetEnabled(mUI.btnSaveAs, false);
    SetVisible(mUI.progressBar, false);
}

DlgEventLog::~DlgEventLog() = default;

void DlgEventLog::Replay(wdk::WindowListener& listener, double time)
{
    if (!mReplay)
        return;

    mReplay->Apply(listener, time * 1000);

    if (mReplay->IsDone())
    {
        SetVisible(mUI.progressBar, false);
        SetEnabled(mUI.btnPlay, true);
        SetEnabled(mUI.btnRecord, true);
        SetEnabled(mUI.btnStop, false);
        SetEnabled(mUI.btnOpen, true);
        SetEnabled(mUI.btnSaveAs, true);
        SetEnabled(mUI.btnSave, !mFileName.isEmpty());
    }
    else
    {
        mUI.progressBar->setValue(mReplay->GetCurrentIndex());
    }
}

void DlgEventLog::RecordEvent(const wdk::WindowEventKeyDown& key, double time)
{
    if (!IsRecording())
        return;

    if (GetValue(mUI.chkKeyDown))
        mTableModel->RecordEvent(key, *mRecorder, GetTime(time));
}
void DlgEventLog::RecordEvent(const wdk::WindowEventKeyUp& key, double time)
{
    if (!IsRecording())
        return;

    if (GetValue(mUI.chkKeyUp))
        mTableModel->RecordEvent(key, *mRecorder, GetTime(time));
}
void DlgEventLog::RecordEvent(const wdk::WindowEventMouseMove& mickey, double time)
{
    if (!IsRecording())
        return;

    if (GetValue(mUI.chkMouseMove))
        mTableModel->RecordEvent(mickey, *mRecorder, GetTime(time));
}
void DlgEventLog::RecordEvent(const wdk::WindowEventMousePress& mickey, double time)
{
    if (!IsRecording())
        return;

    if (GetValue(mUI.chkMousePress))
        mTableModel->RecordEvent(mickey, *mRecorder, GetTime(time));
}
void DlgEventLog::RecordEvent(const wdk::WindowEventMouseRelease& mickey, double time)
{
    if (!IsRecording())
        return;

    if (GetValue(mUI.chkMouseRelease))
        mTableModel->RecordEvent(mickey, *mRecorder, GetTime(time));
}

unsigned DlgEventLog::GetTime(double time)
{
    // convert to milliseconds.
    return time * 1000.0;
}

void DlgEventLog::on_btnPlay_clicked()
{
    ASSERT(mLog && !mLog->IsEmpty());

    mReplay = std::make_unique<app::EventLogPlayer>(mLog.get());
    mReplay->Start(mCurrentTime * 1000);
    mRecorder.reset();

    mUI.progressBar->setMinimum(0);
    mUI.progressBar->setMaximum(mLog->GetNumEvents());
    mUI.progressBar->setValue(0);
    mUI.progressBar->setFormat("%p%");
    SetVisible(mUI.progressBar, true);
    SetEnabled(mUI.btnPlay, false);
    SetEnabled(mUI.btnStop, true);
    SetEnabled(mUI.btnRecord, false);
    SetEnabled(mUI.btnSaveAs, false);
    SetEnabled(mUI.btnSave, false);
    SetEnabled(mUI.btnOpen, false);

}

void DlgEventLog::on_btnRecord_clicked()
{
    mLog = std::make_unique<app::WindowEventLog>();
    mLog->SetTimeMode(GetValue(mUI.cmbTimeMode));
    mTableModel->SetLog(mLog.get());
    mRecorder = std::make_unique<app::EventLogRecorder>(mLog.get());
    mRecorder->Start(mCurrentTime * 1000.0);
    mReplay.reset();

    mUI.progressBar->setMaximum(0);
    mUI.progressBar->setMinimum(0);
    mUI.progressBar->setFormat("Recording ...");
    SetVisible(mUI.progressBar, true);
    SetEnabled(mUI.btnPlay, false);
    SetEnabled(mUI.btnRecord, false);
    SetEnabled(mUI.btnStop, true);
    SetEnabled(mUI.btnOpen, false);
    SetEnabled(mUI.btnSave, false);
    SetEnabled(mUI.btnSaveAs, false);
    SetEnabled(mUI.btnClose, true);
    SetEnabled(mUI.cmbTimeMode, false);
}
void DlgEventLog::on_btnStop_clicked()
{
    mReplay.reset();
    mRecorder.reset();
    SetVisible(mUI.progressBar, false);

    SetEnabled(mUI.btnStop, false);
    SetEnabled(mUI.btnPlay, false);
    SetEnabled(mUI.btnOpen, true);
    SetEnabled(mUI.btnRecord, true);
    SetEnabled(mUI.cmbTimeMode, true);
    if (mLog && !mLog->IsEmpty())
    {
        SetEnabled(mUI.btnPlay, true);
        SetEnabled(mUI.btnSaveAs, true);
        SetEnabled(mUI.btnSave, !mFileName.isEmpty());
    }
}
void DlgEventLog::on_btnOpen_clicked()
{
    const auto& ret = QFileDialog::getOpenFileName(this,
        tr("Select Log File"), QString(),
        tr("Log (*.json)"));
    if (ret.isEmpty())
        return;

    data::JsonFile file;
    auto [success, error] = file.Load(app::ToUtf8(ret));
    if (!success)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Close);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to load JSON file.\n%1").arg(app::FromUtf8(error)));
        msg.exec();
        return;
    }
    auto trace = std::make_unique<app::WindowEventLog>();
    if (!trace->FromJson(file.GetRootObject()))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Close);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to parse JSON file.\n"));
        msg.exec();
        return;
    }
    mLog = std::move(trace);
    mTableModel->SetLog(mLog.get());

    SetValue(mUI.trace, tr("Event List %1").arg(mFileName));
    SetEnabled(mUI.btnSave, true);
    SetEnabled(mUI.btnSaveAs, true);
    SetEnabled(mUI.btnPlay, !mLog->IsEmpty());
    mFileName = ret;
}
void DlgEventLog::on_btnSave_clicked()
{
    ASSERT(mLog && !mLog->IsEmpty());
    ASSERT(!mFileName.isEmpty());

    data::JsonObject json;
    data::JsonFile file;
    file.SetRootObject(json);
    mLog->IntoJson(json);

    auto [success, error] = file.Save(app::ToUtf8(mFileName));
    if (!success)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Close);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to save JSON file.\n%1").arg(app::FromUtf8(error)));
        msg.exec();
        return;
    }
}

void DlgEventLog::on_btnSaveAs_clicked()
{
    ASSERT(mLog && !mLog->IsEmpty());

    const auto& ret = QFileDialog::getSaveFileName(this,
        tr("Save Log as Json"),
        tr("event-log.json"),
        tr("JSON (*.json)"));
    if (ret.isEmpty())
        return;

    data::JsonObject json;
    data::JsonFile file;
    file.SetRootObject(json);
    mLog->IntoJson(json);

    auto [success, error] = file.Save(app::ToUtf8(ret));
    if (!success)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Close);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to save JSON file.\n%1").arg(app::FromUtf8(error)));
        msg.exec();
        return;
    }
    mFileName = ret;
    SetEnabled(mUI.btnSave, true);
    SetValue(mUI.trace, tr("Event List %1").arg(mFileName));
}
void DlgEventLog::on_btnClose_clicked()
{
    mClosed = true;
}

} // namespace