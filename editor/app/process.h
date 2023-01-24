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
#  include <QObject>
#  include <QString>
#  include <QProcess>
#  include <QByteArray>
#  include <QFile>
#  include <QStringList>
#  include <QTimer>
#include "warnpop.h"

#include <functional>

namespace app
{
    // This is a wrapper around QProcess and reads the data into
    // byte buffers for both stdout and stderr and then invokes
    // callbacks when it is able to extract lines of data from the
    // output.
    class Process : public QObject
    {
        Q_OBJECT

    public:
        enum class Error {
            None,
            FailedToStart,
            Crashed,
            Timedout,
            WriteError,
            ReadError,
            OtherError
        };

        Process();
       ~Process();

        // callback invoked when there's a new line extracted from stderror
        std::function<void (const QString& line)> onStdErr;

        // callback invoked when there's a new line extracted from stdout
        std::function<void (const QString& line)> onStdOut;

        // called on normal/abnormal process exit. i.e. when the process
        // exits normally or it has crashed or timed out.
        std::function<void ()> onFinished;

        // Start the given executable optionally setting the workingDir and writing
        // both the stdout and stderr into logfile (if given).
        void Start(const QString& executable,
                   const QStringList& args,
                   const QString& logFile,
                   const QString& workingDir);

        // this will kill the currently running process (if any)
        // when the process is killed no more callbacks are called
        // to indicate new stdout or stderr data or process completion.
        void Kill();

        // returns true if the process is currently running or not.
        bool IsRunning() const
        {
            const auto state = mProcess.state();
            if (state == QProcess::NotRunning)
                return false;
            return true;
        }

        Error GetError() const
        { return mError; }

        void EnableTimeout(bool on_off)
        { mEnableTimeout = on_off; }

        // run the given executable and capture the output
        // into stdout and stderr as a series of text lines.
        // this function will block until the executable
        // has finished running (either completed successfully
        // or encountered an error) so use cautiously.
        static bool RunAndCapture(const QString& executable,
            const QString& working_dir,
            const QStringList& args,
            QStringList* stdout_buffer,
            QStringList* stderr_buffer,
            Error* error_code = nullptr);
    private slots:
        void ProcessStdOut();
        void ProcessStdErr();
        void ProcessFinished(int exitCode, QProcess::ExitStatus status);
        void ProcessError(QProcess::ProcessError error);
        void ProcessState(QProcess::ProcessState state);
        void ProcessTimeout();

    private:
        void ParseStdOut();
        void ParseStdErr();

    private:
        QTimer  mTimeout;
        QString mExecutable;
        QString mWorkingDir;
        QProcess mProcess;
        QByteArray mStdOut;
        QByteArray mStdErr;
        QFile mLogFile;
        Error mError = Error::None;
        bool mKilled = false;
        bool mEnableTimeout = false;
    private:
    };
} // namespace
