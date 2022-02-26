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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QSortFilterProxyModel>
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QTextStream>
#  include <QTextCursor>
#  include <QFile>
#  include <QHash>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <set>
#include <vector>

#include "base/assert.h"
#include "base/color4f.h"
#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/scriptwidget.h"
#include "engine/lua.h"

#include "wdk/keys.h"

namespace {
struct LuaMethodArg {
    std::string name;
    std::string type;

};
enum class LuaMemberType {
    TableProperty,
    ObjectProperty,
    Function,
    Method,
    MetaMethod
};

struct LuaMemberDoc {
    LuaMemberType type = LuaMemberType::Function;
    std::string table;
    std::string name;
    std::string desc;
    std::string ret;
    std::vector<LuaMethodArg> args;
};
std::string table_name;
std::vector<LuaMemberDoc> g_method_docs;

void SetTable(const std::string& name)
{
    table_name = name;
}

void AddMethod(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

void AddMethod(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name)
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
void AddMethod(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name)
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
void AddMethod(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name,
               const std::string& arg2_type, const std::string& arg2_name)
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

void AddMethod(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name,
               const std::string& arg2_type, const std::string& arg2_name,
               const std::string& arg3_type, const std::string& arg3_name)
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

void AddProperty(LuaMemberType type, const std::string& ret, const std::string& name, const std::string& desc)
{
    LuaMemberDoc doc;
    doc.type  = type;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

std::size_t GetNumLuaMethodDocs()
{ return g_method_docs.size(); }
const LuaMemberDoc& GetLuaMethodDoc(size_t index)
{ return g_method_docs[index]; }

#define DOC_TABLE(name) SetTable(name)
#define DOC_METHOD_0(ret, name, desc) AddMethod(LuaMemberType::Method, ret, name, desc)
#define DOC_METHOD_1(ret, name, desc, a0type, a0name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name)
#define DOC_METHOD_2(ret, name, desc, a0type, a0name, a1type, a1name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name)
#define DOC_METHOD_3(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name)
#define DOC_METHOD_4(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name) AddMethod(LuaMemberType::Method, ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name)

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

void InitDoc()
{
    static bool done = false;
    if (done == true) return;

    // global objects
    DOC_TABLE("_G");
    DOC_TABLE_PROPERTY("game.Audio", "Audio", "Global audio engine instance.");
    DOC_TABLE_PROPERTY("game.Physics", "Physics", "Global physics engine instance.");
    DOC_TABLE_PROPERTY("game.ClassLib", "ClassLib", "Global class library instance.");
    DOC_TABLE_PROPERTY("game.KeyValueStore", "State", "Global key-value store instance.");
    DOC_TABLE_PROPERTY("game.Engine", "Game", "Global game engine instance.");
    DOC_TABLE_PROPERTY("game.Scene", "Scene", "The current scene or nil if no scene is being played.");

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

    DOC_FUNCTION_1("void", "RandomSeed", "Seed the random engine with the given seed value.<br>"
                                     "For any given seed the the generated pseudo random number sequence will always be same on every platform.",
                                     "int", "seed");
    DOC_FUNCTION_2("int|float", "Random", "Generate a new pseudo random number between the given (inclusive) min/max values.<br>"
                                        "This is an overloaded function and takes either ints or floats for min/max.<br>"
                                        "The type of the returned value depends on the type of min/max parameters.",
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
                 "int", "length");
    DOC_FUNCTION_2("string", "FormatString", "Format a string with %1, %2,...%n placeholders with N variable arguments.<br>"
                                           "For example: FormatString('this is %1 that is %2', 123, 'foo') returns 'this is 123 that is foo'.<br>"
                                           "Supported types: string, int, float, bool "
                                           "base.FSize, base.FPoint, base.FRect, base.Color4f "
                                           "glm.vec2, glm.vec3, glm.vec4<br>"
                                           "Any given index can be repeated multiple times.",
                 "string", "fmt", "...", "args");

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


    DOC_TABLE("base");
    DOC_FUNCTION_1("void", "debug", "Print a debug message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "warn", "Print a warning message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "error", "Print an error message in the application log.",
                 "string", "message");
    DOC_FUNCTION_1("void", "info", "Print an information message in the application log.",
                 "string", "message");

    DOC_TABLE("trace");
    DOC_FUNCTION_1("void", "marker", "Set a marker message in the application trace.",
                 "string", "message");
    DOC_FUNCTION_2("void", "marker", "Set a marker message in the application trace in the given trace entry.<br>"
                                   "The given trace entry index MUST BE VALID.<br>"
                                   "Do not call this function unless you know what you're doing.<br>"
                                   "For a safer alternative use the overload without index.",
                 "string", "message",
                 "int", "index");
    DOC_FUNCTION_1("int", "enter", "Enter a new tracing scope for measuring time spent inside the scope.<br>"
                                 "You must manually call trace.leave with index that you received from this call. "
                                 "Not doing so will likely crash the application. ",
                 "string", "scope_name");
    DOC_FUNCTION_1("void", "leave", "Leave a tracing scope that was entered previously.<br>"
                                  "The index must be from a previous call to trace.enter.",
                 "int", "index");

    DOC_TABLE("base.FRect");
    DOC_METHOD_0("base.FRect", "new", "Construct a new axis aligned rectangle without any size.");
    DOC_METHOD_4("base.FRect", "new", "Construct a new axis aligned rectangle.",
                 "float", "x", "float", "y", "float", "width", "float", "height");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the rectangle.");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the rectangle.");
    DOC_METHOD_0("float", "GetX", "Get the X position of the rectangle.");
    DOC_METHOD_0("float", "GetY", "Get the Y position of the rectangle.");
    DOC_METHOD_1("void", "SetX", "Set a new X position for the rectangle.", "float", "x");
    DOC_METHOD_1("void", "SetY", "Set a new Y position for the rectangle.", "float", "y");
    DOC_METHOD_1("void", "SetWidth", "Set a new rectangle width.", "float", "width");
    DOC_METHOD_1("void", "SetHeight", "Set a new rectangle height.", "float", "height");
    DOC_METHOD_2("void", "Resize", "Resize the rectangle to new width and height.", "float", "width", "float", "height");
    DOC_METHOD_2("void", "Grow", "Grow (or shrink) the dimensions of the rectangle.", "float", "dx", "float", "dy");
    DOC_METHOD_2("void", "Move", "Move the rectangle to a new x,y position.", "float", "x", "float", "y");
    DOC_METHOD_2("void", "Translate", "Translate (offset) the rectangle relative to the current position.", "float", "dx", "float", "dy");
    DOC_METHOD_0("bool", "IsEmpty", "Returns true if the rectangle is empty (has zero width or height).");
    DOC_METHOD_2("bool", "TestPoint", "Test whether the given point is inside the rectangle or not.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("bool", "TestPoint", "Test whether the given point is inside the rectangle or not.",
                 "base.FPoint", "point");
    DOC_METHOD_2("base.FPoint", "MapToGlobal", "Map a point relative to the rect origin to a global point.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("base.FPoint", "MapToGlobal", "Map a point relative to the rect origin to a global point.",
                 "base.FPoint", "point");
    DOC_METHOD_2("base.FPoint", "MapToLocal", "Map a global point to a local point relative to the rect origin.",
                 "float", "x", "float", "y");
    DOC_METHOD_1("base.FPoint", "MapToLocal", "Map a global point to a local point relative to the rect origin.",
                 "base.FPoint", "point");
    DOC_METHOD_0("base.FRect, base.FRect, base.FRect, base.FRect", "GetQuadrants", "Split the rectangle into 4 quadrants.");
    DOC_METHOD_0("base.FPoint, base.FPoint, base.FPoint, base.FPoint", "GetCorners", "Get the 4 corners of the rectangle.");
    DOC_METHOD_0("base.FPoint", "GetCenter", "Get the center point of the rectangle.");
    DOC_FUNCTION_2("base.FRect", "Combine", "Create an union of the given rectangles.<br>"
                                          "Example: local union = base.FRect.Combine(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_FUNCTION_2("base.FRect", "Intersect", "Create an intersection of the given rectangles.<br>"
                                            "Example: local intersection = base.FRect.Intersect(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_FUNCTION_2("bool", "TestIntersect", "Test whether the rectangles intersect.<br>"
                                          "Example: local ret = base.FRect.TestIntersect(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.",
                 "base.FRect", "rect");

    DOC_TABLE("base.FSize");
    DOC_METHOD_0("base.FSize", "new", "Construct a new size with zero width and height.");
    DOC_METHOD_2("base.FSize", "new", "Construct a new size with the given width and height.",
               "float", "width", "float", "height");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the size.");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the size.");
    DOC_META_METHOD_2("base.FSize", "operator *", "Lua multiplication meta method.", "base.FSize", "size", "float", "scalar");
    DOC_META_METHOD_2("base.FSize", "operator +", "Lua addition meta method.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_META_METHOD_2("base.FSize", "operator -", "Lua subtraction meta method.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "base.FSize", "size");

    DOC_TABLE("base.FPoint");
    DOC_METHOD_0("base.FPoint", "new", "Construct a new point with zero x, y position.");
    DOC_METHOD_0("base.FPoint", "new", "Construct a new point with the given x,y position.");
    DOC_METHOD_0("float", "GetX", "Get the x position.");
    DOC_METHOD_0("float", "GetY", "Get the y position.");
    DOC_META_METHOD_2("base.FPoint", "operator +", "Lua addition meta method.", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_META_METHOD_2("base.FPoint", "operator -", "Lua subtraction meta method.", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_META_METHOD_1("string", "tostring", "Lua tostring meta method.", "base.FPoint", "point");

    DOC_TABLE("base.Colors");
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        DOC_TABLE_PROPERTY("int", name, base::FormatString("Color value for '%1'.", name));
    }

    DOC_TABLE("base.Color4f");
    DOC_METHOD_0("base.Color4f", "new", "Construct a new color with default channel value.");
    DOC_METHOD_4("base.Color4f", "new", "Construct a new color with normalized float [0.0, 1.0] channel values.",
                 "float", "r", "float", "g", "float", "b", "float", "a");
    DOC_METHOD_4("base.Color4f", "new", "Construct a new color with int [0, 255] channel values.<br>"
                                        "The values are expected to be in linear color space.",
                 "int", "r", "int", "g", "int", "b", "int", "a");
    DOC_METHOD_0("float", "GetRed", "Get normalized red channel value.");
    DOC_METHOD_0("float", "GetGreen", "Get normalized green channel value.");
    DOC_METHOD_0("float", "GetBlue", "Get normalized blue channel value.");
    DOC_METHOD_0("float", "GetAlpha", "Get normalized alpha channel value.");
    DOC_METHOD_1("void", "SetRed", "Set normalized red channel value.", "float", "red");
    DOC_METHOD_1("void", "SetGreen", "Set normalized green channel value.", "float", "green");
    DOC_METHOD_1("void", "SetBlue", "Set normalized blue channel value.", "float", "blue");
    DOC_METHOD_1("void", "SetAlpha", "Set normalized alpha channel value.", "float", "alpha");
    DOC_METHOD_1("void", "SetColor", "Set color based on base.Colors color value.", "int", "color");
    DOC_METHOD_1("void", "SetColor", "Set color based on base.Colors color name.", "string", "color");
    DOC_FUNCTION_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color value.", "int", "color");
    DOC_FUNCTION_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color name.", "string", "color");

    DOC_TABLE("data.Reader");
    DOC_METHOD_1("bool, float", "ReadFloat", "Read a float value from the data chunk.",
                "string", "key");
    DOC_METHOD_1("bool, int", "ReadInt", "Read an int value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, bool", "ReadBool", "Read a bool value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, string", "ReadString", "Read a string value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, glm.vec2", "ReadVec2", "Read a glm.vec2 value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, glm.vec3", "ReadVec3", "Read a glm.vec3 value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, glm.vec4", "ReadVec4", "Read a glm.vec4 value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, base.FRect", "ReadFRect", "Read a base.FRect value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, base.FPoint", "ReadFPoint", "Read a base.FPoint value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, base.FSize", "ReadFSize", "Read a base.FSize value from the data chunk.",
                 "string", "key");
    DOC_METHOD_1("bool, base.Color4f", "ReadColor4f", "Read a base.Color4f value from the data chunk.",
                 "string", "key");

    DOC_METHOD_2("bool, float", "Read", "Read a float value from the data chunk.",
                 "string", "key", "float", "default");
    DOC_METHOD_2("bool, int", "Read", "Read an int value from the data chunk.",
                 "string", "key", "int", "default");
    DOC_METHOD_2("bool, bool", "Read", "Read a bool value from the data chunk.",
                 "string", "key", "bool", "default");
    DOC_METHOD_2("bool, string", "Read", "Read a string value from the data chunk.",
                 "string", "key", "string", "default");
    DOC_METHOD_2("bool, glm.vec2", "Read", "Read a glm.vec2 value from the data chunk.",
                 "string", "key", "glm.vec2", "default");
    DOC_METHOD_2("bool, glm.vec3", "Read", "Read a glm.vec3 value from the data chunk.",
                 "string", "key", "glm.vec3", "default");
    DOC_METHOD_2("bool, glm.vec4", "Read", "Read a glm.vec4 value from the data chunk.",
                 "string", "key", "glm.vec4", "default");
    DOC_METHOD_2("bool, base.FRect", "Read", "Read a base.FRect value from the data chunk.",
                 "string", "key", "base.FRect", "default");
    DOC_METHOD_2("bool, base.FPoint", "Read", "Read a base.FPoint value from the data chunk.",
                 "string", "key", "base.FPoint", "default");
    DOC_METHOD_2("bool, base.FSize", "Read", "Read a base.FSize value from the data chunk.",
                 "string", "key", "base.FSize", "default");
    DOC_METHOD_2("bool, base.Color4f", "Read", "Read a base.Color4f value from the data chunk.",
                 "string", "key", "base.Color4f", "default");

    DOC_METHOD_1("bool", "HasValue", "Check whether the given key exists in the data chunk or not.","string", "key");
    DOC_METHOD_1("bool", "HasChunk", "Check whether a data chunk by the given key exists or not.","string", "key");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the data chunk is empty or not.<br>"
                                   "A data chunk is considered empty when it has no values or child data chunks.");
    DOC_METHOD_1("int", "GetNumChunks", "Get the number of data chunks under the given key.","string", "key");
    DOC_METHOD_2("data.Reader", "GetReadChunk", "Get a read chunk at the given index under the given key."
                                                "Returns a new data reader object for that chunk.",
                 "string", "key", "int", "index");

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

    DOC_TABLE("data.JsonObject");
    DOC_METHOD_0("data.JsonObject", "new", "Construct a new JsonObject.<br>"
                                           "A JsonObject is both a data.Reader and data.Writer so you can call all those methods on it.");
    DOC_METHOD_1("bool, string", "ParseString", "Try to parse the given JSON string.<br>"
                                                "Returns true and an empty string on success or false and error string on error.",
                 "string", "json");
    DOC_METHOD_0("string", "ToString", "Dump the contents of the JsonObject into a string.");

    DOC_TABLE("data");
    DOC_METHOD_1("data.JsonObject", "ParseJsonString", "Create a new JsonObject based on the JSON string.<br>"
                                    "Returns a new JsonObject and an empty string on success or nil and error string on error.",
                 "string", "json");
    DOC_FUNCTION_2("data.JsonObject", "ParseJsonString", "Create a new JsonObject based on the JSON data buffer.<br>"
                                                       "Returns a new JsonObject and an empty string on success or nil and an error string on error.",
                 "todo", "json_data", "size_t", "data_len");
    DOC_FUNCTION_2("bool, string", "WriteJsonFile", "Write the contents of the JsonObject into a file.<br>"
                                                  "Returns true and en empty string on success or false and error string on error.",
                 "data.JsonObject", "json", "string", "filename");
    DOC_FUNCTION_1("data.JsonObject", "ReadJsonFile", "Try to read the given JSON file. <br>"
                                                    "Returns new JsonObject and en empty string on success or nil and error string on error.",
                 "string", "filename");
    DOC_FUNCTION_1("data.Writer", "CreateWriter", "Create a new data.Writer object based on the given format string."
                                               "Format string can be one of the following: 'JSON'<br>"
                                               "Returns nil on unsupported format.",
                 "string", "format");
    DOC_FUNCTION_2("bool, string", "WriteFile", "Dump the contents of the given Writer into a file.<br>"
                                              "Returns true and en empty string on success or false and an error string on error.",
                 "data.Writer", "data", "string", "filename");
    DOC_FUNCTION_1("data.Reader, string", "ReadFile", "Try to read the given file in some supported format.<br>"
                                                    "Currently supported formats: JSON.<br>"
                                                    "Returns a new data.Reader and an empty string on success or nil and an error string on error.",
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
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec2", "vec", "int", "index");
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
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec3", "vec", "int", "index");
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
    DOC_META_METHOD_2("float", "operator []", "Lua index meta method.", "glm.vec4", "vec", "int", "index");
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
    DOC_FUNCTION_1("string", "KeyStr", "Convert a key value to a named key string.", "int", "key");
    DOC_FUNCTION_1("string", "BtnStr", "Convert a mouse button value to a named button string.", "int", "button");
    DOC_FUNCTION_1("string", "ModStr", "Convert a modifier key value to a named modifier string.", "int", "modifier");
    DOC_FUNCTION_1("string", "ModBitStr", "Map keyboard key modifier bit string to a named modifier string.", "int", "mod_bits");
    DOC_FUNCTION_1("bool", "TestKeyDown", "Test whether the given keyboard key is currently down.<br>"
                                        "The key value is one of the key values in in wdk.Keys. <br>"
                                        "This function is only available on Windows and Linux. ",
                                        "int", "key");
    DOC_FUNCTION_2("bool", "TestMod", "Test whether the given modifier bit is set in the bitset of modifier keys.",
                 "int", "modifier_bits", "int", "modifier_value");

    DOC_TABLE("wdk.Keys");
    for (const auto& key : magic_enum::enum_values<wdk::Keysym>())
    {
        const std::string name(magic_enum::enum_name(key));
        DOC_TABLE_PROPERTY("int", name, base::FormatString("Key value for '%1'.", name));
    }
    DOC_TABLE("wdk.Mods");
    for (const auto& mod : magic_enum::enum_values<wdk::Keymod>())
    {
        const std::string name(magic_enum::enum_name(mod));
        DOC_TABLE_PROPERTY("int", name, base::FormatString("Modifier value for '%1'.", name));
    }
    DOC_TABLE("wdk.Buttons");
    for (const auto& btn : magic_enum::enum_values<wdk::MouseButton>())
    {
        const std::string name(magic_enum::enum_name(btn));
        DOC_TABLE_PROPERTY("int", name, base::FormatString("Mouse button value for '%1'.", name));
    }
    DOC_TABLE("wdk.KeyBitSet");
    DOC_METHOD_0("wdk.KeyBitSet", "new", "Construct new key symbol bit set.");
    DOC_METHOD_2("void", "Set", "Set a key symbol bit on or off.", "int", "key", "bool", "on");
    DOC_METHOD_1("bool", "Test", "Test whether a key symbol bit is on or off.", "int", "key");
    DOC_METHOD_0("bool", "AnyBit", "Check whether any bit is set.");
    DOC_METHOD_0("unsigned", "Value", "Get the underlying integer value.");
    DOC_METHOD_0("void", "Clear", "Clear all bits.");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator &", "Lua bitwise and meta method.", "wdk.KeyBitSet", "lhs", "wdk.KeyBitSet", "rhs");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator &", "Lua bitwise and meta method.", "wdk.KeyBitSet", "bits", "int", "key");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator |", "Lua bitwise or meta method.", "wdk.KeyBitSet", "lhs", "wdk.KeyBitSet", "rhs");
    DOC_META_METHOD_2("wdk.KeyBitSet", "operator |", "Lua bitwise or meta method.", "wdk.KeyBitSet", "bits", "int", "key");

    DOC_TABLE("uik");
    DOC_FUNCTION_2("uik.Widget", "WidgetCast", "Downcast a Widget object to concrete widget type.<br>"
                                             "Returns nil if the widget doesn't have the right type.",
                 "uik.Widget", "widget", "string", "downcast_type");

    DOC_TABLE("uik.Widget");
    DOC_METHOD_0("string", "GetId", "Get the widget ID.");
    DOC_METHOD_0("string", "GetName", "Get the widget name.");
    DOC_METHOD_0("size_t", "GetHash", "Get the widget hash value based on its contents.");
    DOC_METHOD_0("base.FSize", "GetSize", "Get the size of the widget.");
    DOC_METHOD_0("base.FPoint", "GetPosition", "Get the widget position relative to its parent.");
    DOC_METHOD_0("string", "GetType", "Get the dynamic name of the widget object type.");
    DOC_METHOD_1("void", "SetName", "Set the widget name.", "string", "name");
    DOC_METHOD_1("void", "SetSize", "Set the widget size.", "base.FSize", "size");
    DOC_METHOD_1("void", "SetPosition", "Set the widget position relative to its parent.", "base.FPoint", "position");
    DOC_METHOD_1("bool", "TestFlag", "Test for a widget flag.", "string", "flag_name");
    DOC_METHOD_0("uik.Label", "AsLabel", "Cast the widget to Label.");
    DOC_METHOD_0("uik.PushButton", "AsPushButton", "Cast the widget to PushButton.");
    DOC_METHOD_0("uik.CheckBox", "AsCheckBox", "Cast the widget to CheckBox.");
    DOC_METHOD_0("uik.GroupBox", "AsGroupBox", "Cast the widget to GroupBox.");
    DOC_METHOD_0("uik.SpinBox", "AsSpinBox", "Cast the widget to SpinBox.");
    DOC_METHOD_0("uik.ProgressBar", "AsProgressBar", "Cast the widget to ProgressBar.");
    DOC_METHOD_0("uik.Form", "AsForm", "Cast the widget to Form.");
    DOC_METHOD_0("uik.Slider", "AsSlider", "Cast the widget to Slider.");
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
    DOC_TABLE("uik.Window");
    DOC_METHOD_0("string", "GetId", "Get the window ID.");
    DOC_METHOD_0("string", "GetName", "Get the window name.");
    DOC_METHOD_0("int", "GetNumWidgets", "Get the number of widgets in the window.");
    DOC_METHOD_1("uik.Widget", "FindWidgetById", "Find a widget by the given Widget ID.<br>"
                                                 "Returns nil if there's no such widget.", "string", "id");
    DOC_METHOD_2("uik.Widget", "FindWidgetById", "Find a widget by the given Widget ID and cast it to a concrete widget type.<br>"
                                                 "Returns nil if there's no such widget or the widget doesn't have the right type.",
                 "string", "id", "string", "downcast_type");
    DOC_METHOD_1("uik.Widget", "FindWidgetByName", "Find a widget by the given Widget name.<br>"
                                                   "If there are multiple widgets by the same name it's unspecified which one will be returned.<br>"
                                                   "Returns nil if there's no such widget.",
                 "string", "name");
    DOC_METHOD_2("uik.Widget", "FindWidgetByName", "Find a widget by the given Widget name and cast it to a concrete widget type.<br>"
                                                   "If there are multiple widgets by the same name it's unspecified which one will be returned.<br>"
                                                   "Returns nil if there's no such widget or the widget doesn't have the right type.",
                 "string", "name", "string", "downcast_type");
    DOC_METHOD_1("uik.Widget", "FindWidgetParent", "Find the parent widget of the given widget.<br>"
                                                   "Returns nil if the widget is the root widget and doesn't have a parent.",
                 "uik.Widget", "widget");
    DOC_METHOD_1("uik.Widget", "GetWidget", "Get a widget by the given index.", "int", "index");
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
    DOC_METHOD_1("void", "Play", "Play a scene. Any previous scene is deleted and the new scene is started.",
                 "game.SceneClass|string", "klass|name");
    // todo: resume/suspend
    DOC_METHOD_0("void", "EndPlay", "End the play of the current scene. Will invoke EndPlay callbacks and end game play cleanly.");
    DOC_METHOD_1("void", "Quit", "Quit the game by asking the host application to exit.", "int", "exit_code");
    DOC_METHOD_1("void", "Delay", "Insert a time delay into the engine request queue. <br>"
                                  "All the functions in the game.Engine interface are pushed into a queue and<br>"
                                  "adding a delay will postpone the processing of all subsequent engine requests.<br>"
                                  "For example if Delay(2.0) OpenUI('MyUI') the UI will be opened after 2 seconds.",
                 "float", "seconds");
    DOC_METHOD_1("void", "GrabMouse", "Request the host application to enable/disable mouse grabbing.", "bool", "grab");
    DOC_METHOD_1("void", "ShowMouse", "Request the host application to show/hide the OS mouse cursor.", "bool", "show");
    DOC_METHOD_1("void", "ShowDebug", "Toggle debug messages on/off in the engine.", "bool", "show");
    DOC_METHOD_1("void", "SetFullScreen", "Request the host application to toggle full screen mode.", "bool", "full_screen");
    DOC_METHOD_1("void", "BlockKeyboard", "Toggle blocking keyboard events on/off.<br>"
                                          "When keyboard block is enabled the keyboard events are coming from the OS are not processed,<br>"
                                          "and none of the entity/scene keyboard handlers are called.<br>"
                                          "Note that this does not block low level keyboard polling such as wdk.TestKeyDown from working,<br>"
                                          "but only *event* based keyboard processing is affected.",
                 "bool", "block");
    DOC_METHOD_1("void", "BlockMouse", "Turn mouse blocking on or off.<br>"
                                       "When mouse block is enabled the mouse events coming from the OS are not processed<br>"
                                       "and none of the entity/scene mouse event handlers are called.",
                 "bool", "block");
    DOC_METHOD_1("void", "DebugPrint", "Print a debug message in the game window.", "string", "message");
    DOC_METHOD_0("void", "DebugClear", "Clear all previous debug prints from the game window.");
    DOC_METHOD_1("uik.Window", "OpenUI", "Open a new uik.Window and place it on the top of the UI Window stack.<br>"
                                         "The window will remain open until CloseUI is called.<br>"
                                         "Returns a reference to the window that was opened so it's possible to use the<br>"
                                         "returned window object to query for widgets etc. and set their initial values conveniently.",
                 "uik.Window|string", "window|name");
    DOC_METHOD_0("void", "CloseUI", "Close the topmost UI Window and pop it off the window stack.");
    DOC_METHOD_1("void", "PostEvent", "Post a GameEvent to all OnGameEvent handlers.", "game.GameEvent", "event");
    DOC_METHOD_1("void", "ShowDeveloperUI", "Show or hide the developer UI when supported by the host app/platform.", "bool", "show");
    DOC_METHOD_1("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "This method is only available in the game main script.<br>"
                                        "The initial viewport is a viewport without any dimensions.",
                 "base.FRect", "viewport");
    DOC_METHOD_4("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "This method is only available in the game main script.<br>"
                                        "The initial viewport is a viewport without any dimensions.",
                 "float", "x", "float", "y", "float", "width", "float", "height");
    DOC_METHOD_2("void", "SetViewport", "Set the game's logical (in game units) viewport that covers the currently visible part of the game world.<br>"
                                        "This method is only available in the game main script.<br>"
                                        "The initial viewport is a viewport without any dimensions.<br>"
                                        "This function keeps the viewport at 0,0 and resizes it to the given width and height.",
                 "float", "width", "float", "height");
    DOC_METHOD_0("uik.Window", "GetTopUI", "Get the topmost UI Window from the window stack. If no window is currently open then nil is returned.<br>"
                                           "This method is only available in the game main script.");

    DOC_TABLE("game.ClassLibrary");
    DOC_METHOD_1("game.EntityClass", "FindEntityClassByName", "Find an entity class by name.<br>"
                                                              "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("game.EntityClass", "FindEntityClassById", "Find an entity class by its class ID.<br>"
                                                            "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("game.SceneClass", "FindSceneClassByName", "Find a scene class by name.<br>"
                                                            "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("game.SceneClass", "FindSceneClassById", "Find a scene class by its class ID.<br>"
                                                          "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("audio.GraphClass", "FindAudioGraphClassByName", "Find an audio graph class by name.<br>"
                                                            "Returns nil if no such class object could be found.","string", "name");
    DOC_METHOD_1("audio.GraphClass", "FindAudioGraphClassById", "Find an audio graph class by its class ID.<br>"
                                                          "Returns nil if no such class object could be found.","string", "id");
    DOC_METHOD_1("uik.Window", "FindUIByName", "Find a UI Window by name.<br>"
                                               "Returns nil if no such window object could be found.","string", "name");
    DOC_METHOD_1("uik.Window", "FindUIById", "Find a UI Window by ID.<br>"
                                               "Returns nil if no such window object could be found.","string", "id");

    DOC_TABLE("game.Drawable");
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
                                       "The parameter is identified by it's uniform name in the material shader.<br>"
                                       "Supported values are float, int, base.Color4f, glm.vec2, glm.vec3, glm.vec4",
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

    DOC_TABLE("game.RigidBody");
    DOC_METHOD_0("bool", "IsEnabled", "Check whether the body is enabled in the physics simulation or not.");
    DOC_METHOD_0("bool", "IsSensor", "Check whether the body is a sensor only body.");
    DOC_METHOD_0("bool", "IsBullet", "Check whether the body is a fast moving (bullet) body.");
    DOC_METHOD_0("bool", "CanSleep", "Check whether the body can sleep in the physics simulation or not.");
    DOC_METHOD_0("bool", "DiscardRotation", "Check whether the body discards any rotation or not.");
    DOC_METHOD_0("float", "GetFriction", "Return the friction value of the rigid body.");
    DOC_METHOD_0("float", "GetRestitution", "Return the restitution value of the rigid body.");
    DOC_METHOD_0("float", "GetAngularDamping", "Return the angular damping of the rigid body.");
    DOC_METHOD_0("float", "GetLinearDamping", "Return the linear damping of the rigid body.");
    DOC_METHOD_0("float", "GetDensity", "Get the density value of the rigid body.");
    DOC_METHOD_0("string", "GetPolygonShapeId", "Get the ID of the polygonal shape for the physics body.");
    DOC_METHOD_0("glm.vec2", "GetLinearVelocity", "Get the current linear velocity (m/s) of the rigid body.");
    DOC_METHOD_0("float", "GetAngularVelocity", "Get the current angular (rotational) velocity (rad/s) of the rigid body.");
    DOC_METHOD_1("void", "Enable", "Enable or disable the body in physics simulation.", "bool", "enabled");
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

    DOC_TABLE("game.EntityNode");
    DOC_METHOD_0("string", "GetName", "Get the entity node's human readable instance name.");
    DOC_METHOD_0("string", "GetId", "Get the entity node's instance ID.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the entity node's class type.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the entity node's class type.");
    DOC_METHOD_0("glm.vec2", "GetTranslation", "Get the node's translation relative to its parent.");
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
    DOC_METHOD_1("void", "Translate", "Translate the node relative to its current translation.", "glm.vec2", "delta");
    DOC_METHOD_2("void", "Translate", "Translate the node relative to its current translation.", "float", "dx", "float", "dy");
    DOC_METHOD_1("void", "Rotate", "Rotate the node relative to its current rotation.", "float", "rotation");

    DOC_TABLE("game.EntityClass");
    DOC_METHOD_0("string", "GetId", "Get the entity class ID.");
    DOC_METHOD_0("string", "GetName", "Get the entity class name.");
    DOC_METHOD_0("float", "GetLifetime", "Get the entity lifetime.");
    DOC_METHOD_0("bool|float|string|int|vec2", "index",
                 "Lua index meta method.<br>"
                 "The entity class's script variables are accessible as properties of the entity class object.<br>"
                 "For example a script variable named 'score' would be accessible as object.score.<br>");

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
    DOC_METHOD_0("game.AnimationClass", "GetClass", "Get the class object.");

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
    DOC_METHOD_0("string", "GetClassName", "Get entity class type name.");
    DOC_METHOD_0("string", "GetClassId", "Get entity class type ID.");
    DOC_METHOD_0("int", "GetNumNodes", "Get the number of entity nodes in this entity.");
    DOC_METHOD_0("float", "GetTime", "Get the entity's current accumulated (life) time.");
    DOC_METHOD_0("int" , "GetLayer", "Get the entity's render layer in the scene rendering.");
    DOC_METHOD_1("void", "SetLayer", "Set the entity's render layer in the scene rendering.", "int", "layer");
    DOC_METHOD_0("bool", "IsAnimating", "Checks whether the entity is currently playing an animation or not.");
    DOC_METHOD_0("bool", "HasExpired", "Checks whether the entity has expired, i.e. exceeded it's max lifetime.");
    DOC_METHOD_0("bool", "HasBeenKilled", "Checks whether the entity has been killed.<br>"
                                          "Entities that have been killed will be deleted from the scene at the end of this game loop.");
    DOC_METHOD_0("bool", "HasBeenSpawned", "Checks whether the entity has just been spawned and exists for the first iteration of the game loop.<br>"
                                           "This flag is only ever true on the first iteration of the game loop during the entity's lifetime.");
    DOC_METHOD_0("game.Scene", "GetScene", "Get the current scene.");
    DOC_METHOD_1("game.EntityNode", "GetNode", "Get an entity node at the the given index.", "int", "index");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassName", "Find a node in the entity by it's class name.<br>"
                                                           "If multiple nodes have the same class name it's unspecified which one is returned.<br>"
                                                           "Returns nil if no such node could be found.",
                 "string", "class_name");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassId", "Find a node in the entity by its class ID.<br>"
                                                         "Returns nil if no such node could be found.",
                 "string", "class_id");
    DOC_METHOD_1("game.EntityNode", "FindNodeByInstanceId", "Find a node in the entity by its instance ID.<br>"
                                                            "If multiple nodes have the same ID it's unspecified which one is returned.<br>"
                                                            "Returns nil if no such node could be found.",
                 "string", "id");
    DOC_METHOD_0("game.Animation", "PlayIdle", "Play the entity's idle animation (if any).<br>"
                                               "Returns nil if the entity doesn't have any idle animation or is already playing an animation.");
    DOC_METHOD_0("game.Animation", "PlayAnimationByName", "Play an animation by the given name if found.<br>"
                                                          "Any current animation is replaced by this new animation.<br>"
                                                          "Returns the animation instance or nil if no such animation could be found.");
    DOC_METHOD_0("game.Animation", "PlayAnimationById", "Play an animation by the given ID.<br>"
                                                        "Any current animation is replaced by this new animation.<br>"
                                                        "Returns the animation instance or nil if no such animation could be found.");
    DOC_METHOD_1("bool", "TestFlag", "Test entity flag.", "string", "flag_name");
    DOC_METHOD_0("void", "Die", "Let the entity die and be removed from the scene.");

    DOC_TABLE("game.EntityArgs");
    DOC_OBJECT_PROPERTY("game.EntityClass", "class", "The class object (type) of the entity.");
    DOC_OBJECT_PROPERTY("string", "id", "The instance ID of the entity.<br>"
                                        "The ID should be unique in the current scene across the entities and entity nodes.<br>"
                                        "If no ID is set (id is an empty string) one will be generated when the entity is spawned.");
    DOC_OBJECT_PROPERTY("string", "name", "The instance name of the entity.");
    DOC_OBJECT_PROPERTY("glm.vec2", "scale", "The scaling factor that will apply to all of the entity nodes.<br>"
                                      "Default is (1.0, 1.0).");
    DOC_OBJECT_PROPERTY("glm.vec2", "position", "The initial position of the entity in the scene.<br>"
                                         "Default is (0.0, 0.0)");
    DOC_OBJECT_PROPERTY("float", "rotation", "The initial rotation that will apply to the entity in the scene.<br>"
                                      "Default is 0.0 (i.e no rotation).");
    DOC_OBJECT_PROPERTY("bool", "logging", "Whether to enable life time related engine logs for this entity.<br>"
                                   "Default is true.");

    DOC_TABLE("game.SpatialQueryResultSet");
    DOC_METHOD_0("bool", "IsEmpty", "Check whether the result set is an empty set or not.");
    DOC_METHOD_0("bool", "HasNext", "Check whether the result set has a next item or not.");
    DOC_METHOD_0("bool", "Next", "Move to the next item (if any) in the result set. <br>"
                                 "Returns true if there is a next item or false when there are no more items.");
    DOC_METHOD_0("void", "Begin", "(Re)start the iteration over the result set. <br>"
                                  "The iteration is already started automatically when the query is created.<br>"
                                  "So this only needs to be called if restarting.");
    DOC_METHOD_0("game.EntityNode", "Get", "Get the current item at this point of iteration over the result set.");

    DOC_TABLE("game.ScriptVar");
    DOC_METHOD_0("bool|float|string|int|vec2", "GetValue", "Get the value of the script variable.");
    DOC_METHOD_0("string", "GetName", "Get the script variable name.");
    DOC_METHOD_0("string", "GetId", "Get the script variable ID.");

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

    DOC_TABLE("game.Scene");
    DOC_METHOD_0("bool|float|string|int|vec2", "index", "Lua index meta method.<br>"
                                                        "The scene's script variables are accessible as properties of the scene object.<br>"
                                                        "For example a script variable named 'score' would be accessible as object.score.");
    DOC_METHOD_0("bool|float|string|int|vec2", "newindex", "Lua new index meta method.<br>"
                                                             "The scene's script variables are accessible as properties of the scene object.<br>"
                                                             "For example a script variable named 'score' would be accessible as object.score.");
    DOC_METHOD_0("int", "GetNumEntities", "Get the number of entities currently in the scene.");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceId", "Find an entity with the given instance ID.<br>"
                                                          "Returns nil if no such entity could be found.",
                 "string", "id");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceName", "Find an entity with the given instance name.<br>"
                                                            "Returns nil if no such entity could be found.<br>"
                                                            "In case of multiple entities with the same name the first one with a matching name is returned.<br>",
                 "string", "name");
    DOC_METHOD_1("game.Entity", "GetEntity", "Get an entity at the given index.", "int", "index");
    DOC_METHOD_1("void", "KillEntity", "Flag an entity for removal from the scene. <br>"
                                       "Killing an entity doesn't not immediately remove it from the scene but will only "
                                       "set a flag that will indicate the new state of the entity. The entity will then continue to exist "
                                       "for one more iteration of the game loop until it's deleted at the end of this *next* iteration.<br>"
                                       "This two step design allows any engine subsystems (or game) to realize and react to entities being killed by looking at the kill flag state.",
                                       "game.Entity", "carcass");
    DOC_METHOD_2("game.Entity", "SpawnEntity", "Spawn a new entity in the scene. Spawning an entity doesn't immediately place it in the scene "
                                               "but will only add it to the list of objects to be spawned at the start of the next iteration of game loop."
                                               "At the start of the next game loop each entity that was spawned will have their spawn flag set.<br>"
                                               "This two step design allows any engine subsystem to realize and react to entities being spawned.<br>"
                                               "If link_to_root is true the entity is linked to the root of the scene's entity hierarchy.<br>"
                                               "If link_to_root is false the entity is not linked to the scene hierarchy at all and you should manually call LinkChild later.",
                 "game.EntityArgs", "args", "bool", "link_to_root = true");
    DOC_METHOD_1("glm.mat4", "FindEntityTransform", "Find the transform for transforming the entity into the world/scene coordinate space.",
                 "game.Entity", "entity");
    DOC_METHOD_2("glm.mat4", "FindEntityNodeTransform", "Find the transform for transforming the entity node into the the world/scene coordinate space.",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_1("base.FRect", "FindEntityBoundingRect", "Find the axis aligned bounding box (AABB) for the entity in the scene.", "game.Entity", "entity");
    DOC_METHOD_2("base.FRect", "FindEntityNodeBoundingRect", "Find the axis aligned bounding box (AABB) for the entity node in the scene",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_2("base.FBox", "FindEntityNodeBoundingBox", "Find the oriented bounding box (OOB) for the entity node in the scene.",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_3("glm.vec2", "MapVectorFromEntityNode", "Map a a directional vector relative to entity node coordinate basis into scene/world space.<br>"
                                                    "The resulting vector is not translated, unit length direction vector in world/scene space.",
                 "game.Entity", "entity", "game.EntityNode", "node", "glm.vec2", "vector");
    DOC_METHOD_3("base.FPoint", "MapPointFromEntityNode", "Map a point relative to entity node coordinate space into world/scene space.",
                 "game.Entity", "entity", "game.EntityNode", "node", "base.FPoint", "point");
    DOC_METHOD_0("float", "GetTime", "Get the scene's current time.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the scene's class.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the scene's class.");
    DOC_METHOD_0("game.SceneClass", "GetClass", "Get the scene's class object.");
    DOC_METHOD_1("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given search rectangle.",
                 "base.FRect", "area_of_interest");
    DOC_METHOD_1("game.SpatialQueryResultSet", "QuerySpatialNodes", "Query the scene for entity nodes that have a spatial node attachment and "
                                                                    "whose spatial nodes intersect with the given point.",
                 "base.FPoint|glm.vec2", "point");

    DOC_TABLE("game.Physics");
    DOC_METHOD_2("bool", "ApplyImpulseToCenter", "Apply an impulse in Newtons per second to the center of the given physics node.<br>"
                                                "Returns true if impulse was applied otherwise false.",
                "string|game.EntityNode", "id|node", "glm.vec2", "impulse");
    DOC_METHOD_2("bool", "ApplyForceToCenter", "Apply force in Newtons to the center of the given physics node. <br>"
                                               "Returns true if force was applied otherwise false",
                 "string|game.EntityNode", "id|node", "glm.vec2", "force");
    DOC_METHOD_2("bool", "SetLinearVelocity", "Immediately adjust the linear velocity (m/s) of the rigid body to the given velocity value."
                                              "Returns true if the velocity was adjusted otherwise false.",
                 "string|game.EntityNode", "id|node", "glm.vec2", "velocity");

    DOC_METHOD_1("bool, glm.vec2", "FindCurrentLinearVelocity", "Find the current linear velocity of a physics body.<br>"
                                                                "Returns true and the current velocity if the body was found otherwise false and zero vector.<br>"
                                                                "meters per second in world space.",
                 "string|game.EntityNode", "id|node");
    DOC_METHOD_1("bool, float", "FindCurrentAngularVelocity", "Find the current angular velocity of a physics body.<br>"
                                                              "Returns true and the current velocity if the body was found, otherwise false and 0 velocity.<br>"
                                                              "Radians per second in world space.",
                 "string|game.EntityNode", "id|node");
    DOC_METHOD_1("bool, float", "FindMass", "Find the mass (Kg) of a physics body based on size and density.<br>"
                                            "Returns true and mass if the body was found, otherwise false and 0 mass.",
                 "string|game.EntityNode", "id|node");
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
                                       "Normally the gravity setting is applied through project settings but<br>"
                                       "this function allows explicit control to override that value.<br>"
                                       "The new gravity setting should be called before any physics world is created.",
                 "glm.vec2", "gravity");
    DOC_METHOD_1("void", "SetGravity", "Set the physics engine scaling vector for scaling units from game to physics world and vice versa.<br>"
                                       "Normally the scale setting is applied through project settings but<br>"
                                       "this function allows explicit control to override that value.<br>"
                                       "The new gravity setting should be called before any physics world is created.",
                 "glm.vec2", "scale");


    DOC_TABLE("game.Audio");
    DOC_METHOD_1("bool", "PrepareMusicGraph", "Prepare a new named music graph for playback.<br>"
                                              "The audio graph is initially only prepared and sent to the audio mixer in paused state.<br>"
                                              "In order to start the actual audio playback ResumeMusic must be called separately.<br>"
                                              "Returns true if the audio graph was prepared successfully or false on error.",
                 "audio.GraphClass|string", "graph|name");
    DOC_METHOD_1("bool", "PlayMusic", "Similar to PrepareMusicGraph except the audio playback is also started immediately.",
                 "audio.GraphClass|string", "graph|name");
    DOC_METHOD_2("bool", "PlayMusic", "Similar to PrepareMusicGraph except the audio playback is started after some delay (in milliseconds) elapses.",
                 "audio.GraphClass|string", "graph|name", "unsigned", "delay");
    DOC_METHOD_1("void", "ResumeMusic", "Resume the playback of the named music graph immediately.", "string", "name");
    DOC_METHOD_2("void", "ResumeMusic", "Resume the playback of the named music graph after some delay (in milliseconds) elapses.",
                 "string", "track_name", "unsigned", "delay");
    DOC_METHOD_1("void", "PauseMusic", "Pause the playback of the named music graph immediately.", "string", "name");
    DOC_METHOD_2("void", "PauseMusic", "Pause the playback of the named music graph after some delay (in milliseconds) elapses.",
                 "string", "name", "unsigned", "delay");
    DOC_METHOD_1("void", "KillMusic", "Kill the named music graph immediately.", "string", "name");
    DOC_METHOD_2("void", "KillMusic", "Kill the named music graph after some delay (in milliseconds) elapses.",
                 "string", "name", "unsigned", "delay");
    DOC_METHOD_0("void", "KillAllMusic", "Kill all currently playing music tracks.");
    DOC_METHOD_1("void", "CancelMusicCmds", "Cancel all pending named music track commands.", "string", "name");
    DOC_METHOD_3("void", "SetMusicEffect", "Set an audio effect on the named music track.<br>"
                                           "Effect can be one of the following: 'FadeIn', 'FadeOut'",
                 "string", "track_name", "string", "effect_name", "unsigned", "duration");
    DOC_METHOD_1("void", "SetMusicGain", "Set the overall music gain (volume adjustment) in the audio mixer.", "float", "gain");
    DOC_METHOD_1("void", "PlaySoundEffect", "Play a sound effect immediately.", "audio.GraphClass|string", "graph|name");
    DOC_METHOD_2("void", "PlaySoundEffect", "Play a sound effect after some delay (in milliseconds) elapses.",
                 "audio.GraphClass|string", "graph|name", "unsigned", "delay");
    DOC_METHOD_1("void", "SetSoundEffectGain", "Set the overall sound effect gain (volume adjustment) in the audio mixer.", "float", "gain");

    DOC_TABLE("game.AudioEvent");
    DOC_OBJECT_PROPERTY("string", "type", "The type of the audio event.<br>"
                                          "One of the following: 'TrackDone'");
    DOC_OBJECT_PROPERTY("string", "track", "Name of the track to which the event pertains to.");
    DOC_OBJECT_PROPERTY("string", "source", "Source of the audio event. Either 'music' or 'effect'");

    DOC_TABLE("game.MouseEvent");
    DOC_OBJECT_PROPERTY("glm.vec2", "window_coord", "Mouse cursor position in native window coordinates.");
    DOC_OBJECT_PROPERTY("glm.vec2", "scene_coord", "Mouse cursor position in scene coordinates.<br>"
                                            "Only valid when over_scene is true.");
    DOC_OBJECT_PROPERTY("int", "button", "The mouse button value that was pressed.<br>"
                                  "For a list of available buttons see wdk.Buttons");
    DOC_OBJECT_PROPERTY("int", "modifiers", "A bit string of keyboard modifier keys that were pressed.<br>"
                                     "For a list of available modifiers see wdk.Mods.<br>"
                                     "For testing a modifier use wdk.TestMod(bits, key).");
    DOC_OBJECT_PROPERTY("bool", "over_scene", "True when the mouse is within the game viewport in the window.<br>"
                                       "Indicates whether screen_coords are valid or not.");

    DOC_TABLE("game.GameEvent");
    DOC_OBJECT_PROPERTY("string", "from", "Free form name or identifier of the sender.");
    DOC_OBJECT_PROPERTY("string", "to", "Free form name or identifier of the event receiver.");
    DOC_OBJECT_PROPERTY("string", "message", "Message string.");
    DOC_OBJECT_PROPERTY("...", "value", "The value associated with the event.<br>"
                                        "Possible types: <br>"
                                        "bool, int, float, string<br>"
                                        "glm.vec2, glm.vec3, glm.vec4<br>"
                                        "base.Color4f, base.FPoint, base.FSize, base.FRect");

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


} // namespace

namespace gui
{
class ScriptWidget::TableModel : public QAbstractTableModel
{
public:
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& doc = GetLuaMethodDoc(index.row());
        if (role == Qt::DisplayRole) {
            if (index.column() == 0) return app::FromUtf8(doc.table);
            else if (index.column() == 1) return app::toString(doc.type);
            else if (index.column() == 2) return app::FromUtf8(doc.name);
            else if (index.column() == 3) return app::FromUtf8(doc.desc);
        } else if (role == Qt::DecorationRole && index.column() == 1)  {
            if (doc.type == LuaMemberType::Function || doc.type == LuaMemberType::Method  ||
                doc.type == LuaMemberType::MetaMethod)
                return QIcon("icons:function.png");
            else return QIcon("icons:bullet_red.png");
        }
        return {};
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
            if (section == 0) return "Table";
            else if (section == 1) return "Type";
            else if (section == 2) return "Member";
            else if (section == 3) return "Desc";
        }
        return {};
    }
    virtual int rowCount(const QModelIndex&) const override
    { return GetNumLuaMethodDocs(); }
    virtual int columnCount(const QModelIndex&) const override
    { return 4; }
private:
};
class ScriptWidget::TableModelProxy : public QSortFilterProxyModel
{
public:
    void SetTableModel(TableModel* model)
    { mModel = model; }
    void SetFilterString(const QString& text)
    {
        mFilter = base::ToUpperUtf8(app::ToUtf8(text));
    }
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        if (mFilter.empty())
            return true;
        const auto& doc = GetLuaMethodDoc(row);
        if (base::Contains(base::ToUpperUtf8(doc.name), mFilter))
            return true;
        else if (base::Contains(base::ToUpperUtf8(doc.desc), mFilter))
            return true;
        return false;
    }
