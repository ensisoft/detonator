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
    Property, Function
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

void AddMethod(const std::string& ret, const std::string& name, const std::string& desc)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Function;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

void AddMethod(const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Function;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Function;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    g_method_docs.push_back(std::move(doc));
}
void AddMethod(const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name,
               const std::string& arg2_type, const std::string& arg2_name)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Function;
    doc.table = table_name;
    doc.ret   = ret;
    doc.name  = name;
    doc.desc  = desc;
    doc.args.push_back({arg0_name,  arg0_type});
    doc.args.push_back({arg1_name,  arg1_type});
    doc.args.push_back({arg2_name,  arg2_type});
    g_method_docs.push_back(std::move(doc));
}

void AddMethod(const std::string& ret, const std::string& name, const std::string& desc,
               const std::string& arg0_type, const std::string& arg0_name,
               const std::string& arg1_type, const std::string& arg1_name,
               const std::string& arg2_type, const std::string& arg2_name,
               const std::string& arg3_type, const std::string& arg3_name)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Function;
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

void AddProperty(const std::string& type, const std::string& name, const std::string& desc)
{
    LuaMemberDoc doc;
    doc.type  = LuaMemberType::Property;
    doc.table = table_name;
    doc.ret   = type;
    doc.name  = name;
    doc.desc  = desc;
    g_method_docs.push_back(std::move(doc));
}

std::size_t GetNumLuaMethodDocs()
{ return g_method_docs.size(); }
const LuaMemberDoc& GetLuaMethodDoc(size_t index)
{ return g_method_docs[index]; }

#define DOC_TABLE(name) SetTable(name)
#define DOC_METHOD_0(ret, name, desc) AddMethod(ret, name, desc)
#define DOC_METHOD_1(ret, name, desc, a0type, a0name) AddMethod(ret, name, desc, a0type, a0name)
#define DOC_METHOD_2(ret, name, desc, a0type, a0name, a1type, a1name) AddMethod(ret, name, desc, a0type, a0name, a1type, a1name)
#define DOC_METHOD_3(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name) AddMethod(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name)
#define DOC_METHOD_4(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name) AddMethod(ret, name, desc, a0type, a0name, a1type, a1name, a2type, a2name, a3type, a3name)
#define DOC_PROPERTY(type, name, desc) AddProperty(type, name, desc)

