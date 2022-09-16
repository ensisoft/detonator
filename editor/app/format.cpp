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

#include "base/assert.h"
#include "base/color4f.h"
#include "editor/app/format.h"

namespace app
{

QString toString(QFile::FileError error)
{
    using e = QFile::FileError;
    switch (error)
    {
        case e::NoError:          return "No error occurred.";
        case e::ReadError:        return "An error occurred when reading from the file.";
        case e::WriteError:       return "An error occurred when writing to the file.";
        case e::FatalError:       return "A fatal error occurred.";
        case e::ResourceError:    return "A resource error occurred.";
        case e::OpenError:        return "The file could not be opened.";
        case e::AbortError:       return "The operation was aborted.";
        case e::TimeOutError:     return "A timeout occurred.";
        case e::UnspecifiedError: return "An unspecified error occurred.";
        case e::RemoveError:      return "The file could not be removed.";
        case e::RenameError:      return "The file could not be renamed.";
        case e::PositionError:    return "The position in file could not be changed.";
        case e::ResizeError:      return "The file could not be resized.";
        case e::PermissionsError: return "The file could not be accessed (no permission).";
        case e::CopyError:        return "The file could not be copied.";
    }
    BUG("???");
    return "";
}

QString toString(QLocalSocket::LocalSocketError error)
{
    using e = QLocalSocket::LocalSocketError;
    switch (error)
    {
        case e::ConnectionError:                 return "Connection error";
        case e::ConnectionRefusedError:          return "Connection refused error.";
        case e::DatagramTooLargeError:           return "Datagram too large error.";
        case e::OperationError:                  return "Operation error.";
        case e::PeerClosedError:                 return "Peer closed error.";
        case e::ServerNotFoundError:             return "Server not found error.";
        case e::SocketAccessError:               return "Socket access error.";
        case e::SocketResourceError:             return "Socket resource error.";
        case e::SocketTimeoutError:              return "Socket timeout error.";
        case e::UnknownSocketError:              return "Unknown socket error.";
        case e::UnsupportedSocketOperationError: return "Unsupported socket operation error.";
    }
    BUG("???");
    return "";
}

QString toString(QProcess::ProcessState state)
{
    using s = QProcess::ProcessState;
    switch (state)
    {
        case s::NotRunning: return "Not running";
        case s::Starting:   return "Starting";
        case s::Running:    return "Running";
    }
    BUG("???");
    return "";
}

QString toString(QProcess::ProcessError error)
{
    using e = QProcess::ProcessError;
    switch (error)
    {
        case e::FailedToStart: return "Failed to start";
        case e::Crashed:       return "Crashed";
        case e::ReadError:     return "Read error";
        case e::UnknownError:  return "Unknown error";
        case e::WriteError:    return "Write error";
        case e::Timedout:      return "Timed out";
    }
    BUG("???");
    return "";
}

QString toString(const audio::Format& format)
{
    return app::FromUtf8(audio::ToString(format));
}

QString toString(const base::Color4f& color)
{
    return app::FromUtf8(base::ToString(color));
}

QString toString(const Bytes& bytes)
{
    // use floating points here so we get automatic
    // promotion, and no need to cast in format funcs
    const double GB = 1024 * 1024 * 1024;
    const double MB = 1024 * 1024;
    const double KB = 1024;

    // SI units use kB, MB and GB for base of 1000
    // using base of 1024 gives kibibyes, mebibytes and gibibytes

    if (bytes.bytes >= GB)
        return QString("%1 GiB").arg(bytes.bytes / GB, 0, 'f', 1, ' ');
    else if (bytes.bytes >= MB)
        return QString("%1 MiB").arg(bytes.bytes / MB, 0, 'f', 1, ' ');
    else if (bytes.bytes >= KB)
        return QString("%1 KiB").arg(bytes.bytes / KB, 0, 'f', 1, ' ');

    return QString("%1 b").arg(bytes.bytes, 0, 'f', 1, ' ');
}

} // app
