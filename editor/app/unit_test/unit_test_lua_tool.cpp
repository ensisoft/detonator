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

void unit_test_keywords()
{
    QTextDocument doc;

    app::LuaParser p;

    const char* code = R"(
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

   foo.property = 123
   foo.other.property = 333

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

    for (unsigned i=0; i<p.GetNumBlocks(); ++i)
    {
        const auto& hilight = p.GetBlock(i);
        const char* beg = code + hilight.start;
        const char* end = code + hilight.start + hilight.length;
        const std::string str(beg, end);
        printf("%-20s %-20s start=%d\tlen=%d\n", Str(hilight.type), str.c_str(), hilight.start, hilight.length);
    }

}

int test_main(int argc, char* argv[])
{
    unit_test_keywords();

    return 0;
} //