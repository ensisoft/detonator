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

LuaParser::~LuaParser()
{
    if (mQuery)
        ts_query_delete(mQuery);
    if (mTree)
        ts_tree_delete(mTree);
    if (mParser)
        ts_parser_delete(mParser);
}

const LuaParser::CodeBlock* LuaParser::FindBlock(uint32_t position) const noexcept
{
    // find first highlight with starting position equal or greater than position
    auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), position,
        [](const auto& block, uint32_t position) {
            return block.start < position;
        });
    if (it == mHighlights.end())
        return nullptr;

    if (it->start == position)
        return &(*it);

    while (it != mHighlights.begin())
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
    auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), position,
        [](const auto& block, uint32_t start) {
           return block.start < start;
        });

    BlockList  ret;

    // fetch blocks until block is beyond the current text range.
    for (; it != mHighlights.end(); ++it)
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

bool LuaParser::Parse(const QString& str)
{
    TSLanguage* language = tree_sitter_lua();
    if (!mParser)
    {
        mParser = ts_parser_new();
        ts_parser_set_language(mParser, language);
    }
    ts_parser_reset(mParser);

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

    static std::unordered_set<QString> builtin_functions = {
        "assert", "collectgarbage", "dofile", "error", "getfenv", "getmetatable", "ipairs",
        "load", "loadfile", "loadstring", "module", "next", "pairs", "pcall", "print",
        "rawequal", "rawget", "rawlen", "rawset", "require", "select", "setfenv", "setmetatable",
        "tonumber", "tostring", "type", "unpack", "xpcall",
        "__add", "__band", "__bnot", "__bor", "__bxor", "__call", "__concat",  "__div", "__eq", "__gc",
        "__idiv", "__index", "__le", "__len", "__lt", "__metatable", "__mod", "__mul", "__name", "__newindex",
        "__pairs", "__pow", "__shl", "__shr", "__sub", "__tostring", "__unm"
    };

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
        mQuery = ts_query_new(language, q, strlen(q), &error_offset, &error_type);
        // for debugging/unit tests
        //if (mQuery == nullptr)
        //{
        //    printf("query = %p err at %d, type=%d\n", mQuery, error_offset, error_type);
        //    printf("error = %s\n", q + error_offset);
        //}
        ASSERT(mQuery);
    }

    mHighlights.clear();


    // The problem here is that he QString (and QDocument) is in Unicode (using whatever encoding)
    // and it'd need to be parsed. Apparently someone was sniffing some farts before going on about
    // inventing ts_parser_parse_string_encoding API with 'const char* string' being used for UTF16.
    // What does that mean exactly? UTF16 should use a variable number of 16 bit code-units. Also,
    // should the length then be Unicode characters, bytes or UTF16 sequences ?
    // If we take the incoming string and convert into UTF8 everything sort of works except that
    // all tree-sitter offsets will be wrong since they're now offsets into the UTF8 string buffer
    // and those don't map to Unicode string properly anymore.

    const auto& utf8 = str.toUtf8();
    auto* tree = ts_parser_parse_string_encoding(mParser, nullptr, utf8, utf8.size(), TSInputEncodingUTF8);

    /* for posterity
    std::vector<unsigned short> utf16;
    const auto* p = str.utf16();
    while (*p) {
        utf16.push_back(*p++);
    }
    auto* tree = ts_parser_parse_string_encoding(mParser, nullptr, (const char*)utf16.data(), utf16.size()*2, TSInputEncodingUTF16);
     */

    if (tree == nullptr)
        return false;

    TSNode root = ts_tree_root_node(tree);
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

        CodeBlock block;
        block.type   = BlockType::Keyword;
        block.start  = start;
        block.length = length;
        if (pattern == 0 || pattern == 1 || pattern == 12)
        {
            block.type = BlockType::Keyword;
        }
        else if (pattern == 2)
        {
            block.type = BlockType::Comment;
        }
        else if (pattern == 3)
        {
            const auto name = str.mid((int)start, (int)length);
            if (base::Contains(builtin_functions, name))
                block.type = BlockType::BuiltIn;
            else block.type = BlockType::FunctionCall;
        }
        else if (pattern == 4)
        {
            block.type = BlockType::FunctionCall;
        }
        else if (pattern == 5)
        {
            block.type = BlockType ::MethodCall;
        }
        else if (pattern == 6)
        {
            block.type = BlockType::Property;
        }
        else if (pattern == 7)
        {
            block.type = BlockType::FunctionBody;
        }
        else if (pattern == 8)
        {
            block.type = BlockType::Literal;
        }
        else if (pattern == 9)
        {
            block.type = BlockType::Punctuation;
        }
        else if (pattern == 10)
        {
            block.type = BlockType::Bracket;
        }
        else if (pattern == 11)
        {
            block.type = BlockType::Operator;
        }

        // lower bound returns an iterator pointing to a first value in the range
        // such that the contained value is equal or greater than searched value
        // or end() when no such value is found.
        auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), start,
             [](const CodeBlock& other, uint32_t start) {
                 return other.start < start;
        });
        mHighlights.insert(it, block);
    }

    ts_query_cursor_delete(cursor);

    if (mTree)
        ts_tree_delete(mTree);
    mTree = tree;

    return true;
}

} // namespace

