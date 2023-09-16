// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include <QIcon>
#  include <QString>
#  include <QStringList>
#  include <QTextStream>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <algorithm>
#include <map>

#include "base/assert.h"
#include "base/color4f.h"
#include "base/format.h"
#include "base/utility.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/lua-doc.h"

#include "wdk/keys.h"

namespace {
using namespace app;
QString table_name;
std::vector<LuaMemberDoc> g_method_docs;
std::unordered_map<QString, QString> g_table_docs;

void SetTable(const QString& name, const QString& doc = QString())
{
    g_table_docs[name] = doc;

    table_name = name;

    if (const auto i = name.indexOf("."); i != -1)
    {
        const auto& parent_table = name.mid(0, i);
        const auto& child_table = name.mid(i+1);
        LuaMemberDoc doc;
        doc.type  = LuaMemberType::Table;
        doc.table = parent_table;
        doc.name  = child_table;
        doc.desc  = app::toString("Nested table '%1'", child_table);
        g_method_docs.push_back(doc);
    }
}

void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name,
               const QString& arg1_type, const QString& arg1_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name,
               const QString& arg1_type, const QString& arg1_name,
               const QString& arg2_type, const QString& arg2_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    doc.args.push_back({arg2_name,  arg2_type});
    g_method_docs.push_back(std::move(doc));
}