private:
    std::string mFilter;
    TableModel* mModel = nullptr;
};

ScriptWidget::ScriptWidget(app::Workspace* workspace)
{
    InitDoc();

    mWorkspace = workspace;
    mTableModel.reset(new TableModel);
    mTableModelProxy.reset(new TableModelProxy);
    mTableModelProxy->setSourceModel(mTableModel.get());
    mTableModelProxy->SetTableModel(mTableModel.get());
    QPlainTextDocumentLayout* layout = new QPlainTextDocumentLayout(&mDocument);
    layout->setParent(this);
    mDocument.setDocumentLayout(layout);
    DEBUG("Create ScriptWidget");

    mUI.setupUi(this);
    mUI.actionFindText->setShortcut(QKeySequence::Find);

    mUI.modified->setVisible(false);
    mUI.find->setVisible(false);
    mUI.code->SetDocument(&mDocument);
    mUI.tableView->setModel(mTableModelProxy.get());
    mUI.tableView->setColumnWidth(0, 100);
    mUI.tableView->setColumnWidth(2, 200);
    // capture some special key presses in order to move the
    // selection (current item) in the list widget more conveniently.
    mUI.filter->installEventFilter(this);

    connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ScriptWidget::TableSelectionChanged);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, &ScriptWidget::FileWasChanged);

    std::map<std::string, std::set<std::string>> table_methods;
    for (const auto& method : g_method_docs)
    {
        table_methods[method.table].insert(method.name);
    }

    QString html;
    QTextStream stream(&html);
    stream << R"(
