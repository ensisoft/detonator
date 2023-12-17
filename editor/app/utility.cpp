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
#  include <QDir>
#  include <QByteArray>
#  include <QBuffer>
#  include <QtGlobal>
#  include <QFile>
#  include <QFileInfo>
#  include <QTextStream>
#  include <QRandomGenerator>
#  include <QApplication>
#  include <QStyle>
#include "warnpop.h"

#if defined(LINUX_OS)
#  include <langinfo.h>
#endif

#include <algorithm>
#include <functional>
#include <string_view>
#include <cstring>

#include "base/utility.h"
#include "editor/app/utility.h"

namespace {

QString ToNativeSeparators(QString p)
{
    // QDir::toNativeSeprators doesn't work with paths
    // like foo/bar/file.png ??
#if defined(POSIX_OS)
    return p.replace("\\", "/");
#elif  defined(WINDOWS_OS)
    return p.replace("/", "\\");
#else
#  error Not implemented.
#endif
}

template<typename Rect>
Rect center_rect_on_target(const Rect& target, const Rect& source)
{
    const float target_width = target.width();
    const float target_height = target.height();
    const float src_width = source.width();
    const float src_height = source.height();
    const auto scaler = std::min(target_width / src_width, target_height / src_height);
    const auto actual_width = src_width * scaler;
    const auto actual_height = src_height * scaler;
    const auto x = (target_width - actual_width) / 2.0f;
    const auto y = (target_height - actual_height) / 2.0f;

    return Rect(target.x() + x, target.y() + y, actual_width, actual_height);
}


} // namespace
namespace app
{

QString GenerateScriptVarName(QString suggestion)
{
    if (auto pos = suggestion.lastIndexOf('/'); pos != -1)
        suggestion = suggestion.mid(pos+1);
    else if (auto pos = suggestion.lastIndexOf('\\'); pos != -1)
        suggestion = suggestion.mid(pos+1);

    if (suggestion.isEmpty())
        return "var";

    QString tmp;
    for (int i=0; i<suggestion.size(); ++i)
    {
        if (suggestion[i].isLetterOrNumber())
            tmp += suggestion[i];
        else if (suggestion[i].isSpace())
            tmp += '_';
    }
    return tmp.toLower();
}

QString FindImageJsonFile(const QString& image_file)
{
    // foo.png -> foo.png.json
    QString tmp = image_file;
    tmp.append(".json");
    if (FileExists(tmp))
        return tmp;

    // foo.png, foo.bmp etc. -> foo.json
    const char* suffices[] = {".png", ".bmp", ".jpg", ".jpeg"};
    for (const auto* suffix : suffices)
    {
        tmp = image_file;
        tmp.replace(suffix, ".json", Qt::CaseInsensitive);
        if (!tmp.endsWith(".json", Qt::CaseInsensitive))
            continue;
        if (FileExists(tmp))
            return tmp;
    }
    return "";
}

QString FindJsonImageFile(const QString& json_file)
{
    // foo.png.json -> foo.png
    QString tmp = json_file;
    tmp.replace(".json", "", Qt::CaseInsensitive);
    if (FileExists(tmp))
        return tmp;

    // foo.json -> foo.png, foo.bmp, etc.
    const char* suffices[] = {".png", ".bmp", ".jpg", ".jpeg"};
    for (const auto* suffix : suffices)
    {
        tmp = json_file;
        tmp.replace(".json", suffix, Qt::CaseInsensitive);
        if (FileExists(tmp))
            return tmp;
    }
    return "";
}


QRect CenterRectOnTarget(const QRect& target, const QRect& source)
{
    return center_rect_on_target<QRect>(target, source);
}

QRect CenterRectOnTarget(const QSize& target_size, const QSize& source_size)
{
    return CenterRectOnTarget(QRect(QPoint(0, 0), target_size), QRect(QPoint(0, 0), source_size));
}

QRectF CenterRectOnTarget(const QRectF& target, const QRectF& source)
{
    return center_rect_on_target<QRectF>(target, source);
}
QRectF CenterRectOnTarget(const QSizeF& target_size, const QSizeF& source_size)
{
    return CenterRectOnTarget(QRectF(QPointF(0.0f, 0.0f), target_size), QRectF(QPointF(0.0f, 0.0f), source_size));
}

bool SetTheme(const QString& name)
{
#if defined (WINDOWS_OS)
    class IKvantumThemeChanger
    {
    public:
        virtual ~IKvantumThemeChanger() = default;
        virtual bool setTheme(void* kvantum_style, const QString& config, const QString& svg, const QString& color_config) = 0;
    };
    QStyle* kvantum = QApplication::style();
    if (kvantum)
    {
        // this hack is done so that this executable doesn't need to
        // link against kvantum.dll
        QVariant hack = kvantum->property("__kvantum_theme_hack");
        if (!hack.isValid())
            return false;

        void* ptr = qvariant_cast<void*>(hack);
        auto* theme_changer = static_cast<IKvantumThemeChanger*>(ptr);
        bool ret = true;
        if (name == "glassy")
        {
            if (!theme_changer->setTheme(kvantum,
                ":Glassy/Glassy.kvconfig",
                ":Glassy/Glassy.svg",
                ":Glassy/Glassy.colors"))
                return false;
        }
        else if (name == "darklines")
        {
            if (!theme_changer->setTheme(kvantum,
                ":DarkLines/DarkLines.kvconfig",
                ":DarkLines/DarkLines.svg",
                ":DarkLines/DarkLines.colors"))
                return false;
        }
        else if (name == "kvantum-curves")
        {
            if (!theme_changer->setTheme(kvantum,
                ":KvCurves/KvCurves.kvconfig",
                ":KvCurves/KvCurves.svg",
                ":KvCurves/KvCurves.colors"))
                return false;
        }
        else if (name == "kvantum-dark-red") {
            if (!theme_changer->setTheme(kvantum,
                ":KvDarkRed/KvDarkRed.kvconfig",
                ":KvDarkRed/KvDarkRed.svg",
                ":KvDarkRed/KvDarkRed.colors"))
                return false;
        }
        else if (name == "glow-dark")
        {
            if (!theme_changer->setTheme(kvantum,
                ":kvGlowDark/kvGlowDark.kvconfig",
                ":kvGlowDark/kvGlowDark.svg",
                ":kvGlowDark/kvGlowDark.colors"))
                return false;
        }
        QApplication::setPalette(kvantum->standardPalette());
        return true;
    }
    return false;
#else
    return true;
#endif
}

bool SetStyle(const QString& name)
{
    QStyle* style = QApplication::setStyle(name);
    if (style == nullptr)
        return false;

    QApplication::setPalette(style->standardPalette());
    return true;
}

std::vector<Resolution> ListResolutions()
{
    // some resolutions from
    // https://en.wikipedia.org/wiki/Graphics_display_resolution
    static std::vector<Resolution> resolutions;
    if (resolutions.empty())
    {
        resolutions.push_back({"FHD", 1920, 1080});
        resolutions.push_back({"HD+", 1600, 900});
        resolutions.push_back({"SHD", 1280, 720});
        resolutions.push_back({"UXGA", 1600, 1200});
        resolutions.push_back({"WXGA", 1366, 768});
        resolutions.push_back({"SXGA", 1280, 1024});
        resolutions.push_back({"XGA", 1024, 768});
        resolutions.push_back({"SVGA", 800, 600});
        resolutions.push_back({"WVGA", 768, 480});
        resolutions.push_back({"VGA", 640, 480});
    }
    return resolutions;
}

std::uint64_t GetFileHash(const QString& file)
{
    QFile stream(file);
    if (!stream.open(QFile::ReadOnly))
        return 0;
    const QByteArray& array = stream.readAll();
    if (array.isEmpty())
        return 0;

    const std::string_view view(array.data(), array.size());
    return std::hash<std::string_view>()(view);
}

QString GetFilePath(const QString& file)
{
    const QFileInfo info(file);
    if (!info.isFile())
        return "";

    if (!info.exists())
        return "";

    return ToNativeSeparators(QDir::cleanPath(info.canonicalPath()));
}

QString JoinPath(const QString& lhs, const QString& rhs)
{
    QString s;
    if (lhs.isEmpty())
        s = rhs;
    else if (rhs.isEmpty())
        s = lhs;
    else s = lhs + "/" + rhs;
    return ToNativeSeparators(QDir::cleanPath(s));
}

QString JoinPath(std::initializer_list<QString> parts)
{
    QString ret;
    for (const auto& str : parts)
    {
        ret += str;
        ret += "/";
    }
    return ToNativeSeparators(QDir::cleanPath(ret));
}

QString CleanPath(const QString& path)
{
    QDir dir(path);
    QString s;
    s = dir.canonicalPath();
    if (s.isEmpty())
    {
        QFileInfo info(path);
        s = info.canonicalFilePath();
    }
    if (s.isEmpty())
        s = path;
    s = QDir::cleanPath(s);
    return ToNativeSeparators(s);
}

bool MakePath(const QString& path)
{
    QDir d;
    return d.mkpath(path);
}

bool FileExists(const QString& filename)
{
    return QFileInfo(filename).exists();
}

bool IsDirectory(const QString& path)
{
    return QFileInfo(path).isDir();
}

std::tuple<bool, QString> CopyRecursively(const QString& src, const QString& dst)
{
    const auto& list = QDir(src).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& name : list)
    {
        const auto& path  = JoinPath(src, name);
        if (IsDirectory(path))
        {
            QDir dir(dst);
            if (!dir.mkdir(name))
                return {false, QString("Failed to create directory '%1'").arg(JoinPath(dst, name)) };
            const auto [success, error] = CopyRecursively(path, JoinPath(dst, name));
            if (!success)
                return std::make_tuple(false, error);
        }
        else
        {
            const auto& src_file = JoinPath(src, name);
            const auto& dst_file = JoinPath(dst, name);
            const auto[success, error] = CopyFile(src_file, dst_file);
            if (!success)
                return std::make_tuple(false, error);
        }
    }
    return std::make_tuple(true, "");
}

