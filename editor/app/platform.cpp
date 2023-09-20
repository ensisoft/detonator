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

#include "config.h"

#include "warnpush.h"
#  include <QtGui/QDesktopServices>
#  include <QtGui/QPixmap>
#  include <QtGui/QImage>
#  include <QtGui/QIcon>
#  include <QFile>
#  include <QTextStream>
#  include <QIODevice>
#  include <QStringList>
#  include <QDir>
#  include <QProcess>
#include "warnpop.h"

#if defined(WINDOWS_OS)
#  include <windows.h>
#  include <Shellapi.h> // for ExtractIconEx
#  pragma comment(lib, "Shell32.lib")
#  pragma comment(lib, "Gdi32.lib")
#  pragma comment(lib, "Advapi32.lib") // for ProcessToken functions
#endif
#if defined(LINUX_OS)
#  include <sys/vfs.h>
#endif

#include "editor/app/platform.h"
#include "editor/app/utility.h"

namespace app
{

#if defined(WINDOWS_OS)

// Icon extraction code is nicked from qt\src\gui\image\qtpixmap_win.cpp
QImage qt_fromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h)
{
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = w * h * 4;

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return image;

    // Get bitmap bits
    uchar *data = (uchar *)malloc(bmi.bmiHeader.biSizeImage);

    if (GetDIBits(hdc, bitmap, 0, h, data, &bmi, DIB_RGB_COLORS)) {
        // Create image and copy data into image.
        for (int y=0; y<h; ++y) {
            void *dest = (void *) image.scanLine(y);
            void *src = data + y * image.bytesPerLine();
            memcpy(dest, src, image.bytesPerLine());
        }
    } else {
        qWarning("qt_fromWinHBITMAP(), failed to get bitmap bits");
    }
    free(data);

    return image;
}

QIcon ExtractIcon(const QString& binary)
{
    HICON small_icon[1] = {0};
    ExtractIconEx((const wchar_t*)binary.utf16(), 0, NULL, small_icon, 1);
    if (!small_icon[0])
        return QIcon();

    bool foundAlpha  = false;
    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo = {0};
    bool result = GetIconInfo(small_icon[0], &iconinfo); //x and y Hotspot describes the icon center
    if (!result)
    {
        DeleteDC(hdc);
        return QIcon();
    }

    int w = iconinfo.xHotspot * 2;
    int h = iconinfo.yHotspot * 2;

    BITMAPINFOHEADER bitmapInfo;
    bitmapInfo.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapInfo.biWidth       = w;
    bitmapInfo.biHeight      = h;
    bitmapInfo.biPlanes      = 1;
    bitmapInfo.biBitCount    = 32;
    bitmapInfo.biCompression = BI_RGB;
    bitmapInfo.biSizeImage   = 0;
    bitmapInfo.biXPelsPerMeter = 0;
    bitmapInfo.biYPelsPerMeter = 0;
    bitmapInfo.biClrUsed       = 0;
    bitmapInfo.biClrImportant  = 0;
    DWORD* bits;

    HBITMAP winBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    DrawIconEx( hdc, 0, 0, small_icon[0], iconinfo.xHotspot * 2, iconinfo.yHotspot * 2, 0, 0, DI_NORMAL);
    QImage image = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

    for (int y = 0 ; y < h && !foundAlpha ; y++)
    {
        QRgb *scanLine= reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < w ; x++)
        {
            if (qAlpha(scanLine[x]) != 0)
            {
                foundAlpha = true;
                break;
            }
        }
    }
    if (!foundAlpha)
    {
        //If no alpha was found, we use the mask to set alpha values
        DrawIconEx( hdc, 0, 0, small_icon[0], w, h, 0, 0, DI_MASK);
        QImage mask = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

        for (int y = 0 ; y < h ; y++)
        {
            QRgb *scanlineImage = reinterpret_cast<QRgb *>(image.scanLine(y));
            QRgb *scanlineMask = mask.isNull() ? 0 : reinterpret_cast<QRgb *>(mask.scanLine(y));
            for (int x = 0; x < w ; x++)
            {
                if (scanlineMask && qRed(scanlineMask[x]) != 0)
                    scanlineImage[x] = 0; //mask out this pixel
                else
                    scanlineImage[x] |= 0xff000000; // set the alpha channel to 255
            }
        }
    }

    //dispose resources created by iconinfo call
    DeleteObject(iconinfo.hbmMask);
    DeleteObject(iconinfo.hbmColor);

    SelectObject(hdc, oldhdc); //restore state
    DeleteObject(winBitmap);
    DeleteDC(hdc);
    return QIcon(QPixmap::fromImage(image));
}