<!DOCTYPE html>
<html>
  <head>
    <meta name="qrichtext"/>
    <title>Lua API</title>
    <style type="text/css">
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
  </style>
  </head>
  <body>
)";

    // build TOC with unordered lists.
    stream << "<ul>\n";
    for (const auto& pair : table_methods)
    {
        const auto& table   = app::FromUtf8(pair.first);
        const auto& methods = pair.second;
        stream << QString("<li>%1</li>\n").arg(table);
        stream << QString("<ul>\n");
        for (const auto& m : methods)
        {
            const auto& foo = app::FromUtf8(m);
            stream << QString(R"(<li><a href="#%1_%2">%3</a></li>)").arg(table).arg(foo).arg(foo);
            stream << "\n";
        }
        stream << QString("</ul>\n");
    }
    stream << "</ul>\n";

    // build method documentation bodies.
    for (const auto& member : g_method_docs)
    {
        if (member.type == LuaMemberType::Function ||
            member.type == LuaMemberType::Method ||
            member.type == LuaMemberType::MetaMethod)
        {
            std::string args;
            for (const auto& a : member.args)
            {
                args += "<span class=\"arg\">";
                args += a.type;
                args += "</span> ";
                args += a.name;
                args += ", ";
            }
            if (!args.empty())
            {
                args.pop_back();
                args.pop_back();
            }
            std::string name;
            if (member.type == LuaMemberType::Function)
                name = member.table + "." + member.name;
            else if (member.type == LuaMemberType::Method)
                name = "obj:" + member.name;
            else name = member.name;

            stream << QString(
R"(<div class="method" name="%1_%2" id="%1_%2">
  <div class="signature">
     <span class="return">%3 </span>
     <span class="method">%4</span>(%5)
  </div>
  <div class="description">%6</div>
</div>
)").arg(app::FromUtf8(member.table)).arg(app::FromUtf8(member.name))
                    .arg(app::FromUtf8(member.ret)).arg(app::FromUtf8(name))
                    .arg(app::FromUtf8(args))
                    .arg(app::FromUtf8(member.desc));
        }
        else
        {
            std::string name;
            if (member.type == LuaMemberType::TableProperty)
                name = member.table + "." + member.name;
            else name = "obj." + member.name;

            stream << QString(
R"(<div class="member" name="%1_%2" id="%1_%2">
   <div class="signature">
      <span class="return">%3 </span>
      <span class="method">%4 </span>
   </div>
   <div class="description">%5</div>
</div>
)").arg(app::FromUtf8(member.table)).arg(app::FromUtf8(member.name))
   .arg(app::FromUtf8(member.ret)).arg(app::FromUtf8(name))
   .arg(app::FromUtf8(member.desc));
        }
    }

    stream << R"(
</body>
</html>
)";

    stream.flush();
    //mUI.textEdit->setHtml(html);
    mUI.textBrowser->setHtml(html);
}