void InitDoc()
{
    static bool done = false;
    if (done == true) return;

    DOC_TABLE("util");
    DOC_METHOD_1("float", "GetRotationFromMatrix", "Get the rotational component from the given matrix.",
                 "glm.mat4", "matrix");
    DOC_METHOD_1("glm.vec2", "GetScaleFromMatrix", "Get the scale component from the given matrix.",
                 "glm.vec2", "matrix");
    DOC_METHOD_1("glm.vec2", "GetTranslationFromMatrix", "Get the translation component from the given matrix.",
                 "glm.vec2", "matrix");
    DOC_METHOD_1("void", "RandomSeed", "Seed the random engine with the given seed value.<br>"
                                     "For any given seed the the generated pseudo random number sequence will always be same on every platform.",
                                     "int", "seed");
    DOC_METHOD_2("int|float", "Random", "Generate a new pseudo random number between the given (inclusive) min/max values.<br>"
                                        "This is an overloaded function and takes either ints or floats for min/max.<br>"
                                        "The type of the returned value depends on the type of min/max parameters.",
                 "int|float", "min", "int|float", "max");
    DOC_METHOD_2("string", "JoinPath", "Concatenate file system paths together.<br>"
                                       "No assumption is made regarding the validity of the paths.",
                 "string", "a", "string", "b");
    DOC_METHOD_1("bool", "FileExists", "Check whether the given file exists on the file system or not. <br>"
                                       "The given filename is expected to be UTF-8 encoded."
                                       "Returns true if the file exists otherwise false. ",
                 "string", "filename");
    DOC_METHOD_1("string", "RandomString", "Generate a random alpha numeric string of specified length.<br>"
                                           "Useful for things such as pseudo-unique identifiers.",
                 "int", "length");
    DOC_METHOD_2("string", "FormatString", "Format a string with %1, %2,...%n placeholders with N variable arguments.<br>"
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
    DOC_METHOD_1("void", "debug", "Print a debug message in the application log.",
                 "string", "message");
    DOC_METHOD_1("void", "warn", "Print a warning message in the application log.",
                 "string", "message");
    DOC_METHOD_1("void", "error", "Print an error message in the application log.",
                 "string", "message");
    DOC_METHOD_1("void", "info", "Print an information message in the application log.",
                 "string", "message");

    DOC_TABLE("trace");
    DOC_METHOD_1("void", "marker", "Set a marker message in the application trace.",
                 "string", "message");
    DOC_METHOD_2("void", "marker", "Set a marker message in the application trace in the given trace entry.<br>"
                                   "The given trace entry index MUST BE VALID.<br>"
                                   "Do not call this function unless you know what you're doing.<br>"
                                   "For a safer alternative use the overload without index.",
                 "string", "message",
                 "int", "index");
    DOC_METHOD_1("int", "enter", "Enter a new tracing scope for measuring time spent inside the scope.<br>"
                                 "You must manually call trace.leave with index that you received from this call. "
                                 "Not doing so will likely crash the application. ",
                 "string", "scope_name");
    DOC_METHOD_1("void", "leave", "Leave a tracing scope that was entered previously.<br>"
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
    DOC_METHOD_2("base.FRect", "Combine", "Create an union of the given rectangles.<br>"
                                          "Example: local union = base.FRect.Combine(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_METHOD_2("base.FRect", "Intersect", "Create an intersection of the given rectangles.<br>"
                                            "Example: local intersection = base.FRect.Intersect(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_METHOD_2("bool", "TestIntersect", "Test whether the rectangles intersect.<br>"
                                          "Example: local ret = base.FRect.TestIntersect(a, b)",
                 "base.FRect", "a", "base.FRect", "b");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function.",
                 "base.FRect", "rect");

    DOC_TABLE("base.FSize");
    DOC_METHOD_0("base.FSize", "new", "Construct a new size with zero width and height.");
    DOC_METHOD_2("base.FSize", "new", "Construct a new size with the given width and height.",
               "float", "width", "float", "height");
    DOC_METHOD_0("float", "GetWidth", "Get the width of the size.");
    DOC_METHOD_0("float", "GetHeight", "Get the height of the size.");
    DOC_METHOD_2("base.FSize", "operator *", "Lua multiplication meta function.", "base.FSize", "size", "float", "scalar");
    DOC_METHOD_2("base.FSize", "operator +", "Lua addition meta function.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_METHOD_2("base.FSize", "operator -", "Lua subtraction meta function.", "base.FSize", "lhs", "base.FSize", "rhs");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function.", "base.FSize", "size");

    DOC_TABLE("base.FPoint");
    DOC_METHOD_0("base.FPoint", "new", "Construct a new point with zero x, y position.");
    DOC_METHOD_0("base.FPoint", "new", "Construct a new point with the given x,y position.");
    DOC_METHOD_0("float", "GetX", "Get the x position.");
    DOC_METHOD_0("float", "GetY", "Get the y position.");
    DOC_METHOD_2("base.FPoint", "operator +", "Lua addition meta function", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_METHOD_2("base.FPoint", "operator -", "Lua subtraction meta function", "base.FPoint", "lhs", "base.FPoint", "rhs");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function.", "base.FPoint", "point");

    DOC_TABLE("base.Colors");
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        DOC_PROPERTY("int", name, base::FormatString("Color value for '%1'.", name));
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
    DOC_METHOD_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color value.", "int", "color");
    DOC_METHOD_1("base.Color4f", "FromEnum", "Construct a new color from base.Colors color name.", "string", "color");

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
    DOC_METHOD_2("data.JsonObject", "ParseJsonString", "Create a new JsonObject based on the JSON data buffer.<br>"
                                                       "Returns a new JsonObject and an empty string on success or nil and an error string on error.",
                 "todo", "json_data", "size_t", "data_len");
    DOC_METHOD_2("bool, string", "WriteJsonFile", "Write the contents of the JsonObject into a file.<br>"
                                                  "Returns true and en empty string on success or false and error string on error.",
                 "data.JsonObject", "json", "string", "filename");
    DOC_METHOD_1("data.JsonObject", "ReadJsonFile", "Try to read the given JSON file. <br>"
                                                    "Returns new JsonObject and en empty string on success or nil and error string on error.",
                 "string", "filename");
    DOC_METHOD_1("data.Writer", "CreateWrite", "Create a new data.Writer object based on the given format string."
                                               "Format string can be one of the following: 'JSON'<br>"
                                               "Returns nil on unsupported format.",
                 "string", "format");
    DOC_METHOD_2("bool, string", "WriteFile", "Dump the contents of the given Writer into a file.<br>"
                                              "Returns true and en empty string on success or false and an error string on error.",
                 "data.Writer", "data", "string", "filename");
    DOC_METHOD_1("data.Reader, string", "ReadFile", "Try to read the given file in some supported format.<br>"
                                                    "Currently supported formats: JSON.<br>"
                                                    "Returns a new data.Reader and an empty string on success or nil and an error string on error.",
                 "string", "filename");

    DOC_TABLE("glm");
    DOC_METHOD_2("glm.vec2", "dot", "Compute the dot product of the given vectors.", "glm.vec2", "a", "glm.vec2", "b");
    DOC_METHOD_2("glm.vec3", "dot", "Compute the dot product of the given vectors.", "glm.vec3", "a", "glm.vec3", "b");
    DOC_METHOD_2("glm.vec4", "dot", "Compute the dot product of the given vectors.", "glm.vec4", "a", "glm.vec4", "b");
    DOC_METHOD_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec2", "vec");
    DOC_METHOD_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec3", "vec");
    DOC_METHOD_1("float", "length", "Return the length (magnitude) of the vector.", "glm.vec4", "vec");
    DOC_METHOD_1("glm.vec2", "normalize", "Return a normalized copy of the vector.", "glm.vec2", "vec");
    DOC_METHOD_1("glm.vec3", "normalize", "Return a normalized copy of the vector.", "glm.vec3", "vec");
    DOC_METHOD_1("glm.vec4", "normalize", "Return a normalized copy of the vector.", "glm.vec4", "vec");

    DOC_TABLE("glm.mat4");
    DOC_METHOD_0("glm.vec2, glm.vec2, float", "decompose", "Decompose the given 4x4 transformation matrix.<br>"
                                                           "Returns: <br>"
                                                           "a glm.vec2 with the translation coefficients.<br>"
                                                           "a glm.vec2 with the scale coefficients.<br>"
                                                           "a float with the rotation around Z axis in radians.");

    DOC_TABLE("glm.vec2");
    DOC_METHOD_0("glm.vec2", "new", "Construct a new glm.vec2.");
    DOC_METHOD_2("glm.vec2", "new", "Construct a new glm.vec2.", "float", "x", "float", "y");
    DOC_METHOD_2("float", "operator []", "Lua index meta function", "glm.vec2", "vec", "int", "index");
    DOC_METHOD_2("glm.vec2", "operator +", "Lua addition meta function", "glm.vec2", "a", "glm.vec2", "b");
    DOC_METHOD_2("glm.vec2", "operator -", "Lua subtraction meta function", "glm.vec2", "a", "glm.vec2", "b");
    DOC_METHOD_2("glm.vec2", "operator *", "Lua multiplication meta function", "glm.vec2", "vec", "float", "scalar");
    DOC_METHOD_2("glm.vec2", "operator /", "Lua division meta function", "glm.vec2", "vec", "float", "scalar");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function", "glm.vec2", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec2", "normalize", "Return a normalized copy of the vector.");
    DOC_PROPERTY("float", "x", "X component of the vector.");
    DOC_PROPERTY("float", "y", "Y component of the vector.");

    DOC_TABLE("glm.vec3");
    DOC_METHOD_0("glm.vec3", "new", "Construct a new glm.vec3.");
    DOC_METHOD_3("glm.vec3", "new", "Construct a new glm.vec3.", "float", "x", "float", "y", "float", "z");
    DOC_METHOD_2("float", "operator []", "Lua index meta function", "glm.vec3", "vec", "int", "index");
    DOC_METHOD_2("glm.vec3", "operator +", "Lua addition meta function", "glm.vec3", "a", "glm.vec3", "b");
    DOC_METHOD_2("glm.vec3", "operator -", "Lua subtraction meta function", "glm.vec3", "a", "glm.vec3", "b");
    DOC_METHOD_2("glm.vec3", "operator *", "Lua multiplication meta function", "glm.vec3", "vec", "float", "scalar");
    DOC_METHOD_2("glm.vec3", "operator /", "Lua division meta function", "glm.vec3", "vec", "float", "scalar");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function", "glm.vec3", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec3", "normalize", "Return a normalized copy of the vector.");
    DOC_PROPERTY("float", "x", "X component of the vector.");
    DOC_PROPERTY("float", "y", "Y component of the vector.");
    DOC_PROPERTY("float", "z", "Z component of the vector.");

    DOC_TABLE("glm.vec4");
    DOC_METHOD_0("glm.vec4", "new", "Construct a new glm.vec3.");
    DOC_METHOD_4("glm.vec4", "new", "Construct a new glm.vec3.", "float", "x", "float", "y", "float", "z", "float", "w");
    DOC_METHOD_2("float", "operator []", "Lua index meta function", "glm.vec4", "vec", "int", "index");
    DOC_METHOD_2("glm.vec4", "operator +", "Lua addition meta function", "glm.vec4", "a", "glm.vec4", "b");
    DOC_METHOD_2("glm.vec4", "operator -", "Lua subtraction meta function", "glm.vec4", "a", "glm.vec4", "b");
    DOC_METHOD_2("glm.vec4", "operator *", "Lua multiplication meta function", "glm.vec4", "vec", "float", "scalar");
    DOC_METHOD_2("glm.vec4", "operator /", "Lua division meta function", "glm.vec4", "vec", "float", "scalar");
    DOC_METHOD_1("string", "tostring", "Lua tostring meta function", "glm.vec4", "vec");
    DOC_METHOD_0("float", "length", "Return length (magnitude) of the vector.");
    DOC_METHOD_0("glm.vec4", "normalize", "Return a normalized copy of the vector.");
    DOC_PROPERTY("float", "x", "X component of the vector.");
    DOC_PROPERTY("float", "y", "Y component of the vector.");
    DOC_PROPERTY("float", "z", "Z component of the vector.");
    DOC_PROPERTY("float", "w", "W component of the vector.");

    DOC_TABLE("wdk");
    DOC_METHOD_1("string", "KeyStr", "Convert a key value to a named key string.", "int", "key");
    DOC_METHOD_1("string", "BtnStr", "Convert a mouse button value to a named button string.", "int", "button");
    DOC_METHOD_1("string", "ModStr", "Convert a modifier key value to a named modifier string.", "int", "modifier");
    DOC_METHOD_1("string", "ModBitStr", "Map keyboard key modifier bit string to a named modifier string.", "int", "mod_bits");
    DOC_METHOD_1("bool", "TestKeyDown", "Test whether the given keyboard key is currently down.<br>"
                                        "The key value is one of the key values in in wdk.Keys", "int", "key");
    DOC_METHOD_2("bool", "TestMod", "Test whether the given modifier bit is set in the bitset of modifier keys.",
                 "int", "modifier_bits", "int", "modifier_value");

    DOC_TABLE("wdk.Keys");
    for (const auto& key : magic_enum::enum_values<wdk::Keysym>())
    {
        const std::string name(magic_enum::enum_name(key));
        DOC_PROPERTY("int", name, base::FormatString("Key value for '%1'.", name));
    }
    DOC_TABLE("wdk.Mods");
    for (const auto& mod : magic_enum::enum_values<wdk::Keymod>())
    {
        const std::string name(magic_enum::enum_name(mod));
        DOC_PROPERTY("int", name, base::FormatString("Modifier value for '%1'.", name));
    }
    DOC_TABLE("wdk.Buttons");
    for (const auto& btn : magic_enum::enum_values<wdk::MouseButton>())
    {
        const std::string name(magic_enum::enum_name(btn));
        DOC_PROPERTY("int", name, base::FormatString("Mouse button value for '%1'.", name));
    }

    DOC_TABLE("uik");
    DOC_METHOD_2("uik.Widget", "WidgetCast", "Downcast a Widget object to concrete widget type.<br>"
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
    DOC_PROPERTY("string", "name", "Name of the widget that triggered the action.");
    DOC_PROPERTY("string", "id", "ID of the widget that triggered the action.");
    DOC_PROPERTY("string", "type", "Type of the action in question.");
    DOC_PROPERTY("int|float|bool|string", "value", "The value associated with the action.");


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
    DOC_METHOD_2("void", "SetUniform", "Set a material parameter (shader uniform) value.<br>"
                                       "The parameter is identified by it's uniform name in the material shader.<br>"
                                       "Supported values are float, int, base.Color4f, glm.vec2, glm.vec3, glm.vec4",
                 "string", "name", "float|int|base.Color4f|glm.vec2|glm.vec3|glm.vec4", "value");
    DOC_METHOD_1("float|int|base.Color4f|glm.vec2|glm.vec3|glm.vec4", "GetUniform",
                 "Get a material parameter (shader uniform) value.<br>"
                 "The parameter is identified by its uniform name in the material shader.",
                 "string", "name");
    DOC_METHOD_1("bool", "HasUniform", "Returns whether the given material parameter (shader uniform) exists.", "string", "name");
    DOC_METHOD_1("void", "DeleteUniform", "Delete the given material parameter (shader uniform) value.<br>"
                                          "After the value has been removed the parameter will use the default value defined in the material.",
                 "string", "name");

    DOC_TABLE("game.RigidBody");
    DOC_METHOD_0("float", "GetFriction", "Return the friction value of the rigid body.");
    DOC_METHOD_0("float", "GetRestitution", "Return the restitution value of the rigid body.");
    DOC_METHOD_0("float", "GetAngularDamping", "Return the angular damping of the rigid body.");
    DOC_METHOD_0("float", "GetLinearDamping", "Return the linear damping of the rigid body.");
    DOC_METHOD_0("float", "GetDensity", "Get the density value of the rigid body.");
    DOC_METHOD_0("string", "GetPolygonShapeId", "Get the ID of the polygonal shape for the physics body.");
    DOC_METHOD_0("glm.vec2", "GetLinearVelocity", "Get the current linear velocity (m/s) of the rigid body.");
    DOC_METHOD_0("float", "GetAngularVelocity", "Get the current angular (rotational) velocity (rad/s) of the rigid body.");
    DOC_METHOD_1("void", "AdjustLinearVelocity", "Set a value (m/s) to adjust the linear velocity of the the rigid body.<br>"
                                                 "The adjustment will be applied on the next iteration of the physics update",
                 "glm.vec2", "velocity");
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
                 "string", "flag_name",
                 "bool", "on_off");

    DOC_TABLE("game.EntityNode");
    DOC_METHOD_0("string", "GetName", "Get the entity node's human readable instance name.");
    DOC_METHOD_0("string", "GetId", "Get the entity node's instance ID.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the entity node's class type.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the entity node's class type.");
    DOC_METHOD_0("glm.vec2", "GetTranslation", "Get the node's translation relative to it's parent.");
    DOC_METHOD_0("glm.vec2", "GetScale", "Get the node's scaling factor that applies to this node and all of its children.");
    DOC_METHOD_0("float", "GetRotation", "Get the node's rotation relative to its parent.");
    DOC_METHOD_0("bool", "HasRigidBody", "Checks whether the node has a rigid body item.");
    DOC_METHOD_0("bool", "HasTextItem", "Checks whether the node has a text item.");
    DOC_METHOD_0("bool", "HasDrawable", "Checks whether the node has a drawable item.");
    DOC_METHOD_0("game.RigidBody", "GetRigidBody", "Get the node's rigid body item if any. Returns nil if node has no rigid body.");
    DOC_METHOD_0("game.TextItem", "GetTextItem", "Get the node's text item if any. Returns nil if node has no text item.");
    DOC_METHOD_0("game.Drawable", "GetDrawable", "Get the node's drawable item if any. returns nil if node has no drawable item.");
    DOC_METHOD_1("void", "SetScale", "Set the node's scaling factor that applies to this node and its children.", "glm.vec2", "scale");
    DOC_METHOD_1("void", "SetSize", "Set the size that applies to this node.", "glm.vec2", "size");
    DOC_METHOD_1("void", "SetTranslation", "Set the node's translation relative to its parent.", "glm.vec2", "translation");
    DOC_METHOD_1("void", "SetName", "Set the node's instance name.", "string", "name");
    DOC_METHOD_1("void", "Translate", "Translate the node relative to its current translation.", "glm.vec2", "translation");
    DOC_METHOD_1("void", "Rotate", "Rotate the node relative to its current rotation.", "float", "rotation");

    DOC_TABLE("game.Entity");
    DOC_METHOD_0("string", "GetName", "Get the entity's human readable name.");
    DOC_METHOD_0("string", "GetId", "Get the entity's instance ID.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the entity's class type.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the entity's class type.");
    DOC_METHOD_0("int", "GetNumNodes", "Get the number of entity nodes in this entity.");
    DOC_METHOD_0("float", "GetTime", "Get the entity's current accumulated (life) time.");
    DOC_METHOD_0("int" , "GetLayer", "Get the entity's render layer in the scene rendering.");
    DOC_METHOD_1("void", "SetLayer", "Set the entity's render layer in the scene rendering.", "int", "layer");
    DOC_METHOD_0("bool", "IsPlaying", "Checks whether the entity is currently playing an animation or not.");
    DOC_METHOD_0("bool", "HasExpired", "Checks whether the entity has expired, i.e. exceeded it's max lifetime.");
    DOC_METHOD_0("bool", "HasBeenKilled", "Checks whether the entity has been killed.<br>"
                                          "Entities that have been killed will be deleted from the scene on the next iteration of game loop.");
    DOC_METHOD_0("bool", "HasBeenSpawned", "Checks whether the entity has just been spawned and exists for the first iteration of the game loop.<br>"
                                           "This flag is only ever true on the first iteration of the game loop during the entity's lifetime.");
    DOC_METHOD_1("game.EntityNode", "GetNode", "Get an entity node at the the given index.", "int", "index");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassName", "Find a node in the entity by it's class name.<br>"
                                                           "If multiple nodes have the same class name it's unspecified which one is returned.<br>"
                                                           "Returns nil if no such node could be found.",
                 "string", "class_name");
    DOC_METHOD_1("game.EntityNode", "FindNodeByClassId", "Find a node in the entity by it's class ID.<br>"
                                                         "Returns nil if no such node could be found.",
                 "string", "class_id");
    DOC_METHOD_1("game.EntityNode", "FindNodeByInstanceId", "Find a node in the entity by it's instance ID.<br>"
                                                            "If multiple nodes have the same ID it's unspecified which one is returned.<br>"
                                                            "Returns nil if no such node could be found.",
                 "string", "id");
    DOC_METHOD_0("void", "PlayIdle", "Play the entity's idle animation (if any).");
    DOC_METHOD_0("void", "PlayAnimationByName", "Play an animation track by the given name.");
    DOC_METHOD_0("void", "PlayAnimationById", "Play an animation track by the given ID.");
    DOC_METHOD_1("bool", "TestFlag", "Test entity flag.", "string", "flag_name");

    DOC_TABLE("game.EntityArgs");
    DOC_PROPERTY("game.EntityClass", "class", "The class object (type) of the entity.");
    DOC_PROPERTY("string", "name", "The instance name of the entity.");
    DOC_PROPERTY("glm.vec2", "scale", "The scaling factor that will apply to all of the entity nodes.<br>"
                                      "Default is (1.0, 1.0).");
    DOC_PROPERTY("glm.vec2", "position", "The initial position of the entity in the scene.<br>"
                                         "Default is (0.0, 0.0)");
    DOC_PROPERTY("float", "rotation", "The initial rotation that will apply to the entity in the scene.<br>"
                                      "Default is 0.0 (i.e no rotation).");
    DOC_PROPERTY("bool", "logging", "Whether to enable life time related engine logs for this entity.<br>"
                                   "Default is true.");

    DOC_TABLE("game.Scene");
    DOC_METHOD_0("bool|float|string|int|vec2", "index", "Lua index meta function.<br>"
                                                        "The scene's script script variables are accessible as properties on the scene object.<br>"
                                                        "For example a script variable named 'score' would be accessible as Scene.score.<br>"
                                                        "local score = Scene.score");
    DOC_METHOD_0("bool|float|string|int|vec2", "newindex", "Lua new index meta function<br>"
                                                             "The scene's script script variables are accessible as properties on the scene object.<br>"
                                                             "For example a script variable named 'score' would be accessible as Scene.score.<br>"
                                                             "Scene.score = 123");
    DOC_METHOD_0("int", "GetNumEntities", "Get the number of entities currently in the scene.");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceId", "Find an entity with the given instance ID.<br>"
                                                          "Returns nil if no such entity could be found.",
                 "string", "id");
    DOC_METHOD_1("game.Entity", "FindEntityByInstanceName", "Find an entity with the given instance name.<br>"
                                                            "Returns nil if no such entity could be found.",
                 "string", "name");
    DOC_METHOD_1("game.Entity", "GetEntity", "Get an entity at the given index.", "int", "index");
    DOC_METHOD_1("void", "KillEntity", "Kill the entity. Killing an entity doesn't not immediately remove it from the scene but will only "
                                       "set a flag that will indicate the new state of the entity. The entity will then continue to exist "
                                       "for one more iteration of the game loop until it's deleted at the end of this iteration.<br>"
                                       "This two step design allows any engine subsystems to realize and react to entities being killed.",
                                       "game.Entity", "carcass");
    DOC_METHOD_1("game.Entity", "SpawnEntity", "Spawn a new entity in the scene. Spawning an entity doesn't immediately place it in the scene "
                                               "but will only add it to the list of objects to be spawned at the start of the next iteration of game loop."
                                               "At the start of the next game loop each entity that was spawned will have their spawn flag set.<br>"
                                               "This two step design allows any engine subsystem to realize and react to entities being spawned.",
                 "game.EntityArgs", "args");
    DOC_METHOD_1("glm.mat4", "FindEntityTransform", "Find the transform for transforming the entity into the world/scene coordinate space.",
                 "game.Entity", "entity");
    DOC_METHOD_2("glm.mat4", "FindEntityNodeTransform", "Find the transform for transforming the entity node into the the world/scene coordinate space.",
                 "game.Entity", "entity", "game.EntityNode", "node");
    DOC_METHOD_0("float", "GetTime", "Get the scene's current time.");
    DOC_METHOD_0("string", "GetClassName", "Get the name of the scene's class.");
    DOC_METHOD_0("string", "GetClassId", "Get the ID of the scene's class.");

    DOC_TABLE("game.Physics");
    DOC_METHOD_2("void", "ApplyImpulseToCenter", "Apply an impulse to the center of the given entity node.<br>"
                                                "The entity node should have a rigid body item.",
                "string", "id", "glm.vec2", "impulse");
    DOC_METHOD_2("void", "ApplyImpulseToCenter", "Apply an impulse to the center of the given entity node.<br>"
                                                "The entity node should have a rigid body item.",
                "game.EntityNode", "node", "glm.vec2", "impulse");
    DOC_METHOD_2("void", "SetLinearVelocity", "Immediately adjust the linear velocity of the rigid body to the given velocity value.",
                 "string", "id", "glm.vec2", "velocity");
    DOC_METHOD_2("void", "SetLinearVelocity", "Immediately adjust the linear velocity of the rigid body to the given velocity value.",
                 "game.EntityNode", "node", "glm.vec2", "velocity");


    DOC_TABLE("MouseEvent");
    DOC_PROPERTY("glm.vec2", "window_coord", "Mouse cursor position in native window coordinates.");
    DOC_PROPERTY("glm.vec2", "scene_coord", "Mouse cursor position in scene coordinates.<br>"
                                            "Only valid when over_scene is true.");
    DOC_PROPERTY("int", "button", "The mouse button value that was pressed.<br>"
                                  "For a list of available buttons see wdk.Buttons");
    DOC_PROPERTY("int", "modifiers", "A bit string of keyboard modifier keys that were pressed.<br>"
                                     "For a list of available modifiers see wdk.Mods.<br>"
                                     "For testing a modifier use wdk.TestMod(bits, key).");
    DOC_PROPERTY("bool", "over_scene", "True when the mouse is within the game viewport in the window.<br>"
                                       "Indicates whether sceen_coords are valid or not.");


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
        if (role == Qt::SizeHintRole)
            return QSize(0, 16);
        else if (role == Qt::DisplayRole) {
            if (index.column() == 0) return app::FromUtf8(doc.table);
            else if (index.column() == 1) return app::toString(doc.type);
            else if (index.column() == 2) return app::FromUtf8(doc.name);
            else if (index.column() == 3) return app::FromUtf8(doc.desc);
        } else if (role == Qt::DecorationRole && index.column() == 1)  {
            if (doc.type == LuaMemberType::Function)
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

ScriptWidget::ScriptWidget(app::Workspace* workspace)
{
    InitDoc();

    mWorkspace = workspace;
    mTableModel.reset(new TableModel);
    QPlainTextDocumentLayout* layout = new QPlainTextDocumentLayout(&mDocument);
    layout->setParent(this);
    mDocument.setDocumentLayout(layout);
    DEBUG("Create ScriptWidget");

    mUI.setupUi(this);
    mUI.actionFind->setShortcut(QKeySequence::Find);
    //mUI.actionReplace->setShortcut(QKeySequence::Replace);
    mUI.find->setVisible(false);
    mUI.code->SetDocument(&mDocument);
    mUI.tableView->setModel(mTableModel.get());
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
        if (member.type == LuaMemberType::Function)
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
            stream << QString(
R"(<div class="method" name="%1_%2" id="%1_%2">
  <div class="signature">
     <span class="return">%3 </span>
     <span class="method">%4</span>(%5)
  </div>
  <div class="description">%6</div>
</div>
)").arg(app::FromUtf8(member.table)).arg(app::FromUtf8(member.name))
                    .arg(app::FromUtf8(member.ret)).arg(app::FromUtf8(member.name))
                    .arg(app::FromUtf8(args))
                    .arg(app::FromUtf8(member.desc));
        }
        else
        {
            stream << QString(
R"(<div class="member" name="%1_%2" id="%1_%2">
   <div class="signature">
      <span class="return">%3 </span>
      <span class="method">%4 </span>
   </div>
   <div class="description">%5</div>
</div>
)").arg(app::FromUtf8(member.table)).arg(app::FromUtf8(member.name))
   .arg(app::FromUtf8(member.ret)).arg(app::FromUtf8(member.name))
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
    bar.addAction(mUI.actionFind);
    bar.addAction(mUI.actionReplace);
}
void ScriptWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionSave);
    menu.addAction(mUI.actionFind);
    menu.addAction(mUI.actionReplace);
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

