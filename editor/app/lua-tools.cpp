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
#  include <QString>
#  include <QTextBlock>
#  include <QTextDocument>
#  include <tree_sitter/api.h>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>

#include "base/assert.h"
#include "base/utility.h"
#include "editor/app/utility.h"
#include "editor/app/lua-tools.h"

extern "C" TSLanguage* tree_sitter_lua();

TSPoint  operator + (const TSPoint& lhs, const TSPoint& rhs)
{
    TSPoint ret;
    ret.column = lhs.column + rhs.column;
    ret.row    = lhs.row + rhs.row;
    return ret;
}

namespace {
TSPoint PointFromOffset(const QByteArray& buffer, uint32_t start_offset, uint32_t end_offset)
{
    ASSERT(start_offset <= end_offset);
    ASSERT(end_offset <= buffer.size());

    TSPoint start_position = {0, 0};
    for (uint32_t i=start_offset; i<end_offset; ++i)
    {
       if (buffer[i] == '\n') {
           start_position.row++;
           start_position.column = 0;
        } else {
           start_position.column++;
       }
    }
    return start_position;
}

bool EqualContent(const QByteArray& a, const QByteArray& b, uint32_t start_offset, uint32_t end_offset)
{
    if (start_offset + end_offset > a.size())
        return false;
    if (start_offset + end_offset > b.size())
        return false;

    for (uint32_t i=start_offset; i<end_offset; ++i)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

} // namespace

namespace app {

void LuaTheme::SetTheme(Theme theme)
{
    if (theme == Theme::Monokai)
    {
        const QColor Background("#2e2e2e");
        const QColor Comments("#797979");
        const QColor White("#d6d6d6");
        const QColor Yellow("#e5b567");
        const QColor Green("#b4d273");
        const QColor Orange("#e87d3e");
        const QColor Purple("#9e86c8");
        const QColor Pink("#b05279");
        const QColor Blue("#6c99bb");

        mTable[Key::Keyword]      = Blue;
        mTable[Key::Comment]      = Green;
        mTable[Key::BuiltIn]      = Orange;
        mTable[Key::FunctionBody] = Orange;
        mTable[Key::FunctionCall] = Orange;
        mTable[Key::MethodCall]   = Orange;
        mTable[Key::Property]     = Yellow;
        mTable[Key::Literal]      = Purple;
        mTable[Key::Operator]     = Orange;
        mTable[Key::Bracket]      = Green;
    }
}

const QColor* LuaTheme::GetColor(Key key) const noexcept
{
    return base::SafeFind(mTable, key);
}

LuaParser::LuaParser()
{
    mParser = ts_parser_new();
    ts_parser_set_language(mParser, tree_sitter_lua());
}

LuaParser::~LuaParser()
{
    if (mTree)
        ts_tree_delete(mTree);
    if (mQuery)
        ts_query_delete(mQuery);
    if (mParser)
        ts_parser_delete(mParser);
}

const LuaParser::SyntaxBlock* LuaParser::FindBlock(uint32_t position) const noexcept
{
    // find first highlight with starting position equal or greater than position
    auto it = std::lower_bound(mBlocks.begin(), mBlocks.end(), position,
        [](const auto& block, uint32_t position) {
            return block.start < position;
        });
    if (it == mBlocks.end())
        return nullptr;

    if (it->start == position)
        return &(*it);

    while (it != mBlocks.begin())
    {
        const auto& hilight = *it;
        if (position >= hilight.start && position <= hilight.start + hilight.length)
            return &hilight;
        --it;
    }
    return nullptr;
}

LuaParser::BlockList LuaParser::FindBlocks(uint32_t position, uint32_t text_length) const noexcept
{
    // find first code block with start position greater than or equal to the
    // text position.
    auto it = std::lower_bound(mBlocks.begin(), mBlocks.end(), position,
        [](const auto& block, uint32_t start) {
           return block.start < start;
        });

    BlockList  ret;

    // fetch blocks until block is beyond the current text range.
    for (; it != mBlocks.end(); ++it)
    {
        const auto& highlight = *it;
        const auto start  = highlight.start;
        const auto length = highlight.length;
        if (start >= position + text_length)
            break;

        ret.push_back(highlight);
    }
    return ret;
}

void LuaParser::ClearParseState()
{
    mBlocks.clear();
    if (mTree)
        ts_tree_delete(mTree);

    mTree = nullptr;
}

bool LuaParser::ParseSource(const QString& source)
{
    ts_parser_reset(mParser);

    struct BufferReader {
        static const char* read(void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read)
        {
            auto* buffer = static_cast<QByteArray*>(payload);
            *bytes_read = buffer->size() - byte_index;
            return buffer->constData() + byte_index;
        }
    };

    // The problem here is that he QString (and QDocument) is in Unicode (using whatever encoding)
    // and it'd need to be parsed. Apparently someone was sniffing some farts before going on about
    // inventing ts_parser_parse_string_encoding API with 'const char* string' being used for UTF16.
    // What does that mean exactly? UTF16 should use a variable number of 16 bit code-units. Also,
    // should the length then be Unicode characters, bytes or UTF16 sequences ?
    // If we take the incoming string and convert into UTF8 everything sort of works except that
    // all tree-sitter offsets will be wrong since they're now offsets into the UTF8 string buffer
    // and those don't map to Unicode string properly anymore.
    QByteArray buffer = source.toUtf8();

    TSInput input;
    input.encoding = TSInputEncodingUTF8;
    input.payload  = &buffer;
    input.read     = &BufferReader::read;
    auto* tree = ts_parser_parse(mParser, mTree, input);
    if (tree == nullptr)
        return false;

    ConsumeTree(source, tree);
    FindBuiltins(source);

    if (mTree)
        ts_tree_delete(mTree);
    mTree = tree;
    return true;
}

void LuaParser::EditSource(const Edit& edit)
{
    // the source must have been parsed already.
    ASSERT(mTree);

    ASSERT((edit.characters_added != 0 && edit.characters_removed == 0) ||
           (edit.characters_added == 0 && edit.characters_removed != 0));
    ASSERT(edit.position <= edit.new_source->size());
    ASSERT(edit.position <= edit.old_source->size());

    const QByteArray& old_buffer = edit.old_source->toUtf8();
    const QByteArray& new_buffer = edit.new_source->toUtf8();

    // I.e. the invariant that must hold is that the sources must equal each other
    // from the start until the point of edit. Going to remove this ASSERT for now
    // since it's potentially O(N) scan on every change and the client is correct.
    // use for development only.
    //ASSERT(EqualContent(old_buffer, new_buffer, 0, edit.position));

    // starting point of the edit in rows and columns
    // if we computed the start point for also the new_buffer it should
    // match the start_point for the old buffer completely.!
    const TSPoint start_point = PointFromOffset(old_buffer, 0, edit.position);

    // not a lot of information documentation-wise around for how to use the
    // ts_tree_edit API, but I managed to find this old test code from around 2018
    // https://github.com/tree-sitter/tree-sitter/blob/2169c45da99ac6c999c9b96ff470d0b5229e5248/test/helpers/spy_input.cc
    // https://github.com/tree-sitter/tree-sitter/blob/2169c45da99ac6c999c9b96ff470d0b5229e5248/test/runtime/tree_test.cc
    // This issue from around the same time links to these test files.
    // https://github.com/tree-sitter/tree-sitter/issues/242

    TSInputEdit ts_edit;
    ts_edit.start_byte   = edit.position;
    ts_edit.old_end_byte = edit.position + edit.characters_removed;
    ts_edit.new_end_byte = edit.position + edit.characters_added;
    ts_edit.start_point  = start_point;
    // figure out old ending point in rows and columns. need to use the old copy of
    // the buffer data here. since it seems that, there's no Qt way of getting the
    // edited change from the underlying document
    ts_edit.old_end_point = start_point + PointFromOffset(old_buffer, ts_edit.start_byte, ts_edit.old_end_byte);
    ts_edit.new_end_point = start_point + PointFromOffset(new_buffer, ts_edit.start_byte, ts_edit.new_end_byte);

    ts_tree_edit(mTree, &ts_edit);
}

void LuaParser::ConsumeTree(const QString& source, TSTree* ast)
{
    // S expressions

    // a good place to try and see how/what type of S expressions to create
    // to query the tree is the Lua grammar tests for tree-sitter.
    // https://github.com/Azganoth/tree-sitter-lua/tree/master/test/corpus

    // alternative is to use the test app under third_party/tree-sitter and
    // print the S expression to stdout and see what expression some Lua code maps to.

    // '@something' is a capture
    // '#any-of?' is a predicate, but  PREDICATES ARE NOT ACTUALLY PROCESSED BY tree-sitter !
    // '#any-of? @something' checks a capture for any of the following patterns.
    // '_' is a wildcard
    // ;; is a comment

    if (!mQuery)
    {
        // not working
        // "require"
        const char* q = R"foo(
;; pattern 0
;; a bunch of keywords.
[
  "and"
  "do"
  "else"
  "elseif"
  "end"
  "for"
  "function"
  "goto"
  "if"
  "in"
  "local"
  "not"
  "or"
  "repeat"
  "return"
  "then"
  "until"
  "while"
] @keyword

;; pattern 1, nil
;; @keyword is removed now into literals
(nil) ;; @keyword

;; pattern 2 comment

(comment) @comment

;; pattern 3
;; match MyFunction()  or MyFunction(123)

(call
   function:  (variable name: (identifier) @function_name )
   arguments: (argument_list)
)

;; pattern 4
;; match table.SomeFunction() or table.SomeFunction(123)

(call
   function:  (variable table: (_)
                        field: (identifier) @function_name)
   arguments: (argument_list)
)

;; pattern 5
;; match object:SomeMethod() or object::SomeMethod(123)

(call
    function: (variable table:   (_)
                         method: (identifier) @function_name)
    arguments: (argument_list)
)

;; pattern 6
;; match table.field syntax.
;; note that this will also yield a capture for foo.bar.meh
;; and foo.Function() since those (call) expressions also
;; contain non-terminal (variable table: (identifier) ...
;; but this is actually fine since the previous matches
;; capture just the method/function names.

(variable table: (identifier)
          field: (identifier) @field_name)

;; pattern 7
;; match function MyFunction() ... end
(function_definition_statement name: (identifier) @function_def_name)

;; pattern 8
[
   (true)
   (false)
   (string)
   (number)
   (nil)
] @literal

;; pattern 9
;; punctuation
[
  ";"
  ":"
  "::"
  ","
  "."
] @punctuation_delim

;; pattern 10
;; brackets
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @bracket

;; pattern 11
;; operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "^"
  "#"
  "=="
  "~="
  "<="
  ">="
  "<"
  ">"
  "="
  "&"
  "~"
  "|"
  "<<"
  ">>"
  "//"
  ".."
] @operator

;; pattern 12
(break_statement) @keyword

)foo";
        uint32_t error_offset;
        TSQueryError error_type;
        mQuery = ts_query_new(tree_sitter_lua(), q, strlen(q), &error_offset, &error_type);
        // for debugging/unit tests
        //if (mQuery == nullptr)
        //{
        //    printf("query = %p err at %d, type=%d\n", mQuery, error_offset, error_type);
        //    printf("error = %s\n", q + error_offset);
        //}
        ASSERT(mQuery);
    }

