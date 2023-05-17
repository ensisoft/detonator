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
#  include <QTextDocument>
#include "warnpop.h"

#include "base/test_minimal.h"
#include "editor/app/lua-tools.h"


const char* Str(app::LuaTheme::Key key) {
    using K = app::LuaTheme::Key;
    if (key == K::MethodCall)
        return "MethodCall";
    else if (key == K::FunctionCall)
        return "FunctionCall";
    else if (key == K::BuiltIn)
        return "BuiltIn";
    else if (key == K::Keyword)
        return "Keyword";
    else if (key == K::Comment)
        return "Comment";
    else if (key == K::Literal)
        return "Literal";
    else if (key == K::Bracket)
        return "Bracket";
    else if (key == K::Operator)
        return "Operator";
    else if (key == K::Punctuation)
        return "Punctuation";
    else if (key == K::FunctionBody)
        return "FuncBody";
    else if (key == K::Property)
        return "Property";
    return "???";
}

const app::LuaParser::SyntaxBlock* FindBlock(const app::LuaParser& p, const QString& code, const QString& key)
{
    for (size_t i=0; i<p.GetNumBlocks(); ++i)
    {
        const auto& block = p.GetBlock(i);
        const auto& tmp = code.mid(block.start, block.length);
        if (tmp == key)
            return &block;
    }
    static app::LuaParser::SyntaxBlock fallback;
    fallback.type = app::LuaSyntax::Other;
    return &fallback;
}

void unit_test_syntax()
{
    app::LuaParser p;

    const QString code = R"(
require('kek')

-- comment line

--[[ a comment ]]

local meh = 123

function SomeFunction()
   local heh = 123
   local str = 'jeesus ajaa mopolla'
   local foo = 1.0
   local bar = true
   local kek = nil

   local meh = foo.property
   local huh = foo.other.property

   foo.property = 45
   foo.other_prop.property2 = 333

   if a and b then
      print('a+b')
   elseif a then
       print('a')
   else
      print('else')
   end

   while true do
      print('loop')
      break
   end

   for i=1, 10 do

   end

   assert()

   MyFunction1()
   MyFunction2(123)

   glm.MyTableMethod3()
   glm.MyTableMethod4(333)

   glm.foo.Method5_1()
   glm.foo.Method5_2(123)

   glm.foo:Method6_1()
   glm.foo:Method6_2(123)

   object:MyObjectMethod7()
   object:MyObjectMethod8(123)

   return 1234
end
    )";
    p.ParseSource(code);

    // debug
    /*
    {
        const auto& str = code.toStdString();
        const auto* ptr = str.c_str();

        for (unsigned i = 0; i < p.GetNumBlocks(); ++i)
        {
            const auto& hilight = p.GetBlock(i);
            const char* beg = ptr + hilight.start;
            const char* end = ptr + hilight.start + hilight.length;
            const std::string str(beg, end);
            printf("%-20s %-20s start=%d\tlen=%d\n", Str(hilight.type), str.c_str(), hilight.start, hilight.length);
        }
    }
     */

    TEST_CHECK(FindBlock(p, code, "require")->type == app::LuaSyntax::BuiltIn);
    TEST_CHECK(FindBlock(p, code, "'kek'")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "-- comment line")->type == app::LuaSyntax::Comment);
    TEST_CHECK(FindBlock(p, code, "--[[ a comment ]]")->type == app::LuaSyntax::Comment);
    TEST_CHECK(FindBlock(p, code, "SomeFunction")->type == app::LuaSyntax::FunctionBody);
    TEST_CHECK(FindBlock(p, code, "local")->type == app::LuaSyntax::Keyword); // multiple
    TEST_CHECK(FindBlock(p, code, "123")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "'jeesus ajaa mopolla'")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "1.0")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "true")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "nil")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "property")->type == app::LuaSyntax::Property);
    TEST_CHECK(FindBlock(p, code, "45")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "other_prop")->type == app::LuaSyntax::Property);
    // todo fix this
    //TEST_REQUIRE(FindBlock(p, code, "property2")->type == app::LuaSyntax::Property);
    TEST_CHECK(FindBlock(p, code, "if")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "and")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "then")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "print")->type == app::LuaSyntax::BuiltIn);
    TEST_CHECK(FindBlock(p, code, "'a+b'")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "elseif")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "else")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "end")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "while")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "do")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "break")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "for")->type == app::LuaSyntax::Keyword);
    TEST_CHECK(FindBlock(p, code, "assert")->type == app::LuaSyntax::BuiltIn);
    TEST_CHECK(FindBlock(p, code, "MyFunction1")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "MyFunction2")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "MyTableMethod3")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "MyTableMethod4")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "333")->type == app::LuaSyntax::Literal);
    TEST_CHECK(FindBlock(p, code, "Method5_1")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "Method5_2")->type == app::LuaSyntax::FunctionCall);
    TEST_CHECK(FindBlock(p, code, "Method6_1")->type == app::LuaSyntax::MethodCall);
    TEST_CHECK(FindBlock(p, code, "Method6_2")->type == app::LuaSyntax::MethodCall);
    TEST_CHECK(FindBlock(p, code, "MyObjectMethod7")->type == app::LuaSyntax::MethodCall);
    TEST_CHECK(FindBlock(p, code, "MyObjectMethod8")->type == app::LuaSyntax::MethodCall);
    TEST_CHECK(FindBlock(p, code, "return")->type == app::LuaSyntax::Keyword);
}

void unit_test_symbols()
{
const QString code = R"(
local bleh = true

function SomeFunction()
   local something = true
   local foo = 'balla'
end
    )";

    app::LuaParser p;
    p.ParseSource(code);

    TEST_CHECK(p.FindSymbol("bleh"));
    TEST_CHECK(p.FindSymbol("something"));
    TEST_CHECK(p.FindSymbol("foo"));

    if (const auto* s = p.FindSymbol("bleh"))
        TEST_REQUIRE(s->type == app::LuaSymbol::LocalVariable);
    if (const auto* s = p.FindSymbol("something"))
        TEST_REQUIRE(s->type == app::LuaSymbol::LocalVariable);
    if (const auto* s = p.FindSymbol("foo"))
        TEST_REQUIRE(s->type == app::LuaSymbol::LocalVariable);
}

int test_main(int argc, char* argv[])
{
    unit_test_syntax();
    unit_test_symbols();
    return 0;
} //