ScriptWidget::ScriptWidget(app::Workspace* workspace, const app::Resource& resource) : ScriptWidget(workspace)
{
    const app::Script* script = nullptr;
    resource.GetContent(&script);

    const std::string& uri = script->GetFileURI();
    DEBUG("Editing script: '%1'", uri);
    mFilename     = mWorkspace->MapFileToFilesystem(app::FromUtf8(uri));
    mResourceID   = resource.GetId();
    mResourceName = resource.GetName();
    mWatcher.addPath(mFilename);
    LoadDocument(mFilename);
    setWindowTitle(mResourceName);

    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "help_splitter", mUI.helpSplitter);
}
ScriptWidget::~ScriptWidget()
{
    DEBUG("Destroy ScriptWidget");
}

bool ScriptWidget::CanTakeAction(Actions action, const Clipboard*) const
{
    // canPaste seems to be broken??
    switch (action)
    {
        // todo: could increase/decrease font size on zoom in/out
        case Actions::CanZoomOut:
        case Actions::CanZoomIn:
            return false;
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return false;
        case Actions::CanCut:
        case Actions::CanCopy:
            return mUI.code->hasFocus() &&  mUI.code->CanCopy();
        case Actions::CanPaste:
            return mUI.code->hasFocus() && mUI.code->canPaste();
        case Actions::CanUndo:
            return mUI.code->hasFocus() && mUI.code->CanUndo();
    }
    return false;
}

void ScriptWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionFindText);
    bar.addAction(mUI.actionReplaceText);
    bar.addSeparator();
    bar.addAction(mUI.actionFindHelp);
}
void ScriptWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionFindText);
    menu.addAction(mUI.actionReplaceText);
    menu.addSeparator();
    menu.addAction(mUI.actionFindHelp);
    menu.addSeparator();
    menu.addAction(mUI.actionOpen);
}

void ScriptWidget::Cut(Clipboard&)
{
    // uses the global QClipboard which is fine here 
    // because that allows cutting/pasting between apps
    mUI.code->cut();
}
void ScriptWidget::Copy(Clipboard&) const
{
    // uses the global QClipboard which is fine here 
    // because that allows cutting/pasting between apps
    mUI.code->copy();
}
void ScriptWidget::Paste(const Clipboard&)
{
    // uses the global QClipboard which is fine here 
    // because that allows cutting/pasting between apps
    mUI.code->paste();
}

void ScriptWidget::Save()
{
    on_actionSave_triggered();
}

bool ScriptWidget::SaveState(Settings& settings) const
{
    // todo: if there are changes that have not been saved
    // to the file they're then lost. options are to either
    // ask for save when shutting down or to save to an
    // intermediate scrap file somewhere.
    settings.SetValue("Script", "resource_id", mResourceID);
    settings.SetValue("Script", "resource_name", mResourceName);
    settings.SetValue("Script", "filename", mFilename);
    settings.SaveWidget("Script", mUI.findText);
    settings.SaveWidget("Script", mUI.replaceText);
    settings.SaveWidget("Script", mUI.findBackwards);
    settings.SaveWidget("Script", mUI.findCaseSensitive);
    settings.SaveWidget("Script", mUI.findWholeWords);
    settings.SaveWidget("Script", mUI.mainSplitter);
    settings.SaveWidget("Script", mUI.helpSplitter);
    settings.SaveWidget("Script", mUI.tableView);
    return true;
}
bool ScriptWidget::LoadState(const Settings& settings)
{
    settings.GetValue("Script", "resource_id", &mResourceID);
    settings.GetValue("Script", "resource_name", &mResourceName);
    settings.GetValue("Script", "filename", &mFilename);
    settings.LoadWidget("Script", mUI.findText);
    settings.LoadWidget("Script", mUI.replaceText);
    settings.LoadWidget("Script", mUI.findBackwards);
    settings.LoadWidget("Script", mUI.findCaseSensitive);
    settings.LoadWidget("Script", mUI.findWholeWords);
    settings.LoadWidget("Script", mUI.mainSplitter);
    settings.LoadWidget("Script", mUI.helpSplitter);
    settings.LoadWidget("Script", mUI.tableView);
    if (!mResourceName.isEmpty())
        setWindowTitle(mResourceName);
    if (mFilename.isEmpty())
        return true;
    mWatcher.addPath(mFilename);
    return LoadDocument(mFilename);
}

