// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// This software is copyrighted software. Unauthorized hacking, cracking, distribution
// and general assing around is prohibited.
// Redistribution and use in source and binary forms, with or without modification,
// without permission are prohibited.
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

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
            QStringList& stdoutLines,
            QStringList& stderrLines,
            const QStringList& args = QStringList());

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
