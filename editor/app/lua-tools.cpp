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

const LuaParser::Highlight* LuaParser::FindBlockByOffset(uint32_t position) const noexcept
{
    // find first highlight with starting position equal or greater than position
    auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), position,
        [](const auto& hilight, uint32_t position) {
            return hilight.start < position;
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

void LuaParser::highlightBlock(const QString& text)
{
    // potential problem here that the characters in the plainText() result
    // don't map exactly to the text characters in the text document blocks.
    // Maybe this should use a cursor ? (probably slow as a turtle)
    // Investigate this more later if there's a problem .

    // If this is enabled then we're parsing and reparsing the document
    // all the time. maybe pointless, anyway, let the higher level to
    // decide when to parse the document, for example on a key press or
    // on save or on Enter or whatever.
    //QTextDocument* document = this->document();
    //Parse(document->toPlainText());

    QTextBlock block = currentBlock();
    // character offset into the document where the block begins.
    // the position includes all characters prior to including formatting
    // characters such as newline.
    const auto block_start  = block.position();
    const auto block_length = block.length();

    if (const auto* color = mTheme.GetColor(LuaTheme::Key::Other))
    {
        QTextCharFormat format;
        format.setForeground(*color);
        setFormat(0, block_length, format);
    }

    auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), (uint32_t)block_start,
        [](const auto& hilight, uint32_t start) {
           return hilight.start < start;
        });

    for (; it != mHighlights.end(); ++it)
    {
        const auto& highlight = *it;
        const auto start  = highlight.start;
        const auto length = highlight.length;
        if (start >= block_start + block_length)
            break;

        if (auto* color = mTheme.GetColor(highlight.type))
        {
            QTextCharFormat format;
            format.setForeground(*color);

            const auto block_offset = start - block_start;
            // setting the format is specified in offsets into the block itself!
            setFormat((int)block_offset, (int)length, format);
        }
    }
}

bool LuaParser::Parse(const QString& str)
{
    TSLanguage* language = tree_sitter_lua();
    if (!mParser)
    {
        mParser = ts_parser_new();
        ts_parser_set_language(mParser, language);
        const auto count = ts_language_field_count(language);
        for (int i=0; i<count; ++i)
        {
            const char* name = ts_language_field_name_for_id(language, i);
            if (name == nullptr) continue;
            mFields[name] = i;
        }
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

        Highlight hilight;
        hilight.type   = Highlight::Type::Keyword;
        hilight.start  = start;
        hilight.length = length;
        if (pattern == 0 || pattern == 1 || pattern == 12)
        {
            hilight.type = Highlight::Type::Keyword;
        }
        else if (pattern == 2)
        {
            hilight.type = Highlight::Type::Comment;
        }
        else if (pattern == 3)
        {
            const auto name = str.mid((int)start, (int)length);
            if (base::Contains(builtin_functions, name))
                hilight.type = Highlight::Type::BuiltIn;
            else hilight.type = Highlight::Type::FunctionCall;
        }
        else if (pattern == 4)
        {
            hilight.type = Highlight::Type::FunctionCall;
        }
        else if (pattern == 5)
        {
            hilight.type = Highlight::Type ::MethodCall;
        }
        else if (pattern == 6)
        {
            hilight.type = Highlight::Type::Property;
        }
        else if (pattern == 7)
        {
            hilight.type = Highlight::Type::FunctionBody;
        }
        else if (pattern == 8)
        {
            hilight.type = Highlight::Type::Literal;
        }
        else if (pattern == 9)
        {
            hilight.type = Highlight::Type::Punctuation;
        }
        else if (pattern == 10)
        {
            hilight.type = Highlight::Type::Bracket;
        }
        else if (pattern == 11)
        {
            hilight.type = Highlight::Type::Operator;
        }

        // lower bound returns an iterator pointing to a first value in the range
        // such that the contained value is equal or greater than searched value
        // or end() when no such value is found.
        auto it = std::lower_bound(mHighlights.begin(), mHighlights.end(), start,
             [](const Highlight& other, uint32_t start) {
                 return other.start < start;
        });
        mHighlights.insert(it, hilight);
    }

    ts_query_cursor_delete(cursor);

    if (mTree)
        ts_tree_delete(mTree);
    mTree = tree;

    return true;
}

} // namespace