bool ScriptWidget::HasUnsavedChanges() const
{
    if (!mFileHash)
        return false;

    const auto& plain = mDocument.toPlainText();
    const auto hash = qHash(plain);
    return mFileHash != hash;
}

bool ScriptWidget::ConfirmClose()
{
    const auto& plain = mDocument.toPlainText();
    const auto hash = qHash(plain);
    if (mFileHash == hash)
        return true;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
    const auto ret = msg.exec();
    if (ret == QMessageBox::Cancel)
        return false;
    else if (ret == QMessageBox::No)
        return true;

    on_actionSave_triggered();
    return true;
}

bool ScriptWidget::OnEscape()
{
    mUI.find->setVisible(false);
    mUI.code->setFocus();
    return true;
}

void ScriptWidget::on_actionSave_triggered()
{
    QString filename = mFilename;
    if (filename.isEmpty())
    {
        const auto& luadir = app::JoinPath(mWorkspace->GetDir(), "lua");
        const auto& file = QFileDialog::getSaveFileName(this,
            tr("Save Script As ..."), luadir,
            tr("Lua Scripts (*.lua)"));
        if (file.isEmpty())
            return;
        filename = file;
    }

    const QString& text = mDocument.toPlainText();
    QFile file;
    file.setFileName(filename);
    file.open(QIODevice::WriteOnly);
    if (!file.isOpen())
    {
        ERROR("Failed to open '%1' for writing. (%2)", filename, file.error());
        QMessageBox msg(this);
        msg.setText(tr("There was an error saving the file.\n%1").arg(file.errorString()));
        msg.setIcon(QMessageBox::Critical);
        msg.exec();
        return;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << text;
    INFO("Saved Lua script '%1'", filename);
    NOTE("Saved Lua script '%1'", filename);
    mFilename = filename;
    mFileHash = qHash(text);
    // start watching this file if it wasn't being watched before.
    mWatcher.addPath(mFilename);

    if (mResourceID.isEmpty())
    {
        const QFileInfo info(mFilename);
        mResourceName = info.baseName();

        const auto& URI = mWorkspace->MapFileToWorkspace(mFilename);
        DEBUG("Script file URI '%1'", URI);

        app::Script script;
        script.SetFileURI(app::ToUtf8(URI));
        app::ScriptResource resource(script, mResourceName);
        SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
        SetUserProperty(resource, "help_splitter", mUI.helpSplitter);
        mWorkspace->SaveResource(resource);
        setWindowTitle(mResourceName);
        mResourceID = app::FromUtf8(script.GetId());
    }
    else
    {
        auto* resource = mWorkspace->FindResourceById(mResourceID);
        SetUserProperty(*resource, "main_splitter", mUI.mainSplitter);
        SetUserProperty(*resource, "help_splitter", mUI.helpSplitter);
    }
}

void ScriptWidget::on_actionOpen_triggered()
{
    if (mFilename.isEmpty())
    {
        QMessageBox msg(this);
        msg.setText(tr("You haven't yet saved the file. It cannot be opened in another application."));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }
    emit OpenExternalScript(mFilename);
}

void ScriptWidget::on_actionFindHelp_triggered()
{
    mUI.filter->setFocus();
    auto sizes = mUI.mainSplitter->sizes();
    ASSERT(sizes.size() == 2);
    if (sizes[1] == 0 && sizes[0] > 500)
    {
        sizes[0] = sizes[0] - 500;
        sizes[1] = 500;
        mUI.mainSplitter->setSizes(sizes);
    }
}

void ScriptWidget::on_actionFindText_triggered()
{
    mUI.find->setVisible(true);
    mUI.findText->setFocus();
    SetValue(mUI.find, QString("Find text"));
    SetValue(mUI.findResult, QString(""));
    SetEnabled(mUI.btnReplaceNext, false);
    SetEnabled(mUI.btnReplaceAll, false);
    SetEnabled(mUI.replaceText, false);
}

void ScriptWidget::on_actionReplaceText_triggered()
{
    mUI.find->setVisible(true);
    mUI.findText->setFocus();
    SetValue(mUI.find, QString("Replace text"));
    SetValue(mUI.findResult, QString(""));
    SetEnabled(mUI.btnReplaceNext, true);
    SetEnabled(mUI.btnReplaceAll, true);
    SetEnabled(mUI.replaceText, true);
}

void ScriptWidget::on_btnFindNext_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    QTextCursor cursor = mUI.code->textCursor();
    cursor = mDocument.find(text, cursor, flags);
    if (cursor.isNull() && GetValue(mUI.findBackwards))
        cursor = mDocument.find(text, mDocument.characterCount(), flags);
    else if (cursor.isNull())
        cursor = mDocument.find(text, 0, flags);

    if (cursor.isNull())
    {
        SetValue(mUI.findResult, tr("No results found."));
        return;
    }
    SetValue(mUI.findResult, QString(""));
    mUI.code->setTextCursor(cursor);
}
void ScriptWidget::on_btnFindClose_clicked()
{
    mUI.find->setVisible(false);
}