void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name,
               const QString& arg1_type, const QString& arg1_name,
               const QString& arg2_type, const QString& arg2_name,
               const QString& arg3_type, const QString& arg3_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    doc.args.push_back({arg2_name,  arg2_type});
    doc.args.push_back({arg3_name,  arg3_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name,
               const QString& arg1_type, const QString& arg1_name,
               const QString& arg2_type, const QString& arg2_name,
               const QString& arg3_type, const QString& arg3_name,
               const QString& arg4_type, const QString& arg4_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    doc.args.push_back({arg2_name,  arg2_type});
    doc.args.push_back({arg3_name,  arg3_type});
    doc.args.push_back({arg4_name,  arg4_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(LuaMemberType type, const QString& ret, const QString& name, const QString& desc,
               const QString& arg0_type, const QString& arg0_name,
               const QString& arg1_type, const QString& arg1_name,
               const QString& arg2_type, const QString& arg2_name,
               const QString& arg3_type, const QString& arg3_name,
               const QString& arg4_type, const QString& arg4_name,
               const QString& arg5_type, const QString& arg5_name)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    doc.args.push_back({arg2_name,  arg2_type});
    doc.args.push_back({arg3_name,  arg3_type});
    doc.args.push_back({arg4_name,  arg4_type});
    doc.args.push_back({arg5_name,  arg5_type});
    g_method_docs.push_back(std::move(doc));
}

void AddProperty(LuaMemberType type, const QString& ret, const QString& name, const QString& desc)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

} // namespace

#define DOC_TABLE(name) SetTable(name)
#define DOC_TABLE2(name, doc) SetTable(name, doc)
#define DOC_METHOD_0(ret, name, desc) AddMethod(LuaMemberType::Method, ret, name, desc)
#define DOC_METHOD_1(ret, name, desc, a0type, a0name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name)
#define DOC_METHOD_2(ret, name, desc, a0type, a0name, a1type, a1name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name)
#define DOC_METHOD_3(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name)
#define DOC_METHOD_4(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name)
#define DOC_METHOD_5(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name, a4type, a4name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name, a4type, a4name)
#define DOC_METHOD_6(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name, a4type, a4name, a5type, a5name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name, a4type, a4name, a5type, a5name)

#define DOC_META_METHOD_0(ret, name, desc) AddMethod(LuaMemberType::MetaMethod, ret, name, desc)
#define DOC_META_METHOD_1(ret, name, desc, a0type, a0name) AddMethod(LuaMemberType::MetaMethod, ret, name, desc, a0type, a0name)
#define DOC_META_METHOD_2(ret, name, desc, a0type, a0name, a1type, a1name) AddMethod(LuaMemberType::MetaMethod, ret, name, desc, a0type, a0name, a1type, a1name)
#define DOC_META_METHOD_3(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name) AddMethod(LuaMemberType::MetaMethod, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name)
#define DOC_META_METHOD_4(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name) AddMethod(LuaMemberType::MetaMethod, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name)

#define DOC_FUNCTION_0(ret, name, desc) AddMethod(LuaMemberType::Function, ret, name, desc)
#define DOC_FUNCTION_1(ret, name, desc, a0type, a0name) AddMethod(LuaMemberType::Function, ret, name, desc, a0type, a0name)
#define DOC_FUNCTION_2(ret, name, desc, a0type, a0name, a1type, a1name) AddMethod(LuaMemberType::Function, ret, name, desc, a0type, a0name, a1type, a1name)
#define DOC_FUNCTION_3(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name) AddMethod(LuaMemberType::Function, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name)
#define DOC_FUNCTION_4(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name) AddMethod(LuaMemberType::Function, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name)

#define DOC_TABLE_PROPERTY(type, name, desc) AddProperty(LuaMemberType::TableProperty, type, name, desc)
#define DOC_OBJECT_PROPERTY(type, name, desc) AddProperty(LuaMemberType::ObjectProperty, type, name, desc)


namespace app
{

void InitLuaDoc()
{
    static bool done = false;
    if (done == true) return;

    // global objects
    DOC_TABLE2("_G", "Lua's built-in global data table.");
    DOC_TABLE_PROPERTY("game.Audio", "Audio", "Global audio engine instance.");
    DOC_TABLE_PROPERTY("game.Physics", "Physics", "Global physics engine instance.");
    DOC_TABLE_PROPERTY("game.ClassLibrary", "ClassLib", "Global class library instance.");
    DOC_TABLE_PROPERTY("game.KeyValueStore", "State", "Global key-value store instance.");
    DOC_TABLE_PROPERTY("game.Engine", "Game", "Global game engine instance.");
    DOC_TABLE_PROPERTY("game.Scene", "Scene", "Global scene instance or nil if no scene is being played.");
    DOC_FUNCTION_3("...", "CallMethod", "Call a method on an entity, scene or UI with variable arguments.",
                   "game.Entity|game.Scene|uik.Window", "object", "string", "method", "...", "...");
    DOC_TABLE_PROPERTY("unsigned", "SurfaceWidth", "Current rendering surface width in pixels.");
    DOC_TABLE_PROPERTY("unsigned", "SurfaceHeight", "Current rendering surface height in pixels.");
    DOC_TABLE_PROPERTY("unsigned", "Frame", "Currently running frame number. Starts at zero, can wrap over.");
    DOC_TABLE_PROPERTY("bool", "PreviewMode", "True when doing a preview of an entity, scene or UI.");
    DOC_TABLE_PROPERTY("bool", "EditingMode", "True when live edits to game content are enabled.");

    // Lua built-in functions.
    DOC_FUNCTION_2("void", "assert", "Raises an error if the value of its argument arg is false (i.e., nil or false); otherwise, returns all its arguments. "
                                   "In case of error, message is the error object; when absent, it defaults to \"assertion failed!\" string",
                 "bool|object", "arg", "object", "message = \"assertion failed\"");
    DOC_FUNCTION_2("void", "error", "Raises an error with message as the error object. This function never returns. <br>"
                                  "Usually, error adds some information about the error position at the beginning of the message, "
                                  "if the message is a string. The level argument specifies how to get the error position.<br>"
                                  "With level 1 (the default), the error position is where the error function was called. "
                                  "Level 2 points the error to where the function that called error was called; and so on.<br>"
                                  "Passing a level 0 avoids the addition of error position information to the message.",
                                  "string", "message", "int", "level");
    DOC_FUNCTION_1("string", "tostring", "Receives a value of any type and converts it to a string in a human-readable format.", "object", "v");
    DOC_FUNCTION_2("void", "warn", "Emits a warning with a message composed by the concatenation of all its arguments (which should be strings).",
                   "string", "msg", "string", "...");
    DOC_FUNCTION_1("void", "print", "Receives any number of arguments and prints their values to stdout, converting each argument to a string following the same rules of tostring. ",
                   "...", "...");
    DOC_TABLE_PROPERTY("string", "_VERSION", "The Lua version number.");

    DOC_TABLE("gfx");
    DOC_TABLE("gfx.MaterialClass");
    DOC_FUNCTION_0("string", "GetName", "Get the material class name.");
    DOC_FUNCTION_0("string", "GetId", "Get the material class ID.");

    DOC_TABLE("gfx.Material");

    DOC_TABLE("util");
    DOC_FUNCTION_1("float", "GetRotationFromMatrix", "Get the rotational component from the given matrix.",
                 "glm.mat4", "matrix");
    DOC_FUNCTION_1("glm.vec2", "GetScaleFromMatrix", "Get the scale component from the given matrix.",
                 "glm.vec2", "matrix");
    DOC_FUNCTION_1("glm.vec2", "GetTranslationFromMatrix", "Get the translation component from the given matrix.",
                 "glm.vec2", "matrix");
    DOC_FUNCTION_2("glm.vec2", "RotateVector",  "Transform a vector through a rotation matrix based on the given angle in radians.",
                 "glm.vec2", "vector", "float", "angle");
    DOC_FUNCTION_1("glm.vec2", "ToVec2", "glm.vec2 conversion helper", "base.FPoint", "point");
    DOC_FUNCTION_1("base.FPoint", "ToPoint", "base.FPoint conversion  helper", "glm.vec2", "vec2");

    DOC_FUNCTION_0("number", "GetSeconds", "Get elapsed time in fractional seconds since undefined epoch.");
    DOC_FUNCTION_0("unsigned", "GetMilliseconds", "Get elapsed time in milliseconds since undefined epoch.");

    DOC_FUNCTION_1("void", "RandomSeed", "Seed the random engine with the given seed value.<br>"
                                     "For any given seed the the generated pseudo random number sequence will always be same on every platform.",
                                     "int", "seed");
    DOC_FUNCTION_2("int|float", "Random", "Generate a new pseudo random number between the given (inclusive) min/max values.<br>",
                 "int|float", "min", "int|float", "max");
    DOC_FUNCTION_2("string", "JoinPath", "Concatenate file system paths together.<br>"
                                       "No assumption is made regarding the validity of the paths.",
                 "string", "a", "string", "b");
    DOC_FUNCTION_1("bool", "FileExists", "Check whether the given file exists on the file system or not. <br>"
                                       "The given filename is expected to be UTF-8 encoded."
                                       "Returns true if the file exists otherwise false. ",
                 "string", "filename");
    DOC_FUNCTION_1("string", "RandomString", "Generate a random alpha numeric string of specified length.<br>"
                                           "Useful for things such as pseudo-unique identifiers.",
                 "unsigned", "length");
    DOC_FUNCTION_2("string", "FormatString", "Format a string with %1, %2,...%n placeholders with N variable arguments.<br>"
                                           "For example: FormatString('this is %1 that is %2', 123, 'foo') returns 'this is 123 that is foo'.<br>"
                                           "Any given index can be repeated multiple times.",
                 "string", "fmt", "bool|int|float|string|glm.vec2|glm.vec3|glm.vec4|base.Color4f|base.FSize|base.FRect|base.FPoint", "...");
    DOC_FUNCTION_2("string", "Join", "Concatenate and join the items in a string array together with a separator.",
                   "util.StringArrayInterface", "array", "string", "separator");

    DOC_TABLE("util.RandomEngine");
    DOC_METHOD_1("void", "RandomSeed", "Seed the random engine with the given seed value.<br>"
                                       "For any given seed the generated pseudo random number sequence will always be the same on every platform.",
                 "int", "seed");
    DOC_METHOD_2("int|float", "Random", "Generate a new pseudo random number between the given (inclusive) min/max values.<br>",
                 "int|float", "min", "int|float", "max");

    DOC_TABLE("util.FBox");
    DOC_METHOD_2("util.FBox", "new", "Construct a new object aligned FBox.",
                 "float", "width", "float", "height");
    DOC_METHOD_3("util.FBox", "new", "Construct a new object aligned FBox.",
                 "glm.mat4", "transform", "float", "width", "float", "height");
    DOC_METHOD_1("util.FBox", "new", "Construct a new object aligned FBox. Assumes width=1.0 and height=1.0.",
                 "glm.mat4", "transform");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the box.");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the box.");
    DOC_METHOD_0("float", "GetTopLeft", "Get the the top left corner position.");
    DOC_METHOD_0("float", "GetTopRight", "Get the top right corner position.");
    DOC_METHOD_0("float", "GetBotLeft", "Get the bottom left corner position.");
    DOC_METHOD_0("float", "GetBotRight", "Get the bottom right corner position.");
    DOC_METHOD_0("float", "GetCenter", "Get the position of the center of the box.");
    DOC_METHOD_0("glm.vec2", "GetSize", "Get the size of the box.");
    DOC_METHOD_0("float", "GetRotation", "Get the rotation of the box.");
    DOC_METHOD_1("void", "Transform", "Transform this box by the given transformation matrix.",
                 "glm.mat4", "matrix");
    DOC_METHOD_0("void", "Reset", "Reset the FBox to a unit box with 1.0 width and height.");
    DOC_METHOD_2("void", "Reset", "Reset the FBox to a box with the given with and height.",
                 "float", "width", "float", "height");

    DOC_TABLE("util.IntArrayInterface");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("int", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "int", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("int", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("int", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "int", "value");
    DOC_TABLE("util.FloatArrayInterface");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("float", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "float", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("float", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("float", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "float", "value");
    DOC_TABLE("util.BoolArrayInterface");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("bool", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "bool", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("bool", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("bool", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "bool", "value");
    DOC_TABLE("util.StringArrayInterface");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("string", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "string", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("string", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("string", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "string", "value");
    DOC_TABLE("util.Vec2ArrayInterface");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("glm.vec2", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "glm.vec2", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("glm.vec2", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("glm.vec2", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "glm.vec2", "value");
    DOC_TABLE("util.MaterialRefArray");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the array is empty or not.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the array.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether the array is read only.");
    DOC_METHOD_1("glm.vec2", "GetItem", "Get an array item at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_2("void", "SetItem", "Set an array item to a new value at the given index. The index must be valid.", "unsigned", "index", "glm.vec2", "value");
    DOC_METHOD_0("void", "PopBack", "Pop the last item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("void", "PopFront", "Pop the first item off of the array. If the array is empty nothing is done.");
    DOC_METHOD_0("glm.vec2", "First", "Get the first item in the array. The array must not be empty.");
    DOC_METHOD_0("glm.vec2", "Last", "Get the last item in the array. The array must not be empty.");
    DOC_METHOD_1("void", "PushBack", "Push back (append) a new item at the end of the array.", "glm.vec2", "value");

    DOC_TABLE("base");
    DOC_FUNCTION_1("void", "debug", "Print a debug message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "warn", "Print a warning message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "error", "Print an error message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "info", "Print an information message in the application log.",
                 "string", "message");
    DOC_FUNCTION_3("float|int", "clamp", "Clamp a value to a min/max range.",
                   "float|int", "min", "float|int", "max", "float|int", "value");
    DOC_FUNCTION_3("float|int", "wrap", "Wrap a value from min to max and from max to min.",
                   "float|int", "min", "float|int", "max", "float|int", "value");

    DOC_TABLE("easing");
    DOC_METHOD_2("float", "adjust", "Adjust the value of t based on the easing curve.",
                                    "float", "t", "easing.Curves", "curve");
    DOC_METHOD_2("float", "adjust", "Adjust the value of t based on the easing curve.",
                 "float", "t", "string", "curve");
    DOC_TABLE("easing.Curves");
    for (const auto& value : magic_enum::enum_values<math::Interpolation>())
    {
        const std::string name(magic_enum::enum_name(value));
        DOC_TABLE_PROPERTY("int", QString::fromStdString(name), app::toString("Easing curve value for '%1'.", name));
    }

    DOC_TABLE("trace");
    DOC_FUNCTION_1("void", "marker", "Set a marker message in the application trace.",
                 "string", "message");
    DOC_FUNCTION_2("void", "marker", "Set a marker message in the application trace in the given trace entry.<br>"
                                   "The given trace entry index MUST BE VALID.<br>"
                                   "Do not call this function unless you know what you're doing.<br>"
                                   "For a safer alternative use the overload without index.",
                 "string", "message",
                 "unsigned", "index");
    DOC_FUNCTION_1("unsigned", "enter", "Enter a new tracing scope for measuring time spent inside the scope.<br>"
                                 "You must manually call trace.leave with index that you received from this call. "
                                 "Not doing so will likely crash the application. ",
                 "string", "scope_name");
    DOC_FUNCTION_1("void", "leave", "Leave a tracing scope that was entered previously.<br>"
                                  "The index must be from a previous call to trace.enter.",
                 "unsigned", "index");

    DOC_TABLE("base.FRect");
    DOC_METHOD_0("base.FRect", "new", "Construct a new axis aligned rectangle without any size.");
    DOC_METHOD_4("base.FRect", "new", "Construct a new axis aligned rectangle.",
                 "float", "x", "float", "y", "float", "width", "float", "height");
    DOC_METHOD_0("base.FRect", "Copy", "Create a copy of this object.");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the rectangle.");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the rectangle.");
    DOC_METHOD_0("float", "GetX", "Get the X position of the rectangle.");
    DOC_METHOD_0("float", "GetY", "Get the Y position of the rectangle.");
    DOC_METHOD_1("void", "SetX", "Set a new X position for the rectangle.", "float", "x");
    DOC_METHOD_1("void", "SetY", "Set a new Y position for the rectangle.", "float", "y");
    DOC_METHOD_1("void", "SetWidth", "Set a new rectangle width.", "float", "width");
    DOC_METHOD_1("void", "SetHeight", "Set a new rectangle height.", "float", "height");
    DOC_METHOD_2("void", "Resize", "Resize the rectangle to new width and height.", "float", "width", "float", "height");
    DOC_METHOD_1("void", "Resize", "Resize the rectangle to new width and height.", "base.FSize|glm.vec2", "size");
    DOC_METHOD_2("void", "Grow", "Grow (or shrink) the dimensions of the rectangle.", "float", "dx", "float", "dy");
    DOC_METHOD_1("void", "Grow", "Grow (or shrink) the dimensions of the rectangle.", "base.FSize|glm.vec2", "delta");
    DOC_METHOD_2("void", "Move", "Move the rectangle to a new position.", "float", "x", "float", "y");
    DOC_METHOD_1("void", "Move", "Move the rectangle to a new position.", "base.FPoint|glm.vec2", "pos");
    DOC_METHOD_2("void", "Translate", "Translate (offset) the rectangle relative to the current position.", "float", "dx", "float", "dy");
    DOC_METHOD_1("void", "Translate", "Translate (offset) the rectangle relative to the current position.", "base.FPoint|glm.vec2", "translate");
    DOC_METHOD_0("bool", "IsEmpty", "Returns true if the rectangle is empty.<br>An empty rectangle has either zero width or height.");
    DOC_METHOD_2("bool", "TestPoint", "Test whether the given point is inside the rectangle or not.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("bool", "TestPoint", "Test whether the given point is inside the rectangle or not.",
                 "base.FPoint|glm.vec2", "point");
    DOC_METHOD_2("base.FPoint", "MapToGlobal", "Map a local point relative to the rect origin to a global point.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("base.FPoint", "MapToGlobal", "Map a local point relative to the rect origin to a global point.",
                 "base.FPoint|glm.vec2", "point");
    DOC_METHOD_2("base.FPoint", "MapToLocal", "Map a global point to a local point relative to the rect origin.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("base.FPoint", "MapToLocal", "Map a global point to a local point relative to the rect origin.",
                 "base.FPoint|glm.vec2", "point");
    DOC_METHOD_0("base.FRect, base.FRect, base.FRect, base.FRect", "GetQuadrants",
                 "Split the rectangle into 4 quadrants.<br>"
                 "The returned quadrants are top_left, bottom_left, top_right, bottom_right");
    DOC_METHOD_0("base.FPoint, base.FPoint, base.FPoint, base.FPoint", "GetCorners",
                 "Get the 4 corners of the rectangle.<br>"
                 "The corners are top_left, bottom_left, top_right, bottom_right");
    DOC_METHOD_0("base.FPoint", "GetCenter", "Get the center point of the rectangle.");
    DOC_FUNCTION_2("base.FRect", "Combine", "Create an union of the given rectangles.<br>"
                                            "If a is an empty rect then b is returned.<br>"
                                            "If b is an empty rect then a is returned.<br>",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_FUNCTION_2("base.FRect", "Intersect", "Create an intersection of the given rectangles.<br>",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_FUNCTION_2("bool", "TestIntersect", "Test whether the rectangles intersect.<br>"
                                            "If either rect is an empty rect then an empty rect is returned.",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.",
                 "base.FRect", "rect");

    DOC_TABLE("base.FSize");
    DOC_METHOD_0("base.FSize", "new", "Construct a new size with zero width and height.");
    DOC_METHOD_2("base.FSize", "new", "Construct a new size with the given width and height.",
               "float", "width", "float", "height");
    DOC_METHOD_0("base.FSize", "Copy", "Create a copy of this object.");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the size.");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the size.");
    DOC_METHOD_0("bool", "IsZero", "Check whether the size is a zero size. A size is zero when it has no width or height.");
    DOC_META_METHOD_2("base.FSize", "operator *", "Lua multiplication meta method.", "base.FSize", "size", "float", "scalar");
    DOC_META_METHOD_2("base.FSize", "operator +", "Lua addition meta method.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_META_METHOD_2("base.FSize", "operator -", "Lua subtraction meta method.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "base.FSize", "size");

    DOC_TABLE("base.FPoint");
    DOC_METHOD_0("base.FPoint", "new", "Construct a new point with zero x, y position.");
    DOC_METHOD_2("base.FPoint", "new", "Construct a new point with the given x,y position.", "float", "x", "float", "y");
    DOC_METHOD_0("base.FPoint", "Copy", "Create a copy of this object.");
    DOC_METHOD_0("float", "GetX", "Get the x position.");
    DOC_METHOD_0("float", "GetY", "Get the y position.");
    DOC_METHOD_1("void", "SetX", "Set the x position.", "float", "x");
    DOC_METHOD_1("void", "SetY", "Set the y position.", "float", "x");
    DOC_META_METHOD_2("base.FPoint", "operator +", "Lua addition meta method.", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_META_METHOD_2("base.FPoint", "operator -", "Lua subtraction meta method.", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "base.FPoint", "point");
    DOC_FUNCTION_2("float", "Distance", "Compute the actual distance between two points.", "base.FPoint", "a", "base.FPoint", "b");
    DOC_FUNCTION_2("float", "SquareDistance", "Compute the square distance between two points. This function offers better performance when "
                                              "the actual distance is not needed but only a value that can be compared to other squared distances. "
                                              "This is sufficient when for example finding the object closest to any other object and the actual distance is irrelevant.",
                   "base.FPoint", "a", "base.FPoint", "b");

    DOC_TABLE("base.Colors");
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        DOC_TABLE_PROPERTY("int", QString::fromStdString(name), app::toString("Color value for '%1'.", name));
    }

    DOC_TABLE("base.Color4f");
    DOC_METHOD_0("base.Color4f", "new", "Construct a new color with default channel value.");
    DOC_METHOD_4("base.Color4f", "new", "Construct a new color with normalized float [0.0, 1.0] channel values.",
                 "float", "r", "float", "g", "float", "b", "float", "a");
    DOC_METHOD_4("base.Color4f", "new", "Construct a new color with int [0, 255] channel values.",
                 "int", "r", "int", "g", "int", "b", "int", "a");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "base.Color4f", "color");
    DOC_METHOD_0("base.Color4f", "Copy", "Create a copy of this object.");
    DOC_METHOD_0("float", "GetRed", "Get normalized red channel value.");
    DOC_METHOD_0("float", "GetGreen", "Get normalized green channel value.");
    DOC_METHOD_0("float", "GetBlue", "Get normalized blue channel value.");
    DOC_METHOD_0("float", "GetAlpha", "Get normalized alpha channel value.");
    DOC_METHOD_1("void", "SetRed", "Set normalized red channel value.", "float", "red");
    DOC_METHOD_1("void", "SetGreen", "Set normalized green channel value.", "float", "green");
    DOC_METHOD_1("void", "SetBlue", "Set normalized blue channel value.", "float", "blue");
    DOC_METHOD_1("void", "SetAlpha", "Set normalized alpha channel value.", "float", "alpha");
    DOC_METHOD_1("void", "SetColor", "Set color based on base.Colors color value.", "base.Colors", "color");
    DOC_METHOD_1("void", "SetColor", "Set color based on base.Colors color name.", "string", "color");
    DOC_FUNCTION_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color value.", "base.Colors", "color");
    DOC_FUNCTION_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color name.", "string", "color");

    DOC_TABLE("data.Reader");
    DOC_METHOD_1("bool, float", "ReadFloat", "Read a float value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, int", "ReadInt", "Read an int value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, bool", "ReadBool", "Read a bool value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, string", "ReadString", "Read a string value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, glm.vec2", "ReadVec2", "Read a glm.vec2 value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, glm.vec3", "ReadVec3", "Read a glm.vec3 value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, glm.vec4", "ReadVec4", "Read a glm.vec4 value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, base.FRect", "ReadFRect", "Read a base.FRect value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, base.FPoint", "ReadFPoint", "Read a base.FPoint value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, base.FSize", "ReadFSize", "Read a base.FSize value from the data chunk.", "string", "key");
    DOC_METHOD_1("bool, base.Color4f", "ReadColor4f", "Read a base.Color4f value from the data chunk.", "string", "key");
    DOC_METHOD_2("bool, float", "Read", "Read a float value from the data chunk.", "string", "key", "float", "default");
    DOC_METHOD_2("bool, int", "Read", "Read an int value from the data chunk.", "string", "key", "int", "default");
    DOC_METHOD_2("bool, bool", "Read", "Read a bool value from the data chunk.", "string", "key", "bool", "default");
    DOC_METHOD_2("bool, string", "Read", "Read a string value from the data chunk.", "string", "key", "string", "default");
    DOC_METHOD_2("bool, glm.vec2", "Read", "Read a glm.vec2 value from the data chunk.", "string", "key", "glm.vec2", "default");
    DOC_METHOD_2("bool, glm.vec3", "Read", "Read a glm.vec3 value from the data chunk.", "string", "key", "glm.vec3", "default");
    DOC_METHOD_2("bool, glm.vec4", "Read", "Read a glm.vec4 value from the data chunk.", "string", "key", "glm.vec4", "default");
    DOC_METHOD_2("bool, base.FRect", "Read", "Read a base.FRect value from the data chunk.", "string", "key", "base.FRect", "default");
    DOC_METHOD_2("bool, base.FPoint", "Read", "Read a base.FPoint value from the data chunk.", "string", "key", "base.FPoint", "default");
    DOC_METHOD_2("bool, base.FSize", "Read", "Read a base.FSize value from the data chunk.", "string", "key", "base.FSize", "default");
    DOC_METHOD_2("bool, base.Color4f", "Read", "Read a base.Color4f value from the data chunk.", "string", "key", "base.Color4f", "default");
    DOC_METHOD_1("bool", "HasValue", "Check whether the given key exists in the data chunk or not.","string", "key");
    DOC_METHOD_1("bool", "HasChunk", "Check whether a data chunk by the given key exists or not.","string", "key");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the data chunk is empty or not.<br>"
                                   "A data chunk is considered empty when it has no values or child data chunks.");
    DOC_METHOD_1("unsigned", "GetNumChunks", "Get the number of data chunks under the given key.","string", "key");
    DOC_METHOD_2("data.Reader", "GetReadChunk", "Get a read chunk at the given index under the given key.<br>"
                                                "Returns a new data reader object for that chunk.<br>"
                                                "Both key and index must be valid.",
                 "string", "key", "unsigned", "index");

    DOC_TABLE("data.Writer");
    DOC_METHOD_2("void", "Write", "Write a float value to the data chunk.", "string", "key", "float", "value");
    DOC_METHOD_2("void", "Write", "Write an int value to the data chunk.", "string", "key", "int", "value");
    DOC_METHOD_2("void", "Write", "Write a bool value to the data chunk.", "string", "key", "bool", "value");
    DOC_METHOD_2("void", "Write", "Write a string value to the data chunk.", "string", "key", "string", "value");
    DOC_METHOD_2("void", "Write", "Write a glm.vec2 value to the data chunk.", "string", "key", "glm.vec2", "value");
    DOC_METHOD_2("void", "Write", "Write a glm.vec3 value to the data chunk.", "string", "key", "glm.vec3", "value");
    DOC_METHOD_2("void", "Write", "Write a glm.vec4 value to the data chunk.", "string", "key", "glm.vec4", "value");
    DOC_METHOD_2("void", "Write", "Write a base.FRect value to the data chunk.", "string", "key", "base.FRect", "value");
    DOC_METHOD_2("void", "Write", "Write a base.FPoint value to the data chunk.", "string", "key", "base.FPoint", "value");
    DOC_METHOD_2("void", "Write", "Write a base.FSize value to the data chunk.", "string", "key", "base.FSize", "value");
    DOC_METHOD_2("void", "Write", "Write a base.Color4f value to the data chunk.", "string", "key", "base.Color4f", "value");
    DOC_METHOD_1("bool", "HasValue", "Check whether the given key exists in the data chunk or not.", "string", "key");
    DOC_METHOD_0("data.Writer", "NewWriteChunk", "Create a new writer object for a new data chunk.");
    DOC_METHOD_2("void", "AppendChunk", "Append the data chunk to a chunk array under the given key.",
                 "string", "key", "data.Writer", "chunk");
    DOC_METHOD_1("bool, string", "WriteFile", "Write the contents of this writer chunk to a file.<br>"
                                              "Returns true and an empty string on success or false and error string on error.",
                 "string", "file");

    DOC_TABLE("data.JsonObject");
    DOC_METHOD_0("data.JsonObject", "new", "Construct a new JsonObject.<br>"
                                           "A JsonObject is both a data.Reader and data.Writer so you can call all those methods on it.");
    DOC_METHOD_1("bool, string", "ParseString", "Try to parse the given JSON string.<br>"
                                                "Returns true and an empty string on success or false and error string on error.",
                 "string", "json");
    DOC_METHOD_0("string", "ToString", "Dump the contents of the JsonObject into a string.");

    DOC_TABLE("data");
    DOC_METHOD_1("data.JsonObject, string", "ParseJsonString", "Create a new JsonObject based on the JSON string.<br>"
                                                               "Returns a new JsonObject object and an empty string on success or nil and an error string on error.",
                 "string", "json");
    DOC_FUNCTION_2("data.JsonObject, string", "ParseJsonString", "Create a new JsonObject based on the JSON data buffer.<br>"
                                                                 "Returns a new JsonObject and an empty string on success or nil and an error string on error.",
                 "todo", "json_data", "size_t", "data_len");
    DOC_FUNCTION_2("bool, string", "WriteJsonFile", "Write the contents of the JsonObject into a file.<br>"
                                                    "Returns true and an empty string on success or false and error string on error.",
                 "data.JsonObject", "json", "string", "filename");
    DOC_FUNCTION_1("data.JsonObject, string", "ReadJsonFile", "Try to read the given JSON file. <br>"
                                                              "Returns new JsonObject object and en empty string on success or nil and error string on error.",
                 "string", "filename");
    DOC_FUNCTION_1("data.Writer", "CreateWriter", "Create a new data.Writer object based on the given format string.<br>"
                                                  "Format string can be one of the following: 'JSON'<br>"
                                                  "Returns nil on unsupported format.",
                 "string", "format");
    DOC_FUNCTION_2("bool, string", "WriteFile", "Dump the contents of the given Writer into a file.<br>"
                                                "Returns true and en empty string on success or false and an error string on error.",
                 "data.Writer", "data", "string", "filename");
    DOC_FUNCTION_1("data.Reader, string", "ReadFile", "Try to read the given file in some supported format.<br>"
                                                      "Currently supported formats: JSON.<br>"
                                                      "Returns a new data.Reader object and an empty string on success or nil and an error string on error.",
                 "string", "filename");

    DOC_TABLE("glm");
    DOC_FUNCTION_2("glm.vec2", "dot", "Compute the dot product between two vectors.", "glm.vec2", "a", "glm.vec2", "b");
    DOC_FUNCTION_2("glm.vec3", "dot", "Compute the dot product between two vectors.", "glm.vec3", "a", "glm.vec3", "b");
    DOC_FUNCTION_2("glm.vec4", "dot", "Compute the dot product between two vectors.", "glm.vec4", "a", "glm.vec4", "b");
    DOC_FUNCTION_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec2", "vec");
    DOC_FUNCTION_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec3", "vec");
    DOC_FUNCTION_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec4", "vec");
    DOC_FUNCTION_1("glm.vec2", "normalize", "Return a normalized copy of the vector.", "glm.vec2", "vec");
    DOC_FUNCTION_1("glm.vec3", "normalize", "Return a normalized copy of the vector.", "glm.vec3", "vec");
    DOC_FUNCTION_1("glm.vec4", "normalize", "Return a normalized copy of the vector.", "glm.vec4", "vec");

    DOC_TABLE("glm.mat4");
    DOC_FUNCTION_0("glm.vec2, glm.vec2, float", "decompose", "Decompose the given 4x4 transformation matrix.<br>"
                                                           "Returns: <br>"
                                                           "a glm.vec2 with the translation coefficients.<br>"
                                                           "a glm.vec2 with the scale coefficients.<br>"
                                                           "a float with the rotation around Z axis in radians.");

    DOC_TABLE("glm.vec2");
    DOC_METHOD_0("glm.vec2", "new", "Construct a new glm.vec2.");
    DOC_METHOD_2("glm.vec2", "new", "Construct a new glm.vec2.", "float", "x", "float", "y");
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec2", "vec", "unsigned", "index");
    DOC_META_METHOD_2("glm.vec2", "operator +", "Lua addition meta method.", "glm.vec2", "a", "glm.vec2", "b");
    DOC_META_METHOD_2("glm.vec2", "operator -", "Lua subtraction meta method.", "glm.vec2", "a", "glm.vec2", "b");
    DOC_META_METHOD_2("glm.vec2", "operator *", "Lua multiplication meta method.", "glm.vec2|float", "a", "glm.vec2|float", "b");
    DOC_META_METHOD_2("glm.vec2", "operator /", "Lua division meta method.", "glm.vec2|float", "a", "glm.vec2|float", "b");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "glm.vec2", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec2", "normalize", "Return a normalized copy of the vector.");
    DOC_OBJECT_PROPERTY("float", "x", "X component of the vector.");
    DOC_OBJECT_PROPERTY("float", "y", "Y component of the vector.");

    DOC_TABLE("glm.vec3");
    DOC_METHOD_0("glm.vec3", "new", "Construct a new glm.vec3.");
    DOC_METHOD_3("glm.vec3", "new", "Construct a new glm.vec3.", "float", "x", "float", "y", "float", "z");
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec3", "vec", "unsigned", "index");
    DOC_META_METHOD_2("glm.vec3", "operator +", "Lua addition meta method.", "glm.vec3", "a", "glm.vec3", "b");
    DOC_META_METHOD_2("glm.vec3", "operator -", "Lua subtraction meta  method.", "glm.vec3", "a", "glm.vec3", "b");
    DOC_META_METHOD_2("glm.vec3", "operator *", "Lua multiplication meta method.", "glm.vec3|float", "a", "glm.vec3|float", "b");
    DOC_META_METHOD_2("glm.vec3", "operator /", "Lua division meta method.", "glm.vec3|float", "a", "glm.vec3|float", "b");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "glm.vec3", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec3", "normalize", "Return a normalized copy of the vector.");
    DOC_OBJECT_PROPERTY("float", "x", "X component of the vector.");
    DOC_OBJECT_PROPERTY("float", "y", "Y component of the vector.");
    DOC_OBJECT_PROPERTY("float", "z", "Z component of the vector.");

    DOC_TABLE("glm.vec4");
    DOC_METHOD_0("glm.vec4", "new", "Construct a new glm.vec3.");
    DOC_METHOD_4("glm.vec4", "new", "Construct a new glm.vec3.", "float", "x", "float", "y", "float", "z", "float", "w");
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec4", "vec", "unsigned", "index");
    DOC_META_METHOD_2("glm.vec4", "operator +", "Lua addition meta method.", "glm.vec4", "a", "glm.vec4", "b");
    DOC_META_METHOD_2("glm.vec4", "operator -", "Lua subtraction meta method.", "glm.vec4", "a", "glm.vec4", "b");
    DOC_META_METHOD_2("glm.vec4", "operator *", "Lua multiplication meta method.", "glm.vec4|float", "a", "glm.vec4|float", "b");
    DOC_META_METHOD_2("glm.vec4", "operator /", "Lua division meta method.", "glm.vec4|float", "b", "glm.vec4|float", "b");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "glm.vec4", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec4", "normalize", "Return a normalized copy of the vector.");
    DOC_OBJECT_PROPERTY("float", "x", "X component of the vector.");
    DOC_OBJECT_PROPERTY("float", "y", "Y component of the vector.");
    DOC_OBJECT_PROPERTY("float", "z", "Z component of the vector.");
    DOC_OBJECT_PROPERTY("float", "w", "W component of the vector.");

    DOC_TABLE("wdk");
    DOC_FUNCTION_1("string", "KeyStr", "Convert a key symbol to a named key string.", "wdk.Keys", "key");
    DOC_FUNCTION_1("string", "BtnStr", "Convert a mouse button to a named button string.", "wdk.Buttons", "button");
    DOC_FUNCTION_1("string", "ModStr", "Convert a modifier key symbol to a named modifier string.", "wdk.Mods", "modifier");
    DOC_FUNCTION_1("string", "ModBitStr", "Map keyboard key modifier bit set to a named modifier string.", "unsigned", "modifier_bits");
    DOC_FUNCTION_1("bool", "TestKeyDown", "Test whether the given keyboard key is currently down.<br>"
                                        "The key value is one of the symbolic keys in wdk.Keys. <br>"
                                        "This function is only available on desktop Windows and Linux. ",
                                        "wdk.Keys", "key");
    DOC_FUNCTION_2("bool", "TestMod", "Test whether the given modifier bit is set in the bitset of modifier keys.",
                 "unsigned", "modifier_bits", "wdk.Mods", "modifier");

    DOC_TABLE("wdk.Keys");
    for (const auto& key : magic_enum::enum_values<wdk::Keysym>())
    {
        const std::string name(magic_enum::enum_name(key));
        DOC_TABLE_PROPERTY("unsigned", QString::fromStdString(name), app::toString("Key value for '%1'.", name));
    }
    DOC_TABLE("wdk.Mods");
    for (const auto& mod : magic_enum::enum_values<wdk::Keymod>())
    {
        const std::string name(magic_enum::enum_name(mod));
        DOC_TABLE_PROPERTY("unsigned", QString::fromStdString(name), app::toString("Modifier value for '%1'.", name));
    }
    DOC_TABLE("wdk.Buttons");
    for (const auto& btn : magic_enum::enum_values<wdk::MouseButton>())
    {
        const std::string name(magic_enum::enum_name(btn));
        DOC_TABLE_PROPERTY("unsigned", QString::fromStdString(name), app::toString("Mouse button value for '%1'.", name));
    }
    DOC_TABLE("wdk.KeyBitSet");
    DOC_METHOD_0("wdk.KeyBitSet", "new", "Construct new key symbol bit set.");
    DOC_METHOD_2("void", "Set", "Set a key symbol bit on or off.", "unsigned", "key", "bool", "on");
    DOC_METHOD_1("bool", "Test", "Test whether a key symbol bit is on or off.", "unsigned", "key");
    DOC_METHOD_0("bool", "AnyBit", "Check whether any bit is set.");
    DOC_METHOD_0("void", "Clear", "Clear all bits.");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator &", "Lua bitwise and meta method.", "wdk.KeyBitSet", "lhs", "wdk.KeyBitSet", "rhs");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator &", "Lua bitwise and meta method.", "wdk.KeyBitSet", "bits", "unsigned", "key");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator |", "Lua bitwise or meta method.", "wdk.KeyBitSet", "lhs", "wdk.KeyBitSet", "rhs");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator |", "Lua bitwise or meta method.", "wdk.KeyBitSet", "bits", "unsigned", "key");

    DOC_TABLE("uik");
    DOC_TABLE("uik.Widget");
    DOC_METHOD_0("string", "GetId", "Get the widget ID.");
    DOC_METHOD_0("string", "GetName", "Get the widget name.");
    DOC_METHOD_0("size_t", "GetHash", "Get the widget hash value based on its contents.");
    DOC_METHOD_0("base.FSize", "GetSize", "Get the size of the widget.");
    DOC_METHOD_0("base.FPoint", "GetPosition", "Get the widget position relative to its parent.");
    DOC_METHOD_0("string", "GetType", "Get the dynamic name of the widget object type.");
    DOC_METHOD_1("void", "SetName", "Set the widget name.", "string", "name");
    DOC_METHOD_1("void", "SetSize", "Set the widget size.", "base.FSize", "size");
    DOC_METHOD_2("void", "SetSize", "Set the widget size.", "float", "width", "float", "height");
    DOC_METHOD_1("void", "SetPosition", "Set the widget position relative to its parent.", "base.FPoint", "position");
    DOC_METHOD_2("void", "SetPosition", "Set the widget position relative to its parent.", "float", "x", "float", "y");
    DOC_METHOD_1("bool", "TestFlag", "Test for a widget flag.", "string", "flag_name");
    DOC_METHOD_0("bool", "IsEnabled", "Check whether the widget is enabled or not.");
    DOC_METHOD_0("bool", "IsVisible", "Check whether the widget is visible or not.");
    DOC_METHOD_2("void", "Grow", "Grow the widget in size.", "float", "dw", "float", "dh");
    DOC_METHOD_2("void", "Translate", "Translate the widget relative to it's current position.", "float", "dx", "float", "dy");
    DOC_METHOD_1("void", "SetVisible", "Change widget visibility.", "bool", "visible");
    DOC_METHOD_1("void", "Enable", "Enable widget.", "bool", "enable");
    DOC_METHOD_2("void", "SetStyleProperty", "Set a painter specific styling property value on the widget.<br>"
                                             "These style properties take precedence over any other styling.",
                 "string", "key", "int|float|bool|string|base.Color4f", "value");
    DOC_METHOD_1("void", "DeleteStyleProperty", "Delete a specific styling property by the given key.",
                 "string", "key");
    DOC_METHOD_1("int|float|bool|string|base.Color4f", "GetStyleProperty", "Get a styling property by the given key.<br>"
                                                                           "Returns nil if no such property exists.",
                 "string", "key");
    DOC_METHOD_2("void", "SetStyleMaterial", "Set a painter specific material style property string.",
                 "string", "key", "string", "material_style_string");
    DOC_METHOD_1("void", "DeleteStyleMaterial", "Delete a styling material property by the given key.", "string", "key");
    DOC_METHOD_1("string", "GetStyleMaterial", "Get a material styling property by the given key.<br>"
                                               "Returns nil if no such material key exists.", "string", "key");
    DOC_METHOD_2("void", "SetColor", "Set a widget material by the given key to a solid color material.",
                 "string", "key", "base.Color4f", "color");
    DOC_METHOD_2("void", "SetMaterial", "Set a widget material to a material identified by its class ID or name.",
                 "string", "key", "string", "material");
    DOC_METHOD_5("void", "SetGradient", "Set a widget material to a color gradient material.",
                 "string", "key", "base.Color4f", "top_left", "base.Color4f", "top_right", "base.Color4f", "bottom_left", "base.Color4f", "bottom_right");

    DOC_METHOD_0("uik.Label", "AsLabel", "Cast the widget to Label. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.PushButton", "AsPushButton", "Cast the widget to PushButton. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.CheckBox", "AsCheckBox", "Cast the widget to CheckBox. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.GroupBox", "AsGroupBox", "Cast the widget to GroupBox. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.SpinBox", "AsSpinBox", "Cast the widget to SpinBox. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.ProgressBar", "AsProgressBar", "Cast the widget to ProgressBar. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.Form", "AsForm", "Cast the widget to Form. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.Slider", "AsSlider", "Cast the widget to Slider. Returns nil if the cast failed.");
    DOC_METHOD_0("uik.RadioButton", "AsRadioButton", "Cast the widget to RadioButton. Returns nil if the cast failed.");
    DOC_TABLE("uik.Label");
    DOC_METHOD_0("string", "GetText", "Get the label text.");
    DOC_METHOD_1("void", "SetText", "Set the label text.", "string", "text");
    DOC_TABLE("uik.CheckBox");
    DOC_METHOD_0("string", "GetText", "Get the checkbox text.");
    DOC_METHOD_1("void", "SetText", "Set the checkbox text.", "string", "text");
    DOC_METHOD_0("bool", "IsChecked", "Check whether the checkbox is checked or not.");
    DOC_METHOD_1("void", "SetChecked", "Set the checkbox check value", "bool", "checked");
    DOC_TABLE("uik.GroupBox");
    DOC_METHOD_0("string", "GetText", "Get the groupbox text.");
    DOC_METHOD_1("void", "SetText", "Set the groupbox text.", "string", "text");
    DOC_TABLE("uik.PushButton");
    DOC_METHOD_0("string", "GetText", "Get the pushbutton text.");
    DOC_METHOD_1("void", "SetText", "Set the pushbutton text.", "string", "text");
    DOC_TABLE("uik.ProgressBar");
    DOC_METHOD_0("string", "GetText", "Get the progress bar text.");
    DOC_METHOD_1("void", "SetText", "Set the progress bar text.", "string", "text");
    DOC_METHOD_0("void", "ClearValue", "Clear progress bar progress value. <br>"
                                       "After this the progress bar has no progress value and will show a busy indicator instead.");
    DOC_METHOD_1("void", "SetValue", "Set the normalized progress bar progress value.", "float", "value");
    DOC_METHOD_0("bool", "HasValue", "Check whether progress bar currently has a progress value or not.");
    DOC_METHOD_0("float", "GetValue", "Get the current progress bar value if any. If no progress value is set then 0.0 is returned.");
    DOC_TABLE("uik.SpinBox");
    DOC_METHOD_1("void", "SetMin", "Set the minimum value allowed by the spin box.", "int", "min");
    DOC_METHOD_1("void", "SetMax", "Set the maximum value allowed by the spin box.", "int", "max");
    DOC_METHOD_1("void", "SetValue", "Set the current spin box value.", "int", "value");
    DOC_METHOD_0("int", "GetMin", "Get the minimum value allowed by the spin box.");
    DOC_METHOD_0("int", "GetMax", "Get the maximum value allowed by the spin box.");
    DOC_METHOD_0("int", "GetValue", "Get the current spin box value.");
    DOC_TABLE("uik.Slider");
    DOC_METHOD_1("void", "SetValue", "Set the current (normalized) slider value.", "float", "value");
    DOC_METHOD_0("float", "GetValue", "Get the current (normalized) slider value.");
    DOC_TABLE("uik.RadioButton");
    DOC_METHOD_0("void", "Select", "Select this radio button in this radio button group.");
    DOC_METHOD_0("bool", "IsSelected", "Check whether this radio button is currently selected or not");
    DOC_METHOD_0("string", "GetText", "Get the radio button text.");
    DOC_METHOD_1("void", "SetText", "Set the radio button text.", "string", "text");

    DOC_TABLE("uik.Window");
    DOC_METHOD_0("string", "GetId", "Get the window ID.");
    DOC_METHOD_0("string", "GetName", "Get the window name.");
    DOC_METHOD_0("unsigned", "GetNumWidgets", "Get the number of widgets in the window.");
    DOC_METHOD_1("uik.Widget", "FindWidgetById", "Find a widget by the given Widget ID.<br>"
                                                 "The returned widget will already be downcast to the right widget type.<br>"
                                                 "Returns nil if there's no such widget.",
                 "string", "id");
    DOC_METHOD_1("uik.Widget", "FindWidgetByName", "Find a widget by the given Widget name.<br>"
                                                   "If there are multiple widgets by the same name it's unspecified which one will be returned.<br>"
                                                   "The returned widget will already be downcast to the right widget type.<br>"
                                                   "Returns nil if there's no such widget.",
                 "string", "name");
    DOC_METHOD_1("uik.Widget", "FindWidgetParent", "Find the parent widget of the given widget.<br>"
                                                   "Returns nil if the widget is the root widget and doesn't have a parent."
                                                   "The returned widget will already be downcast to the right widget type.<br>",
                 "uik.Widget", "widget");
    DOC_METHOD_1("uik.Widget", "GetWidget", "Get a widget by the given index. The indexing is zero based and the index must be a valid index.<br>"
                                            "The returned widget will already be downcast to the right widget type.<br>",
                 "unsigned", "index");

    DOC_TABLE("uik.Action");
    DOC_TABLE_PROPERTY("string", "name", "Name of the widget that triggered the action.");
    DOC_TABLE_PROPERTY("string", "id", "ID of the widget that triggered the action.");
    DOC_TABLE_PROPERTY("string", "type", "Type of the action in question.");
    DOC_TABLE_PROPERTY("int|float|bool|string", "value", "The value associated with the action.");

    DOC_TABLE("game");
    DOC_TABLE_PROPERTY("glm.vec2", "X", "Unit length X vector. Defined as glm.vec2(1.0, 0.0)");
    DOC_TABLE_PROPERTY("glm.vec2", "Y", "Unit length Y vector. Defined as glm.vec2(0.0, 1.0)");
    DOC_TABLE_PROPERTY("string", "home", "Platform specific filesystem path to game's 'home' folder.<br>"
                                         "For example on Linux: /home/user/.GameStudio/<ID>");
    DOC_TABLE_PROPERTY("string", "name", "Name of the game. See the project settings for how to change the name.");
    DOC_TABLE_PROPERTY("string", "OS", "Name of the underlying operating system.<br>"
                                       "One of 'LINUX', 'WIN32' or 'WASM'");

    DOC_TABLE("game.Engine");
    DOC_METHOD_1("game.Scene", "Play", "Play a scene. Any previous scene is deleted and the new scene is started.<br>"
                                       "Returns a reference to the new scene for convenience.",
                 "game.SceneClass|string", "klass|name");

    // todo: resume/suspend
    DOC_METHOD_0("void", "EndPlay", "End the play of the current scene. Will invoke EndPlay callbacks and end game play cleanly.");
    DOC_METHOD_1("void", "Quit", "Quit the game by asking the host application to exit.", "int", "exit_code");
    DOC_METHOD_1("void", "Delay", "Insert a time delay into the engine request queue. <br>"
                                  "All the functions in the game.Engine interface are pushed into a queue and "
                                  "adding a delay will postpone the processing of all subsequent engine requests.<br>"
                                  "For example if Delay(2.0) OpenUI('MyUI') the UI will be opened after 2 seconds.",
                 "float", "seconds");
    DOC_METHOD_1("void", "GrabMouse", "Request the host application to enable/disable mouse grabbing.", "bool", "grab");
    DOC_METHOD_1("void", "ShowMouse", "Request the host application to show/hide the OS mouse cursor.", "bool", "show");
    DOC_METHOD_1("void", "ShowDebug", "Toggle debug messages on/off in the engine.", "bool", "show");
    DOC_METHOD_1("void", "SetFullScreen", "Request the host application to toggle full screen mode.", "bool", "full_screen");
    DOC_METHOD_1("void", "BlockKeyboard", "Toggle blocking keyboard events on/off.<br>"
                                          "When keyboard block is enabled the keyboard events coming from the OS are not processed, "
                                          "and none of the entity/scene keyboard handlers are called.<br>"
                                          "Note that this does not block low level keyboard polling such as wdk.TestKeyDown from working "
                                          "but only *event* based keyboard processing is affected.",
                 "bool", "block");
    DOC_METHOD_1("void", "BlockMouse", "Turn mouse blocking on or off.<br>"
                                       "When mouse block is enabled the mouse events coming from the OS are not processed "
                                       "and none of the entity/scene mouse event handlers are called.",
                 "bool", "block");
    DOC_METHOD_1("void", "DebugPause", "Initiate a debug pause or leave previous debug pause.", "bool", "pause");
    DOC_METHOD_1("void", "DebugPrint", "Print a debug message in the game window.", "string", "message");
    DOC_METHOD_4("void", "DebugDrawCircle", "Draw a debug circle with the given radius around the center point in game space.",
                 "glm.vec2|base.FPoint", "center", "float", "radius", "base.Color4f", "color", "float", "line_width");
    DOC_METHOD_4("void", "DebugDrawLine", "Draw a debug line from point A to point B in game space.",
                 "glm.vec2|base.FPoint", "a", "glm.vec2|base.FPoint", "b", "base.Color4f", "color", "float", "line_width");
    DOC_METHOD_6("void", "DebugDrawLine", "Draw a debug line from point x0,y0 to x1,y1 in game space.",
                 "float", "x0", "float", "y0", "float", "x1", "float", "y1", "base.Color4f", "color", "float", "line_width");
    DOC_METHOD_4("void", "DebugDrawRect", "Draw a debug rect from top left corner to bottom right corner in game space.",
                 "glm.vec2|base.FPoint", "top_left", "glm.vec2|base.FPoint", "bottom_right", "base.Color4f", "color", "float", "line_width");
    DOC_METHOD_0("void", "DebugClear", "Clear all previous debug prints from the game window.");
    DOC_METHOD_1("uik.Window", "OpenUI", "Open a new uik.Window and place it on the top of the UI Window stack.<br>"
                                         "The window will remain open until CloseUI is called.<br>"
                                         "Returns a reference to the window that was opened so it's possible to use the "
                                         "returned window object to query for widgets etc. and set their initial values conveniently.",
                 "uik.Window|string", "window|name");
    DOC_METHOD_1("void", "CloseUI", "Request to close the topmost UI and pop it off of the window stack. <br>"
                                    "If there's no UI open when the request executes nothing is done.<br>"
                                    "This method is deprecated and should not be used. There's no conditional "
                                    "to check whether the close is a duplicate so it's easy to manipulate the UI"
                                    " stack incorrectly by adding superfluous close commands. A better alternative is "
                                    "to use CloseUI(ui, exit_code) which will filter out duplicate close requests."
                                    "The associated exit_code will be passed to any OnUIClose event handlers as the result value.",
                 "int", "exit_code");
    DOC_METHOD_2("void", "CloseUI", "Request to conditionally close the topmost UI if the UI name matches the given name.<br>"
                                    "If there's no UI open or if the UI's name does not match the name_filer then nothing is done.<br>"
                                    "The associated exit_code will be passed to any OnUIClose event handlers as the result value.",
                 "string", "name_filter", "int", "exit_code");
    DOC_METHOD_1("void", "PostEvent", "Post a GameEvent to all OnGameEvent handlers.", "game.GameEvent", "event");
    DOC_METHOD_1("void", "ShowDeveloperUI", "Show or hide the developer UI when supported by the host app/platform.", "bool", "show");
    DOC_METHOD_2("void", "EnableEffect", "Enable/disable a rendering effect. <br>Possible effects: 'Bloom'.",
                 "string", "name", "bool", "on_off");
    DOC_METHOD_1("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "The initial viewport is a viewport without any dimensions.",
                 "base.FRect", "viewport");
    DOC_METHOD_4("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "The initial viewport is a viewport without any dimensions.",
                 "float", "x", "float", "y", "float", "width", "float", "height");
    DOC_METHOD_2("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "The initial viewport is a viewport without any dimensions.<br>"
                                        "This function keeps the viewport at 0,0 and resizes it to the given width and height.",
                 "float", "width", "float", "height");
    DOC_METHOD_0("uik.Window", "GetTopUI", "Get the topmost UI Window from the window stack. If no window is currently open then nil is returned.<br>");

    DOC_TABLE("game.ClassLibrary");
    DOC_METHOD_1("game.EntityClass", "FindEntityClassByName", "Find an entity class by name.<br>"
                                                              "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("game.EntityClass", "FindEntityClassById", "Find an entity class by class ID.<br>"
                                                            "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("game.SceneClass", "FindSceneClassByName", "Find a scene class by name.<br>"
                                                            "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("game.SceneClass", "FindSceneClassById", "Find a scene class by class ID.<br>"
                                                          "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("audio.GraphClass", "FindAudioGraphClassByName", "Find an audio graph class by name.<br>"
                                                            "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("audio.GraphClass", "FindAudioGraphClassById", "Find an audio graph class by class ID.<br>"
                                                          "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("uik.Window", "FindUIByName", "Find a UI Window by name.<br>"
                                               "Returns nil if no such window object could be found.","string", "name");
    DOC_METHOD_1("uik.Window", "FindUIById", "Find a UI Window by ID.<br>"
                                               "Returns nil if no such window object could be found.","string", "id");

    DOC_TABLE2("game.DrawableCommand", "The drawable command type is used to send commands to the gfx drawable object associated "
                                       "with the game.Drawable inside the renderer.<br>"
                                       "It carries a command name and a variable number of arguments. You can use a DrawableCommand "
                                       "to for example send a command to a particle engine to emit some particles.");
    DOC_META_METHOD_0("...", "index", "Lua index meta method.");
    DOC_META_METHOD_0("...", "newindex", "Lua new index meta method.");
    DOC_OBJECT_PROPERTY("string", "name", "The name of the command.");

    DOC_TABLE("game.Drawable");
    DOC_METHOD_1("void", "Command", "Send a command to the gfx drawable in the renderer.", "string", "cmd_name");
    DOC_METHOD_2("void", "Command", "Send a command to the gfx drawable in the renderer.", "string", "cmd_name", "int|float|string", "cmd_arg");
    DOC_METHOD_2("void", "Command", "Send a command to the gfx drawable in the renderer.<br><br>"
                                    "my_drawable:Command('example', { arg = 123, other_arg = 321.0 })"
                                    "", "string", "cmd_name", "table", "cmd_ags");

    DOC_METHOD_1("void", "SetMaterialId", "Set drawable material to a material specified by its class ID.<br>"
                                          "Remember that you most likely also want to reset the material time, active texture map and clear any previous uniforms.",
                 "string", "id");
    DOC_METHOD_1("bool", "SetMaterial", "Set drawable material to a material specified by its name. If no such material exists no change is done."
                                        "Returns true on success or false on failure."
                                        "Remember that you most likely also want to reset the material time, active texture map and clear any previous uniforms.",
                 "string", "name");
    DOC_METHOD_1("bool", "SetMaterial", "Set drawable item material to a new material class. The class object must be a valid object.",
                 "gfx.MaterialClass", "material");
    DOC_METHOD_1("bool", "SetActiveTextureMap", "Set the currently active texture map on this drawable item's material to the one identified by its name."
                                                "If no such material map exists no change is done. Returns true on success or false on failure.",
                 "string", "name");
    DOC_METHOD_0("string", "GetMaterialId", "Get the graphics subsystem material ID.");
    DOC_METHOD_0("string", "GetDrawableId", "Get the graphics subsystem drawable ID.");
    DOC_METHOD_0("int", "GetLayer", "Get the render layer index.");
    DOC_METHOD_0("float", "GetLineWidth", "Get the line width (in pixels) used when when rasterizing the shape using lines.");
    DOC_METHOD_0("float", "GetTimeScale", "Get the scaler value used to modify the drawable item time.");
    DOC_METHOD_1("void", "SetTimeScale", "Set the scaler value for scaling the drawable item time.", "float", "scaler");
    DOC_METHOD_1("bool", "TestFlag", "Test the drawable for a set flag.", "string", "flag");
    DOC_METHOD_2("void", "SetFlag", "Set a drawable flag.", "string", "flag", "bool", "on_off");
    DOC_METHOD_0("bool", "IsVisible", "Check whether the drawable is currently visible or not.");
    DOC_METHOD_1("void", "SetVisible", "Hide or show the drawable.", "bool", "visible");
    DOC_METHOD_2("void", "SetUniform", "Set a material parameter (shader uniform) value.<br>"
                                       "The parameter is identified by its uniform name in the material shader.<br>",
                 "string", "name", "float|int|base.Color4f|glm.vec2|glm.vec3|glm.vec4", "value");
    DOC_METHOD_1("float|int|base.Color4f|glm.vec2|glm.vec3|glm.vec4", "FindUniform",
                 "Find a material parameter (shader uniform) value by name.<br>"
                 "The parameter is identified by its uniform name in the material shader.<br>"
                 "Returns nil if no such uniform exists.",
                 "string", "name");
    DOC_METHOD_1("bool", "HasUniform", "Returns whether the given material parameter (shader uniform) exists.", "string", "name");
    DOC_METHOD_1("void", "DeleteUniform", "Delete the given material parameter (shader uniform) value.<br>"
                                          "After the value has been removed the parameter will use the default value defined in the material.",
                 "string", "name");
    DOC_METHOD_0("void", "ClearUniforms", "Clear all previously set uniforms from the drawable item.<br>"
                                          "You likely want to do this after having change the material ID.");
    DOC_METHOD_0("bool", "HasMaterialTimeAdjustment", "Check whether the drawable item has a pending material time adjustment.");
    DOC_METHOD_1("void", "AdjustMaterialTime", "Adjust the material time to the given value on the next renderer update.",
                 "float", "time");
    DOC_METHOD_0("float", "GetMaterialTime", "Get the current material time based on the renderer's material time for this drawable item.");

    DOC_TABLE("game.RigidBody");
    DOC_METHOD_0("bool", "IsEnabled", "Check whether the body is enabled in the physics simulation or not.");
    DOC_METHOD_0("bool", "IsSensor", "Check whether the body is a sensor only body.");
    DOC_METHOD_0("bool", "IsBullet", "Check whether the body is a fast moving (bullet) body.");
    DOC_METHOD_0("bool", "CanSleep", "Check whether the body can sleep in the physics simulation or not.");
    DOC_METHOD_0("bool", "DiscardRotation", "Check whether the body discards any rotation or not.");
    DOC_METHOD_0("float", "GetFriction", "Get the friction value of the rigid body.");
    DOC_METHOD_0("float", "GetRestitution", "Get the restitution value of the rigid body.");
    DOC_METHOD_0("float", "GetAngularDamping", "Get the angular damping of the rigid body.");
    DOC_METHOD_0("float", "GetLinearDamping", "Get the linear damping of the rigid body.");
    DOC_METHOD_0("float", "GetDensity", "Get the density value of the rigid body.");
    DOC_METHOD_0("string", "GetPolygonShapeId", "Get the ID of the polygonal shape for the physics body.");
    DOC_METHOD_0("glm.vec2", "GetLinearVelocity", "Get the current linear velocity (m/s) of the rigid body.");
    DOC_METHOD_0("float", "GetAngularVelocity", "Get the current angular (rotational) velocity (rad/s) of the rigid body.");
    DOC_METHOD_1("void", "Enable", "Enable or disable the body in physics simulation.", "bool", "enabled");
    DOC_METHOD_1("void", "ApplyImpulse", "Apply linear impulse to the center of the body.<br>"
                                         "The impulse will be applied on the next iteration of the physics update.",
                 "glm.vec2", "impulse");
    DOC_METHOD_2("void", "ApplyImpulse", "Apply linear impulse to the center of the body.<br>"
                                         "The impulse will be applied on the next iteration of the physics update.",
                 "float", "x", "float", "y");

    DOC_METHOD_1("void", "AdjustLinearVelocity", "Set a value (m/s) to adjust the linear velocity of the the rigid body.<br>"
                                                 "The adjustment will be applied on the next iteration of the physics update",
                 "glm.vec2", "velocity");
    DOC_METHOD_2("void", "AdjustLinearVelocity", "Set a value (m/s) to adjust the linear velocity of the the rigid body.<br>"
                                                 "The adjustment will be applied on the next iteration of the physics update",
                 "float", "x", "float", "y");
    DOC_METHOD_1("void", "AdjustAngularVelocity", "Set a value (rad/s) to adjust the rotational velocity of the rigid body.<br>"
                                                  "The adjustment will be applied on the next iteration of the physics update.",
                 "float", "velocity");
    DOC_METHOD_1("bool", "TestFlag", "Test rigid body for a set flag. Returns true if the flag is set, otherwise false.<br>"
                                     "Possible flags: 'Bullet', 'Sensor', 'Enabled', 'CanSleep', 'DiscardRotation'",
                 "string", "flag_name");
    DOC_METHOD_2("void", "SetFlag", "Set a rigid body flag. <br>"
                                    "Possible flags: 'Bullet', 'Sensor', 'Enabled', 'CanSleep', 'DiscardRotation'.",
                 "string", "flag_name",
                 "bool",  "on_off");
    DOC_METHOD_0("string", "GetSimulationType", "Get the type of the rigid body simulation.<br>"
                                                "Possible values: 'Static', 'Kinematic', 'Dynamic'");
    DOC_METHOD_0("string", "GetCollisionShapeType", "Get the rigid body collision shape.<br>"
                                                    "Possible values. 'Box', 'Circle', 'RightTriangle', 'IsoscelesTriangle', 'Trapezoid', 'Parallelogram', 'SemiCircle', 'Polygon'<br>"
                                                    "When the type is 'Polygon' you can get the shape's ID through GetPolygonShapeId.");

    DOC_TABLE("game.TextItem");
    DOC_METHOD_0("string", "GetText", "Get the current UTF-8 encoded text.");
    DOC_METHOD_0("base.Color4f", "GetColor", "Get the current text color.");
    DOC_METHOD_0("int", "GetLayer", "Get the render layer index.");
    DOC_METHOD_0("string", "GetFontName", "Get the (encoded) name of the font used to render the text. E.g., &quot;app://fonts/font.otf&quot;");
    DOC_METHOD_0("int", "GetFontSize", "Get the size of the pixel font size used to render the text.");
    DOC_METHOD_0("float", "GetLineHeight", "Get the fractional line height scaler value used to advance the text rasterizer from one line to another.");
    DOC_METHOD_1("void", "SetText", "Set a new UTF-8 encoded text to be displayed.<br>"
                                    "If the item has 'StaticContent' flag set this will have no effect.",
                 "string", "text");
    DOC_METHOD_1("void", "SetColor", "Set the new color for rendering the text.<br>"
                                     "If the item has 'StaticContent' flag set this will have no effect.",
                 "base.Color4f", "color");
    DOC_METHOD_0("bool", "TestFlag", "Test text item for a set flag. Returns true if the flag is set, otherwise false.<br>"
                                     "Possible flags: 'VisibleInGame', 'BlinkText', 'UnderlineText', 'StaticContent'");
    DOC_METHOD_2("void", "SetFlag", "Set a text item flag.<br>"
                                    "Possible flags: 'VisibleInGame', 'BlinkText', 'UnderlineText', 'StaticContent'",
                 "string", "flag_name","bool", "on_off");

    DOC_TABLE("game.SpatialNode");
    DOC_METHOD_0("string", "GetShape", "Get the shape used for spatial indexing.<br>"
                                       "Possible values. 'AABB'");
    DOC_METHOD_0("bool", "IsEnabled", "Check whether the spatial node is enabled or not.<br>"
                                      "Nodes that are not enabled will not be indexed.");
    DOC_METHOD_1("void", "Enable", "Enable or disable spatial index.", "bool", "on_off");

    DOC_TABLE("game.EntityNode");
    DOC_METHOD_0("string", "GetName", "Get the entity node's human readable instance name.");
    DOC_METHOD_0("string", "GetId", "Get the entity node instance ID.");
    DOC_METHOD_0("string", "GetTag", "Get the node's instance tag string.");
    DOC_METHOD_0("string", "GetClassName", "Get the entity node class name.");
    DOC_METHOD_0("string", "GetClassId", "Get the entity node class ID.");
    DOC_METHOD_0("string", "GetClassTag", "Get the entity node class tag string.");
    DOC_METHOD_0("glm.vec2", "GetTranslation", "Get the node's translation relative to its parent.");
    DOC_METHOD_0("glm.vec2", "GetSize", "Get the node's size. Remember that this might not be the same as the node's world size "
                                        "because there might be a scaling transform at some point.");
    DOC_METHOD_0("glm.vec2", "GetScale", "Get the node's scaling factor that applies to this node and all of its children.");
    DOC_METHOD_0("float", "GetRotation", "Get the node's rotation relative to its parent.");
    DOC_METHOD_0("bool", "HasRigidBody", "Checks whether the node has a rigid body.");
    DOC_METHOD_0("bool", "HasTextItem", "Checks whether the node has a text item.");
    DOC_METHOD_0("bool", "HasDrawable", "Checks whether the node has a drawable item.");
    DOC_METHOD_0("bool", "HasSpatialNode", "Checks whether the node has a spatial indexing node.");
    DOC_METHOD_0("game.RigidBody", "GetRigidBody", "Get the node's rigid body item if any. Returns nil if node has no rigid body.");
    DOC_METHOD_0("game.TextItem", "GetTextItem", "Get the node's text item if any. Returns nil if node has no text item.");
    DOC_METHOD_0("game.Drawable", "GetDrawable", "Get the node's drawable item if any. Returns nil if node has no drawable item.");
    DOC_METHOD_0("game.SpatialNode", "GetSpatialNode", "Get the node's spatial node if any. Returns nil if node has no spatial node.");
    DOC_METHOD_0("game.Entity", "GetEntity", "Get the entity that owns this entity node.");
    DOC_METHOD_1("void", "SetScale", "Set the node's scaling factor that applies to this node and its children.", "glm.vec2", "scale");
    DOC_METHOD_2("void", "SetScale", "Set the node's scaling factor that applies to this node and its children.", "float", "sx", "float", "sy");
    DOC_METHOD_1("void", "SetSize", "Set the size that applies to this node.", "glm.vec2", "size");
    DOC_METHOD_2("void", "SetSize", "Set the size that applies to this node.", "float", "width", "float", "height");
    DOC_METHOD_1("void", "SetTranslation", "Set the node's translation relative to its parent.", "glm.vec2", "translation");
    DOC_METHOD_2("void", "SetTranslation", "Set the node's translation relative to its parent.", "float", "x", "float", "y");
    DOC_METHOD_1("void", "SetName", "Set the node's instance name.", "string", "name");
    DOC_METHOD_1("void", "SetRotation", "Set the new rotation value in radians relative to the node's parent.", "float", "angle");
    DOC_METHOD_1("void", "Translate", "Translate the node relative to its current translation.", "glm.vec2", "delta");
    DOC_METHOD_2("void", "Translate", "Translate the node relative to its current translation.", "float", "dx", "float", "dy");
    DOC_METHOD_1("void", "Rotate", "Rotate the node in radians relative to its current rotation.", "float", "delta");
    DOC_METHOD_1("void", "Grow", "Grow the node in size by some amount.", "glm.vec2", "size");
    DOC_METHOD_2("void", "Grow", "Grow the node in size by some amount.", "float", "dx", "float", "dy");

    DOC_TABLE("game.EntityClass");
    DOC_METHOD_0("string", "GetId", "Get the entity class ID.");
    DOC_METHOD_0("string", "GetName", "Get the entity class name.");
    DOC_METHOD_0("string", "GetTag", "Get entity tag string.");
    DOC_METHOD_0("float", "GetLifetime", "Get the entity lifetime.");
    DOC_METHOD_0("bool|float|string|int|vec2", "index",
                 "Lua index meta method.<br>"
                 "The entity class's script variables are accessible as properties of the entity class object.<br>"
                 "For example a script variable named 'score' would be accessible as object.score.<br>");

    DOC_TABLE("game.ActuatorClass");
    DOC_METHOD_0("string", "GetName", "Get the actuator class name.");
    DOC_METHOD_0("string", "GetId", "Get the actuator class ID.");
    DOC_METHOD_0("string", "GetNodeId", "Get the entity node ID that the actuator will apply on.");
    DOC_METHOD_0("float", "GetStartTime", "Get the animation time in seconds when the actuator will start.");
    DOC_METHOD_0("float", "GetDuration", "Get the duration of the actuator's operation in seconds.");
    DOC_METHOD_0("string", "GetType", "Get the type of actuator. <br>"
                                      "One of: 'Transform', 'Kinematic', 'SetValue', 'SetFlag', 'Material'");
    DOC_TABLE("game.Actuator");
    DOC_METHOD_0("string", "GetClassId", "Get the actuator class ID.");
    DOC_METHOD_0("string", "GetName", "Get the actuator class name.");
    DOC_METHOD_0("string", "GetNodeId", "Get the entity node ID that the actuator will apply on.");
    DOC_METHOD_0("float", "GetStartTime", "Get the animation time in seconds when the actuator will start.");
    DOC_METHOD_0("float", "GetDuration", "Get the duration of the actuator's operation.");
    DOC_METHOD_0("game.TransformActuator", "AsTransformActuator", "Cast the actuator to a TransformActuator. Returns nil if the cast failed.");
    DOC_METHOD_0("game.SetFlagActuator", "AsFlagActuator", "Cast the actuator to a SetFlagActuator. Returns nil if the cast failed.");
    DOC_METHOD_0("game.SetValueActuator", "AsValueActuator", "Cast the actuator to a SetValueActuator. Returns nil if the cast failed.");
    DOC_METHOD_0("game.KinematicActuator", "AsKinematicActuator", "Cast the actuator to a KinematicActuator. Returns nil if the cast failed.");
    DOC_METHOD_0("game.MaterialActuator", "AsMaterialActuator", "Cast the actuator to a MaterialActuator. Returns nil if the cast failed.");

    DOC_TABLE("game.TransformActuator");
    DOC_METHOD_1("void", "SetEndPosition", "Set the ending position for actuator movement.<br>"
                                           "This takes effect only when the actuator is not static.",
                 "glm.vec2", "position");
    DOC_METHOD_2("void", "SetEndPosition", "Set the ending position for actuator movement.<br>"
                                           "This takes effect only when the actuator is not static.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("void", "SetEndScale", "Set the ending scale for actuator movement.<br>"
                                           "This takes effect only when the actuator is not static.",
                 "glm.vec2", "scale");
    DOC_METHOD_2("void", "SetEndScale", "Set the ending scale for actuator movement.<br>"
                                           "This takes effect only when the actuator is not static.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("void", "SetEndSize", "Set the ending size for actuator movement.<br>"
                                        "This takes effect only when the actuator is not static.",
                 "glm.vec2", "scale");
    DOC_METHOD_2("void", "SetEndSize", "Set the ending size for actuator movement.<br>"
                                        "This takes effect only when the actuator is not static.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("void", "SetEndRotation", "Set the ending rotation in radians for the actuator movement.<br>"
                                           "This takes effect only when the actuator is not static.",
                 "float", "angle");

    // todo:
    DOC_TABLE("game.SetFlagActuator");
    DOC_TABLE("game.SetValueActuator");
    DOC_TABLE("game.SetKinematicActuator");
    DOC_TABLE("game.MaterialActuator");

    DOC_TABLE("game.Animator");
    DOC_METHOD_0("string", "GetName", "Get the animator name.");
    DOC_METHOD_0("string", "GetId", "Get the animator class ID.");
    DOC_METHOD_1("bool", "HasValue", "Check whether the animator has a value by the given name.", "string", "name");
    DOC_METHOD_2("void", "SetValue", "Set an animator value.",
                 "string", "name", "bool|int|float|string|glm.vec2", "value");
    DOC_METHOD_1("bool|int|float|string|glm.vec2", "FindValue", "Find an animator value if any. If no such value exists then return nil.",
                 "string", "name");
    DOC_METHOD_0("float", "GetTime", "Get the animator time. When the animator is not transitioning from one state to another the time "
                                     "measures the time spent in the current animation state. When a transition to another state is taking "
                                     "place the time measures the time spent in transition.");
    DOC_META_METHOD_0("...", "index", "Lua index meta method.");
    DOC_META_METHOD_0("...", "newindex", "Lua new index meta method.");

    DOC_TABLE("game.AnimationClass");
    DOC_METHOD_0("string", "GetName", "Get the animation class name.");
    DOC_METHOD_0("string", "GetId", "Get the animation class ID.");
    DOC_METHOD_0("float", "GetDuration", "Get the duration of the animation in seconds.");
    DOC_METHOD_0("float", "GetDelay", "Get the animation delay in seconds.");
    DOC_METHOD_0("bool", "IsLooping", "Check whether the animation is looping or not.");

    DOC_TABLE("game.Animation");
    DOC_METHOD_0("string", "GetClassName", "Get the animation class name.");
    DOC_METHOD_0("string", "GetClassId", "Get the animation class ID.");
    DOC_METHOD_0("bool", "IsComplete", "Check whether the animation has finished or not.");
    DOC_METHOD_0("bool", "IsLooping", "Check whether the animation is looping or not.");
    DOC_METHOD_1("void", "SetDelay", "Set the animation delay in seconds.", "float", "delay");
    DOC_METHOD_0("float", "GetDelay", "Get the animation delay in seconds.");
    DOC_METHOD_0("float", "GetCurrentTime", "Get the current animation time in seconds.");
    DOC_METHOD_0("float", "GetDuration", "Get the animation duration in seconds.");
    DOC_METHOD_0("game.AnimationClass", "GetClass", "Get the class object.");
    DOC_METHOD_2("game.Actuator", "FindActuatorById", "Find an animation actuator by its class ID.<br>"
                                                      "Returns nil if no such actuator could be found.<br>"
                                                      "Takes an optional type string for down casting the actuator to a specific type.",
                 "string", "id", "string", "type");
    DOC_METHOD_2("game.Actuator", "FindActuatorByName", "Find an animation actuator by its class name.<br>"
                                                        "Returns nil if no such actuator could be found.<br>"
                                                        "In case multiple actuators have the same name it's unspecified which one is returned.<br>"
                                                        "Takes an optional type string for down casting the actuator to a specific type.",
                 "string", "name", "string", "type");

    DOC_TABLE("game.Entity");
    DOC_METHOD_0("bool|float|string|int|vec2", "index", "Lua index meta method.<br>"
                                                        "The entity's script variables are accessible as properties of the entity object.<br>"
                                                        "For example a script variable named 'score' would be accessible as object.score.<br>");
    DOC_METHOD_0("bool|float|string|int|vec2", "newindex", "Lua new index meta method.<br>"
                                                           "The entity's script variables are accessible as properties of the entity object.<br>"
                                                           "For example a script variable named 'score' would be accessible as object.score.<br>"
                                                           "Note that you cannot write to the variable if it is marked as 'Read Only'");
    DOC_METHOD_0("string", "GetName", "Get the entity's human readable name.");
    DOC_METHOD_0("string", "GetId", "Get entity instance ID.");
    DOC_METHOD_0("string", "GetClassName", "Get the entity class name.");
    DOC_METHOD_0("string", "GetClassId", "Get the entity class ID.");
    DOC_METHOD_0("string", "GetTag", "Get the entity tag string.");
    DOC_METHOD_0("unsigned", "GetNumNodes", "Get the number of entity nodes in this entity.");
    DOC_METHOD_0("float", "GetTime", "Get the entity's current accumulated (life) time.");
    DOC_METHOD_0("int" , "GetLayer", "Get the entity's render layer in the scene rendering.");
    DOC_METHOD_1("void", "SetLayer", "Set the entity's render layer in the scene rendering.", "int", "layer");
    DOC_METHOD_1("void", "SetTag", "Set entity tag string.", "string", "tag");
    DOC_METHOD_0("bool", "IsVisible", "Checks whether the entity is currently visible or not.");
    DOC_METHOD_0("bool", "IsAnimating", "Checks whether the entity is currently playing an animation or not.");
    DOC_METHOD_0("bool", "IsDying", "Checks whether the entity has been killed, I.e. someone called Die or KillEntity on it.");
    DOC_METHOD_0("bool", "HasExpired", "Checks whether the entity has expired, i.e. exceeded it's max lifetime.");
    DOC_METHOD_0("bool", "HasBeenKilled", "Checks whether the entity has been killed.<br>"
                                          "Entities that have been killed will be deleted from the scene at the end of this game loop.");
    DOC_METHOD_0("bool", "HasBeenSpawned", "Checks whether the entity has just been spawned and exists for the first iteration of the game loop.<br>"
                                           "This flag is only ever true on the first iteration of the game loop during the entity's lifetime.");
    DOC_METHOD_0("bool", "HasAnimator", "Checks whether the entity has an animator component or not.");
    DOC_METHOD_0("game.Animator", "GetAnimator", "Get the entity animator instance if any.");
    DOC_METHOD_0("game.Scene", "GetScene", "Get the current scene.");
    DOC_METHOD_1("game.EntityNode", "GetNode", "Get an entity node at the the given index. The indexing is 0 based and the index must be a valid index.",
                 "unsigned", "index");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassName", "Find a node in the entity by its class name. Returns nil if no such node could be found.<br>"
                                                           "If multiple nodes have the same class name it's unspecified which one is returned.<br>",
                 "string", "name");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassId", "Find a node in the entity by class ID. Returns nil if no such node could be found.",
                 "string", "id");
    DOC_METHOD_1("game.EntityNode", "FindNodeByInstanceId", "Find a node in the entity by instance ID. Returns nil if no such node could be found.<br>",
                 "string", "id");
    DOC_METHOD_0("game.Animation", "PlayIdle", "Play the entity's idle animation (if any).<br>"
                                               "Returns nil if the entity doesn't have any idle animation or is already playing an animation.");
    DOC_METHOD_1("game.Animation", "PlayAnimationByName", "Play an animation by the given name if found.<br>"
                                                          "Any current animation is replaced by this new animation.<br>"
                                                          "Returns the animation instance or nil if no such animation could be found.",
                 "string", "name");
    DOC_METHOD_1("game.Animation", "PlayAnimationById", "Play an animation by the given ID.<br>"
                                                        "Any current animation is replaced by this new animation.<br>"
                                                        "Returns the animation instance or nil if no such animation could be found.",
                 "string", "id");
    DOC_METHOD_1("bool", "TestFlag", "Test entity flag.<br>"
                                     "Possible flags: 'VisibleInGame', 'LimitLifetime', 'KillAtLifetime', 'KillAtBoundary', 'TickEntity', 'UpdateEntity', 'WantsKeyEvents', 'WantsMouseEvents'",
                 "string", "flag_name");
    DOC_METHOD_2("void", "SetFlag", "Set entity flag. Possible flags: 'VisibleInGame', 'LimitLifetime', 'KillAtLifetime', 'KillAtBoundary', 'TickEntity', 'UpdateEntity', 'WantsKeyEvents', 'WantsMouseEvents'",
                 "string", "name", "bool", "on_off");
    DOC_METHOD_1("void", "SetVisible", "Set entity visibility flag.", "bool", "on_off");
    DOC_METHOD_0("void", "Die", "Let the entity die and be removed from the scene.");
    DOC_METHOD_2("void", "SetTimer", "Set a named timer on the entity.<br>"
                                     "The timer's resolution is based on the game's update resolution configured in the project settings.<br>"
                                     "When the timer fires OnTimer entity callback is called and the provided value 'jitter' indicates the delta time "
                                     "between the ideal time and the actual time when he timer fired. A negative value indicates that the timer"
                                     "fired late.",
                 "string", "name", "float", "time");
    DOC_METHOD_3("void", "PostEvent", "Post an event to this entity.<br>"
                                      "The entity will be able to process this event in its OnEvent callback.",
                 "string", "message", "string", "sender", "bool|int|float|string|glm.vec2|glm.vec3|glm.vec4", "value");
    DOC_METHOD_1("void", "PostEvent", "Post an event to this entity.<br>"
                                      "The entity will be able to process this event in its OnEvent callback.",
                 "game.EntityEvent", "event");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarById", "Find a script variable by ID. Returns nil if no such variable was found.", "string", "id");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarByName", "Find a script variable byt name. Returns nil if no such variable was found.", "string", "name");


    DOC_TABLE("game.EntityEvent");
    DOC_OBJECT_PROPERTY("string", "message", "Free form message string.");
    DOC_OBJECT_PROPERTY("string", "sender", "Free form sender string.");
    DOC_OBJECT_PROPERTY("int|float|string|glm.vec2|glm.vec3|glm.vec4", "value", "Value associated with the event.");

    DOC_TABLE("game.EntityArgs");
    DOC_OBJECT_PROPERTY("game.EntityClass", "class", "The class object (type) of the entity.");
    DOC_OBJECT_PROPERTY("string", "id", "The instance ID of the entity.<br>"
                                        "The ID should be unique in the current scene across the entities and entity nodes.<br>"
                                        "If no ID is set (id is an empty string) one will be generated when the entity is spawned.");
    DOC_OBJECT_PROPERTY("string", "name", "The instance name of the entity.<br>"
                                          "Default is an empty string (no name).");
    DOC_OBJECT_PROPERTY("glm.vec2", "scale", "The scaling factor that will apply to all of the entity nodes.<br>"
                                      "Default is (1.0, 1.0).");
    DOC_OBJECT_PROPERTY("glm.vec2", "position", "The initial position of the entity in the scene.<br>"
                                         "Default is (0.0, 0.0)");
    DOC_OBJECT_PROPERTY("float", "rotation", "The initial rotation that will apply to the entity in the scene.<br>"
                                      "Default is 0.0 (i.e no rotation).");
    DOC_OBJECT_PROPERTY("bool", "logging", "Whether to enable life time related engine logs for this entity.<br>"
                                   "Default is true.");
    DOC_OBJECT_PROPERTY("int", "layer", "The scene layer index for the entity.<br>"
                                        "Default is 0.");

    DOC_TABLE("game.SpatialQueryResultSet");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the result set is an empty set or not.");
    DOC_METHOD_0("bool", "HasNext", "Check whether the result set has a next item or not.");
    DOC_METHOD_0("bool", "Next", "Move to the next item (if any) in the result set. <br>"
                                 "Returns true if there is a next item or false when there are no more items.");
    DOC_METHOD_0("void", "Begin", "(Re)start the iteration over the result set. <br>"
                                  "The iteration is already started automatically when the query is created, "
                                  "so this only needs to be called if restarting.");
    DOC_METHOD_0("game.EntityNode", "Get", "Get the current item at this point of iteration over the result set.");
    DOC_METHOD_0("game.EntityNode", "GetNext", "Get the current item and move onto next item in the result set.");
    DOC_METHOD_1("game.EntityNode", "Find", "Find an entity node in the result set by invoking the specified Lua callback on each entity node in the set.<br>"
                                            "Your callback Lua function should return true to indicate the object having been found or false to indicate no match.<br>"
                                            "The found object is then returned from the Find function for convenience. In case nothing matched nil is returned.<br>"
                                            "The iteration of the of the result set is done from whatever is the current position and it stops when a match is found or"
                                            "when the iteration has reached the end.<br>",
                                            "function", "predicate");
    DOC_METHOD_1("void", "Filter", "Filter the result set by invoking the specified Lua callback on each entity node in the set.<br>"
                                   "The callback function should return true to keep the node in the set or false to have it removed.<br>"
                                   "This function always begins and ends the iteration at the beginning. I.e. the iteration is restarted both when "
                                   "starting and finishing the filtering.",
                 "function", "predicate");

    DOC_TABLE("game.ScriptVar");
    DOC_METHOD_0("bool|float|string|int|vec2", "GetValue", "Get the value of the script variable.");
    DOC_METHOD_0("string", "GetName", "Get the script variable name.");
    DOC_METHOD_0("string", "GetId", "Get the script variable ID.");
    DOC_METHOD_0("bool", "IsReadOnly", "Check whether this variable is read only or not.");
    DOC_METHOD_0("bool", "IsArray", "Check whether this variable is an array or not.");
    DOC_METHOD_0("bool", "IsPrivate", "Check whether this variable is private or not.");

    DOC_TABLE("game.SceneClass");
    DOC_METHOD_0("bool|float|string|int|vec2", "index", "Lua index meta method.<br>"
                                                        "The scene class's script variables are accessible as properties of the scene class object.<br>"
                                                        "For example a script variable named 'score' would be accessible as object.score.<br>");
    DOC_METHOD_0("string", "GetName", "Get the name of the class.");
    DOC_METHOD_0("string", "GetId", "Get the ID of the class.");
    DOC_METHOD_0("size_t", "GetNumScriptVars", "Get the number of script variables.");
    DOC_METHOD_1("game.ScriptVar", "GetScriptVar", "Get the script variable at the given index. The index must be valid.", "size_t", "index");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarById", "Find a script variable by id. <br>"
                                                        "Returns nil if no such script variable could be found.",
                 "string", "id");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarByName", "Find a script variable by name.<br>"
                                                          "Returns nil if no such script variable could be found.",
                 "string", "name");
    DOC_METHOD_0("float|nil", "GetLeftBoundary", "Get the left boundary of the scene if any. If not set then nil is returned.");
    DOC_METHOD_0("float|nil", "GetRightBoundary", "Get the right boundary of the scene if any. If not set then nil is returned.");
    DOC_METHOD_0("float|nil", "GetTopBoundary", "Get the top boundary of the scene if any. If not set then nil is returned.");
    DOC_METHOD_0("float|nil", "GetBottomBoundary", "Get the bottom boundary of the scene if any. If not set then nil is returned.");

    DOC_TABLE("game.EntityList");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the entity list is an empty list or not.");
    DOC_METHOD_0("bool", "HasNext", "Check whether the entity list has a next item or not.");
    DOC_METHOD_0("bool", "Next", "Move to the next item (if any) in the list. <br>"
                                 "Returns true if there is a next item or false when there are no more items.");
    DOC_METHOD_0("void", "Begin", "(Re)start the iteration over the list. <br>"
                                  "The iteration is already started automatically when the list is created,<br>"
                                  "so this only needs to be called if restarting.");
    DOC_METHOD_0("game.Entity", "Get", "Get the current item at this point of iteration over the list.");
    DOC_METHOD_1("game.Entity", "GetAt", "Get an item at a given index. The indexing is zero based and the index must be a valid index.", "unsigned", "index");
    DOC_METHOD_0("game.Entity", "GetNext", "Get the current item and move on to the next item.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the entity list.");
    DOC_FUNCTION_2("game.EntityList", "Join", "Join two entity lists together into a new entity list.",
                   "game.EntityList", "first", "game.EntityList", "second");
    DOC_METHOD_2("void", "ForEach", "Call a callback function one ach entity in the list. Any optional argument is passed as-is to the callback.",
                 "function", "callback", "...", "...");

    DOC_TABLE("game.MapLayer");
    DOC_METHOD_0("string", "GetClassName", "Get the map layer's class name.");
    DOC_METHOD_0("string", "GetClassId", "Get the map layer's class ID.");
    DOC_METHOD_0("unsigned", "GetWidth" ,"Get the width  of this layer in tiles. If the layer has a scaling factor the layer width may be different from the map width.");
    DOC_METHOD_0("unsigned", "GetHeight" ,"Get the width  of this layer in tiles. If the layer has a scaling factor the layer height may be different from the map height.");
    DOC_METHOD_0("float", "GetTileSizeScale", "Get the scaling factor for scaling the layer tiles to world units. I.e. the layer's tile width/height is the map tile width/height * scale.");

    DOC_TABLE("game.Map");
    DOC_METHOD_0("string", "GetClassName", "Get the map's class name.");
    DOC_METHOD_0("string", "GetClassId", "Get map's class ID.");
    DOC_METHOD_0("unsigned", "GetNumLayers", "Get the number of layers in the map.");
    DOC_METHOD_0("unsigned", "GetMapWidth", "Get the width of the map in tiles.");
    DOC_METHOD_0("unsigned", "GetMapHeight", "Get the height of the map in tiles.");
    DOC_METHOD_0("float", "GetTileWidth", "Get the width of the tile in units.");
    DOC_METHOD_0("float", "GetTileHeight", "Get the height of the tile in units.");
    DOC_METHOD_1("game.MapLayer", "GetLayer", "Get a map layer at the given index. The index must be valid.", "unsigned", "index");
    DOC_METHOD_1("number, number", "MapToTile", "Map a point on the tile layer plane to a tile (row, col) coordinate.<br>"
                                                "The result is *not* clamped, thus it's possible to produce negative values or row/col that would be out of bounds on tile grid.<br>"
                                                "Use ClampRowCol to clamp the coordinates to a valid range.",
                                                "glm.vec2|base.FPoint", "point");
    DOC_METHOD_2("number, number", "MapToTile", "Map a point on the tile layer plane to a tile (row, col) coordinate.<br>"
                                                "The result is *not* clamped, thus it's possible to produce negative values or row/col that would be out of bounds on tile grid.<br>"
                                                "Use ClampRowCol to clamp the coordinates to a valid range.",
                 "float", "x", "float", "y");
    DOC_METHOD_2("unsigned,unsigned", "ClampRowCol", "Clamp a row and a column to a valid range on the map layer.",
                                                     "int", "row", "int", "col");
    DOC_METHOD_1("glm.vec2|base.FPoint", "MapPointFromScene", "Map a point from the scene plane to the map plane.",
                 "glm.vec2|base.FPoint", "point");
    DOC_METHOD_1("glm.vec2|base.FPoint", "MapPointToScene", "Map a point from the map plane to scene.",
                 "glm.vec2|base.FPoint", "point");
    DOC_METHOD_1("glm.vec2", "MapVectorFromScene", "Map a direction vector from the scene plane to the map plane.",
                 "glm.vec2", "vector");
    DOC_METHOD_1("glm.vec2", "MapVectorToScene", "Map a direction vector from the map plan plane to scene.",
                 "glm.vec2", "vector");


    DOC_TABLE("game.Scene");
    DOC_METHOD_0("bool|float|string|int|vec2", "index", "Lua index meta method.<br>"
                                                        "The scene's script variables are accessible as properties of the scene object.<br>"
                                                        "For example a script variable named 'score' would be accessible as object.score.");
    DOC_METHOD_0("bool|float|string|int|vec2", "newindex", "Lua new index meta method.<br>"
                                                           "The scene's script variables are accessible as properties of the scene object.<br>"
                                                           "For example a script variable named 'score' would be accessible as object.score.");
    DOC_METHOD_1("game.EntityList", "ListEntitiesByClassName", "List all entities of the given class identified by its class name", "string", "class");
    DOC_METHOD_1("game.EntityList", "ListEntitiesByTag", "List all entities that match the given tag string.", "string", "tag");
    DOC_METHOD_0("unsigned", "GetNumEntities", "Get the number of entities currently in the scene.");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceId", "Find an entity with the given instance ID.<br>"
                                                          "Returns nil if no such entity could be found.",
                 "string", "id");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceName", "Find an entity with the given instance name.<br>"
                                                            "Returns nil if no such entity could be found.<br>"
                                                            "In case of multiple entities with the same name the first one with a matching name is returned.",
                 "string", "name");
    DOC_METHOD_1("game.Entity", "GetEntity", "Get an entity at the given index. The indexing is zero based and the index must be a valid index.", "unsigned", "index");
    DOC_METHOD_1("void", "KillEntity", "Flag an entity for removal from the scene. <br>"
                                       "Killing an entity doesn't not immediately remove it from the scene but will only "
                                       "set a flag that will indicate the new state of the entity. The entity will then continue to exist "
                                       "for one more iteration of the game loop until it's deleted at the end of this *next* iteration.<br>"
                                       "This two step design allows any engine subsystems (or game) to realize and react to entities being killed by looking at the kill flag state.",
                                       "game.Entity", "carcass");
    DOC_METHOD_2("game.Entity", "SpawnEntity", "Spawn a new entity in the scene.<br>"
                                               "Spawning a new entity doesn't immediately place the entity in the scene but will only add it to the list of "
                                               "entities to be spawned at the start of the next iteration of the game loop.<br>"
                                               "Then each entity that was just spawned will have their HasBeenSpawned flag on.<br>"
                                               "If link_to_root is true the entity is linked to the current scene's entity hierarchy.<br>"
                                               "If link_to_root is false the entity is not linked to the current scene's entity hierarchy and you should manually call LinkChild later.<br>"
                                               "This is a low level method of spawning and you're likely better off using another SpawnEntity instead which is more convenient to use.",
                 "game.EntityArgs", "args", "bool", "link_to_root = true");
    DOC_METHOD_1("game.Entity", "SpawnEntity", "Spawn a new entity in the scene with default arguments for everything.<br>",
                 "string", "klass_name");
    DOC_METHOD_2("game.Entity", "SpawnEntity", "Spawn a new entity in the scene.<br>"
                                               "Spawning a new entity doesn't immediately place the entity in the scene but will only add it to the list of "
                                               "entities to be spawned at the start of the next iteration of the game loop and "
                                               "then each entity that was just spawned will have their HasBeenSpawned flag on.<br><br>"
                                               "If link is true the entity is linked to the current scene's entity hierarchy.<br>"
                                               "If link is false the entity is not linked to the current scene's entity hierarchy and you should manually call LinkChild later.<br>"
                                               "The args table is a Lua table for packing all the spawn arguments with the following keys.<br><br>"
                                               " &nbsp;&nbsp; id,   string, the ID for the entity. Default = '' <br>"
                                               " &nbsp;&nbsp; name, string, the name for the entity. Default = ''<br>"
                                               " &nbsp;&nbsp; sx,   float, scale factor for X axis. Default = 1.0<br>"
                                               " &nbsp;&nbsp; sy,   float, scale factor for Y axis. Default = 1.0<br>"
                                               " &nbsp;&nbsp; x,    float, translation on the X axis. Default = 0.0<br>"
                                               " &nbsp;&nbsp; y,    float, translation on the Y axis. Default = 0.0<br>"
                                               " &nbsp;&nbsp; r,    float, rotation in radians around the Z axis. Default = 0.0<br>"
                                               " &nbsp;&nbsp; pos,  glm.vec2 translation vector. (Alternative for x, y). Default = glm.vec2(0.0, 0.0)<br>"
                                               " &nbsp;&nbsp; scale, glm.vec2 scaling vector (Alternative for sx, sy). Default = glm.vec2(1.0, 1.0)<br>"
                                               " &nbsp;&nbsp; logging, bool, Flag to enable/disable entity logging. Default = false<br>"
                                               " &nbsp;&nbsp; layer, int, Scene layer index. Default = 0<br>"
                                               " &nbsp;&nbsp; link, bool, Flag to control linking to scene root in scene graph. Default = true",
                 "string", "klass_name", "table", "args");

    DOC_METHOD_1("glm.mat4", "FindEntityTransform", "Find the transform for transforming the entity into the world/scene coordinate space.",
                 "game.Entity", "entity");
    DOC_METHOD_2("glm.mat4", "FindEntityNodeTransform", "Find the transform for transforming the entity node into the the world/scene coordinate space.",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_1("base.FRect", "FindEntityBoundingRect", "Find the axis aligned bounding box (AABB) for the entity in the scene.", "game.Entity", "entity");
    DOC_METHOD_2("base.FRect", "FindEntityNodeBoundingRect", "Find the axis aligned bounding box (AABB) for the entity node in the scene",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_2("base.FBox", "FindEntityNodeBoundingBox", "Find the object oriented bounding box (OOB) for the entity node in the scene.",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_3("glm.vec2", "MapVectorFromEntityNode", "Map a a directional vector relative to entity node coordinate basis into scene/world space.<br>"
                                                    "The resulting vector is not translated, unit length direction vector in world/scene space.",
                 "game.Entity", "entity", "game.EntityNode", "node", "glm.vec2", "vector");
    DOC_METHOD_3("base.FPoint", "MapPointFromEntityNode", "Map a point relative to entity node coordinate space into world/scene space.",
                 "game.Entity", "entity", "game.EntityNode", "node", "base.FPoint", "point");
    DOC_METHOD_3("glm.vec2", "MapPointFromEntityNode", "Map a point relative to entity node coordinate space into world/scene space.",
                 "game.Entity", "entity", "game.EntityNode", "node", "glm.vec2", "point");
    DOC_METHOD_0("game.Map", "GetMap", "Get the associated tilemap if any. If there's no current map nil is returned.");
    DOC_METHOD_0("float", "GetTime", "Get the scene's current time.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the scene's class.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the scene's class.");
    DOC_METHOD_0("game.SceneClass", "GetClass", "Get the scene's class object.");
    DOC_METHOD_3("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given line between point A and point B.",
                 "base.FPoint|glm.vec2", "a", "base.FPoint|glm.vec2", "b", "string", "mode");
    DOC_METHOD_1("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given search rectangle.",
                 "base.FRect", "area_of_interest");
    DOC_METHOD_2("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given point.<br>"
                                                                    "Mode defines distance based filtering for the found objects.<br>"
                                                                    " - 'All' to find all objects.<br>"
                                                                    " - 'Closest' to find  the closest only.<br>"
                                                                    " - 'First' to find the first object",
                 "base.FPoint|glm.vec2", "point", "string", "mode");
    DOC_METHOD_3("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given point within the given radius from the point.<br>"
                                                                    " - 'All' to find all objects.<br>"
                                                                    " - 'Closest' to find  the closest only.<br>"
                                                                    " - 'First' to find the first object",
                 "base.FPoint|glm.vec2", "point", "float", "radius", "string", "mode");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarById", "Find a script variable by its ID. Returns nil if no such variable was found.", "string", "id");
    DOC_METHOD_1("game.ScriptVar", "FindScriptVarByName", "Find a script variable byt its name. Returns nil if no such variable was found.", "string", "name");

    DOC_TABLE("game.RayCastResult");
    DOC_OBJECT_PROPERTY("game.EntityNode", "node", "The entity node (with rigid body) that intersected with the ray.");
    DOC_OBJECT_PROPERTY("glm.vec2", "point", "The point of intersection in physics world space.");
    DOC_OBJECT_PROPERTY("glm.vec2", "normal", "The normal of the rigid body surface at the point of the ray/body intersection.");
    DOC_OBJECT_PROPERTY("float", "fraction", "The normalized fraction distance along the ray from the start of the ray until the hit point.");

    DOC_TABLE("game.RayCastResultVector");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the result vector is an empty vector or not.");
    DOC_METHOD_0("bool", "HasNext", "Check whether the result vector has a next item or not.");
    DOC_METHOD_0("bool", "Next", "Move to the next item (if any) in the result vector. <br>"
                                 "Returns true if there is a next item or false when there are no more items.");
    DOC_METHOD_0("void", "Begin", "(Re)start the iteration over the result vector. <br>"
                                  "The iteration is already started automatically when the result vector is created,<br>"
                                  "so this only needs to be called if restarting.");
    DOC_METHOD_0("game.RayCastResult", "Get", "Get the current item at this point of iteration over the result vector.");
    DOC_METHOD_1("game.RayCastResult", "GetAt", "Get a result at a given index. The index is zero based and the index must be a valid index." , "unsigned", "index");
    DOC_METHOD_0("game.RayCastResult", "GetNext", "Get the current item and move onto next.");
    DOC_METHOD_0("unsigned", "Size", "Get the number of items in the ray cast result vector.");

    DOC_TABLE("game.Physics");
    DOC_METHOD_3("game.RayCastResultVector", "RayCast", "Perform ray cast to find entity nodes with rigid bodies that intersect with the bounded ray between start and end points.<br"
                                                        "The casting is performed in the physics world coordinate space. <br>"
                                                        "You can use MapVectorFromGame to transform points from game world space to physics world space.<br>"
                                                        "Possible modes, 'Closest', 'First', 'All'<br>"
                                                        "Closest = finds the entity node closest to the starting point of the ray.<br>"
                                                        "First   = finds the node that happened to intersect when tested first.<br>"
                                                        "All     = find all nodes that intersect with the ray.",
                 "glm.vec2", "start", "glm.vec2", "end", "string", "mode = 'All'");

    DOC_METHOD_2("bool", "ApplyImpulseToCenter", "Apply an impulse in Newtons per second to the center of the given physics node.<br>"
                                                "Returns true if impulse was applied otherwise false.",
                "game.EntityNode|string", "node|id", "glm.vec2", "impulse");
    DOC_METHOD_2("bool", "ApplyForceToCenter", "Apply force in Newtons to the center of the given physics node. <br>"
                                               "Returns true if force was applied otherwise false",
                 "game.EntityNode|string", "node|id", "glm.vec2", "force");
    DOC_METHOD_2("bool", "SetLinearVelocity", "Immediately adjust the linear velocity (m/s) of the rigid body to the given velocity value."
                                              "Returns true if the velocity was adjusted otherwise false.",
                 "game.EntityNode|string", "node|id", "glm.vec2", "velocity");

    DOC_METHOD_1("bool, glm.vec2", "FindCurrentLinearVelocity", "Find the current linear velocity of a physics body in meters/s in world space.<br>"
                                                                "Returns true and the current velocity if the body was found otherwise false and zero vector.<br>",
                 "game.EntityNode|string", "node|id");
    DOC_METHOD_1("bool, float", "FindCurrentAngularVelocity", "Find the current angular velocity of a physics body in radians/s in world space.<br>"
                                                              "Returns true and the current velocity if the body was found, otherwise false and 0 velocity.<br>",
                 "game.EntityNode|string", "node|id");
    DOC_METHOD_1("bool, float", "FindMass", "Find the mass (Kg) of a physics body based on size and density.<br>"
                                            "Returns true and mass if the body was found, otherwise false and 0 mass.",
                 "game.EntityNode|string", "node|id");
    DOC_METHOD_0("glm.vec2", "GetScale", "Get the current scaling coefficient for scaling game units to physics world.");
    DOC_METHOD_0("glm.vec2", "GetGravity", "Get the current physics world gravity vector.");
    DOC_METHOD_0("float", "GetTimeStep", "Get the current time step (in seconds) taken on every simulation step.<br>"
                                         "This value is the 1.0/updates where updates is the number of engine updates taken per second.<br>");
    DOC_METHOD_0("unsigned", "GetNumPositionIterations", "Get the number of position iterations taken on every physics update.<br>"
                                                    "This value can be changed in the project|engine settings.");
    DOC_METHOD_0("unsigned", "GetNumVelocityIterations", "Get the number of velocity iterations taken on every physics update.<br>"
                                                    "This value can be changed in the project|engine settings.");
    DOC_METHOD_1("glm.vec2", "MapVectorFromGame", "Map a vector from the game world space into physics world.",
                 "glm.vec2", "vector");
    DOC_METHOD_1("glm.vec2", "MapVectorToGame", "Map a vector from physics world into game world",
                 "glm.vec2", "vector");
    DOC_METHOD_1("float", "MapAngleFromGame", "Map an angle (radians) from game world into physics world.",
                 "float", "angle");
    DOC_METHOD_1("float", "MapAngleToGame", "Map an angle (radians) from physics world into game world.",
                 "float", "angle");
    DOC_METHOD_1("void", "SetGravity", "Set the physics engine gravity vector.<br>"
                                       "Normally the gravity setting is applied through project settings but "
                                       "this function allows explicit control to override that value.<br>"
                                       "The new gravity setting should be called before any physics world is created, i.e. before any scene is loaded",
                 "glm.vec2", "gravity");
    DOC_METHOD_1("void", "SetScale", "Set the physics engine scaling vector for scaling units from game to physics world and vice versa.<br>"
                                     "Normally the scale setting is applied through project settings but "
                                     "this function allows explicit control to override that value.<br>"
                                     "The new scale setting should be called before any physics world is created., i.e. before any scene is loaded",
                 "glm.vec2", "scale");


    DOC_TABLE("game.Audio");
    DOC_METHOD_1("bool", "PrepareMusicGraph", "Prepare a new named music graph for playback. The name of the music graph resource can be used "
                                              "to later identify the audio track when calling functions such as ResumeMusic or PauseMusic.<br>"
                                              "The audio graph is initially only prepared and sent to the audio mixer in paused state.<br>"
                                              "In order to start the actual audio playback ResumeMusic must be called separately.<br>"
                                              "Returns true if the audio graph was prepared successfully or false on error.",
                 "audio.GraphClass|string", "graph|name");
    DOC_METHOD_1("bool", "PlayMusic", "Similar to PrepareMusicGraph except the audio playback is also started immediately."
                                      "Returns true on successful play or false on error.",
                 "audio.GraphClass|string", "graph|name");
    DOC_METHOD_2("bool", "PlayMusic", "Similar to PrepareMusicGraph except the audio playback is started after some delay (in milliseconds) elapses."
                                      "Returns true on successful play or false on error.",
                 "audio.GraphClass|string", "graph|name", "unsigned", "delay");
    DOC_METHOD_1("void", "ResumeMusic", "Resume the playback of the named music graph immediately.",
                 "string", "name");
    DOC_METHOD_2("void", "ResumeMusic", "Resume the playback of the named music graph after some delay (in milliseconds) elapses.",
                 "string", "name", "unsigned", "delay");
    DOC_METHOD_1("void", "PauseMusic", "Pause the playback of the named music graph immediately.", "string", "name");
    DOC_METHOD_2("void", "PauseMusic", "Pause the playback of the named music graph after some delay (in milliseconds) elapses.",
                 "string", "name", "unsigned", "delay");
    DOC_METHOD_1("void", "KillMusic", "Kill the named music graph immediately.", "string", "name");
    DOC_METHOD_2("void", "KillMusic", "Kill the named music graph after some delay (in milliseconds) elapses.",
                 "string", "name", "unsigned", "delay");
    DOC_METHOD_0("void", "KillAllMusic", "Kill all currently playing music graphs.");
    DOC_METHOD_1("void", "CancelMusicCmds", "Cancel all pending named music graph commands.<br>"
                                            "The music graph should be previously created with PrepareMusicGraph or PlayMusic.",
                 "string", "name");
    DOC_METHOD_3("void", "SetMusicEffect", "Set an audio effect on the named music graph.<br>"
                                           "Effect can be one of the following: 'FadeIn', 'FadeOut'",
                 "string", "graph_name", "string", "effect_name", "unsigned", "duration");
    DOC_METHOD_1("void", "SetMusicGain", "Set the overall music gain (volume adjustment) in the audio mixer.", "float", "gain");
    DOC_METHOD_1("bool", "PlaySoundEffect", "Play a sound effect audio graph immediately. Returns true on successful playback or false on error.",
                 "audio.GraphClass|string", "graph|name");
    DOC_METHOD_2("bool", "PlaySoundEffect", "Play a sound effect audio graph after some delay (in milliseconds) elapses. Returns true on successful playback or false on error.",
                 "audio.GraphClass|string", "graph|name", "unsigned", "delay");
    DOC_METHOD_1("void", "SetSoundEffectGain", "Set the overall sound effect gain (volume adjustment) in the audio mixer.", "float", "gain");
    DOC_METHOD_1("void", "EnableEffects", "Enable or disable actual sound effect playing. "
                                          "Toggling the flag does not affect currently playing effects.<br>"
                                          "The initial value is enabled.", "bool", "on_off");

    DOC_TABLE("game.AudioEvent");
    DOC_OBJECT_PROPERTY("string", "type", "The type of the audio event.<br>"
                                          "One of the following: 'TrackDone'<br>"
                                          "This property is read only.");
    DOC_OBJECT_PROPERTY("string", "track", "Name of the audio graph to which the event pertains to.<br>This property is read only.");
    DOC_OBJECT_PROPERTY("string", "source", "Source of the audio event. Either 'music' or 'effect'.<br>This property is read only.");

    DOC_TABLE("game.MouseEvent");
    DOC_OBJECT_PROPERTY("glm.vec2", "map_coord", "Mouse cursor position on the map plane.<br>"
                                                 "Only valid when over_scene is true and there is a map.<br>"
                                                 "This property is read only.");
    DOC_OBJECT_PROPERTY("glm.vec2", "window_coord", "Mouse cursor position in native window coordinates.<br>"
                                                    "This property is read only.");
    DOC_OBJECT_PROPERTY("glm.vec2", "scene_coord", "Mouse cursor position in scene coordinates.<br>"
                                                   "Only valid when over_scene is true.<br>"
                                                   "This property is read only.");
    DOC_OBJECT_PROPERTY("wdk.Buttons", "button", "The mouse button value that was pressed.<br>This property is read only.");
    DOC_OBJECT_PROPERTY("unsigned", "modifiers", "A bit string of keyboard modifier keys that were pressed.<br>"
                                                 "For a list of available modifiers see wdk.Mods.<br>"
                                                 "For testing a modifier use wdk.TestMod(bits, key).<br>"
                                                 "This property is read only.");
    DOC_OBJECT_PROPERTY("bool", "over_scene", "True when the mouse is within the game viewport in the window.<br>"
                                              "Indicates whether screen_coords are valid or not.<br>"
                                              "This property is read only.");

    DOC_TABLE("game.GameEvent");
    DOC_META_METHOD_0("...", "index", "Lua index meta method.");
    DOC_META_METHOD_0("...", "newindex", "Lua new index meta method.");
    DOC_OBJECT_PROPERTY("string|game.Entity|game.Scene", "from", "Free form name or identifier of the event sender or Scene or Entity object.");
    DOC_OBJECT_PROPERTY("string|game.Entity|game.Scene", "to", "Free form name or identifier of the event receiver or Scene or Entity object.");
    DOC_OBJECT_PROPERTY("string", "message", "Message string.");
    DOC_OBJECT_PROPERTY("...", "value", "The value associated with the event.<br>"
                                        "Possible types: <br>"
                                        "bool, int, float, string<br>"
                                        "glm.vec2, glm.vec3, glm.vec4<br>"
                                        "base.Color4f, base.FPoint, base.FSize, base.FRect<br>"
                                        "game.Entity, game.Scene");

    DOC_TABLE("game.KeyValueStore");
    DOC_META_METHOD_0("...", "index", "Lua index meta method.");
    DOC_META_METHOD_0("...", "newindex", "Lua new index meta method.");
    DOC_METHOD_2("void", "SetValue", "Set a value by the given key in the key-value store.",
                 "string", "key", "bool|int|float|string|glm.vec2|glm.vec3|glm.vec4|base.Color4f|base.FSize|base.FRect|base.FPoint", "value");
    DOC_METHOD_1("...", "GetValue", "Get a value by the given key in the key-value store. <br>"
                                    "Returns nil if the given key doesn't exist.",
                 "string", "key");
    DOC_METHOD_2("...", "GetValue", "Get a value by the given key if it exists or return default.",
                 "string", "key", "bool|int|float|string|glm.vec2|glm.vec3|glm.vec4|base.Color4f|base.FSize|base.FRect|base.FPoint", "default");
    DOC_METHOD_1("bool", "HasValue", "Check whether the given key exists in the key-value store.", "string", "key");
    DOC_METHOD_0("void", "Clear", "Remove all keys and values from the store.");
    DOC_METHOD_1("void", "Persist", "Serialize the contents of the key-value store into a data object.",
                 "data.JsonObject|data.Writer", "data");
    DOC_METHOD_1("bool", "Restore", "Deserialize the contents of the key-value store from a data object.",
                 "data.JsonObject|data.Reader", "data");
    DOC_METHOD_2("void", "InitValue", "Initialize a key with a value if it doesn't yet exist.",
                 "string", "key", "bool|int|float|string|glm.vec2|glm.vec3|glm.vec4|base.Color4f|base.FSize|base.FRect|base.FPoint", "value");
    DOC_METHOD_1("void", "DelValue", "Delete a value by the given key from the store.", "string", "key");

    // Lua standard libraries
    DOC_TABLE("math");
    DOC_FUNCTION_1("number", "abs", "Returns the absolute value of x.", "number", "x");
    DOC_FUNCTION_1("float", "acos", "Returns the arc cosine of x (in radians).", "float", "x");
    DOC_FUNCTION_1("float", "asin", "Returns the arc sine of x (in radians).", "float", "x");
    DOC_FUNCTION_2("float", "atan2", "Returns the arc tangent of y/x (in radians), but uses the signs of both parameters to find the quadrant of the result. "
                                      "(It also handles correctly the case of x being zero.)",
                   "float", "y", "float", "x = 1.0");
    DOC_FUNCTION_1("int", "ceil", "Returns the smallest integer larger than or equal to x.", "float", "x");
    DOC_FUNCTION_1("float", "cos", "Returns the cosine of x (assumed to be in radians).", "number", "x");
    DOC_FUNCTION_1("float", "cosh", "Returns the hyperbolic cosine of x.", "number", "x");
    DOC_FUNCTION_1("float", "deg", "Returns the angle x (given in radians) in degrees.", "number", "x");
    DOC_FUNCTION_1("float", "exp", "Returns the value e power x.", "float", "x");
    DOC_FUNCTION_1("int", "floor", "Returns the largest integer smaller than or equal to x.", "number", "x");
    DOC_FUNCTION_2("number", "fmod", "Returns the remainder of the division of x by y that rounds the quotient towards zero.", "number", "x", "number", "y");
    DOC_FUNCTION_1("number, number", "frexp", "Returns m and e such that x = m2e, e is an integer and the absolute value of m is in the range [0.5, 1) (or zero when x is zero).", "number", "x");
    DOC_FUNCTION_2("number", "ldexp", "Returns m2e (e should be an integer).", "number", "m", "integer", "e");
    DOC_FUNCTION_2("float", "log", "Returns the natural logarithm of x.", "float", "x", "float", "base=e");
    DOC_FUNCTION_1("number", "log10", "Returns the base-10 logarithm of x.", "number", "x");
    DOC_FUNCTION_1("number", "max", "Returns the maximum value among its arguments.", "number", "...");
    DOC_FUNCTION_1("number", "min", "Returns the minimum value among its arguments.", "number", "...");
    DOC_FUNCTION_1("int, float", "modf", "Returns two numbers, the integral part of x and the fractional part of x.", "float", "x");
    DOC_FUNCTION_1("float", "rad", "Returns the angle x (given in degrees) in radians.", "float", "x");
    DOC_FUNCTION_1("float", "sin", "Returns the sine of x (assumed to be in radians).", "float", "x");
    DOC_FUNCTION_1("float", "sinh", "Returns the hyperbolic sine of x.", "float", "x");
    DOC_FUNCTION_1("float", "sqrt", "Returns the square root of x. (Note you can also use the expression x^0.5 to compute this value.", "float", "x");
    DOC_FUNCTION_1("float", "tan", "Returns the tangent of x (assumed to be in radians).", "float", "x");
    DOC_FUNCTION_1("float", "tanh", "Returns the hyperbolic tangent of x.", "float", "x");
    DOC_FUNCTION_0("float", "random", "Returns a pseudo-random real number in the range [0, 1].");
    DOC_FUNCTION_1("int", "random", "Returns a pseudo random integer number in the range [1, m].", "int", "m");
    DOC_FUNCTION_2("int", "random", "Returns a pseudo random integer number in the range [m, n].", "int", "m", "int", "n");
    DOC_FUNCTION_1("void", "randomseed", "Seed the pseudo random generator. ", "number", "x");
    DOC_FUNCTION_1("int", "tointeger", "If the value is convertible to integer returns that integer. Otherwise, returns nil.", "number", "x");
    DOC_FUNCTION_1("string", "type", "Returns \"integer\" if x is an integer, \"float\" if it is a float, or \"fail\" if x is not a number.", "number", "x");
    DOC_FUNCTION_2("bool", "ult", "Returns a boolean, true if and only if integer m is below integer n when they are compared as unsigned integers.", "int", "m", "int", "n");
    DOC_TABLE_PROPERTY("number", "pi", "The value of pi.");
    DOC_TABLE_PROPERTY("number", "huge", "The value HUGE_VAL, a value larger than or equal to any other numerical value.");

    // Lua scripts that are packaged with the editor.

    // Keyboard script
    DOC_TABLE2("KB", "Keyboard utilities for common keyboard handling functionality.<br>"
                     "If you want to use this pre-built functionality you should include this in your own script with<br>"
                     "require('app://scripts/utility/keyboard.lua')<br>"
                     "Then you need to make sure that you call KB.KeyUp and KB.KeyDown on any key event your script receives.");
    DOC_FUNCTION_1("bool", "TestKeyDown", "Test whether the logical game action key is currently active or not, i.e. down or not.",
                   "unsigned", "key");
    DOC_FUNCTION_3("void", "KeyDown", "Receive native keyboard key down event and convert it to a logical action key based on the key map.",
                   "unsigned", "symbol", "unsigned", "modifier_bits", "table", "map");
    DOC_FUNCTION_3("void", "KeyUp", "Receive native keyboard key up event nad convert it to a logical action key.",
                   "unsigned", "symbol", "unsigned", "modifier_bits", "table", "map");

    DOC_TABLE2("KB.Keys", "This table defines logical game action keys such as Up, Down etc. "
                          "You can use these for logical actions in your game instead of relying on "
                          "key press events directly. This allows the use of an intermediate mapping table "
                          "to establish a key binding for performing actions in the game.");
    DOC_TABLE_PROPERTY("unsigned", "Left", "Logical game action key for 'Left'.");
    DOC_TABLE_PROPERTY("unsigned", "Right", "Logical game action key for 'Right'.");
    DOC_TABLE_PROPERTY("unsigned", "Up", "Logical game action key for 'Up'.");
    DOC_TABLE_PROPERTY("unsigned", "Down", "Logical game action key for 'Down'.");
    DOC_TABLE_PROPERTY("unsigned", "Fire", "Logical game action key for 'Fire'.");

    DOC_TABLE2("KB.WASD", "A logical key binding for binding WASD keys to logical game action keys.<br>"
                          "W = Up, A = Left, S = Down, D = Right, Space = Fire");
    DOC_TABLE2("KB.ARROW", "A logical key binding for binding  arrow keys to logical game action keys.<br>"
                           "Up arrow = Up, Left Arrow = Left, Right Arrow = Right, Down Arrow = Down and Space = Fire");

    DOC_TABLE("util");
    DOC_FUNCTION_2("glm.vec2", "RandomVec2", "Generate a random glm.vec2 with x and y inside min, max (inclusive).",
                                             "float", "min", "float", "max");
    DOC_FUNCTION_3("...", "lerp", "Linearly interpolate between two values y0 and y1.",
                   "...", "y0", "...", "y1", "float", "t");
    DOC_FUNCTION_4("...", "interpolate", "Interpolate between two values y0 and y1 with an easing curve to adjust t.",
                   "...", "y0", "...", "y1", "float", "t", "easing.Curves", "curve");

    DOC_TABLE2("Camera", "Camera (and viewport) handling routines. The camera can be used to manipulate and change the viewport (FRect) over time "
                         "in order to create effects such as a camera shake");
    DOC_FUNCTION_1("void", "SetViewport", "Set the viewport rectangle to be manipulated.", "base.FRect", "viewport");
    DOC_FUNCTION_2("void", "Shake", "Start shaking the camera.<br>"
                                    "The shake effect is defined by its strength in game units and its duration in seconds.<br>"
                                    "In order to perform the shake you should then call the Update method and take the resulting viewport with the shake effect applied "
                                    "and use that as your game's viewport setting.",
                   "float", "strength", "float", "duration");
    DOC_FUNCTION_1("base.FRect", "Update", "Update the camera with the given time step.<br>"
                                           "Returns the updated viewport with any effects/movement applied.",
                   "float", "dt");

    std::sort(g_method_docs.begin(), g_method_docs.end(),
        [](const auto& left, const auto& right) {
            if (left.table < right.table)
                return true;
            else if (left.table == right.table && left.name < right.name)
                return true;
            else return false;
        });
    done = true;
}

QString FindLuaDocTableMatch(const QString& word)
{
    // check for a known table name suffix.
    for (const auto& item : g_method_docs)
    {
        const auto& table = item.table;
        if (table.endsWith(word))
            return table;
    }
    return "";
}

QString FindLuaDocFieldMatch(const QString& word)
{
    for (const auto& item : g_method_docs)
    {
        if (item.name.contains(word))
            return item.name;
    }
    return "";
}

QString FormatArgHelp(const LuaMemberDoc& doc, LuaHelpStyle style, LuaHelpFormat format)
{
    if (doc.type == LuaMemberType::ObjectProperty ||
        doc.type == LuaMemberType::TableProperty)
        return doc.ret;

    if (doc.type == LuaMemberType::Function || doc.type == LuaMemberType::Method)
    {
        if (style == LuaHelpStyle::DescriptionFormat)
        {
            QString str;
            for (const auto& arg: doc.args)
            {
                str += arg.type;
                str += " ";
                str += arg.name;
                str += ", ";
            }
            if (!str.isEmpty())
                str.chop(2);
            return str;
        }
        else if (style == LuaHelpStyle::FunctionCallFormat)
        {
            QString str;
            for (const auto& arg : doc.args)
            {
                str += arg.name;
                str += ", ";
            }
            if (!str.isEmpty())
                str.chop(2);
            return "(" + str + ")";
        }
    }
    return "";
}

QString FormatHelp(const LuaMemberDoc& doc, LuaHelpFormat forat)
{
    QString str = doc.desc;
    return str.replace("<br>", "\n");
}

QString FormatArgCompletion(const LuaMemberDoc& doc)
{
    if (doc.type == LuaMemberType::ObjectProperty ||
        doc.type == LuaMemberType::TableProperty ||
        doc.type == LuaMemberType::Table)
        return "";

    QString str;
    for (const auto& arg : doc.args)
    {
        str += arg.name;
        str += ", ";
    }
    if (!str.isEmpty())
        str.chop(2);

    return "(" + str + ")";
}

std::size_t GetNumLuaMethodDocs()
{ return g_method_docs.size(); }
const LuaMemberDoc& GetLuaMethodDoc(size_t index)
{ return g_method_docs[index]; }

QString ParseType(const QString& str)
{
    QStringList list = str.split(".");
    if (list.size() == 2)
    {
        // assume table_name.type_name
        // for example glm.vec2
        return QString("<a href=\"#%1\">%1</a>").arg(str);
    }
    return str;
}

QString ParseLuaDocTypeString(const QString& str)
{
    QString ret;

    // multiple objects are separated by ,
    // for example string,bool means string and bool.
    // this is used to return multiple values from functions.
    QStringList objects = str.split(",", Qt::SkipEmptyParts);
    for (int i=0; i<objects.size(); ++i)
    {
        QString object = objects[i].trimmed();
        // conditional objects are indicated by |
        // for example string|glm.vec2 would indicate either a string or glm.vec2
        QStringList types = object.split("|", Qt::SkipEmptyParts);
        if (types.isEmpty())
        {
            ret.append(ParseType(object));
        }
        else
        {
            for (int i=0; i<types.size(); ++i)
            {
                ret.append(ParseType(types[i]));
                if (i+1<types.size())
                    ret.append("|");
            }
        }
        if (i+1 < objects.size())
            ret.append(", ");
    }
    return ret;
}

QString GenerateLuaDocHtmlAnchor(const LuaMemberDoc& doc)
{
    return app::toString("%1_%2", doc.table, doc.name);
}

QString GenerateLuaDocHtml()
{
    static QString html;
    if (!html.isEmpty())
        return html;

    std::map<QString, std::set<QString>> table_methods;
    for (size_t i=0; i<app::GetNumLuaMethodDocs(); ++i)
    {
        const auto& method = app::GetLuaMethodDoc(i);
        table_methods[method.table].insert(method.name);
    }

    QTextStream stream(&html);
    stream << R"(
<!DOCTYPE html>
<html>
  <head>
    <meta name="qrichtext"/>
    <title>Lua API</title>
    <style type="text/css">
    body {
      font-size: 16px;
    }
    div {
      margin:0px;
    }
    div.method {
      margin-bottom: 20px;
    }
    div.description {
        margin-bottom: 10px;
        margin-left: 0px;
        word-wrap: break-word;
    }
    div.signature {
        font-family: monospace;
    }
    span.return {
       font-weight: bold;
       color: DarkRed;
    }
    span.method {
       font-style: italic;
       font-weight: bold;
    }
    span.arg {
       font-weight: bold;
       color: DarkRed;
    }
    span.table_name {
       font-size: 20px;
       font-weight: bold;
       font-style: italic;
    }
    span.table_desc {
        font-size: 18px;
    }

  </style>
  </head>
  <body>
)";

    // build TOC with unordered lists.
    stream << "<ul>\n";
    for (const auto& [table, methods] : table_methods)
    {
        stream << toString("<li id=\"%1\">%1</li>\n", table);
        stream << QString("<ul>\n");
        for (const auto& method_name : methods)
        {
            const auto& method_anchor = toString("%1_%2", table, method_name);
            stream << toString(R"(<li><a href="#%1">%2</a></li>)", method_anchor, method_name);
            stream << "\n";
        }
        stream << QString("</ul>\n");
    }
    stream << "</ul>\n";

    QString current_table;

    // build method documentation bodies.
    for (size_t i=0; i<app::GetNumLuaMethodDocs(); ++i)
    {
        const auto& member = app::GetLuaMethodDoc(i);
        if (member.table != current_table)
        {
            QString table_desc;
            QString table_name = member.table;
            if (const auto* str = base::SafeFind(g_table_docs, member.table))
                table_desc = *str;
            stream << toString("<br><span class=\"table_name\">%1</span>", table_name);
            stream << toString("<br><span class=\"table_desc\">%1</span>", table_desc);
            stream << "<hr>";
            current_table = table_name;
        }

        if (member.type == app::LuaMemberType::Function ||
            member.type == app::LuaMemberType::Method ||
            member.type == app::LuaMemberType::MetaMethod)
        {
            QString method_args;
            for (const auto& a : member.args)
            {
                method_args.append("<span class=\"arg\">");
                method_args.append(app::ParseLuaDocTypeString(a.type));
                method_args.append("</span> ");
                method_args.append(a.name);
                method_args.append(", ");
            }
            if (!method_args.isEmpty())
                method_args.chop(2);

            QString name;
            if (member.type == app::LuaMemberType::Function)
                name = member.table + "." + member.name;
            else if (member.type == app::LuaMemberType::Method)
            {
                if (member.name == "new")
                    name = member.table + ":new";
                else  name = "obj:" + member.name;
            }
            else name = member.name;

            const auto& method_html_name = app::toString("%1_%2", member.table, member.name);
            const auto& method_html_anchor = app::toString("%1_%2", member.table, member.name);
            const auto& method_return = app::ParseLuaDocTypeString(member.ret);
            const auto& method_desc = member.desc;
            const auto& method_name = name;

            stream << QString(
R"(<div class="method" name="%1" id="%2">
  <div class="signature">
     <span class="return">%3 </span>
     <span class="method">%4</span>(%5)
  </div>
  <div class="description">%6</div>
</div>
)").arg(method_html_name)
   .arg(method_html_anchor)
   .arg(method_return)
   .arg(name)
   .arg(method_args)
   .arg(method_desc);
        }
        else
        {
            QString name;
            if (member.type == app::LuaMemberType::TableProperty)
                name = member.table + "." + member.name;
            else if (member.type == app::LuaMemberType::Table)
                name = member.table + "." + member.name;
            else name = "obj." + member.name;

            const auto& prop_html_name = app::toString("%1_%2", member.table, member.name);
            const auto& prop_html_anchor = app::toString("%1_%2", member.table, member.name);
            const auto& prop_return = app::ParseLuaDocTypeString(member.ret);
            const auto& prop_desc = member.desc;
            const auto& prop_name = name;

            stream << QString(
R"(<div class="member" name="%1" id="%2">
   <div class="signature">
      <span class="return">%3 </span>
      <span class="method">%4 </span>
   </div>
   <div class="description">%5</div>
</div>
)").arg(prop_html_name)
   .arg(prop_html_anchor)
   .arg(prop_return)
   .arg(prop_name)
   .arg(prop_desc);
        }
    } // for

    stream << R"(
</body>
</html>
)";

    stream.flush();
    return html;
}

QVariant LuaDocTableModel::data(const QModelIndex& index, int role) const
{
    const auto& doc = GetDocItem(index);
    if (mMode == Mode::HelpView)
    {
        if (role == Qt::DisplayRole)
        {
            if (index.column() == 0) return doc.table;
            else if (index.column() == 1) return app::toString(doc.type);
            else if (index.column() == 2) return doc.name;
            else if (index.column() == 3) return doc.desc;
        }
        else if (role == Qt::DecorationRole && index.column() == 1)
        {
            if (doc.type == app::LuaMemberType::Function ||
                doc.type == app::LuaMemberType::Method ||
                doc.type == app::LuaMemberType::MetaMethod)
                return QIcon("icons:function.png");
            else return QIcon("icons:bullet_red.png");
        }
    }
    else if (mMode == Mode::CodeCompletion)
    {
        if (role == Qt::DisplayRole)
        {
            if (index.column() == 0) return app::toString(doc.type);
            else if (index.column() == 1) return doc.name;
            else if (index.column() == 2) return app::FormatArgHelp(doc, LuaHelpStyle::FunctionCallFormat, LuaHelpFormat::PlainText);
        }
        else if (role == Qt::DecorationRole && index.column() == 0)
        {
            if (doc.type == app::LuaMemberType::Function ||
                doc.type == app::LuaMemberType::Method ||
                doc.type == app::LuaMemberType::MetaMethod)
                return QIcon("icons:function.png");
            else return QIcon("icons:bullet_red.png");
        }
    }
    return {};
}

QVariant LuaDocTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        if (mMode == Mode::HelpView)
        {
            if (section == 0) return "Table";
            else if (section == 1) return "Type";
            else if (section == 2) return "Member";
            else if (section == 3) return "Desc";
        }
        else if (mMode == Mode::CodeCompletion)
        {
            if (section == 0) return "Type";
            else if (section == 1) return "Member";
            else if (section == 2) return "Args";
        }
    }
    return {};
}
int LuaDocTableModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(app::GetNumLuaMethodDocs() + mDynamicCompletions.size());
}

int LuaDocTableModel::columnCount(const QModelIndex&) const
{
    if (mMode == Mode::HelpView)
        return 4;
    else if (mMode ==Mode::CodeCompletion)
        return 3;
    else BUG("Unknown table mode");
}

const LuaMemberDoc& LuaDocTableModel::GetDocItem(size_t index) const
{
    const auto num_static_docs = app::GetNumLuaMethodDocs();
    const auto num_dynamic_docs = mDynamicCompletions.size();
    if (index >= num_static_docs)
    {
        const auto dynamic_index = index - num_static_docs;
        ASSERT(dynamic_index < mDynamicCompletions.size());
        return mDynamicCompletions[dynamic_index];
    }
    return GetLuaMethodDoc(index);
}

const LuaMemberDoc& LuaDocTableModel::GetDocItem(const QModelIndex& index) const
{
    ASSERT(index.row() >= 0);
    return GetDocItem(index.row());
}

void LuaDocModelProxy::SetFindFilter(const QString& filter)
{
    mFindString = filter.toUpper();
}
void LuaDocModelProxy::SetTableNameFilter(const QString& name)
{
    mTableName = name;
}

void LuaDocModelProxy::SetFieldNameFilter(const QString& name)
{
    mFieldName = name;
}

bool LuaDocModelProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    // do the fastest test up front before doing any string
    // based filter testing.
    const auto& doc = mModel->GetDocItem(row);
    if (!mBits.test(doc.type))
        return false;

    bool find_string_match = true;
    bool table_name_string_match = true;
    bool field_name_string_match = true;
    if (!mFindString.isEmpty())
    {
        find_string_match = doc.name.contains(mFindString, Qt::CaseInsensitive) ||
                            doc.desc.contains(mFindString, Qt::CaseInsensitive) ||
                            doc.table.contains(mFindString, Qt::CaseInsensitive);
    }
    if (!mTableName.isEmpty())
    {
        table_name_string_match = doc.table.endsWith(mTableName);
    }
    if (!mFieldName.isEmpty())
    {
        field_name_string_match = doc.name.startsWith(mFieldName, Qt::CaseInsensitive);
    }
    return find_string_match && table_name_string_match && field_name_string_match;
}


} // namespace