QString GetPlatformName()
{
    OSVERSIONINFOEX info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&info)) == FALSE)
        return "";

    SYSTEM_INFO sys = {};
    GetSystemInfo(&sys);

    QString ret;

    // http://msdn.microsoft.com/en-us/library/ms724833(v=VS.85).aspx
    // major versions
    if (info.dwMajorVersion == 10)
    {
        if (info.dwMinorVersion == 0)
        {
            if (info.wProductType == VER_NT_WORKSTATION)
                ret = "Windows 10 Insider Preview";
            else ret = "Windows Server Technical Preview";
        }
    }
    else if (info.dwMajorVersion == 6)
    {
        if (info.dwMinorVersion == 3)
        {
            if (info.wProductType == VER_NT_WORKSTATION)
                ret = "Windows 8.1";
            else ret = "Windows Server 2012 R2";
        }
        else if (info.dwMinorVersion == 2)
        {
            if (info.wProductType == VER_NT_WORKSTATION)
                ret = "Windows 8";
            else ret = "Windows Server 2012";
        }
        else if (info.dwMinorVersion == 1)
        {
            if (info.wProductType == VER_NT_WORKSTATION)
                ret = "Windows 7";
            else ret = "Windows Server 2008 R2";
        }
        else if (info.dwMinorVersion == 0)
        {
            if (info.wProductType == VER_NT_WORKSTATION)
                ret = "Windows Vista";
            else ret = "Windows Server 2008";
        }
    }
    else if (info.dwMajorVersion == 5)
    {
        // MSDN says this API is available from Windows 2000 Professional
        // but the headers don't have the definition....
#ifndef VER_SUITE_WH_SERVER
        enum {
            VER_SUITE_WH_SERVER = 0x00008000
        };
#endif
        if (info.dwMinorVersion == 2 && GetSystemMetrics(SM_SERVERR2))
            ret = "Windows Server 2003 R2";
        else if (info.dwMinorVersion == 2 && GetSystemMetrics(SM_SERVERR2) == 0)
            ret = "Windows Server 2003";
        else if (info.dwMinorVersion == 2 && info.wSuiteMask & VER_SUITE_WH_SERVER)
            ret = "Windows Home Server";
        else if (info.dwMinorVersion == 2 && info.wProductType == VER_NT_WORKSTATION  && sys.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            ret = "Windows XP Professional x64 Edition";
        else if (info.dwMinorVersion == 1)
        {
            ret = "Windows XP";
            if (info.wSuiteMask & VER_SUITE_PERSONAL)
                ret += " Home Edition";
            else ret += " Professional";
        }
        else if (info.dwMinorVersion == 0)
        {
            ret = "Windows 2000";
            if (info.wProductType == VER_NT_WORKSTATION)
                ret += " Professional";
            else
            {
                if (info.wSuiteMask & VER_SUITE_DATACENTER)
                    ret += " Datacenter Server";
                else if (info.wSuiteMask & VER_SUITE_ENTERPRISE)
                    ret += " Advanced Server";
                else ret += " Server";
            }
        }
    }
    // include service pack (if any)
    if (info.szCSDVersion)
    {
        ret += " ";
        ret += QString::fromWCharArray(info.szCSDVersion);
    }
    ret += QString(" (build %1)").arg(info.dwBuildNumber);
    if (info.dwMajorVersion >= 6)
    {
        if (sys.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            ret += ", 64-bit";
        else if (sys.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            ret += ", 32-bit";
    }

    return ret;
}

QString ResolveMountPoint(const QString& directory)
{
    QDir dir(directory);
    QString path = dir.canonicalPath();
    if (path.isEmpty())
        return "";

    // extract the drive letter
    QString ret;
    ret.append(path[0]);
    ret.append(":");
    return ret;
}

quint64 GetFreeDiskSpace(const QString& filename)
{
    const auto& native = QDir::toNativeSeparators(filename);

    ULARGE_INTEGER large = {};
    if (!GetDiskFreeSpaceEx((const wchar_t*)native.utf16(), &large, nullptr, nullptr))
        return 0;

    return large.QuadPart;
}

void OpenFile(const QString& file)
{
    HINSTANCE ret = ShellExecute(nullptr,
        L"open",
        (const wchar_t*)file.utf16(),
        NULL, // executable parameters, don't care
        NULL, // working directory
        SW_SHOWNORMAL);
}

void OpenWeb(const QString& url)
{
    QDesktopServices::openUrl(url);
}

void ShutdownComputer()
{
    HANDLE hToken = NULL;
    HANDLE hProc  = GetCurrentProcess();
    OpenProcessToken(hProc, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

    TOKEN_PRIVILEGES tkp = {0};
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

    ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE,
      SHTDN_REASON_MAJOR_OPERATINGSYSTEM |
      SHTDN_REASON_MINOR_UPGRADE |
      SHTDN_REASON_FLAG_PLANNED);

}

#elif defined(LINUX_OS)

QIcon ExtractIcon(const QString& binary)
{
    return QIcon();
}

QString GetPlatformName()
{
   // works for Ubuntu and possibly for Debian
   // most likely broken for other distros
   QFile file("/etc/lsb-release");
   if (!file.open(QIODevice::ReadOnly))
       return "GNU/Linux";

   QTextStream stream(&file);
   QString line;
   do
   {
       line = stream.readLine();
       if (line.isEmpty() || line.isNull())
           continue;

       QStringList split = line.split("=");
       if (split.size() != 2)
           continue;

       if (split[0] == "DISTRIB_DESCRIPTION")
       {
           // the value is double quoted, e.g. "Ubuntu 9.04", ditch the quotes
           QString val = split[1];
           val.chop(1);
           val.remove(0, 1);
           return val;
       }
   }
   while(!line.isNull());
   return "GNU/Linux";
}

QString ResolveMountPoint(const QString& directory)
{
    QDir dir(directory);
    QString path = dir.canonicalPath(); // resolves symbolic links

    // have to read /proc/mounts and compare the mount points to
    // the given directory path. an alternative could be /etc/mtab
    // but apparently /proc/mounts is more up to date and should exist
    // on any new modern kernel
    QFile mtab("/proc/mounts");
    if (!mtab.open(QIODevice::ReadOnly))
        return "";

    QStringList mounts;
    QString line;
    QTextStream stream(&mtab);
    do
    {
        line = stream.readLine();
        if (line.isEmpty() || line.isNull())
            continue;

        QStringList split = line.split(" ");
        if (split.size() != 6)
            continue;

        // /proc/mounts should look something like this...
        // rootfs / rootfs rw 0 0
        // none /sys sysfs rw,nosuid,nodev,noexec 0 0
        // none /proc proc rw,nosuid,nodev,noexec 0 0
        // udev /dev tmpfs rw 0 0
        // ...
        mounts.append(split[1]);
    }
    while (!line.isNull());

    QString mount_point;
    for (int i=0; i<mounts.size(); ++i)
    {
        if (path.startsWith(mounts[i]))
        {
            if (mounts[i].size() > mount_point.size())
                mount_point = mounts[i];
        }
    }
    return mount_point;
}

quint64 GetFreeDiskSpace(const QString& filename)
{
    const auto native = QDir::toNativeSeparators(filename);

    const auto u8 = ToUtf8(native);

    struct statfs64 st = {};
    if (statfs64(u8.c_str(), &st) == -1)
        return 0;

    // f_bsize is the "optimal transfer block size"
    // not sure if this reliable and always the same as the
    // actual file system block size. If it is different then
    // this is incorrect.
    auto ret = st.f_bsize * st.f_bavail;
    return ret;
}

// gnome alternatives
// gnome-open
// gnome-session-quit --power-off --no-prompt

QString openfile_command = "xdg-open";
QString shutdown_command = "systemctl poweroff";

void SetOpenFileCommand(const QString& cmd)
{
    openfile_command = cmd;
}

void SetShutdownCommand(const QString& cmd)
{
    shutdown_command = cmd;
}

void OpenFile(const QString& file)
{
    //QDesktopServices::openUrl("file:///" + file);
    QStringList args;
    args << file;
    QProcess::startDetached(openfile_command, args);
}

void OpenWeb(const QString& url)
{
    QDesktopServices::openUrl(url);
}

void shutdownComputer()
{
    QStringList list = shutdown_command.split(" ", QString::SkipEmptyParts);
    if (list.size() < 1)
        return;

    QString cmd = list[0];
    list.removeFirst();
    QProcess::startDetached(cmd, list);
}

QString GetOpenFileCommand()
{
    return openfile_command;
}

QString GetShutdownCommand()
{
    return shutdown_command;
}


#endif

void OpenFolder(const QString& folder)
{
    // just a simple forward for now.
    OpenFile(folder);
}

} // app