void ScriptWidget::on_btnReplaceNext_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;
    QString replacement = GetValue(mUI.replaceText);

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    QTextCursor cursor = mUI.code->textCursor();
    cursor = mDocument.find(text, cursor, flags);
    if (cursor.isNull() && GetValue(mUI.findBackwards))
        cursor = mDocument.find(text, mDocument.characterCount(), flags);
    else if (cursor.isNull())
        cursor = mDocument.find(text, 0, flags);

    if (cursor.isNull())
    {
        SetValue(mUI.findResult, tr("No results found."));
        return;
    }
    SetValue(mUI.findResult, QString(""));
    // find returns with a selection, so no need to move the cursor and play
    // with the anchor. insertText will delete the selection
    cursor.insertText(replacement);
    mUI.code->setTextCursor(cursor);
}
void ScriptWidget::on_btnReplaceAll_clicked()
{
    QString text = GetValue(mUI.findText);
    if (text.isEmpty())
        return;
    QString replacement = GetValue(mUI.replaceText);

    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindBackward, GetValue(mUI.findBackwards));
    flags.setFlag(QTextDocument::FindCaseSensitively, GetValue(mUI.findCaseSensitive));
    flags.setFlag(QTextDocument::FindWholeWords, GetValue(mUI.findWholeWords));

    int count = 0;

    QTextCursor cursor(&mDocument);
    while (!cursor.isNull())
    {
        cursor = mDocument.find(text, cursor, flags);
        if (cursor.isNull())
            break;

        //cursor.movePosition(QTextCursor::MoveOperation::WordRight, QTextCursor::MoveMode::KeepAnchor);
        // find returns with a selection, so no need to move the cursor and play
        // with the anchor. insertText will delete the selection
        cursor.insertText(replacement);
        count++;
    }
    SetValue(mUI.findResult, QString("Replaced %1 occurrences.").arg(count));
}