std::tuple<bool, QString> CopyFile(const QString& src, const QString& dst)
{
    const QFileInfo src_info(src);
    const QFileInfo dst_info(dst);
    if (src_info.canonicalFilePath() == dst_info.canonicalFilePath())
        return std::make_tuple(true, "");
    QFile src_io(src);
    QFile dst_io(dst);
    if (!src_io.open(QIODevice::ReadOnly))
        return std::make_tuple(false, src_io.errorString());
    if (!dst_io.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return std::make_tuple(false, dst_io.errorString());
    const auto& buffer = src_io.readAll();
    if (dst_io.write(buffer) == -1)
        return std::make_tuple(false, dst_io.errorString());
    if (!dst_io.setPermissions(src_io.permissions()))
        return std::make_tuple(false, dst_io.errorString());
    return std::make_tuple(true, "");
}

QString FromUtf8(const std::string& str)
{
    return QString::fromUtf8(str.c_str());
}

QString FromLatin(const std::string& str)
{
    return QString::fromLatin1(str.c_str());
}

QString FromLocal8Bit(const std::string& s)
{
#if defined(WINDOWS_OS)
    return QString::fromUtf8(s.c_str());
#elif defined(LINUX_OS)
    const char* codeset = nl_langinfo(CODESET);
    if (!std::strcmp(codeset, "UTF-8"))
        return FromUtf8(s);

    return QString::fromLocal8Bit(s.c_str());
#endif
}

std::string ToUtf8(const QString& str)
{
    const auto& bytes = str.toUtf8();
    return std::string(bytes.data(), bytes.size());
}

std::string ToLatin(const QString& str)
{
    const auto& bytes = str.toLatin1();
    return std::string(bytes.data(), bytes.size());
}

std::string ToLocal8Bit(const QString& str)
{
#if defined(WINDOWS_OS)
    return ToUtf8(str);
#elif defined(LINUX_OS)
    const char* codeset = nl_langinfo(CODESET);
    if (!std::strcmp(codeset, "UTF-8"))
        return ToUtf8(str);

    const auto& bytes = str.toLocal8Bit();
    return std::string(bytes.data(),
        bytes.size());

#else
#  error Unimplemented method
#endif
}

std::ofstream OpenBinaryOStream(const QString& file)
{
#if defined(WINDOWS_OS)
    std::ofstream out(file.toStdWString(), std::ios::out | std::ios::binary | std::ios::trunc);
#elif defined(LINUX_OS)
    std::ofstream out(ToLocal8Bit(file), std::ios::out | std::ios::binary | std::ios::trunc);
#else
#  error Unimplemented function
#endif
    return out;
}

std::ifstream OpenBinaryIStream(const QString& file)
{
#if defined(WINDOWS_OS)
    std::ifstream in(file.toStdWString(), std::ios::in | std::ios::binary);
#elif defined(LINUX_OS)
    std::ifstream in(ToLocal8Bit(file), std::ios::in | std::ios::binary);
#else
    error unimplemented function
#endif
    return in;

}

bool WriteBinaryFile(const QString& file, const void* data, std::size_t bytes)
{
    std::ofstream out = OpenBinaryOStream(file);
    if (!out.is_open())
        return false;

    out.write(static_cast<const char*>(data), bytes);
    return true;
}

bool WriteTextFile(const QString& file, const QString& content, QFile::FileError* err_val, QString* err_str)
{
    QFile out;
    out.setFileName(file);
    out.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (!out.isOpen())
    {
        if (err_val) *err_val = out.error();
        if (err_str) *err_str = out.errorString();
        return false;
    }

    QTextStream stream(&out);
    stream.setCodec("UTF-8");
    stream << content;
    return true;
}

bool WriteTextFile(const QString& file, const std::string& contents, QFile::FileError* err_val, QString* err_str)
{
    QFile out;
    out.setFileName(file);
    out.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (!out.isOpen())
    {
        if (err_val) *err_val = out.error();
        if (err_str) *err_str = out.errorString();
    }
    if (contents.empty())
        return true;
    out.write(&contents[0], contents.size());
    out.close();
    return true;
}
bool WriteTextFile(const QString& file, const char* contents, QFile::FileError* err_val, QString* err_str)
{
    QFile out;
    out.setFileName(file);
    out.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (!out.isOpen())
    {
        if (err_val) *err_val = out.error();
        if (err_str) *err_str = out.errorString();
    }
    if (!contents)
        return true;
    const auto len = std::strlen(contents);
    if (!len)
        return true;

    out.write(contents, len);
    out.close();
    return true;
}


QString ReadTextFile(const QString& file, QFile::FileError* err_val, QString* err_str)
{
    QFile in;
    in.setFileName(file);
    in.open(QIODevice::ReadOnly);
    if (!in.isOpen())
    {
        if (err_val) *err_val = in.error();
        if (err_str) *err_str = in.errorString();
        return "";
    }
    return QString::fromUtf8(in.readAll());
}

QByteArray ReadBinaryFile(const QString& file, QFile::FileError* err_val, QString* err_str)
{
    QFile in;
    in.setFileName(file);
    in.open(QIODevice::ReadOnly);
    if (!in.isOpen())
    {
        if (err_val) *err_val = in.error();
        if (err_str) *err_str = in.errorString();
    }
    return in.readAll();
}

QString RandomString()
{
   static const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
   static const int randomStringLength = 12; // assuming you want random strings of 12 characters

   QString randomString;
   for(int i=0; i<randomStringLength; ++i)
   {
       const auto random   = QRandomGenerator::global()->generate();
       const auto index    = random % possibleCharacters.length();
       const auto nextChar = possibleCharacters.at(index);
       randomString.append(nextChar);
   }
   return randomString;
}

static QString gAppHome;

void InitializeAppHome(const QString& appname)
{
    const auto& home = QDir::homePath();
    const auto& appdir = home + "/" + appname;

    // if we cannot create the directory we're fucked so then throw.
    QDir dir;
    if (!dir.mkpath(appdir))
        throw std::runtime_error(ToUtf8(QString("failed to create %1").arg(appdir)));

    gAppHome = QDir::cleanPath(appdir);
}

QString GetAppHomeFilePath(const QString& name)
{
    // pathstr is an absolute path so then this is also
    // an absolute path.
    return JoinPath(gAppHome, name);
}

QString GetAppInstFilePath(const QString& name)
{
    return JoinPath(QCoreApplication::applicationDirPath(), name);
}

bool ValidateQVariantJsonSupport(const QVariant& variant)
{
    // Qt API brainfart (QVariant::Type is deprecated)
    const auto type = (QMetaType::Type)variant.type();
    if (type == QMetaType::Type::QVariantMap)
        return ValidateQVariantMapJsonSupport(qvariant_cast<QVariantMap>(variant));

    // todo:there's more but these should work at least.
    return type == QMetaType::Type::Float ||
           type == QMetaType::Type::Double ||
           type == QMetaType::Type::Int ||
           type == QMetaType::Type::UInt ||
           type == QMetaType::Type::QString ||
           type == QMetaType::Type::QStringList ||
           type == QMetaType::Type::QColor ||
           type == QMetaType::Type::ULongLong ||
           type == QMetaType::Type::LongLong ||
           type == QMetaType::Type::Bool;
}
bool ValidateQVariantMapJsonSupport(const QVariantMap& map)
{
    for (const auto& value : map)
    {
        if (!ValidateQVariantJsonSupport(value))
            return false;
    }
    return true;
}

size_t VariantHash(const QVariant& variant)
{
    QByteArray byte_array;

    QBuffer buffer;
    buffer.setBuffer(&byte_array);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);
    stream << variant;

    // the variant should have supported serialized write into a data stream
    // if not, then we'd have to create a special case for that sucker.
    ASSERT(byte_array.size() != 0);

    return qHashBits(byte_array.data(), byte_array.size());
}

} // namespace
