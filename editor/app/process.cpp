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

#define LOGTAG "process"

#include "config.h"

#include "warnpush.h"
#  include <QTextStream>
#include "warnpop.h"

#include <cctype>
#include <string>

#include "base/assert.h"
#include "editor/app/eventlog.h"
#include "editor/app/format.h"
#include "editor/app/process.h"

namespace {

void ParseLines(QByteArray& buff, QStringList& lines)
{
    std::string temp;

    for (int i=0; i<buff.size(); ++i)
    {
        const auto byte = buff.at(i);
        if (byte == '\n' || byte == '\r')
        {
            const auto line = QString::fromStdString(temp);
            lines << line;
            temp.clear();
        }
        else if (byte == '\b')
        {
            temp.push_back('.');
        }
        else if (!std::iscntrl((unsigned)byte))
        {
            temp.push_back(byte);
        }
    }
    const auto leftOvers = temp.size();
    buff = buff.right(leftOvers);
}

} // namespace

namespace app
{

Process::Process()
{
    connect(&mProcess, (void (QProcess::*)(int,QProcess::ExitStatus))&QProcess::finished, this, &Process::ProcessFinished);
    connect(&mProcess, &QProcess::errorOccurred, this, &Process::ProcessError);
    connect(&mProcess, &QProcess::stateChanged, this, &Process::ProcessState);
    connect(&mProcess, &QProcess::readyReadStandardOutput, this, &Process::ProcessStdOut);
    connect(&mProcess, &QProcess::readyReadStandardError, this, &Process::ProcessStdErr);
    connect(&mTimeout, &QTimer::timeout, this, &Process::ProcessTimeout);
}

Process::~Process()
{
    const auto state = mProcess.state();
    ASSERT(state == QProcess::NotRunning
        && "Process is still running. It should be either stopped or waited to complete.");
    Q_UNUSED(state);
}

void Process::Start(const QString& executable,
                    const QStringList& args,
                    const QString& logFile,
                    const QString& workingDir)
{
    ASSERT(mProcess.state() == QProcess::NotRunning);
    ASSERT(!mLogFile.isOpen());

    mStdOut.clear();
    mStdErr.clear();
    mError = Error::None;
    if (!logFile.isEmpty())
    {
        mLogFile.setFileName(logFile);
        mLogFile.open(QIODevice::Truncate | QIODevice::WriteOnly);
        if (!mLogFile.isOpen())
        {
            WARN("Unable to write log file %1, %2", logFile, mLogFile.error());
        }
    }

    mExecutable = executable;
    mWorkingDir = workingDir;

    if (!workingDir.isEmpty())
        mProcess.setWorkingDirectory(workingDir);
    mProcess.blockSignals(false);
    mProcess.setProcessChannelMode(QProcess::SeparateChannels);
    mProcess.start(executable, args);
    if (mEnableTimeout)
        mTimeout.start(30 * 1000);
}

void Process::Kill()
{
    if (mProcess.state() == QProcess::Running)
    {
        // terminate will ask the process nicely to exit
        // QProcess::kill will just kill it forcefully.
        // looks like terminate will send sigint and unrar obliges and exits cleanly.
        // however this means that process's return state is normal exit.
        // whereas unrar just dies.
        mProcess.blockSignals(true);
        mProcess.kill();

        // this can block the UI thread but unfortunately the
        // QProcess has the issue that after calling kill
        // properties such as state() != Running might not hold!
        // we wait here until the process finishes and then
        // QProcess should be in reasonable state again.
        mProcess.waitForFinished();

        if (mLogFile.isOpen())
        {
            // dump the last buffer bits into log.
            mLogFile.write(mStdOut);
            mLogFile.write("\n");
            mLogFile.write(mStdErr);
            mLogFile.write("\n");
            mLogFile.write("***killed by user ***");
            mLogFile.flush();
            mLogFile.close();
        }
    }
}

// static
bool Process::RunAndCapture(const QString& executable,
    QStringList& stdoutLines,
    QStringList& stderrLines,
    const QStringList& args)
{
    Process process;
    process.onStdErr = [&](const QString& line) {
        stderrLines.append(line);
    };
    process.onStdOut = [&](const QString& line) {
        stdoutLines.append(line);
    };

    const auto& logFile    = QString("");
    const auto& workingDir = QString("");
    process.Start(executable, args, logFile, workingDir);

    const auto NeverTimeout = -1;
    process.mProcess.waitForFinished(NeverTimeout);
    if (process.GetError() != Error::None)
        return false;

    return true;
}

void Process::ProcessStdOut()
{
    // input was received, it's still running. restart the timer.
    if (mEnableTimeout)
        mTimeout.start(30 * 1000);

    const QByteArray& buff = mProcess.readAllStandardOutput();
    mStdOut.append(buff);
    if (mStdOut.isEmpty())
        return;

    QStringList lines;
    ParseLines(mStdOut, lines);

    QTextStream stream(&mLogFile);
    stream.setCodec("UTF-8");

    for (const auto& line : lines)
    {
        if (onStdOut)
            onStdOut(line);
        if (mLogFile.isOpen())
            stream << line << "\r\n";
    }
    if (mLogFile.isOpen())
        mLogFile.flush();
}

void Process::ProcessStdErr()
{
    // input was received, it's still running, restart the timer.
    if (mEnableTimeout)
        mTimeout.start(30 * 1000);

    const QByteArray& buff = mProcess.readAllStandardError();
    mStdErr.append(buff);
    if (mStdErr.isEmpty())
        return;

    QStringList lines;
    ParseLines(mStdErr, lines);

    QTextStream stream(&mLogFile);
    stream.setCodec("UTF-8");

    for (const auto& line : lines)
    {
        if (onStdErr)
            onStdErr(line);
        if (mLogFile.isOpen())
            stream << line << "\r\n";
    }
    if (mLogFile.isOpen())
        mLogFile.flush();
}

void Process::ProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    DEBUG("%1 finished exitCode: %2 exitStatus: %3",
        mExecutable, exitCode, status);

