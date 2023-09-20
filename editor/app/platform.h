// Copyright (c) 2010-2015 Sami Väisänen, Ensisoft 
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
#  include <QtGlobal> // for quint64
#include "warnpush.h"

class QString;
class QPixmap;
class QIcon;

// platform specific functions that are not provided by Qt
// and some extra utility functions.

namespace app
{

// extract application icon from a 3rd party executable specified
// by binary. binary is expected to be the complete path to the
// executable in question.
QIcon ExtractIcon(const QString& binary);

// return the name of the operating system that we're running on
// for example Mint Linux, Ubuntu, Windows XP, Windows 7  etc.
QString GetPlatformName();

// resolve the directory path to a mount-point / disk 
QString ResolveMountPoint(const QString& directory);

// get free space available on the disk that contains
// the object identified by filename
quint64 GetFreeDiskSpace(const QString& filename);

// open a file on the local computer
void OpenFile(const QString& file);

void OpenWeb(const QString& url);

void OpenFolder(const QString& folder);

// perform computer shutdown.
void ShutdownComputer();

#if defined(LINUX_OS)
  void SetOpenFileCommand(const QString& cmd);
  void SetShutdownCommand(const QString& cmd);
  QString GetOpenFileCommand();
  QString GetShutdownCommand();
#endif

} // app