void ScriptWidget::on_actionFind_triggered()
{
    mUI.find->setVisible(true);
    mUI.findText->setFocus();
    SetValue(mUI.find, QString("Find text"));
    SetValue(mUI.findResult, QString(""));
    SetEnabled(mUI.btnReplaceNext, false);
    SetEnabled(mUI.btnReplaceAll, false);
    SetEnabled(mUI.replaceText, false);
}

void ScriptWidget::on_actionReplace_triggered()
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

void ScriptWidget::FileWasChanged()
{
    DEBUG("File was changed '%1'", mFilename);

    mWatcher.addPath(mFilename);

    // our hash is computed on save and load. if the hash of the
    // file contents is now something else then someone else has
    // changed the file somewhere else.
    QFile io;
    io.setFileName(mFilename);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        // todo: the file could have been removed or renamed.
        ERROR("Failed to open '%1' for reading. ('%2')", mFilename, io.error());
        return;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const size_t hash = qHash(stream.readAll());
    if (hash == mFileHash)
        return;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("The file has been modified. Reload file?"));
    if (msg.exec() == QMessageBox::No)
        return;

    LoadDocument(mFilename);
}

void ScriptWidget::keyPressEvent(QKeyEvent *key)
{
    if (key->key() == Qt::Key_Escape ||
       (key->key() == Qt::Key_G && key->modifiers() & Qt::ControlModifier))
    {
        mUI.find->setVisible(false);
        return;
    }
    QWidget::keyPressEvent(key);
}

bool ScriptWidget::LoadDocument(const QString& file)
{
    QFile io;
    io.setFileName(file);
    io.open(QIODevice::ReadOnly);
    if (!io.isOpen())
    {
        ERROR("Failed to open '%1' for reading. (%2)", file, io.error());
        return false;
    }
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    const QString& data = stream.readAll();

    mDocument.setPlainText(data);
    mFileHash = qHash(data);
    mFilename = file;
    DEBUG("Loaded script file '%1'", mFilename);
    return true;
}

void ScriptWidget::TableSelectionChanged(const QItemSelection, const QItemSelection&)
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    for (const auto& i : indices)
    {
        const auto& method = g_method_docs[i.row()];
        const auto& name = QString("%1_%2").arg(app::FromUtf8(method.table)).arg(app::FromUtf8(method.name));
        //mUI.textEdit->scrollToAnchor(name);
        mUI.textBrowser->scrollToAnchor(name);
        DEBUG("ScrollToAnchor '%1'", name);
    }
}

} // namespace
