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
#  include <QString>
#  include <QColor>
#  include <QJsonObject>
#  include <QIcon>
#  include <QVariant>
#  include <QVariantMap>
#  include <neargye/magic_enum.hpp>
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <string_view>
#include <tuple>

#include "base/assert.h"

#include "base/snafu.h"

// general dumping ground for utility type of functionality
// preferably not specific to any GUI or application.
// inherited from previous projects.

namespace app
{

struct Resolution {
    QString name;
    unsigned width  = 0;
    unsigned height = 0;
};

std::vector<Resolution> ListResolutions();

// Compute a file hash based on the contents of the file.
// Simply returns 0 if the file could not be read.
std::uint64_t GetFileHash(const QString& file);

// Concatenate two strings as file system path.
// Returns the path formatted with native separators.
QString JoinPath(const QString& lhs, const QString& rhs);

// Clean the path string. Returns with native separators.
QString CleanPath(const QString& path);

// Make path. returns true if successful, otherwise false.
bool MakePath(const QString& path);

bool IsDirectory(const QString& path);

std::tuple<bool, QString> CopyRecursively(const QString& src_dir_path, const QString& dst_dir_path);

std::tuple<bool, QString> CopyFile(const QString& src, const QString& dst);

// Convert the UTF-8 encoded string to a QString.
QString FromUtf8(const std::string& str);
// Convert the ASCII encoded string to a QString.
QString FromLatin(const std::string& str);
// Convert the 8-bit encoded string to a QString.
QString FromLocal8Bit(const std::string& str);

// Convert and encode the QString into UTF-8 encoded std::string.
std::string ToUtf8(const QString& str);

// Convert the QString into ASCII only std::string.
// This conversion may result in loss of data if the
// string contains characters that are not representable
// in ASCII.
// Useful for converting strings that are not natural language/text.
std::string ToLatin(const QString& str);

// Convert QString to a narrow std::string in
// local (computer specific) encoding.
// The output is either UTF-8 OR some local 8-bit encoding.
std::string ToLocal8Bit(const QString& str);

// Open a binary std::ofstream for writing to a file.
std::ofstream OpenBinaryOStream(const QString& file);
// Open a binary std::ifstream for read from file.
std::ifstream OpenBinaryIStream(const QString& file);

// Simple helper function to quickly dump binary data to a file.
// Returns true if succesful, otherwise false.
bool WriteAsBinary(const QString& file, const void* data, std::size_t bytes);

// Simple helper function to write the contents of vector<T>
// in binary to a file.
template<typename T>
bool WriteAsBinary(const QString& file, const std::vector<T>& data)
{
    return WriteAsBinary(file, (const void*)data.data(),
        sizeof(T) * data.size());
}

// Read the contents of a file into a vector of some generic type T
template<typename T>
bool ReadAsBinary(const QString& file, std::vector<T>& data)
{
    std::ifstream in = OpenBinaryIStream(file);
    if (!in.is_open())
        return false;

    in.seekg(0, std::ios::end);
    const auto size = (size_t)in.tellg();
    in.seekg(0, std::ios::beg);

    if (size % sizeof(T))
        return false;

    data.resize(size / sizeof(T));
    in.read((char*)&data[0], size);
    return true;
}

// Simple helper to dump a string into a text file.
// Previous contents of the file will be overwritten.
// Returns false on failure.
bool WriteTextFile(const QString& file, const QString& contents, QString* error = nullptr);

// Read the contents of a text file into a string.
// Provides no error checking.
QString ReadTextFile(const QString& file);

// Generate a random string.
QString RandomString();

// Initialize the home directory for the application
// in the user's home directory.
void InitializeAppHome(const QString& appname);

// Get an absolute path to a file/folder in the application
// home directory.
QString GetAppHomeFilePath(const QString& name);
// Get an absolute path to a file/folder in the application
// install directory (where our currently running executable is).
QString GetAppInstFilePath(const QString& name);

bool ValidateQVariantJsonSupport(const QVariant& variant);
bool ValidateQVariantMapJsonSupport(const QVariantMap& map);
size_t VariantHash(const QVariant& variant);

template<typename Enum>
Enum EnumFromString(const QString& str, Enum backup, bool* success = nullptr)
{
    const auto& enum_val = magic_enum::enum_cast<Enum>(ToUtf8(str));
    if (!enum_val.has_value()) {
        if (success) *success = false;
        return backup;
    }
    if (success) *success = true;
    return enum_val.value();
}

template<typename Enum>
QString EnumToString(Enum value)
{
    const std::string_view& str(magic_enum::enum_name(value));
    return QString::fromLocal8Bit(str.data(), str.size());
}

inline void JsonWrite(QJsonObject& object, const char* name, QColor color)
{
    object[name] = QString::number(color.rgba());
}

inline void JsonWrite(QJsonObject& object, const char* name, float value)
{
    object[name] = value;
}

inline void JsonWrite(QJsonObject& object, const char* name, int value)
{
    object[name] = value;
}
inline void JsonWrite(QJsonObject& object, const char* name, unsigned value)
{
    object[name] = static_cast<int>(value);
}
inline void JsonWrite(QJsonObject& object, const char* name, const QString& value)
{
    object[name] = value;
}
inline void JsonWrite(QJsonObject& object, const char* name, const std::string& value)
{
    object[name] = FromUtf8(value);
}
inline void JsonWrite(QJsonObject& object, const char* name, bool value)
{
    object[name] = value;
}
inline void JsonWrite(QJsonObject& object, const char* name, const glm::vec2& value)
{
    QJsonObject vec;
    vec["x"] = value.x;
    vec["y"] = value.y;
    object[name] = vec;
}

template<typename Enum>
inline void JsonWrite(QJsonObject& object, const char* name, Enum value)
{
    static_assert(std::is_enum<Enum>::value);
    object[name] = EnumToString(value);
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, glm::vec2* out)
{
    if (!object.contains(name) || !object[name].isObject())
        return false;
    const auto& vec = object[name].toObject();
    if (!vec.contains("x") || !vec.contains("y"))
        return false;
    out->x = vec["x"].toDouble();
    out->y = vec["y"].toDouble();
    return true;
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, QColor* out)
{
    if (!object.contains(name) || !object[name].isString())
        return false;
    QString value = object[name].toString();
    out->setRgba(value.toUInt());
    return true;
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, float* out)
{
    if (!object.contains(name) || !object[name].isDouble())
        return false;
    *out = object[name].toDouble();
    return true;
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, int* out)
{
    // todo: any way to check isInteger??
    if (!object.contains(name))
        return false;
    *out = object[name].toInt();
    return true;
}
inline bool JsonReadSafe(const QJsonObject& object, const char* name, unsigned* out)
{
    // todo: any way to check isInteger??
    if (!object.contains(name))
        return false;
    *out = object[name].toInt();
    return true;
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, QString* out)
{
    if (!object.contains(name) || !object[name].isString())
        return false;
    *out = object[name].toString();
    return true;
}
inline bool JsonReadSafe(const QJsonObject& object, const char* name, std::string* out)
{
    if (!object.contains(name) || !object[name].isString())
        return false;
    QString str = object[name].toString();
    *out = ToUtf8(str);
    return true;
}

inline bool JsonReadSafe(const QJsonObject& object, const char* name, bool* out)
{
    if (!object.contains(name) || !object[name].isBool())
        return false;
    *out = object[name].toBool();
    return true;
}
template<typename Enum>
inline bool JsonReadSafe(const QJsonObject& object, const char* name, Enum* out)
{
    static_assert(std::is_enum<Enum>::value);
    QString str;
    if (!JsonReadSafe(object, name, &str))
        return false;
    bool ok = false;
    Enum e;
    e = EnumFromString(str, e, &ok);
    if (!ok)
        return false;
    *out = e;
    return true;
}

struct MemberRecursionGuard {
    MemberRecursionGuard(const void* who, std::string where) : mObject(who), mWhere(where)
    {
        auto& map = GetMap();
        if (!map[who].insert(where).second) {
            BUG("You've recursed. Now behold a core dump!");
        }
    }
   ~MemberRecursionGuard()
    {
        auto& map = GetMap();
        auto it = map.find(mObject);
        ASSERT(it != map.end());
        auto& entries = it->second;
        ASSERT(entries.find(mWhere) != entries.end());
        entries.erase(mWhere);
        if (entries.empty()) {
            map.erase(mObject);
        }
    }
    MemberRecursionGuard() = delete;
    MemberRecursionGuard(const MemberRecursionGuard&) = delete;
    MemberRecursionGuard& operator=(const MemberRecursionGuard&) = delete;

    using EntrySet = std::unordered_set<std::string>;
    using ObjectMap = std::unordered_map<const void*, EntrySet>;
private:
    static ObjectMap& GetMap()
    {
        static ObjectMap map;
        return map;
    }
    const void* mObject = nullptr;
    const std::string mWhere;
};

#define RECURSION_GUARD(who, where) \
    MemberRecursionGuard recursion_guard(who, where)

} // namespace