    ProcessStdOut();
    ProcessStdErr();

    mTimeout.stop();

    if (mLogFile.isOpen())
    {
        mLogFile.flush();
        mLogFile.close();
    }
    if (onFinished)
        onFinished();
}

void Process::ProcessError(QProcess::ProcessError error)
{
    DEBUG("%1 error %2", mExecutable, error);
    ERROR("%1 error %2", mExecutable, error);

    // Umh.. Qt's QProcessError doesn't have a "no error" enum.
    // so apprently we don't really know if an error happened
    // unless we set a flag in this signal
    switch (error)
    {
        case QProcess::FailedToStart:
            mError = Error::FailedToStart;
            break;
        case QProcess::Crashed:
            mError = Error::Crashed;
            break;
        case QProcess::Timedout:
            mError = Error::Timedout;
            break;
        case QProcess::ReadError:
            mError = Error::ReadError;
            break;
        case QProcess::WriteError:
            mError = Error::WriteError;
        case QProcess::UnknownError:
            mError = Error::OtherError;
            break;
    }
    if (mLogFile.isOpen())
    {
        mLogFile.write(mStdOut);
        mLogFile.write("\r\n");
        mLogFile.write(mStdErr);
        mLogFile.write("\r\n");
        mLogFile.write("*** process error ***");
        mLogFile.flush();
    }
}

void Process::ProcessState(QProcess::ProcessState state)
{
    DEBUG("%1 state %2", mExecutable, state);
}

void Process::ProcessTimeout()
{
    if (!mEnableTimeout)
        return;

    DEBUG("%1 has been timed out. No input was detected.", mExecutable);

    mError  = Error::Timedout;
    mKilled = true;
    mProcess.blockSignals(true);
    mProcess.kill();

    // this can block the UI thread but unfortunately the
    // QProcess has the issue that after calling kill
    // properties such as state() != Running might not hold!
    // we wait here until the process finishes and then
    // QProcess should be in reasonable state again.
    mProcess.waitForFinished();

    mTimeout.stop();

    if (mLogFile.isOpen())
    {
        mLogFile.write(mStdOut);
        mLogFile.write("\r\n");
        mLogFile.write(mStdErr);
        mLogFile.write("\r\n");
        mLogFile.write("*** process timeout ***");
        mLogFile.flush();
        mLogFile.close();
    }

    if (onFinished)
        onFinished();

}

} // namespace