    mBlocks.clear();

    TSNode root = ts_tree_root_node(ast);
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, mQuery, root);

    TSQueryMatch match;
    uint32_t capture_index = 0;
    while (ts_query_cursor_next_capture(cursor, &match, &capture_index))
    {
        const TSQueryCapture& capture = match.captures[capture_index];
        const TSNode& node = capture.node;
        const auto start   = ts_node_start_byte(node);
        const auto end     = ts_node_end_byte(node);
        const auto length  = end - start;
        const auto pattern = match.pattern_index;

        // for debugging/unit-test dev
        //const auto bleh = narrow.substr(start, length);
        //printf("%-20s pattern=%d\tcapture=%d\tstart=%d\tlen=%d\n", bleh.c_str(), pattern, capture_index, start, length);

        SyntaxBlock block;
        block.type   = LuaSyntax::Keyword;
        block.start  = start;
        block.length = length;
        if (pattern == 0 || pattern == 1 || pattern == 12)
            block.type = LuaSyntax::Keyword;
        else if (pattern == 2)
            block.type = LuaSyntax::Comment;
        else if (pattern == 3)
            block.type = LuaSyntax::FunctionCall;
        else if (pattern == 4)
            block.type = LuaSyntax::FunctionCall;
        else if (pattern == 5)
            block.type = LuaSyntax ::MethodCall;
        else if (pattern == 6)
        {
            // kludge for not matching table.Function() as a property
            if (end < source.size() && source[end] == '(')
                continue;
            block.type = LuaSyntax::Property;
        }
        else if (pattern == 7)
            block.type = LuaSyntax::FunctionBody;
        else if (pattern == 8)
            block.type = LuaSyntax::Literal;
        else if (pattern == 9)
            block.type = LuaSyntax::Punctuation;
        else if (pattern == 10)
            block.type = LuaSyntax::Bracket;
        else if (pattern == 11)
            block.type = LuaSyntax::Operator;
        else BUG("Missing capture branch.");

        // lower bound returns an iterator pointing to a first value in the range
        // such that the contained value is equal or greater than searched value
        // or end() when no such value is found.
        auto it = std::lower_bound(mBlocks.begin(), mBlocks.end(), start,
             [](const SyntaxBlock& other, uint32_t start) {
                 return other.start < start;
        });
        mBlocks.insert(it, block);
    }

    ts_query_cursor_delete(cursor);
    //ts_tree_delete(ast);
}

void LuaParser::FindBuiltins(const QString& source)
{
    static std::unordered_set<QString> builtin_functions = {
        "assert", "collectgarbage", "dofile", "error", "getfenv", "getmetatable", "ipairs",
        "load", "loadfile", "loadstring", "module", "next", "pairs", "pcall", "print",
        "rawequal", "rawget", "rawlen", "rawset", "require", "select", "setfenv", "setmetatable",
        "tonumber", "tostring", "type", "unpack", "xpcall",
        "__add", "__band", "__bnot", "__bor", "__bxor", "__call", "__concat",  "__div", "__eq", "__gc",
        "__idiv", "__index", "__le", "__len", "__lt", "__metatable", "__mod", "__mul", "__name", "__newindex",
        "__pairs", "__pow", "__shl", "__shr", "__sub", "__tostring", "__unm"
    };
    for (auto& block : mBlocks)
    {
        if (block.type != LuaSyntax::FunctionCall)
            continue;
        const auto& name = source.mid((int)block.start, (int)block.length);
        if (base::Contains(builtin_functions, name))
            block.type = LuaSyntax::BuiltIn;
    }
}

} // namespace