void ScriptWidget::on_btnNavBack_clicked()
{
    mUI.textBrowser->backward();
}
void ScriptWidget::on_btnNavForward_clicked()
{
    mUI.textBrowser->forward();
}
void ScriptWidget::on_btnRejectReload_clicked()
{
    SetEnabled(mUI.code, true);
    SetEnabled(mUI.actionFindText, true);
    SetEnabled(mUI.actionReplaceText, true);
    SetEnabled(mUI.actionSave, true);
    SetVisible(mUI.modified, false);
}
void ScriptWidget::on_btnAcceptReload_clicked()
{
    LoadDocument(mFilename);
    SetEnabled(mUI.code, true);
    SetEnabled(mUI.actionFindText, true);
    SetEnabled(mUI.actionReplaceText, true);
    SetEnabled(mUI.actionSave, true);
    SetVisible(mUI.modified, false);
}

void ScriptWidget::on_filter_textChanged(const QString& text)
{
    mTableModelProxy->SetFilterString(text);
    mTableModelProxy->invalidate();

    const auto count = mTableModelProxy->rowCount();
    auto* model = mUI.tableView->selectionModel();
    model->reset();
}
void ScriptWidget::on_textBrowser_backwardAvailable(bool available)
{
    mUI.btnNavBack->setEnabled(available);
}
void ScriptWidget::on_textBrowser_forwardAvailable(bool available)
{
    mUI.btnNavForward->setEnabled(available);
}

void ScriptWidget::FileWasChanged()
{
    DEBUG("File change was detected. [file='%1']", mFilename);

    // watch for further modifications
    mWatcher.addPath(mFilename);

    // if already notifying then ignore.
    if (mUI.modified->isVisible())
        return;

    // block recursive signals from happening, i.e.
    // if we've already reacted to this signal and opened the dialog
    // then don't open yet another dialog.
    QSignalBlocker blocker(&mWatcher);

    // our hash is computed on save and load. if the hash of the
    // file contents is now something else then someone else has
    // changed the file somewhere else.
    QFile io;
    io.setFileName(mFilename);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        // todo: the file could have been removed or renamed.
        ERROR("Failed to open file for reading. [file='%1', error='%2']", mFilename, io.error());
        return;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const size_t hash = qHash(stream.readAll());
    if (hash == mFileHash)
        return;

    // Disable code editing until the reload request has been dismissed
    SetEnabled(mUI.code, false);
    SetEnabled(mUI.actionFindText, false);
    SetEnabled(mUI.actionReplaceText, false);
    SetEnabled(mUI.actionSave, false);
    SetVisible(mUI.modified, true);
}

void ScriptWidget::keyPressEvent(QKeyEvent *key)
{
    if (key->key() == Qt::Key_Escape ||
       (key->key() == Qt::Key_G && key->modifiers() & Qt::ControlModifier))
    {
        mUI.find->setVisible(false);
        mUI.code->setFocus();
        return;
    }
    QWidget::keyPressEvent(key);
}
bool ScriptWidget::eventFilter(QObject* destination, QEvent* event)
{
    const auto count = mTableModelProxy->rowCount();

    // returning true will eat the event and stop from
    // other event handlers ever seeing the event.
    if (destination != mUI.filter)
        return false;
    else if (event->type() != QEvent::KeyPress)
        return false;
    if (count == 0)
        return false;

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool shift = key->modifiers() & Qt::ShiftModifier;
    const bool alt   = key->modifiers() & Qt::AltModifier;

    int current = 0;
    auto* model = mUI.tableView->selectionModel();
    const auto& selection = model->selectedRows();
    if (!selection.isEmpty())
        current = selection[0].row();

    // Ctrl+N, Ctrl+P (similar to DlgOpen) don't work here reliable
    // since they are global hot keys which take precedence.

    if (alt && key->key() == Qt::Key_N)
        current = math::wrap(0, count-1, current+1);
    else if (alt && key->key() == Qt::Key_P)
        current = math::wrap(0, count-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, count-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, count-1, current+1);
    else return false;

    model->setCurrentIndex(mTableModel->index(current, 0),
                           QItemSelectionModel::SelectionFlag::ClearAndSelect |
                           QItemSelectionModel::SelectionFlag::Rows);
    return true;
}

bool ScriptWidget::LoadDocument(const QString& file)
{
    QFile io;
    io.setFileName(file);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        ERROR("Failed to open script file for reading. [file='%1', error=%2]", file, io.error());
        return false;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const QString& data = stream.readAll();

    mDocument.setPlainText(data);
    mFileHash = qHash(data);
    mFilename = file;
    DEBUG("Loaded script file. [file='%1']", mFilename);
    return true;
}

void ScriptWidget::TableSelectionChanged(const QItemSelection, const QItemSelection&)
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    for (const auto& i : indices)
    {
        const auto& m = mTableModelProxy->mapToSource(i);
        const auto& method = g_method_docs[m.row()];
        const auto& name = QString("%1_%2").arg(app::FromUtf8(method.table)).arg(app::FromUtf8(method.name));
        mUI.textBrowser->scrollToAnchor(name);
        //DEBUG("Scroll to anchor. [anchor='%1']", name);
    }
}

} // namespace
