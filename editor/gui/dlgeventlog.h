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
#  include "ui_dlgeventlog.h"
#  include <QTimer>
#  include <QDialog>
#  include <QString>
#include "warnpop.h"

#include "wdk/events.h"
#include "wdk/listener.h"

namespace app {
    class WindowEventLog;
    class EventLogPlayer;
    class EventLogRecorder;
} // app

namespace gui
{
    class DlgEventLog : public QDialog
    {
        Q_OBJECT

    public:
        DlgEventLog(QWidget* parent);
       ~DlgEventLog();

        void Replay(wdk::WindowListener& listener, double time);

        void RecordEvent(const wdk::WindowEventKeyDown& key, double time);
        void RecordEvent(const wdk::WindowEventKeyUp& key, double time);
        void RecordEvent(const wdk::WindowEventMouseMove& mickey, double time);
        void RecordEvent(const wdk::WindowEventMousePress& mickey, double time);
        void RecordEvent(const wdk::WindowEventMouseRelease& mickey, double time);

        void SetTime(double time)
        { mCurrentTime = time; }
        bool IsClosed() const
        { return mClosed; }
        bool IsRecording() const
        { return !!mRecorder; }
        bool IsPlaying() const
        { return !!mReplay; }
    private:
        unsigned GetTime(double time);
    private slots:
        void on_btnPlay_clicked();
        void on_btnRecord_clicked();
        void on_btnStop_clicked();
        void on_btnOpen_clicked();
        void on_btnSave_clicked();
        void on_btnSaveAs_clicked();
        void on_btnClose_clicked();
    private:
        class TableModel;

        Ui::Dialog mUI;
    private:
        bool mClosed = false;
        std::unique_ptr<TableModel> mTableModel;
        std::unique_ptr<app::WindowEventLog> mLog;
        std::unique_ptr<app::EventLogPlayer> mReplay;
        std::unique_ptr<app::EventLogRecorder> mRecorder;
        QString mFileName;
        double mCurrentTime = 0;
    };
} // namespace
